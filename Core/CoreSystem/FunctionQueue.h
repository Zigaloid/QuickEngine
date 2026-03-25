#pragma once

#include <functional>
#include <vector>
#include <string>
#include <mutex>
#include <shared_mutex>
#include "Profiler/Profiler.h"

namespace Core {

    /**
     * @brief Simple thread-safe class that allows adding function calls to a list and executing them in order
     */
    class FunctionQueue {
    public:
        using Function = std::function<void()>;

    private:
        std::vector<Function> m_functions;
        std::vector<std::string> m_names; // Optional names for debugging
        mutable std::shared_mutex m_mutex; // Use shared_mutex to allow concurrent reads

    public:
        FunctionQueue() = default;
        ~FunctionQueue() = default;

        // Non-copyable but movable
        FunctionQueue(const FunctionQueue&) = delete;
        FunctionQueue& operator=(const FunctionQueue&) = delete;
        FunctionQueue(FunctionQueue&&) = default;
        FunctionQueue& operator=(FunctionQueue&&) = default;

        /**
         * @brief Add a function to the queue (thread-safe)
         * @param func Function to add (can be lambda, function pointer, std::function, etc.)
         * @param name Optional name for debugging purposes
         */
        void AddFunction(Function func, const std::string& name = "") {
            std::unique_lock<std::shared_mutex> lock(m_mutex);
            m_functions.emplace_back(std::move(func));
            m_names.emplace_back(name);
        }

        /**
         * @brief Add a function with parameters using lambda capture (thread-safe)
         * @param func Function to add
         * @param name Optional name for debugging purposes
         */
        template<typename Func, typename... Args>
        void AddFunction(Func&& func, Args&&... args, const std::string& name = "") {
            std::unique_lock<std::shared_mutex> lock(m_mutex);
            m_functions.emplace_back([func = std::forward<Func>(func), args...]() mutable {
                func(args...);
            });
            m_names.emplace_back(name);
        }

        /**
         * @brief Execute all functions in the order they were added (thread-safe)
         * @return Number of functions executed
         */
        size_t ExecuteAll() {
            std::shared_lock<std::shared_mutex> lock(m_mutex);
            size_t executed = 0;
            for (const auto& func : m_functions) {
                if (func) {
                    func();
                    ++executed;
                }
            }
            return executed;
        }

        /**
         * @brief Execute all functions and then clear the queue (thread-safe)
         * @return Number of functions executed
         */
        size_t ExecuteAndClear() {
            std::unique_lock<std::shared_mutex> lock(m_mutex);
            size_t executed = 0;
            for (const auto& func : m_functions) {
                if (func) {
                    func();
                    ++executed;
                }
            }
            m_functions.clear();
            m_names.clear();
            return executed;
        }

        /**
         * @brief Clear all functions from the queue without executing them (thread-safe)
         */
        void Clear() 
        {
            DECLARE_FUNC_LOW();
            std::unique_lock<std::shared_mutex> lock(m_mutex);
            m_functions.clear();
            m_names.clear();
        }

        /**
         * @brief Get the number of functions in the queue (thread-safe)
         */
        size_t Size() const {
            std::shared_lock<std::shared_mutex> lock(m_mutex);
            return m_functions.size();
        }

        /**
         * @brief Check if the queue is empty (thread-safe)
         */
        bool IsEmpty() const {
            std::shared_lock<std::shared_mutex> lock(m_mutex);
            return m_functions.empty();
        }

        /**
         * @brief Get the names of all functions in the queue (for debugging) (thread-safe)
         */
        std::vector<std::string> GetFunctionNames() const {
            std::shared_lock<std::shared_mutex> lock(m_mutex);
            return m_names; // Return copy to avoid race conditions
        }

        /**
         * @brief Execute a single function at the specified index (thread-safe)
         * @param index Index of the function to execute
         * @return true if function was executed, false if index is out of bounds
         */
        bool ExecuteAt(size_t index) {
            std::shared_lock<std::shared_mutex> lock(m_mutex);
            if (index >= m_functions.size() || !m_functions[index]) {
                return false;
            }
            m_functions[index]();
            return true;
        }

        /**
         * @brief Remove a function at the specified index without executing it (thread-safe)
         * @param index Index of the function to remove
         * @return true if function was removed, false if index is out of bounds
         */
        bool RemoveAt(size_t index) {
            std::unique_lock<std::shared_mutex> lock(m_mutex);
            if (index >= m_functions.size()) {
                return false;
            }
            m_functions.erase(m_functions.begin() + index);
            m_names.erase(m_names.begin() + index);
            return true;
        }
    };

} // namespace Core