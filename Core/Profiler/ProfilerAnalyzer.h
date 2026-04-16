#pragma once

#include <vector>
#include <deque>
#include <map>
#include <string>
#include <memory>
#include <thread>

// Forward declarations to avoid circular dependency
namespace Core {
    class CoreSystem;
}

namespace Profiler {

// Forward declarations
class ProfileEvent;
enum class EventType;

// ── Internal Data Structures ──────────────────────────────────────────────────

/** @brief Raw (unpaired) profiler event record. */
struct RawEvent
{
    size_t      threadHash;    // Hash of thread ID
    std::string name;
    uint64_t    clockCycles;
    bool        isStart;
};

/** @brief Paired start/end profiler event with computed duration. */
struct AnalyzedEvent
{
    size_t      threadHash;
    std::string name;
    uint64_t    startCycles;
    uint64_t    endCycles;
    uint64_t    duration;
    int         depth;
};

/** @brief Frame boundary marker. */
struct FrameMarker
{
    size_t   threadHash;
    uint64_t clockCycles;
    uint64_t frameNumber;

    FrameMarker(size_t hash, uint64_t cycles, uint64_t frame)
        : threadHash(hash), clockCycles(cycles), frameNumber(frame)
    {
    }
};

// ── Timeline Flamegraph Data Structures ───────────────────────────────────────

/** @brief Single node in a timeline-based flamegraph. */
struct TimelineFlameNode
{
    std::string name;
    uint64_t    startTime   = 0;
    uint64_t    endTime     = 0;
    uint64_t    duration    = 0;
    int         depth       = 0;

    const AnalyzedEvent* sourceEvent = nullptr;

    float    x          = 0.0f;
    float    width      = 0.0f;
    uint32_t eventIndex = 0;

    explicit TimelineFlameNode(const AnalyzedEvent* event) : sourceEvent(event)
    {
        if (event)
        {
            name      = event->name;
            startTime = event->startCycles;
            endTime   = event->endCycles;
            duration  = event->duration;
            depth     = event->depth;
        }
    }

    /** @param threadTotalTime Total time for the owning thread.
     *  @return Fraction of thread time consumed by this event. */
    float GetPercentage(uint64_t threadTotalTime) const
    {
        return threadTotalTime > 0 ? static_cast<float>(duration) / static_cast<float>(threadTotalTime) : 0.0f;
    }
};

/** @brief All timeline events and frame markers for a single thread. */
struct TimelineThreadData
{
    size_t      threadHash;
    std::string threadName;
    std::vector<std::unique_ptr<TimelineFlameNode>> events;
    std::vector<FrameMarker> frameMarkers;

    uint64_t threadStartTime = 0;
    uint64_t threadEndTime   = 0;
    uint64_t threadDuration  = 0;
    int      maxDepth        = 0;

    TimelineThreadData(size_t hash, const std::string& name)
        : threadHash(hash), threadName(name)
    {
    }

    void CalculateTimelineLayout();
    std::vector<TimelineFlameNode*> GetEventsAtDepth(int targetDepth) const;
    std::vector<TimelineFlameNode*> GetEventsInTimeRange(uint64_t startTime, uint64_t endTime) const;
    std::vector<const FrameMarker*> GetFrameMarkersInTimeRange(uint64_t startTime, uint64_t endTime) const;
};

/** @brief Complete timeline flamegraph for all threads. */
struct TimelineFlameGraphData
{
    std::vector<std::unique_ptr<TimelineThreadData>> threads;
    uint64_t globalStartTime = UINT64_MAX;
    uint64_t globalEndTime   = 0;
    uint64_t globalDuration  = 0;

    TimelineThreadData* FindThread(size_t threadHash);
    TimelineThreadData* GetThread(size_t index);
    size_t              GetThreadCount() const { return threads.size(); }

    void CalculateGlobalTimeline();
    std::vector<TimelineFlameNode*>  GetAllEventsInTimeRange(uint64_t startTime, uint64_t endTime) const;
    std::vector<const FrameMarker*>  GetAllFrameMarkersInTimeRange(uint64_t startTime, uint64_t endTime) const;
};

// ── Legacy Flamegraph Data Structures ─────────────────────────────────────────

/** @brief Node in the legacy accumulated flamegraph tree. */
struct FlameNode
{
    std::string name;
    uint64_t    selfTime  = 0;
    uint64_t    totalTime = 0;
    uint32_t    callCount = 0;

    uint64_t startTime = 0;
    uint64_t endTime   = 0;

    std::vector<std::unique_ptr<FlameNode>> children;
    FlameNode* parent = nullptr;

    float x     = 0.0f;
    float width = 0.0f;
    int   level = 0;

    void AddChild(std::unique_ptr<FlameNode> child);
    FlameNode* FindChild(const std::string& functionName);

    float GetPercentage(uint64_t threadTotalTime) const
    {
        return threadTotalTime > 0 ? static_cast<float>(totalTime) / static_cast<float>(threadTotalTime) : 0.0f;
    }
};

/** @brief Accumulated flamegraph tree for a single thread. */
struct ThreadFlameData
{
    size_t      threadHash;
    std::string threadName;
    std::unique_ptr<FlameNode> rootNode;
    uint64_t    totalThreadTime  = 0;
    uint64_t    threadStartTime  = 0;
    uint64_t    threadEndTime    = 0;
    int         maxDepth         = 0;

    ThreadFlameData(size_t hash, const std::string& name)
        : threadHash(hash), threadName(name)
    {
        rootNode = std::make_unique<FlameNode>();
        rootNode->name = "Root";
    }

    void CalculateLayout();
    void CalculateNodeLayout(FlameNode* node, float startX, float width,
        uint64_t timelineStart, uint64_t timelineEnd);
};

/** @brief Complete accumulated flamegraph for all threads. */
struct FlameGraphData
{
    std::vector<std::unique_ptr<ThreadFlameData>> threads;

    size_t GetThreadCount() const { return threads.size(); }
    void CalculateGlobalTimeline();
};

// ── ProfilerAnalyzer ──────────────────────────────────────────────────────────

/** @brief Converts raw profiler events into analyzed flamegraph data. */
class ProfilerAnalyzer
{
public:
    void LoadFromFile(const std::string& filename);
    void AnalyzeCallStack();
    void PrintReport() const;
    void PrintCallStack() const;

    /** @param events        Source events.
     *  @param clearExisting If true, discards existing analyzed data first. */
    void LoadFromProfileEvents(const std::deque<ProfileEvent>& events, bool clearExisting = true);

    /** @param events Events to append. */
    void AppendFromProfileEvents(const std::deque<ProfileEvent>& events);

    /** @brief Clears all analyzed events, frame markers, and thread maps. */
    void Clear();

    /** @return Timeline-based flamegraph data for all threads. */
    std::unique_ptr<TimelineFlameGraphData> GenerateTimelineFlameGraphData() const;

    /** @return Legacy accumulated flamegraph data for all threads. */
    std::unique_ptr<FlameGraphData> GenerateFlameGraphData() const;

    void CalculateSelfTimes(FlameNode* node) const;

    const std::vector<AnalyzedEvent>& GetEvents() const { return m_analyzedEvents; }
    const std::vector<FrameMarker>&   GetFrameMarkers() const { return m_frameMarkers; }

    size_t GetEventCount() const { return m_analyzedEvents.size(); }
    size_t GetFrameMarkerCount() const { return m_frameMarkers.size(); }
    size_t GetThreadCount() const;

private:
    std::vector<AnalyzedEvent>       m_analyzedEvents;
    std::vector<FrameMarker>         m_frameMarkers;
    std::map<size_t, std::vector<std::string>> m_callStacks;
    std::map<size_t, std::string>    m_threadHashToString;

    void MatchStartEndPairs(const std::vector<RawEvent>& rawEvents);
    void ReconstructCallStack();

    void ConvertProfileEventsToRawEvents(const std::deque<ProfileEvent>& events,
        std::vector<RawEvent>& outRawEvents,
        std::vector<FrameMarker>& outFrameMarkers);
};

} // namespace Profiler
