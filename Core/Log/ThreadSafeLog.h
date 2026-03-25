#pragma once

#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <functional>
#include <chrono>
#include <cstdarg>

namespace Core {

// Severity levels for log messages
enum class ELogLevel
{
    Info,
    Warning,
    Error
};

// A single log entry with metadata
struct SLogEntry
{
    ELogLevel level;
    std::string message;
    std::thread::id threadId;
    std::chrono::system_clock::time_point timestamp;
};

// Thread-safe log that accepts messages from any thread and
// outputs them sequentially on a dedicated background thread.
class CThreadSafeLog
{
public:
    using OutputCallback = std::function<void(const SLogEntry&)>;

    CThreadSafeLog();
    ~CThreadSafeLog();

    // Start the background output thread. Must be called before logging.
    void Start();

    // Stop the background output thread, flushing remaining messages.
    void Stop();

    // Log a message at a given severity level (thread-safe).
    void Log(ELogLevel level, const char* format, ...);

    // Convenience helpers (thread-safe).
    void Info(const char* format, ...);
    void Warning(const char* format, ...);
    void Error(const char* format, ...);

    // Set a custom output callback. If not set, defaults to stdout.
    void SetOutputCallback(OutputCallback callback);

    // Returns the number of messages currently pending in the queue.
    size_t GetPendingCount() const;

    // Returns true if the background thread is running.
    bool IsRunning() const { return m_running.load(); }

private:
    void OutputLoop();
    void Enqueue(ELogLevel level, const char* format, va_list args);
    std::string FormatString(const char* fmt, va_list vl);
    static const char* LevelToString(ELogLevel level);

    std::queue<SLogEntry> m_queue;
    mutable std::mutex m_mutex;
    std::condition_variable m_condition;
    std::thread m_outputThread;
    std::atomic<bool> m_running{ false };
    OutputCallback m_outputCallback;
};

} // namespace Core