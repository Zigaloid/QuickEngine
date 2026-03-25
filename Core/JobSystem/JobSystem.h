#pragma once

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

#include "CoreSystem/CoreDebugChannels.h"
#include "Profiler/Profiler.h"

// Forward declaration
template<typename T>
class ThreadSafeQueue;

// C++20 Concepts for type safety
template<typename F>
concept Callable = std::invocable<F>;

template<typename F, typename... Args>
concept CallableWithArgs = std::invocable<F, Args...>;

// Array processing concepts for type safety
template<typename Container>
concept ArrayLike = requires(Container c) {
    { c.begin() } -> std::forward_iterator;
    { c.end() } -> std::forward_iterator;
    { c.size() } -> std::convertible_to<size_t>;
};

template<typename F, typename T>
concept ArrayProcessor = std::invocable<F, T&> || std::invocable<F, const T&>;

template<typename F, typename T>
concept ArrayReducer = requires(F f, T a, T b) {
    { f(a, b) } -> std::convertible_to<T>;
};

// Job class that wraps any callable
class Job {
public:
    template<Callable F>
    Job(F&& func) : m_task(std::forward<F>(func)) {}

    // Default constructor for empty job (used for shutdown signaling)
    Job() = default;

    // Add move constructor and assignment
    Job(Job&&) = default;
    Job& operator=(Job&&) = default;

    // Delete copy constructor and assignment to avoid expensive copies
    Job(const Job&) = delete;
    Job& operator=(const Job&) = delete;

    void execute() {
        if (m_task) {
            m_task();
        }
    }

    bool valid() const { return static_cast<bool>(m_task); }

private:
    std::function<void()> m_task;
};

// Thread-safe queue for jobs
template<typename T>
class ThreadSafeQueue {
public:
    ThreadSafeQueue() = default;

    // Disable copy constructor and assignment
    ThreadSafeQueue(const ThreadSafeQueue&) = delete;
    ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;

    void push(T item) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push(std::move(item));
        m_condition.notify_one();
    }

    bool tryPop(T& item) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_queue.empty()) {
            return false;
        }
        item = std::move(m_queue.front());
        m_queue.pop();
        return true;
    }

    // Fixed: Remove callback parameter since we're not using it anymore
    bool waitAndPop(T& item, const std::atomic<bool>& shouldShutdown) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_condition.wait(lock, [this, &shouldShutdown] {
            return !m_queue.empty() || shouldShutdown.load(std::memory_order_acquire);
            });

        // Check shutdown flag again after waking up
        if (shouldShutdown.load(std::memory_order_acquire)) {
            return false; // Shutdown requested
        }

        if (!m_queue.empty()) {
            item = std::move(m_queue.front());
            m_queue.pop();
            return true;
        }

        return false;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.empty();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.size();
    }

    // Add method to wake up all waiting threads
    void notifyAll() {
        m_condition.notify_all();
    }

private:
    mutable std::mutex m_mutex;
    std::queue<T> m_queue;
    std::condition_variable m_condition;
};

// Main JobSystem class with C++20 features
class JobSystem {
public:
    explicit JobSystem(size_t numThreads = std::thread::hardware_concurrency())
        : m_shutdown(false), m_numThreads(numThreads), m_totalJobs(0) {

        if (m_numThreads == 0) {
            m_numThreads = 1;
        }

        // Create worker threads
        m_workers.reserve(m_numThreads);
        for (size_t i = 0; i < m_numThreads; ++i) {
            m_workers.emplace_back(&JobSystem::workerLoop, this, i);
        }

        JobSystemDebug.printf("JobSystem initialized with %zu worker threads\n", m_numThreads);
    }

    ~JobSystem() {
        shutdown();
    }

    // Submit a job that returns void - using C++20 concepts
    template<Callable F>
    void submitJob(F&& func) {
        if (m_shutdown.load(std::memory_order_acquire)) {
            JobSystemDebug.warning("Cannot submit job: JobSystem is shutting down\n");
            return;
        }

        // Increment total jobs counter when submitting
        m_totalJobs.fetch_add(1, std::memory_order_acq_rel);

        m_jobQueue.push(Job([func = std::forward<F>(func), this]() {
            try {
                func();
            }
            catch (const std::exception& e) {
                JobSystemDebug.warning("Job threw exception: %s\n", e.what());
            }
            catch (...) {
                JobSystemDebug.warning("Job threw unknown exception\n");
            }

            // Decrement total jobs when job completes
            {
                std::lock_guard<std::mutex> lock(m_completionMutex);
                m_totalJobs.fetch_sub(1, std::memory_order_acq_rel);
            }
            m_jobCompleted.notify_all();
            }));
    }

    // Submit a job that returns a value via std::future - using C++20 invoke_result_t
    template<typename F, typename... Args>
        requires CallableWithArgs<F, Args...>
    auto submitJobWithResult(F&& func, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>> {

        using ReturnType = std::invoke_result_t<F, Args...>;

        if (m_shutdown.load(std::memory_order_acquire)) {
            std::promise<ReturnType> promise;
            promise.set_exception(std::make_exception_ptr(
                std::runtime_error("Cannot submit job: JobSystem is shutting down")));
            return promise.get_future();
        }

        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            [func = std::forward<F>(func), ...args = std::forward<Args>(args)]() mutable {
                return std::invoke(func, args...);
            }
        );

        std::future<ReturnType> result = task->get_future();

        // Increment total jobs counter when submitting
        m_totalJobs.fetch_add(1, std::memory_order_acq_rel);

        m_jobQueue.push(Job([task, this]() {
            try {
                (*task)();
            }
            catch (...) {
                // Exception will be stored in the future
            }

            // Decrement total jobs when job completes
            {
                std::lock_guard<std::mutex> lock(m_completionMutex);
                m_totalJobs.fetch_sub(1, std::memory_order_acq_rel);
            }
            m_jobCompleted.notify_all();
            }));

        return result;
    }

    // ===== NEW ARRAY SPLITTING FUNCTIONALITY =====

    // Parallel for_each - splits array into chunks and processes them in parallel
    template<ArrayLike Container, ArrayProcessor<typename Container::value_type> Func>
    void parallelForEach(Container& container, Func func, size_t chunkSize = 0) {
        if (container.empty()) return;

        // Calculate optimal chunk size if not provided
        if (chunkSize == 0) {
            chunkSize = calculateOptimalChunkSize(container.size());
        }

        std::vector<std::future<void>> futures;
        auto it = container.begin();
        const auto end = container.end();

        JobSystemDebug.printf("Processing array of size %zu with chunk size %zu\n", 
                             container.size(), chunkSize);

        while (it != end) {
            auto chunkEnd = it;
            std::advance(chunkEnd, std::min(chunkSize, static_cast<size_t>(std::distance(it, end))));

            auto future = submitJobWithResult([it, chunkEnd, func]() mutable {
                std::for_each(it, chunkEnd, func);
            });

            futures.push_back(std::move(future));
            it = chunkEnd;
        }

        // Wait for all chunks to complete
        for (auto& future : futures) {
            future.wait();
        }
    }

    // Parallel transform - applies transformation to array elements in parallel
    template<ArrayLike InputContainer, ArrayLike OutputContainer, 
             typename Func>
        requires std::invocable<Func, typename InputContainer::value_type>
    void parallelTransform(const InputContainer& input, OutputContainer& output, 
                          Func func, size_t chunkSize = 0) {
        output.resize(input.size());
        
        if (input.empty()) return;

        if (chunkSize == 0) {
            chunkSize = calculateOptimalChunkSize(input.size());
        }

        std::vector<std::future<void>> futures;
        auto inputIt = input.begin();
        auto outputIt = output.begin();
        const auto inputEnd = input.end();

        JobSystemDebug.printf("Transforming array of size %zu with chunk size %zu\n", 
                             input.size(), chunkSize);

        while (inputIt != inputEnd) {
            auto inputChunkEnd = inputIt;
            auto outputChunkStart = outputIt;
            
            size_t remainingElements = std::distance(inputIt, inputEnd);
            size_t currentChunkSize = std::min(chunkSize, remainingElements);
            
            std::advance(inputChunkEnd, currentChunkSize);
            std::advance(outputIt, currentChunkSize);

            auto future = submitJobWithResult([inputIt, inputChunkEnd, outputChunkStart, func]() mutable {
                std::transform(inputIt, inputChunkEnd, outputChunkStart, func);
            });

            futures.push_back(std::move(future));
            inputIt = inputChunkEnd;
        }

        // Wait for all transformations to complete
        for (auto& future : futures) {
            future.wait();
        }
    }

    // Parallel reduce - reduces array to single value using parallel computation
    template<ArrayLike Container, typename T, ArrayReducer<T> BinaryOp>
    T parallelReduce(const Container& container, T init, BinaryOp op, size_t chunkSize = 0) {
        if (container.empty()) return init;

        if (chunkSize == 0) {
            chunkSize = calculateOptimalChunkSize(container.size());
        }

        std::vector<std::future<T>> futures;
        auto it = container.begin();
        const auto end = container.end();

        JobSystemDebug.printf("Reducing array of size %zu with chunk size %zu\n", 
                             container.size(), chunkSize);

        // Submit reduction jobs for each chunk
        while (it != end) {
            auto chunkEnd = it;
            std::advance(chunkEnd, std::min(chunkSize, static_cast<size_t>(std::distance(it, end))));

            auto future = submitJobWithResult([it, chunkEnd, init, op]() -> T {
                return std::reduce(it, chunkEnd, init, op);
            });

            futures.push_back(std::move(future));
            it = chunkEnd;
        }

        // Collect partial results and combine them
        T result = init;
        for (auto& future : futures) {
            result = op(result, future.get());
        }

        return result;
    }

    // Parallel find - searches for element in parallel across array chunks
    template<ArrayLike Container, typename T>
    auto parallelFind(const Container& container, const T& value, size_t chunkSize = 0) 
        -> decltype(container.begin()) {
        if (container.empty()) return container.end();

        if (chunkSize == 0) {
            chunkSize = calculateOptimalChunkSize(container.size());
        }

        std::vector<std::future<decltype(container.begin())>> futures;
        auto it = container.begin();
        const auto end = container.end();

        JobSystemDebug.printf("Searching array of size %zu with chunk size %zu\n", 
                             container.size(), chunkSize);

        // Submit search jobs for each chunk
        while (it != end) {
            auto chunkEnd = it;
            auto chunkStart = it;
            std::advance(chunkEnd, std::min(chunkSize, static_cast<size_t>(std::distance(it, end))));

            auto future = submitJobWithResult([chunkStart, chunkEnd, &value, end]() -> decltype(container.begin()) {
                auto found = std::find(chunkStart, chunkEnd, value);
                return (found != chunkEnd) ? found : end;
            });

            futures.push_back(std::move(future));
            it = chunkEnd;
        }

        // Check results from all chunks
        for (auto& future : futures) {
            auto result = future.get();
            if (result != container.end()) {
                return result; // Found in this chunk
            }
        }

        return container.end(); // Not found
    }

    // Parallel sort - sorts array using parallel merge sort approach
    template<ArrayLike Container>
    void parallelSort(Container& container, size_t chunkSize = 0) {
        if (container.size() <= 1) return;

        if (chunkSize == 0) {
            chunkSize = calculateOptimalChunkSize(container.size());
        }

        // If container is small enough, sort sequentially
        if (container.size() <= chunkSize) {
            std::sort(container.begin(), container.end());
            return;
        }

        JobSystemDebug.printf("Sorting array of size %zu with chunk size %zu\n", 
                             container.size(), chunkSize);

        // Sort chunks in parallel
        std::vector<std::future<void>> sortFutures;
        auto it = container.begin();
        const auto end = container.end();

        while (it != end) {
            auto chunkEnd = it;
            std::advance(chunkEnd, std::min(chunkSize, static_cast<size_t>(std::distance(it, end))));

            auto future = submitJobWithResult([it, chunkEnd]() {
                std::sort(it, chunkEnd);
            });

            sortFutures.push_back(std::move(future));
            it = chunkEnd;
        }

        // Wait for all chunks to be sorted
        for (auto& future : sortFutures) {
            future.wait();
        }

        // Merge sorted chunks
        mergeChunks(container, chunkSize);
    }

    // Batch processing - processes array in batches with custom batch processor
    template<ArrayLike Container, typename BatchProcessor>
        requires std::invocable<BatchProcessor, decltype(std::declval<Container>().begin()), 
                                              decltype(std::declval<Container>().end())>
    void processBatches(Container& container, BatchProcessor processor, size_t batchSize = 0) {
        if (container.empty()) return;

        if (batchSize == 0) {
            batchSize = calculateOptimalChunkSize(container.size());
        }

        std::vector<std::future<void>> futures;
        auto it = container.begin();
        const auto end = container.end();

        JobSystemDebug.printf("Processing array in batches of size %zu\n", batchSize);

        while (it != end) {
            auto batchEnd = it;
            std::advance(batchEnd, std::min(batchSize, static_cast<size_t>(std::distance(it, end))));

            auto future = submitJobWithResult([it, batchEnd, processor]() {
                processor(it, batchEnd);
            });

            futures.push_back(std::move(future));
            it = batchEnd;
        }

        // Wait for all batches to complete
        for (auto& future : futures) {
            future.wait();
        }
    }

    // ===== END ARRAY SPLITTING FUNCTIONALITY =====

    // Wait for all currently queued jobs to complete
    void waitForAll() {
        std::unique_lock<std::mutex> lock(m_completionMutex);
        m_jobCompleted.wait(lock, [this] {
            return m_totalJobs.load(std::memory_order_acquire) == 0;
            });
    }

    // Wait for all jobs to complete with a timeout
    template<typename Rep, typename Period>
    bool waitForAll(const std::chrono::duration<Rep, Period>& timeout) {
        std::unique_lock<std::mutex> lock(m_completionMutex);
        return m_jobCompleted.wait_for(lock, timeout, [this] {
            return m_totalJobs.load(std::memory_order_acquire) == 0;
            });
    }

    // Get the number of worker threads
    [[nodiscard]] size_t getThreadCount() const noexcept {
        return m_numThreads;
    }

    // Get the number of pending jobs
    [[nodiscard]] size_t getPendingJobCount() const noexcept {
        return m_jobQueue.size();
    }

    // Get the number of active (currently executing) jobs
    [[nodiscard]] size_t getActiveJobCount() const noexcept {
        return m_totalJobs.load(std::memory_order_acquire);
    }

    // Check if the system is shutting down
    [[nodiscard]] bool isShuttingDown() const noexcept {
        return m_shutdown.load(std::memory_order_acquire);
    }

    // Shutdown the job system
    void shutdown() {
        bool expected = false;
        if (!m_shutdown.compare_exchange_strong(expected, true, std::memory_order_release)) {
            return; // Already shutting down
        }

        JobSystemDebug.printf("Shutting down JobSystem...\n");

        // Wake up all waiting worker threads
        m_jobQueue.notifyAll();

        // Wait for all threads to finish
        for (auto& worker : m_workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }

        JobSystemDebug.printf("JobSystem shutdown complete\n");
    }

private:
    void workerLoop(size_t threadId) {
        // Register this worker thread with the profiler for human-readable names
        Profiler::ProfileLogger::GetInstance().RegisterThread(
            std::format("Worker {}", threadId));

        JobSystemDebug.printf("Worker thread %zu started\n", threadId);

        while (!m_shutdown.load(std::memory_order_acquire)) {
            Job job;

            // Simple waitAndPop without callback - job manages its own completion
            if (!m_jobQueue.waitAndPop(job, m_shutdown)) {
                // Shutdown was requested
                break;
            }

            if (job.valid()) {
                job.execute();
            }
        }

        JobSystemDebug.printf("Worker thread %zu stopped\n", threadId);
    }

    // Calculate optimal chunk size based on array size and thread count
    [[nodiscard]] size_t calculateOptimalChunkSize(size_t arraySize) const noexcept {
        if (arraySize == 0) return 1;
        
        // Aim for 2-4 chunks per thread to ensure good load balancing
        size_t targetChunks = m_numThreads * 3;
        size_t chunkSize = (arraySize + targetChunks - 1) / targetChunks;
        
        // Ensure minimum chunk size to avoid overhead
        constexpr size_t minChunkSize = 100;
        return std::max(chunkSize, minChunkSize);
    }

    // Helper function for merging sorted chunks
    template<ArrayLike Container>
    void mergeChunks(Container& container, size_t chunkSize) {
        using ValueType = typename Container::value_type;
        std::vector<ValueType> temp(container.size());
        
        // Merge pairs of chunks iteratively until fully merged
        size_t currentChunkSize = chunkSize;
        while (currentChunkSize < container.size()) {
            auto it = container.begin();
            auto tempIt = temp.begin();
            
            while (it != container.end()) {
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
            
            // Copy back to original container
            std::copy(temp.begin(), temp.begin() + container.size(), container.begin());
            currentChunkSize *= 2;
        }
    }

    std::vector<std::thread> m_workers;
    ThreadSafeQueue<Job> m_jobQueue;
    std::atomic<bool> m_shutdown;
    std::atomic<size_t> m_totalJobs;  // Changed from activeJobs_
    size_t m_numThreads;

    std::mutex m_completionMutex;
    std::condition_variable m_jobCompleted;
};

