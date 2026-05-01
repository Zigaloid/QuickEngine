#pragma once

#include "Profiler/Profiler.h"

#include <functional>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>

namespace Core {

/** @brief Simple thread-safe queue of callables that can be executed in insertion order. */
class FunctionQueue {
public:
    using Function = std::function<void()>;

private:
    std::vector<Function>    m_functions;
    std::vector<std::string> m_names;
    mutable std::shared_mutex m_mutex;

public:
    FunctionQueue() = default;
    ~FunctionQueue() = default;

    FunctionQueue(const FunctionQueue&) = delete;
    FunctionQueue& operator=(const FunctionQueue&) = delete;
    FunctionQueue(FunctionQueue&&) = default;
    FunctionQueue& operator=(FunctionQueue&&) = default;

    /**
     * @brief Add a function to the queue (thread-safe).
     * @param func Function to add.
     * @param name Optional name for debugging purposes.
     */
    void AddFunction(Function func, const std::string& name = "")
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_functions.emplace_back(std::move(func));
        m_names.emplace_back(name);
    }

    /**
     * @brief Execute all functions in insertion order without clearing the queue (thread-safe).
     * @return Number of functions executed.
     */
    size_t ExecuteAll()
    {
        DECLARE_FUNC_LOW();
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        size_t executed = 0;
        for (const auto& func : m_functions)
        {
            if (func)
            {
                func();
                ++executed;
            }
        }
        return executed;
    }

    /**
     * @brief Execute all functions then clear the queue (thread-safe).
     * @return Number of functions executed.
     */
    size_t ExecuteAndClear()
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        size_t executed = 0;
        for (const auto& func : m_functions)
        {
            if (func)
            {
                func();
                ++executed;
            }
        }
        m_functions.clear();
        m_names.clear();
        return executed;
    }

    /** @brief Clear all functions without executing them (thread-safe). */
    void Clear()
    {
        DECLARE_FUNC_LOW();
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_functions.clear();
        m_names.clear();
    }

    /** @brief Return the number of functions currently in the queue (thread-safe). */
    size_t Size() const
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        return m_functions.size();
    }

    /** @brief Return true if the queue contains no functions (thread-safe). */
    bool IsEmpty() const
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        return m_functions.empty();
    }

    /** @brief Return the names of all queued functions for debugging (thread-safe). */
    std::vector<std::string> GetFunctionNames() const
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        return m_names;
    }

    /**
     * @brief Execute the function at the given index without removing it (thread-safe).
     * @param index Zero-based index of the function to execute.
     * @return true if the function was found and executed, false if index is out of bounds.
     */
    bool ExecuteAt(size_t index)
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        if (index >= m_functions.size() || !m_functions[index]) return false;
        m_functions[index]();
        return true;
    }

    /**
     * @brief Remove the function at the given index without executing it (thread-safe).
     * @param index Zero-based index of the function to remove.
     * @return true if the function was removed, false if index is out of bounds.
     */
    bool RemoveAt(size_t index)
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        if (index >= m_functions.size()) return false;
        m_functions.erase(m_functions.begin() + static_cast<std::ptrdiff_t>(index));
        m_names.erase(m_names.begin() + static_cast<std::ptrdiff_t>(index));
        return true;
    }
};

} // namespace Core
