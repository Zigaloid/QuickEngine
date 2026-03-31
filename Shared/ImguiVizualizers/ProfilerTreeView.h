#pragma once

#include "imgui.h"
#include "Profiler/ProfilerAnalyzer.h"
#include "ProfilerViewUtils.h"
#include <memory>
#include <vector>
#include <string>
#include <map>

namespace Profiler {

    // -------------------------------------------------------------------------
    // A tree node that aggregates call-stack information for the tree view.
    // Each node represents a unique function name at a particular position in
    // the call hierarchy.  Children are keyed by function name so that
    // repeated calls to the same function at the same depth are merged.
    // -------------------------------------------------------------------------
    struct CallTreeNode {
        std::string name;
        uint64_t inclusiveCycles = 0;   // Total time including children
        uint64_t exclusiveCycles = 0;   // Time spent in this function only
        uint32_t callCount = 0;         // Number of times this call appeared
        int      depth = 0;

        std::vector<std::unique_ptr<CallTreeNode>> children;

        // Find or create a child with the given name
        CallTreeNode* GetOrCreateChild(const std::string& childName);
    };

    // -------------------------------------------------------------------------
    // ProfilerTreeView — renders profiler data as an interactive call-stack
    // tree with inclusive/exclusive timings, call counts, and text filtering.
    // -------------------------------------------------------------------------
    class ProfilerTreeView
    {
    public:
        ProfilerTreeView();
        ~ProfilerTreeView() = default;

        // Main entry point — creates its own window.
        void Render(const char* windowTitle, TimelineFlameGraphData* data, bool* isOpen = nullptr);

        // Content-only rendering (no window Begin/End).
        // Must be called between ImGui::Begin / ImGui::End.
        void RenderContent(TimelineFlameGraphData* data);

        // Thread filter: when non-empty, only threads whose name is in this list are shown.
        // An empty list means "show all threads".
        void SetThreadFilter(const std::vector<std::string>& threadNames) { m_ThreadFilter = threadNames; }
        void ClearThreadFilter() { m_ThreadFilter.clear(); }

    private:
        // Rebuild the call tree from the raw timeline data
        void RebuildTree(TimelineFlameGraphData* data);

        // Rendering helpers
        void RenderToolbar();
        void RenderThreadTree(const CallTreeNode& threadRoot, size_t threadIndex);
        void RenderNodeRow(const CallTreeNode& node, uint64_t threadTotalCycles);

        // Filtering
        bool PassesFilter(const CallTreeNode& node) const;
        bool SubtreePassesFilter(const CallTreeNode& node) const;
        bool IsThreadInFilter(const std::string& threadName) const;

        // Internal content rendering (shared by Render and RenderContent)
        void RenderTreeContent(TimelineFlameGraphData* data);

        // Cached tree roots — one per thread
        std::vector<std::unique_ptr<CallTreeNode>> m_ThreadRoots;

        // The data pointer we built the tree from (used for dirty-checking)
        const TimelineFlameGraphData* m_LastBuiltData = nullptr;
        size_t m_LastEventFingerprint = 0;

        // UI state
        char m_FilterBuffer[256] = {};
        bool m_SortByExclusive = false;
        bool m_ShowCallCounts = true;
        double m_CPUFrequency = 3.0e9;

        // Thread name filter (empty = show all)
        std::vector<std::string> m_ThreadFilter;
    };

} // namespace Profiler