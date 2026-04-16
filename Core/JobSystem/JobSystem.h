#pragma once

#include "CoreSystem/CoreDebugChannels.h"
#include "Profiler/Profiler.h"

#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <atomic>
#include <memory>
#include <chrono>
#include <type_traits>
#include <concepts>
#include <string_view>
#include <ranges>
#include <format>
#include <algorithm>
#include <numeric>

// Forward declaration
template<typename T>
class ThreadSafeQueue;

// ── C++20 Concepts ────────────────────────────────────────────────────────────

template<typename F>
concept Callable = std::invocable<F>;

template<typename F, typename... Args>
concept CallableWithArgs = std::invocable<F, Args...>;

template<typename Container>
concept ArrayLike = requires(Container c)
{
    { c.begin() } -> std::forward_iterator;
    { c.end()   } -> std::forward_iterator;
    { c.size()  } -> std::convertible_to<size_t>;
};

template<typename F, typename T>
concept ArrayProcessor = std::invocable<F, T&> || std::invocable<F, const T&>;

template<typename F, typename T>
concept ArrayReducer = requires(F f, T a, T b)
{
    { f(a, b) } -> std::convertible_to<T>;
};

// ── Job ───────────────────────────────────────────────────────────────────────

/** @brief Wraps any callable for submission to the job system. */
class Job
{
public:
    template<Callable F>
    Job(F&& func) : m_task(std::forward<F>(func)) {}

    Job() = default;
    Job(Job&&) = default;
    Job& operator=(Job&&) = default;
    Job(const Job&) = delete;
    Job& operator=(const Job&) = delete;

    void Execute()
    {
        if (m_task)
            m_task();
    }

    bool IsValid() const { return static_cast<bool>(m_task); }

private:
    std::function<void()> m_task;
};

// ── ThreadSafeQueue ───────────────────────────────────────────────────────────

/** @brief Lock-based queue safe for concurrent push/pop from multiple threads. */
template<typename T>
class ThreadSafeQueue
{
public:
    ThreadSafeQueue() = default;
    ThreadSafeQueue(const ThreadSafeQueue&) = delete;
    ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;

    void Push(T item)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push(std::move(item));
        m_condition.notify_one();
    }

    bool TryPop(T& item)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_queue.empty())
            return false;
        item = std::move(m_queue.front());
        m_queue.pop();
        return true;
    }

    bool WaitAndPop(T& item, const std::atomic<bool>& shouldShutdown)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_condition.wait(lock, [this, &shouldShutdown]
        {
            return !m_queue.empty() || shouldShutdown.load(std::memory_order_acquire);
        });

        if (shouldShutdown.load(std::memory_order_acquire))
            return false;

        if (!m_queue.empty())
        {
            item = std::move(m_queue.front());
            m_queue.pop();
            return true;
        }
        return false;
    }

    bool IsEmpty() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.empty();
    }

    size_t Size() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.size();
    }

    void NotifyAll()
    {
        m_condition.notify_all();
    }

private:
    mutable std::mutex      m_mutex;
    std::queue<T>           m_queue;
    std::condition_variable m_condition;
};

// ── JobSystem ─────────────────────────────────────────────────────────────────

/** @brief Multi-threaded job scheduler with C++20 parallel array utilities. */
class JobSystem
{
public:
    explicit JobSystem(size_t numThreads = std::thread::hardware_concurrency())
        : m_shutdown(false), m_numThreads(numThreads), m_totalJobs(0)
    {
        if (m_numThreads == 0)
            m_numThreads = 1;

        m_workers.reserve(m_numThreads);
        for (size_t i = 0; i < m_numThreads; ++i)
            m_workers.emplace_back(&JobSystem::WorkerLoop, this, i);

        JobSystemDebug.printf("JobSystem initialized with %zu worker threads\n", m_numThreads);
    }

    ~JobSystem()
    {
        Shutdown();
    }

    // ── Job Submission ───────────────────────────────────────────────────────

    /** @param func Callable to run on a worker thread. */
    template<Callable F>
    void SubmitJob(F&& func)
    {
        if (m_shutdown.load(std::memory_order_acquire))
        {
            JobSystemDebug.warning("Cannot submit job: JobSystem is shutting down\n");
            return;
        }

        m_totalJobs.fetch_add(1, std::memory_order_acq_rel);

        m_jobQueue.Push(Job([func = std::forward<F>(func), this]()
        {
            try
            {
                func();
            }
            catch (const std::exception& e)
            {
                JobSystemDebug.warning("Job threw exception: %s\n", e.what());
            }
            catch (...)
            {
                JobSystemDebug.warning("Job threw unknown exception\n");
            }

            {
                std::lock_guard<std::mutex> lock(m_completionMutex);
                m_totalJobs.fetch_sub(1, std::memory_order_acq_rel);
            }
            m_jobCompleted.notify_all();
        }));
    }

    /** @brief Submit a job returning a value; caller receives a std::future. */
    template<typename F, typename... Args>
        requires CallableWithArgs<F, Args...>
    auto SubmitJobWithResult(F&& func, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>
    {
        using ReturnType = std::invoke_result_t<F, Args...>;

        if (m_shutdown.load(std::memory_order_acquire))
        {
            std::promise<ReturnType> promise;
            promise.set_exception(std::make_exception_ptr(
                std::runtime_error("Cannot submit job: JobSystem is shutting down")));
            return promise.get_future();
        }

        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            [func = std::forward<F>(func), ...args = std::forward<Args>(args)]() mutable
            {
                return std::invoke(func, args...);
            });

        std::future<ReturnType> result = task->get_future();
        m_totalJobs.fetch_add(1, std::memory_order_acq_rel);

        m_jobQueue.Push(Job([task, this]()
        {
            try
            {
                (*task)();
            }
            catch (...)
            {
                // Exception stored in future.
            }

            {
                std::lock_guard<std::mutex> lock(m_completionMutex);
                m_totalJobs.fetch_sub(1, std::memory_order_acq_rel);
            }
            m_jobCompleted.notify_all();
        }));

        return result;
    }

    // ── Parallel Array Utilities ─────────────────────────────────────────────

    /** @brief Process all elements of a container in parallel chunks. */
    template<ArrayLike Container, ArrayProcessor<typename Container::value_type> Func>
    void ParallelForEach(Container& container, Func func, size_t chunkSize = 0)
    {
        if (container.empty()) return;

        if (chunkSize == 0)
            chunkSize = CalculateOptimalChunkSize(container.size());

        std::vector<std::future<void>> futures;
        auto       it  = container.begin();
        const auto end = container.end();

        JobSystemDebug.printf("Processing array of size %zu with chunk size %zu\n", container.size(), chunkSize);

        while (it != end)
        {
            auto chunkEnd = it;
            std::advance(chunkEnd, std::min(chunkSize, static_cast<size_t>(std::distance(it, end))));

            auto future = SubmitJobWithResult([it, chunkEnd, func]() mutable
            {
                std::for_each(it, chunkEnd, func);
            });

            futures.push_back(std::move(future));
            it = chunkEnd;
        }

        for (auto& future : futures)
            future.wait();
    }

    /** @brief Apply a transformation to each element in parallel. */
    template<ArrayLike InputContainer, ArrayLike OutputContainer, typename Func>
        requires std::invocable<Func, typename InputContainer::value_type>
    void ParallelTransform(const InputContainer& input, OutputContainer& output, Func func, size_t chunkSize = 0)
    {
        output.resize(input.size());
        if (input.empty()) return;

        if (chunkSize == 0)
            chunkSize = CalculateOptimalChunkSize(input.size());

        std::vector<std::future<void>> futures;
        auto       inputIt  = input.begin();
        auto       outputIt = output.begin();
        const auto inputEnd = input.end();

        JobSystemDebug.printf("Transforming array of size %zu with chunk size %zu\n", input.size(), chunkSize);

        while (inputIt != inputEnd)
        {
            auto   inputChunkEnd    = inputIt;
            auto   outputChunkStart = outputIt;
            size_t remaining        = std::distance(inputIt, inputEnd);
            size_t currentChunk     = std::min(chunkSize, remaining);

            std::advance(inputChunkEnd, currentChunk);
            std::advance(outputIt,      currentChunk);

            auto future = SubmitJobWithResult([inputIt, inputChunkEnd, outputChunkStart, func]() mutable
            {
                std::transform(inputIt, inputChunkEnd, outputChunkStart, func);
            });

            futures.push_back(std::move(future));
            inputIt = inputChunkEnd;
        }

        for (auto& future : futures)
            future.wait();
    }

    /** @brief Reduce a container to a single value using parallel computation. */
    template<ArrayLike Container, typename T, ArrayReducer<T> BinaryOp>
    T ParallelReduce(const Container& container, T init, BinaryOp op, size_t chunkSize = 0)
    {
        if (container.empty()) return init;

        if (chunkSize == 0)
            chunkSize = CalculateOptimalChunkSize(container.size());

        std::vector<std::future<T>> futures;
        auto       it  = container.begin();
        const auto end = container.end();

        JobSystemDebug.printf("Reducing array of size %zu with chunk size %zu\n", container.size(), chunkSize);

        while (it != end)
        {
            auto chunkEnd = it;
            std::advance(chunkEnd, std::min(chunkSize, static_cast<size_t>(std::distance(it, end))));

            auto future = SubmitJobWithResult([it, chunkEnd, init, op]() -> T
            {
                return std::reduce(it, chunkEnd, init, op);
            });

            futures.push_back(std::move(future));
            it = chunkEnd;
        }

        T result = init;
        for (auto& future : futures)
            result = op(result, future.get());

        return result;
    }

    /** @brief Search for a value in parallel across chunks. */
    template<ArrayLike Container, typename T>
    auto ParallelFind(const Container& container, const T& value, size_t chunkSize = 0)
        -> decltype(container.begin())
    {
        if (container.empty()) return container.end();

        if (chunkSize == 0)
            chunkSize = CalculateOptimalChunkSize(container.size());

        std::vector<std::future<decltype(container.begin())>> futures;
        auto       it  = container.begin();
        const auto end = container.end();

        JobSystemDebug.printf("Searching array of size %zu with chunk size %zu\n", container.size(), chunkSize);

        while (it != end)
        {
            auto chunkEnd   = it;
            auto chunkStart = it;
            std::advance(chunkEnd, std::min(chunkSize, static_cast<size_t>(std::distance(it, end))));

            auto future = SubmitJobWithResult([chunkStart, chunkEnd, &value, end]() -> decltype(container.begin())
            {
                auto found = std::find(chunkStart, chunkEnd, value);
                return (found != chunkEnd) ? found : end;
            });

            futures.push_back(std::move(future));
            it = chunkEnd;
        }

        for (auto& future : futures)
        {
            auto result = future.get();
            if (result != container.end())
                return result;
        }
        return container.end();
    }

    /** @brief Sort a container using parallel chunk sorting then merge. */
    template<ArrayLike Container>
    void ParallelSort(Container& container, size_t chunkSize = 0)
    {
        if (container.size() <= 1) return;

        if (chunkSize == 0)
            chunkSize = CalculateOptimalChunkSize(container.size());

        if (container.size() <= chunkSize)
        {
            std::sort(container.begin(), container.end());
            return;
        }

        JobSystemDebug.printf("Sorting array of size %zu with chunk size %zu\n", container.size(), chunkSize);

        std::vector<std::future<void>> sortFutures;
        auto       it  = container.begin();
        const auto end = container.end();

        while (it != end)
        {
            auto chunkEnd = it;
            std::advance(chunkEnd, std::min(chunkSize, static_cast<size_t>(std::distance(it, end))));

            auto future = SubmitJobWithResult([it, chunkEnd]()
            {
                std::sort(it, chunkEnd);
            });

            sortFutures.push_back(std::move(future));
            it = chunkEnd;
        }

        for (auto& future : sortFutures)
            future.wait();

        MergeChunks(container, chunkSize);
    }

    /** @brief Process a container in batches using a custom batch processor. */
    template<ArrayLike Container, typename BatchProcessor>
        requires std::invocable<BatchProcessor,
                                decltype(std::declval<Container>().begin()),
                                decltype(std::declval<Container>().end())>
    void ProcessBatches(Container& container, BatchProcessor processor, size_t batchSize = 0)
    {
        if (container.empty()) return;

        if (batchSize == 0)
            batchSize = CalculateOptimalChunkSize(container.size());

        std::vector<std::future<void>> futures;
        auto       it  = container.begin();
        const auto end = container.end();

        JobSystemDebug.printf("Processing array in batches of size %zu\n", batchSize);

        while (it != end)
        {
            auto batchEnd = it;
            std::advance(batchEnd, std::min(batchSize, static_cast<size_t>(std::distance(it, end))));

            auto future = SubmitJobWithResult([it, batchEnd, processor]()
            {
                processor(it, batchEnd);
            });

            futures.push_back(std::move(future));
            it = batchEnd;
        }

        for (auto& future : futures)
            future.wait();
    }

    // ── Synchronisation ──────────────────────────────────────────────────────

    /** @brief Block until all submitted jobs have completed. */
    void WaitForAll()
    {
        std::unique_lock<std::mutex> lock(m_completionMutex);
        m_jobCompleted.wait(lock, [this]
        {
            return m_totalJobs.load(std::memory_order_acquire) == 0;
        });
    }

    template<typename Rep, typename Period>
    bool WaitForAll(const std::chrono::duration<Rep, Period>& timeout)
    {
        std::unique_lock<std::mutex> lock(m_completionMutex);
        return m_jobCompleted.wait_for(lock, timeout, [this]
        {
            return m_totalJobs.load(std::memory_order_acquire) == 0;
        });
    }

    // ── Accessors ────────────────────────────────────────────────────────────

    [[nodiscard]] size_t GetThreadCount()      const noexcept { return m_numThreads; }
    [[nodiscard]] size_t GetPendingJobCount()  const noexcept { return m_jobQueue.Size(); }
    [[nodiscard]] size_t GetActiveJobCount()   const noexcept { return m_totalJobs.load(std::memory_order_acquire); }
    [[nodiscard]] bool   IsShuttingDown()      const noexcept { return m_shutdown.load(std::memory_order_acquire); }

    void Shutdown()
    {
        bool expected = false;
        if (!m_shutdown.compare_exchange_strong(expected, true, std::memory_order_release))
            return;

        JobSystemDebug.printf("Shutting down JobSystem...\n");
        m_jobQueue.NotifyAll();

        for (auto& worker : m_workers)
        {
            if (worker.joinable())
                worker.join();
        }

        JobSystemDebug.printf("JobSystem shutdown complete\n");
    }

private:
    // ── Worker Loop ──────────────────────────────────────────────────────────

    void WorkerLoop(size_t threadId)
    {
        Profiler::ProfileLogger::GetInstance().RegisterThread(
            std::format("Worker {}", threadId));

        JobSystemDebug.printf("Worker thread %zu started\n", threadId);

        while (!m_shutdown.load(std::memory_order_acquire))
        {
            Job job;
            if (!m_jobQueue.WaitAndPop(job, m_shutdown))
                break;

            if (job.IsValid())
                job.Execute();
        }

        JobSystemDebug.printf("Worker thread %zu stopped\n", threadId);
    }

    // ── Helpers ──────────────────────────────────────────────────────────────

    [[nodiscard]] size_t CalculateOptimalChunkSize(size_t arraySize) const noexcept
    {
        if (arraySize == 0) return 1;
        size_t targetChunks = m_numThreads * 3;
        size_t chunkSize    = (arraySize + targetChunks - 1) / targetChunks;
        constexpr size_t minChunkSize = 100;
        return std::max(chunkSize, minChunkSize);
    }

    template<ArrayLike Container>
    void MergeChunks(Container& container, size_t chunkSize)
    {
        using ValueType = typename Container::value_type;
        std::vector<ValueType> temp(container.size());

        size_t currentChunkSize = chunkSize;
        while (currentChunkSize < container.size())
        {
            auto it     = container.begin();
            auto tempIt = temp.begin();

            while (it != container.end())
            {
                auto mid = it;
                auto end = it;

                std::advance(mid, std::min(currentChunkSize,
                    static_cast<size_t>(std::distance(it, container.end()))));
                std::advance(end, std::min(currentChunkSize * 2,
                    static_cast<size_t>(std::distance(it, container.end()))));

                std::merge(it, mid, mid, end, tempIt);
                tempIt += std::distance(it, end);
                it = end;
            }

            std::copy(temp.begin(), temp.begin() + container.size(), container.begin());
            currentChunkSize *= 2;
        }
    }

    // ── Members ──────────────────────────────────────────────────────────────

    std::vector<std::thread>  m_workers;
    ThreadSafeQueue<Job>      m_jobQueue;
    std::atomic<bool>         m_shutdown;
    std::atomic<size_t>       m_totalJobs;
    size_t                    m_numThreads = 0;

    std::mutex              m_completionMutex;
    std::condition_variable m_jobCompleted;
};
