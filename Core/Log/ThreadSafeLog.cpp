#include "ThreadSafeLog.h"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstdio>

namespace Core {

CThreadSafeLog::CThreadSafeLog()
{
}

CThreadSafeLog::~CThreadSafeLog()
{
    Stop();
}

void CThreadSafeLog::Start()
{
    if (m_running.load())
        return;

    m_running.store(true);
    m_outputThread = std::thread(&CThreadSafeLog::OutputLoop, this);
}

void CThreadSafeLog::Stop()
{
    if (!m_running.load())
        return;

    m_running.store(false);
    m_condition.notify_one();

    if (m_outputThread.joinable())
        m_outputThread.join();
}

void CThreadSafeLog::Log(ELogLevel level, const char* format, ...)
{
    va_list vl;
    va_start(vl, format);
    Enqueue(level, format, vl);
    va_end(vl);
}

void CThreadSafeLog::Info(const char* format, ...)
{
    va_list vl;
    va_start(vl, format);
    Enqueue(ELogLevel::Info, format, vl);
    va_end(vl);
}

void CThreadSafeLog::Warning(const char* format, ...)
{
    va_list vl;
    va_start(vl, format);
    Enqueue(ELogLevel::Warning, format, vl);
    va_end(vl);
}

void CThreadSafeLog::Error(const char* format, ...)
{
    va_list vl;
    va_start(vl, format);
    Enqueue(ELogLevel::Error, format, vl);
    va_end(vl);
}

void CThreadSafeLog::SetOutputCallback(OutputCallback callback)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_outputCallback = std::move(callback);
}

size_t CThreadSafeLog::GetPendingCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_queue.size();
}

void CThreadSafeLog::Enqueue(ELogLevel level, const char* format, va_list args)
{
    SLogEntry entry;
    entry.level = level;
    entry.message = FormatString(format, args);
    entry.threadId = std::this_thread::get_id();
    entry.timestamp = std::chrono::system_clock::now();

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push(std::move(entry));
    }
    m_condition.notify_one();
}

void CThreadSafeLog::OutputLoop()
{
    while (true)
    {
        SLogEntry entry;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_condition.wait(lock, [this]() {
                return !m_queue.empty() || !m_running.load();
            });

            // Drain remaining messages before exiting
            if (!m_running.load() && m_queue.empty())
                break;

            entry = std::move(m_queue.front());
            m_queue.pop();
        }

        if (m_outputCallback)
        {
            m_outputCallback(entry);
        }
        else
        {
            // Default output: [LEVEL][ThreadId][Timestamp] Message
            auto timeT = std::chrono::system_clock::to_time_t(entry.timestamp);
            std::tm tm{};
#ifdef _WIN32
            localtime_s(&tm, &timeT);
#else
            localtime_r(&timeT, &tm);
#endif
            std::ostringstream oss;
            oss << "[" << LevelToString(entry.level) << "]"
                << "[" << entry.threadId << "]"
                << "[" << std::put_time(&tm, "%H:%M:%S") << "] "
                << entry.message;

            if (entry.level == ELogLevel::Error)
                std::cerr << oss.str();
            else
                std::cout << oss.str();
        }
    }
}

std::string CThreadSafeLog::FormatString(const char* fmt, va_list vl)
{
    static const int bufferSize = 1024;
    char buffer[bufferSize];
    std::string result;
    int nsize = vsnprintf(buffer, bufferSize, fmt, vl);
    if (nsize < 0)
    {
        return "[format error]";
    }
    if (nsize >= bufferSize)
    {
        std::vector<char> largeBuffer(nsize + 1);
        vsnprintf(largeBuffer.data(), largeBuffer.size(), fmt, vl);
        result = largeBuffer.data();
    }
    else
    {
        result = buffer;
    }
    return result;
}

const char* CThreadSafeLog::LevelToString(ELogLevel level)
{
    switch (level)
    {
    case ELogLevel::Info:    return "INFO";
    case ELogLevel::Warning: return "WARNING";
    case ELogLevel::Error:   return "ERROR";
    default:                 return "UNKNOWN";
    }
}

} // namespace Core