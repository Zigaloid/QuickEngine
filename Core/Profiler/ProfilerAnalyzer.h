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

    // Internal structures
    struct RawEvent {
        size_t threadHash;        // Changed from std::thread::id to size_t
        std::string name;
        uint64_t clockCycles;
        bool isStart;
    };

    struct AnalyzedEvent {
        size_t threadHash;        // Changed from std::thread::id to size_t
        std::string name;
        uint64_t startCycles;
        uint64_t endCycles;
        uint64_t duration;
        int depth;
    };

    // Frame marker structure
    struct FrameMarker {
        size_t threadHash;
        uint64_t clockCycles;
        uint64_t frameNumber;

        FrameMarker(size_t hash, uint64_t cycles, uint64_t frame)
            : threadHash(hash), clockCycles(cycles), frameNumber(frame) {}
    };

    // Timeline-based flamegraph data structures
    struct TimelineFlameNode {
        std::string name;
        uint64_t startTime = 0;      // Actual start time of this specific event
        uint64_t endTime = 0;        // Actual end time of this specific event
        uint64_t duration = 0;       // Duration of this specific event
        int depth = 0;               // Call stack depth

        // Reference to the original event
        const AnalyzedEvent* sourceEvent = nullptr;

        // Visualization helpers
        float x = 0.0f;              // X position in flamegraph (0.0 to 1.0)
        float width = 0.0f;          // Width in flamegraph (0.0 to 1.0)

        // Color/display helpers
        uint32_t eventIndex = 0;     // Index for unique coloring

        TimelineFlameNode(const AnalyzedEvent* event) : sourceEvent(event) {
            if (event) {
                name = event->name;
                startTime = event->startCycles;
                endTime = event->endCycles;
                duration = event->duration;
                depth = event->depth;
            }
        }

        // Calculate percentage of total thread time
        float GetPercentage(uint64_t threadTotalTime) const {
            return threadTotalTime > 0 ? static_cast<float>(duration) / threadTotalTime : 0.0f;
        }
    };

    struct TimelineThreadData {
        size_t threadHash;
        std::string threadName;
        std::vector<std::unique_ptr<TimelineFlameNode>> events;  // All events in chronological order
        std::vector<FrameMarker> frameMarkers;                   // Frame markers for this thread

        uint64_t threadStartTime = 0;     // Earliest event in this thread
        uint64_t threadEndTime = 0;       // Latest event in this thread
        uint64_t threadDuration = 0;      // Total thread timeline duration
        int maxDepth = 0;                 // Maximum call stack depth

        TimelineThreadData(size_t hash, const std::string& name)
            : threadHash(hash), threadName(name) {}

        // Calculate layout positions for all events based on timeline
        void CalculateTimelineLayout();

        // Get events at a specific depth level
        std::vector<TimelineFlameNode*> GetEventsAtDepth(int targetDepth) const;

        // Get all events that overlap with a given time range
        std::vector<TimelineFlameNode*> GetEventsInTimeRange(uint64_t startTime, uint64_t endTime) const;

        // Get frame markers in a time range
        std::vector<const FrameMarker*> GetFrameMarkersInTimeRange(uint64_t startTime, uint64_t endTime) const;
    };

    struct TimelineFlameGraphData {
        std::vector<std::unique_ptr<TimelineThreadData>> threads;
        uint64_t globalStartTime = UINT64_MAX;
        uint64_t globalEndTime = 0;
        uint64_t globalDuration = 0;

        // Find thread data by hash
        TimelineThreadData* FindThread(size_t threadHash);

        // Get thread by index for iteration
        TimelineThreadData* GetThread(size_t index);
        size_t GetThreadCount() const { return threads.size(); }

        // Calculate global timeline
        void CalculateGlobalTimeline();

        // Get all events across all threads in a time range
        std::vector<TimelineFlameNode*> GetAllEventsInTimeRange(uint64_t startTime, uint64_t endTime) const;

        // Get all frame markers across all threads in a time range
        std::vector<const FrameMarker*> GetAllFrameMarkersInTimeRange(uint64_t startTime, uint64_t endTime) const;
    };

    // Legacy flamegraph data structures (kept for compatibility)
    struct FlameNode {
        std::string name;
        uint64_t selfTime = 0;       // Time spent in this function (excluding children)
        uint64_t totalTime = 0;      // Total time including children
        uint32_t callCount = 0;      // Number of times this function was called

        // Timing bounds for visualization
        uint64_t startTime = 0;      // Earliest start time for this node
        uint64_t endTime = 0;        // Latest end time for this node

        // Tree structure
        std::vector<std::unique_ptr<FlameNode>> children;
        FlameNode* parent = nullptr;

        // Visualization helpers
        float x = 0.0f;              // X position in flamegraph (0.0 to 1.0)
        float width = 0.0f;          // Width in flamegraph (0.0 to 1.0)
        int level = 0;               // Depth level for Y positioning

        // Helper methods
        void AddChild(std::unique_ptr<FlameNode> child);
        FlameNode* FindChild(const std::string& functionName);

        // Calculate percentage of total thread time
        float GetPercentage(uint64_t threadTotalTime) const {
            return threadTotalTime > 0 ? static_cast<float>(totalTime) / threadTotalTime : 0.0f;
        }
    };

    struct ThreadFlameData {
        size_t threadHash;
        std::string threadName;
        std::unique_ptr<FlameNode> rootNode;
        uint64_t totalThreadTime = 0;    // Total time for this thread
        uint64_t threadStartTime = 0;    // Earliest event in this thread
        uint64_t threadEndTime = 0;      // Latest event in this thread
        int maxDepth = 0;                // Maximum call stack depth

        ThreadFlameData(size_t hash, const std::string& name)
            : threadHash(hash), threadName(name) {
            rootNode = std::make_unique<FlameNode>();
            rootNode->name = "Root";
        }

        // Calculate layout positions for all nodes
        void CalculateLayout();
        void CalculateNodeLayout(FlameNode* node, float startX, float width,
            uint64_t timelineStart, uint64_t timelineEnd);
    };

    struct FlameGraphData {
        std::vector<std::unique_ptr<ThreadFlameData>> threads;

        size_t GetThreadCount() const { return threads.size(); }

        // Calculate global timeline
        void CalculateGlobalTimeline();
    };

    class ProfilerAnalyzer {
    public:
        // Existing file-based methods
        void LoadFromFile(const std::string& filename);
        void AnalyzeCallStack();
        void PrintReport() const;
        void PrintCallStack() const;

        // NEW: Direct data loading methods
        void LoadFromProfileEvents(const std::deque<ProfileEvent>& events, bool clearExisting = true);
        void AppendFromProfileEvents(const std::deque<ProfileEvent>& events);
        void Clear();

        // Timeline-based flamegraph generation
        std::unique_ptr<TimelineFlameGraphData> GenerateTimelineFlameGraphData() const;

        // Legacy accumulated flamegraph generation (kept for compatibility)
        std::unique_ptr<FlameGraphData> GenerateFlameGraphData() const;
        void CalculateSelfTimes(FlameNode* node) const;

        // Data access methods
        const std::vector<AnalyzedEvent>& GetEvents() const { return m_analyzedEvents; }
        const std::vector<FrameMarker>& GetFrameMarkers() const { return m_frameMarkers; }

        // NEW: Statistics and info methods
        size_t GetEventCount() const { return m_analyzedEvents.size(); }
        size_t GetFrameMarkerCount() const { return m_frameMarkers.size(); }
        size_t GetThreadCount() const;

    private:
        std::vector<AnalyzedEvent> m_analyzedEvents;
        std::vector<FrameMarker> m_frameMarkers;             // Store frame markers separately
        std::map<size_t, std::vector<std::string>> m_callStacks;    // Changed key from std::thread::id to size_t
        std::map<size_t, std::string> m_threadHashToString;

        // Internal processing methods
        void MatchStartEndPairs(const std::vector<RawEvent>& rawEvents);
        void ReconstructCallStack();

        // NEW: Convert ProfileEvent to internal format
        void ConvertProfileEventsToRawEvents(const std::deque<ProfileEvent>& events,
            std::vector<RawEvent>& outRawEvents,
            std::vector<FrameMarker>& outFrameMarkers);
    };

} // namespace Profiler