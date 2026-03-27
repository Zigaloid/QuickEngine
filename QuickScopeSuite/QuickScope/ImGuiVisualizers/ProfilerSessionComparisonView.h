#pragma once

#include "imgui.h"
#include "Profiler/ProfilerAnalyzer.h"
#include "ProfilerViewUtils.h"
#include "ImGuiBarGraph.h"
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <set>

namespace Profiler {

    // -------------------------------------------------------------------------
    // Describes one session's data for cross-session comparison.
    // -------------------------------------------------------------------------
    struct SessionInfo {
        std::string displayName;
        TimelineFlameGraphData* data = nullptr;     // Non-owning pointer
        bool selected = false;                      // Whether to include in comparison
    };

    // -------------------------------------------------------------------------
    // Aggregated per-thread cost data for a single session.
    // -------------------------------------------------------------------------
    struct ThreadCostEntry {
        std::string threadName;
        double averageFrameCostMs = 0.0;            // Average frame cost in milliseconds
        uint64_t totalCycles = 0;
        uint64_t frameCount = 0;
    };

    // -------------------------------------------------------------------------
    // Aggregated per-function cost for drill-down within a thread.
    // -------------------------------------------------------------------------
    struct FunctionCostEntry {
        std::string functionName;
        double averageCostMs = 0.0;                 // Average cost per frame in milliseconds
        uint64_t totalCycles = 0;
        uint64_t callCount = 0;
    };

    // -------------------------------------------------------------------------
    // A level in the drill-down breadcrumb stack.
    // -------------------------------------------------------------------------
    struct DrillDownLevel {
        std::string threadName;                     // Thread being drilled into
        std::string functionName;                   // Function at this level (empty = thread root)
        int depth = 0;                              // Call stack depth (0 = top-level)
    };

    // -------------------------------------------------------------------------
    // ProfilerSessionComparisonView — compares average frame costs across
    // multiple open profiler sessions using grouped bar graphs.
    //
    // Top level: per-thread average frame cost, one bar per session per thread.
    // Double-click a bar ? drill into that thread's top-level call stack.
    // Double-click again ? drill deeper into child calls.
    // A breadcrumb trail allows navigating back up.
    // -------------------------------------------------------------------------
    class ProfilerSessionComparisonView
    {
    public:
        ProfilerSessionComparisonView();
        ~ProfilerSessionComparisonView() = default;

        // Main entry point — creates its own window.
        void Render(const char* windowTitle, bool* isOpen = nullptr);

        // Content-only rendering (no window Begin/End).
        void RenderContent();

        // Set the list of available sessions (called each frame by the host).
        void SetSessions(const std::vector<SessionInfo>& sessions);

        // Toggle selection of a session by index.
        void ToggleSessionSelection(size_t index);

        // Select / deselect all sessions.
        void SelectAll();
        void DeselectAll();

    private:
        // Shared content implementation
        void RenderComparisonContent();

        // Render the session selection panel
        void RenderSessionSelector();

        // Render the breadcrumb navigation bar
        void RenderBreadcrumbs();

        // Render the thread-level bar graph (top level)
        void RenderThreadComparison();

        // Render the function-level bar graph (drill-down)
        void RenderFunctionComparison();

        // Compute average per-thread frame costs for selected sessions
        void RecomputeThreadCosts();

        // Compute average per-function costs at the current drill-down level
        void RecomputeFunctionCosts();

        // Gather all frame intervals for a session's data
        struct FrameInterval {
            uint64_t frameNumber;
            uint64_t startCycles;
            uint64_t endCycles;
        };
        static std::vector<FrameInterval> GatherFrameIntervals(TimelineFlameGraphData* data);

        // Accumulate per-thread costs across all frames
        static std::vector<ThreadCostEntry> AccumulateThreadCosts(
            TimelineFlameGraphData* data,
            const std::vector<FrameInterval>& frames);

        // Accumulate per-function costs for a specific thread at a given depth
        // and optionally filtered to children of a specific parent function.
        static std::vector<FunctionCostEntry> AccumulateFunctionCosts(
            TimelineFlameGraphData* data,
            const std::vector<FrameInterval>& frames,
            const std::string& threadName,
            int depth,
            const std::string& parentFunctionName);

        // Session data
        std::vector<SessionInfo> m_Sessions;

        // Drill-down state
        std::vector<DrillDownLevel> m_BreadcrumbStack;  // Empty = thread level

        // Cached cost data (invalidated when sessions or drill-down change)
        bool m_CostsDirty = true;

        // Thread-level costs: map<threadName, vector<cost per selected session>>
        struct ThreadGroupData {
            std::string threadName;
            std::vector<double> sessionCostsMs;         // One entry per selected session
            std::vector<std::string> sessionNames;      // Corresponding session names
        };
        std::vector<ThreadGroupData> m_ThreadGroups;

        // Function-level costs (when drilled down)
        struct FunctionGroupData {
            std::string functionName;
            std::vector<double> sessionCostsMs;
            std::vector<std::string> sessionNames;
        };
        std::vector<FunctionGroupData> m_FunctionGroups;

        // CPU frequency
        double m_CPUFrequency = 3.0e9;

        // Colors for sessions (cycled)
        static ImU32 GetSessionColor(size_t sessionIndex);

        // Bar graph interaction: returns the index of the double-clicked group, or -1
        int RenderGroupedBarGraph(const char* id,
                                  const std::vector<std::string>& groupLabels,
                                  const std::vector<std::vector<double>>& groupValues,
                                  const std::vector<std::string>& seriesNames);
    };

} // namespace Profiler