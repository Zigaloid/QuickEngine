#pragma once

#include "imgui.h"
#include "Profiler/ProfilerAnalyzer.h"
#include "ProfilerViewUtils.h"
#include "ImGuiButterflyChart.h"
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <set>

namespace Profiler {

    // -------------------------------------------------------------------------
    // ProfilerFrameComparisonView — renders a butterfly chart comparing the
    // per-function costs of two selected profiler frames.
    // -------------------------------------------------------------------------
    class ProfilerFrameComparisonView
    {
    public:
        ProfilerFrameComparisonView();
        ~ProfilerFrameComparisonView() = default;

        // Main entry point — creates its own window.
        void Render(const char* windowTitle, TimelineFlameGraphData* data,
                    const std::set<uint64_t>& selectedFrames, bool* isOpen = nullptr);

        // Content-only rendering (no window Begin/End).
        // Must be called between ImGui::Begin / ImGui::End.
        void RenderContent(TimelineFlameGraphData* data,
                           const std::set<uint64_t>& selectedFrames);

        // Thread filter: when non-empty, only threads whose name is in this list
        // contribute to the frame cost comparison. An empty list means "all threads".
        void SetThreadFilter(const std::vector<std::string>& threadNames) { m_ThreadFilter = threadNames; }
        void ClearThreadFilter() { m_ThreadFilter.clear(); }

    private:
        // Internal content rendering (shared by Render and RenderContent)
        void RenderComparisonContent(TimelineFlameGraphData* data,
                                     const std::set<uint64_t>& selectedFrames);

        // CPU frequency for accurate timing calculations
        double m_CPUFrequency = 3.0e9;

        // Butterfly chart widget
        ImGuiButterflyChart m_ButterflyChart;

        // Cost display mode (inclusive vs exclusive)
        CostMode m_CostMode = CostMode::Inclusive;

        // Thread name filter (empty = all threads)
        std::vector<std::string> m_ThreadFilter;
    };

} // namespace Profiler