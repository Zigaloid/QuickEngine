#include "Profiler.h"
#include "ProfilerAnalyzer.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace Profiler {

REFL_DEFINE_OBJECT(CEventArray)
    REFL_DEFINE_OBJECT_PTR_VECTOR_MEMBER(CEventArray, m_events),
REFL_DEFINE_END

// Reflection map definition for ProfileEvent
REFL_DEFINE_OBJECT(ProfileEvent)
    REFL_DEFINE_STRING_MEMBER(ProfileEvent, threadIdStr),
    REFL_DEFINE_STRING_MEMBER(ProfileEvent, threadNameStr),
    REFL_DEFINE_STRING_MEMBER(ProfileEvent, name),
    REFL_DEFINE_INT_MEMBER(ProfileEvent, clockCyclesHigh),
    REFL_DEFINE_INT_MEMBER(ProfileEvent, clockCyclesLow),
    REFL_DEFINE_INT_MEMBER(ProfileEvent, type),
    REFL_DEFINE_BOOL_MEMBER(ProfileEvent, isStart),
    REFL_DEFINE_INT_MEMBER(ProfileEvent, frameNumberHigh),
    REFL_DEFINE_INT_MEMBER(ProfileEvent, frameNumberLow),
REFL_DEFINE_END

std::atomic<uint64_t> FrameProfiler::s_currentFrame{0};

ProfileLogger& ProfileLogger::GetInstance()
{
    static ProfileLogger instance;
    return instance;
}

ProfileLogger::~ProfileLogger()
{
    FlushToFile();
}

void ProfileLogger::RegisterThread(const std::string& name)
{
    RegisterThread(std::this_thread::get_id(), name);
}

void ProfileLogger::RegisterThread(std::thread::id threadId, const std::string& name)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_threadNames[threadId] = name;
}

std::string ProfileLogger::GetThreadName(std::thread::id threadId) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_threadNames.find(threadId);
    if (it != m_threadNames.end())
    {
        return it->second;
    }
    return {};
}

void ProfileLogger::LogEvent(std::thread::id threadId, const char* name, uint64_t clockCycles, bool isStart)
{
    if (!m_enabled.load(std::memory_order_relaxed))
    {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    // Look up thread name from registry
    std::string threadName;
    auto it = m_threadNames.find(threadId);
    if (it != m_threadNames.end())
    {
        threadName = it->second;
    }

    m_events.emplace_back(threadId, name, clockCycles, isStart, threadName);
    m_estimatedMemoryUsage += m_events.back().EstimateMemoryUsage();
    EnforceMemoryBudget();
}

void ProfileLogger::LogFrameEvent(std::thread::id threadId, uint64_t clockCycles, uint64_t frameNumber)
{
    if (!m_enabled.load(std::memory_order_relaxed))
    {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    // Look up thread name from registry
    std::string threadName;
    auto it = m_threadNames.find(threadId);
    if (it != m_threadNames.end())
    {
        threadName = it->second;
    }

    m_events.emplace_back(threadId, clockCycles, frameNumber, threadName);
    m_estimatedMemoryUsage += m_events.back().EstimateMemoryUsage();
    EnforceMemoryBudget();
}

void ProfileLogger::LogFrameEvent(uint64_t frameNumber)
{
    LogFrameEvent(std::this_thread::get_id(), GetClockCycles(), frameNumber);
}

void ProfileLogger::SetOutputFile(const std::string& filename)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_outputFile = filename;
}

void ProfileLogger::EnforceMemoryBudget()
{
    size_t budget = m_memoryBudget.load(std::memory_order_relaxed);
    if (budget == 0)
    {
        return; // Unlimited
    }

    while (m_estimatedMemoryUsage > budget && !m_events.empty())
    {
        size_t evictedSize = m_events.front().EstimateMemoryUsage();
        m_estimatedMemoryUsage -= evictedSize;
        m_events.pop_front();

        // Keep serialize index consistent: if we evicted an unserialized event,
        // it's already gone. If we evicted a serialized event, adjust the index.
        if (m_serializeIndex > 0)
        {
            m_serializeIndex--;
        }
    }
}

void ProfileLogger::TransferToAnalyzer(ProfilerAnalyzer& analyzer, bool clearAfterTransfer)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_events.empty())
    {
        return;
    }

    // Convert ProfileEvents to the analyzer's format and transfer directly
    analyzer.LoadFromProfileEvents(m_events, clearAfterTransfer);

    if (clearAfterTransfer)
    {
        std::cout << "Profiler: Transferred " << m_events.size() << " events directly to analyzer and cleared buffer" << std::endl;
        m_events.clear();
        m_estimatedMemoryUsage = 0;
    }
    else
    {
        std::cout << "Profiler: Transferred " << m_events.size() << " events directly to analyzer (kept in buffer)" << std::endl;
    }
}

std::vector<ProfileEvent> ProfileLogger::CopyEvents() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return std::vector<ProfileEvent>(m_events.begin(), m_events.end());
}

size_t ProfileLogger::GetEventCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_events.size();
}

void ProfileLogger::FlushToFile()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_events.empty())
    {
        return;
    }

    // Check for orphaned end events and remove them
    std::vector<ProfileEvent> validEvents;
    validEvents.reserve(m_events.size());

    // Create a key for tracking start/end pairs per thread
    struct EventKey
    {
        std::string threadIdStr;
        std::string name;

        bool operator==(const EventKey& other) const
        {
            return threadIdStr == other.threadIdStr && name == other.name;
        }
    };

    struct EventKeyHash
    {
        size_t operator()(const EventKey& key) const
        {
            std::hash<std::string> hasher;
            return hasher(key.threadIdStr) ^ (hasher(key.name) << 1);
        }
    };

    std::unordered_map<EventKey, int, EventKeyHash> balanceTracker;
    size_t orphanedEndEvents = 0;

    // First pass: count start/end events and identify orphaned ends
    for (const auto& event : m_events)
    {
        if (event.GetEventType() == EventType::Scoped)
        {
            EventKey key{ event.threadIdStr, event.name };

            if (event.isStart)
            {
                balanceTracker[key]++;
                validEvents.push_back(event);
            }
            else
            {
                auto it = balanceTracker.find(key);
                if (it != balanceTracker.end() && it->second > 0)
                {
                    // Found matching start event
                    it->second--;
                    validEvents.push_back(event);
                }
                else
                {
                    // Orphaned end event - skip it
                    orphanedEndEvents++;
                }
            }
        }
        else
        {
            // Frame events are always valid
            validEvents.push_back(event);
        }
    }

    if (orphanedEndEvents > 0)
    {
        std::cout << "Profiler: Filtered out " << orphanedEndEvents << " orphaned end events" << std::endl;
    }

    std::ofstream file(m_outputFile, std::ios::out);
    if (!file.is_open())
    {
        std::cerr << "Failed to open profile log file: " << m_outputFile << std::endl;
        return;
    }

    // Write header if file is empty
    file.seekp(0, std::ios::end);
    if (file.tellp() == 0)
    {
        file << "ThreadID,Name,ClockCycles,EventType,FrameNumber\n";
    }

    // Write valid events in CSV format
    for (const auto& event : validEvents)
    {
        file << event.threadIdStr << ","
             << event.name << ","
             << event.GetClockCycles() << ",";

        if (event.GetEventType() == EventType::Frame)
        {
            file << "FRAME," << event.GetFrameNumber();
        }
        else
        {
            file << (event.isStart ? "START" : "END") << ",0";
        }

        file << "\n";
    }

    file.flush();

    size_t originalCount = m_events.size();
    size_t validCount    = validEvents.size();
    m_events.clear();
    m_estimatedMemoryUsage = 0;

    std::cout << "Profiler: Flushed " << validCount << " valid events (filtered "
              << (originalCount - validCount) << " orphaned) to " << m_outputFile << std::endl;
}

ScopedProfiler::ScopedProfiler(const char* name)
    : m_name(name), m_threadId(std::this_thread::get_id())
{
    uint64_t cycles = GetClockCycles();
    ProfileLogger::GetInstance().LogEvent(m_threadId, m_name, cycles, true);
}

ScopedProfiler::~ScopedProfiler()
{
    uint64_t cycles = GetClockCycles();
    ProfileLogger::GetInstance().LogEvent(m_threadId, m_name, cycles, false);
}

void FrameProfiler::MarkFrame()
{
    uint64_t currentFrame = s_currentFrame.fetch_add(1, std::memory_order_relaxed);
    ProfileLogger::GetInstance().LogFrameEvent(currentFrame + 1);
}

void FrameProfiler::MarkFrame(uint64_t frameNumber)
{
    s_currentFrame.store(frameNumber, std::memory_order_relaxed);
    ProfileLogger::GetInstance().LogFrameEvent(frameNumber);
}

std::string ProfileLogger::SerializeRecentEvents()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string result;
    CEventArray eventArray;
    for (size_t i = m_serializeIndex; i < m_events.size(); ++i)
    {
        eventArray.m_events.push_back(&m_events[i]);
    }
    m_serializeIndex = m_events.size();

    auto writeResult = eventArray.WriteToJsonString();
    if (writeResult.IsSuccess())
    {
        result = writeResult.GetValue();
    }
    else
    {
        result.clear();
    }
    return result;
}

std::vector<uint8_t> ProfileLogger::SerializeRecentEventsBinary()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<uint8_t> result;
    CEventArray eventArray;
    for (size_t i = m_serializeIndex; i < m_events.size(); ++i)
    {
        eventArray.m_events.push_back(&m_events[i]);
    }
    m_serializeIndex = m_events.size();

    auto writeResult = eventArray.WriteToBinaryBuffer();
    if (writeResult.IsSuccess())
    {
        result = writeResult.GetValue();
    }
    else
    {
        result.clear();
    }
    return result;
}

} // namespace Profiler
