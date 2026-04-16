#pragma once
#include <chrono>
#include <string>
#include <thread>
#include <atomic>
#include <fstream>
#include <mutex>
#include <deque>
#include <unordered_map>
#include "Reflection\Reflection.h"

// Forward declaration for direct transfer
namespace Profiler {
    class ProfilerAnalyzer;
}

namespace Profiler {

enum class EventType
{
    Scoped,     // Traditional start/end events for functions/scopes
    Frame       // Frame boundary markers
};

/** @brief Serializable profiler event, compatible with the reflection system. */
class ProfileEvent : public CReflectedBase
{
public:
    REFL_DECLARE_OBJECT(ProfileEvent, CReflectedBase);

    std::string threadIdStr;        // Thread ID as string for serialization
    std::string threadNameStr;      // Human-readable thread name
    std::string name;               // Event name (changed from const char* for reflection)
    int clockCyclesHigh = 0;        // Upper 32 bits of clock cycles
    int clockCyclesLow  = 0;        // Lower 32 bits of clock cycles
    int type            = 0;        // EventType as int (0 = Scoped, 1 = Frame)
    bool isStart        = false;    // true = start, false = end (only relevant for Scoped events)
    int frameNumberHigh = 0;        // Upper 32 bits of frame number
    int frameNumberLow  = 0;        // Lower 32 bits of frame number

    // Default constructor for reflection/deserialization
    ProfileEvent()
        : threadIdStr(), threadNameStr(), name(), clockCyclesHigh(0), clockCyclesLow(0),
          type(0), isStart(false), frameNumberHigh(0), frameNumberLow(0)
    {
    }

    // Constructor for scoped events
    ProfileEvent(std::thread::id tid, const char* n, uint64_t cycles, bool start, const std::string& threadName = "")
        : threadNameStr(threadName), name(n), type(static_cast<int>(EventType::Scoped)), isStart(start),
          frameNumberHigh(0), frameNumberLow(0)
    {
        SetThreadId(tid);
        SetClockCycles(cycles);
    }

    // Constructor for frame events
    ProfileEvent(std::thread::id tid, uint64_t cycles, uint64_t frame, const std::string& threadName = "")
        : threadNameStr(threadName), name("Frame"), type(static_cast<int>(EventType::Frame)), isStart(false)
    {
        SetThreadId(tid);
        SetClockCycles(cycles);
        SetFrameNumber(frame);
    }

    // Helper accessors for 64-bit values stored as two ints
    void SetClockCycles(uint64_t cycles)
    {
        clockCyclesHigh = static_cast<int>(cycles >> 32);
        clockCyclesLow  = static_cast<int>(cycles & 0xFFFFFFFF);
    }

    uint64_t GetClockCycles() const
    {
        return (static_cast<uint64_t>(static_cast<unsigned int>(clockCyclesHigh)) << 32)
             | static_cast<uint64_t>(static_cast<unsigned int>(clockCyclesLow));
    }

    void SetFrameNumber(uint64_t frame)
    {
        frameNumberHigh = static_cast<int>(frame >> 32);
        frameNumberLow  = static_cast<int>(frame & 0xFFFFFFFF);
    }

    uint64_t GetFrameNumber() const
    {
        return (static_cast<uint64_t>(static_cast<unsigned int>(frameNumberHigh)) << 32)
             | static_cast<uint64_t>(static_cast<unsigned int>(frameNumberLow));
    }

    void SetThreadId(std::thread::id tid)
    {
        std::ostringstream oss;
        oss << tid;
        threadIdStr = oss.str();
    }

    std::thread::id GetThreadId() const
    {
        // Note: std::thread::id cannot be reconstructed from string;
        // use threadIdStr for serialized comparisons
        return std::thread::id{};
    }

    const std::string& GetThreadIdStr() const { return threadIdStr; }

    EventType GetEventType() const { return static_cast<EventType>(type); }
    void SetEventType(EventType et) { type = static_cast<int>(et); }
    const char* GetName() const { return name.c_str(); }
    const char* GetThreadName() const { return threadNameStr.c_str(); }

    // Approximate memory footprint of this event (object + heap allocations)
    size_t EstimateMemoryUsage() const
    {
        return sizeof(ProfileEvent)
            + threadIdStr.capacity()
            + threadNameStr.capacity()
            + name.capacity();
    }
};

/** @brief Reflection-compatible array wrapper for a set of ProfileEvent pointers. */
class CEventArray : public CReflectedBase
{
public:
    REFL_DECLARE_OBJECT(CEventArray, CReflectedBase);
    std::vector<Profiler::ProfileEvent*> m_events;
};

/** @brief Singleton that records profiler events and flushes them to file or analyzer. */
class ProfileLogger
{
public:
    static ProfileLogger& GetInstance();

    /** @param name Human-readable name for the calling thread. */
    void RegisterThread(const std::string& name);

    /** @param threadId Thread to name.
     *  @param name     Human-readable name. */
    void RegisterThread(std::thread::id threadId, const std::string& name);

    /** @param threadId Thread to look up.
     *  @return Human-readable name, or empty string if not registered. */
    std::string GetThreadName(std::thread::id threadId) const;

    /** @param threadId   Thread emitting the event.
     *  @param name       Event name.
     *  @param clockCycles Timestamp.
     *  @param isStart    true = start event, false = end event. */
    void LogEvent(std::thread::id threadId, const char* name, uint64_t clockCycles, bool isStart);

    /** @param threadId    Thread emitting the frame marker.
     *  @param clockCycles Timestamp.
     *  @param frameNumber Frame number. */
    void LogFrameEvent(std::thread::id threadId, uint64_t clockCycles, uint64_t frameNumber);

    /** @param frameNumber Frame number (uses current thread). */
    void LogFrameEvent(uint64_t frameNumber);

    void SetEnabled(bool enabled) { m_enabled.store(enabled, std::memory_order_relaxed); }
    bool IsEnabled() const { return m_enabled.load(std::memory_order_relaxed); }

    void FlushToFile();

    /** @param filename Output log file path. */
    void SetOutputFile(const std::string& filename);

    void Clear()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_events.clear();
        m_serializeIndex = 0;
        m_estimatedMemoryUsage = 0;
    }

    // ── Memory Budget Management ──────────────────────────────────────────────

    /** @param bytes Maximum memory budget in bytes (0 = unlimited). Default is 10 MB. */
    void SetMemoryBudget(size_t bytes) { m_memoryBudget.store(bytes, std::memory_order_relaxed); }
    size_t GetMemoryBudget() const { return m_memoryBudget.load(std::memory_order_relaxed); }
    size_t GetEstimatedMemoryUsage() const { return m_estimatedMemoryUsage; }

    /** @param analyzer          Destination analyzer.
     *  @param clearAfterTransfer If true, clears the internal event buffer. */
    void TransferToAnalyzer(ProfilerAnalyzer& analyzer, bool clearAfterTransfer = true);

    /** @return A copy of all currently buffered events. */
    std::vector<ProfileEvent> CopyEvents() const;

    /** @return Number of events currently buffered. */
    size_t GetEventCount() const;

    /** @brief Serializes all events accumulated since the last call.
     *  @return JSON string containing an array of serialized ProfileEvent objects. */
    std::string SerializeRecentEvents();

    /** @brief Serializes all events accumulated since the last call into a compact binary buffer.
     *  @return Binary buffer, or empty vector on failure. */
    std::vector<uint8_t> SerializeRecentEventsBinary();

private:
    ProfileLogger() = default;
    ~ProfileLogger();

    // Evicts oldest events from the front of the deque until memory usage
    // is at or below the configured budget. Must be called under m_mutex.
    void EnforceMemoryBudget();

    std::atomic<bool>   m_enabled{false};
    mutable std::mutex  m_mutex;
    std::deque<ProfileEvent> m_events;
    std::string         m_outputFile{"profile.log"};
    size_t              m_serializeIndex{0};

    // Thread name registry (thread::id -> human-readable name)
    std::unordered_map<std::thread::id, std::string> m_threadNames;

    // Memory tracking
    std::atomic<size_t> m_memoryBudget{10 * 1024 * 1024};
    size_t              m_estimatedMemoryUsage{0};
};

/** @brief RAII scoped profiler that logs start/end events. */
class ScopedProfiler
{
public:
    explicit ScopedProfiler(const char* name);
    ~ScopedProfiler();

private:
    const char*     m_name;
    std::thread::id m_threadId;
};

/** @brief Utility for marking frame boundaries in the profiler. */
class FrameProfiler
{
public:
    static void MarkFrame();

    /** @param frameNumber Explicit frame number to record. */
    static void MarkFrame(uint64_t frameNumber);

    static uint64_t GetCurrentFrame() { return s_currentFrame.load(std::memory_order_relaxed); }

private:
    static std::atomic<uint64_t> s_currentFrame;
};

/** @brief High-resolution clock cycle counter. */
inline uint64_t GetClockCycles()
{
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
    return __rdtsc();
#else
    auto now = std::chrono::high_resolution_clock::now();
    return static_cast<uint64_t>(now.time_since_epoch().count());
#endif
}

/** @brief CPU frequency detection for accurate cycle-to-time conversion.
 *         The result is cached after the first call so the measurement overhead
 *         (a short sleep) is paid only once per process. */
inline double GetCPUFrequency()
{
    static double cachedFrequency = 0.0;
    static bool   isInitialized   = false;

    if (isInitialized)
    {
        return cachedFrequency;
    }

#if defined(_WIN32) && defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
    auto startTime = std::chrono::high_resolution_clock::now();
    uint64_t startCycles = __rdtsc();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    uint64_t endCycles = __rdtsc();
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime);

    if (endCycles > startCycles && duration.count() > 0)
    {
        double cyclesPerNanosecond = static_cast<double>(endCycles - startCycles) / duration.count();
        double freq = cyclesPerNanosecond * 1e9;
        if (freq >= 0.5e9 && freq <= 6.0e9)
        {
            cachedFrequency = freq;
            isInitialized = true;
            return cachedFrequency;
        }
    }
#endif
    cachedFrequency = 3.0e9;
    isInitialized = true;
    return cachedFrequency;
}

} // namespace Profiler

// ── Convenience Macros ────────────────────────────────────────────────────────
#define PROFILE_SCOPE(name)              Profiler::ScopedProfiler _prof(name)
#define PROFILE_FUNCTION()               PROFILE_SCOPE(__FUNCTION__)
#define PROFILE_FRAME()                  Profiler::FrameProfiler::MarkFrame()
#define PROFILE_FRAME_NUMBER(frameNum)   Profiler::FrameProfiler::MarkFrame(frameNum)

// Instrumentation levels:
#define INST_LEVEL_VLOW   1  // Called once or on initialization etc.
#define INST_LEVEL_LOW    2  // Called infrequently, e.g. once per FRAME
#define INST_LEVEL_MEDIUM 3  // Called frequently, e.g. multiple times per FRAME
#define INST_LEVEL_HIGH   4  // Called very frequently, many times per FRAME
#define INST_LEVEL_VHIGH  5  // Called in tight loops, e.g. per vertex, per pixel

#define INST_LEVEL INST_LEVEL_MEDIUM

#if INST_LEVEL >= INST_LEVEL_VLOW
#define DECLARE_FUNC_VLOW() PROFILE_FUNCTION()
#else
#define DECLARE_FUNC_VLOW()
#endif
#if INST_LEVEL >= INST_LEVEL_LOW
#define DECLARE_FUNC_LOW() PROFILE_FUNCTION()
#else
#define DECLARE_FUNC_LOW()
#endif
#if INST_LEVEL >= INST_LEVEL_MEDIUM
#define DECLARE_FUNC_MEDIUM() PROFILE_FUNCTION()
#else
#define DECLARE_FUNC_MEDIUM()
#endif
#if INST_LEVEL >= INST_LEVEL_HIGH
#define DECLARE_FUNC_HIGH() PROFILE_FUNCTION()
#else
#define DECLARE_FUNC_HIGH()
#endif
#if INST_LEVEL >= INST_LEVEL_VHIGH
#define DECLARE_FUNC_VHIGH() PROFILE_FUNCTION()
#else
#define DECLARE_FUNC_VHIGH()
#endif

#if INST_LEVEL >= INST_LEVEL_VLOW
#define DECLARE_SCOPE_VLOW(_name_) PROFILE_SCOPE(_name_)
#else
#define DECLARE_SCOPE_VLOW()
#endif
#if INST_LEVEL >= INST_LEVEL_LOW
#define DECLARE_SCOPE_LOW(_name_) PROFILE_SCOPE(_name_)
#else
#define DECLARE_SCOPE_LOW(_name_)
#endif
#if INST_LEVEL >= INST_LEVEL_MEDIUM
#define DECLARE_SCOPE_MEDIUM(_name_) PROFILE_SCOPE(_name_)
#else
#define DECLARE_SCOPE_MEDIUM(_name_)
#endif
#if INST_LEVEL >= INST_LEVEL_HIGH
#define DECLARE_SCOPE_HIGH(_name_) PROFILE_SCOPE(_name_)
#else
#define DECLARE_SCOPE_HIGH(_name_)
#endif
#if INST_LEVEL >= INST_LEVEL_VHIGH
#define DECLARE_SCOPE_VHIGH(_name_) PROFILE_SCOPE(_name_)
#else
#define DECLARE_SCOPE_VHIGH(_name_)
#endif
