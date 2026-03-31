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

    // Information about a single outlier frame
    struct OutlierFrameInfo {
        uint64_t frameNumber = 0;
        uint64_t durationCycles = 0;
        double   durationMs = 0.0;
        double   deviationPct = 0.0;    // How far above average (%)
    };

    // -------------------------------------------------------------------------
    // ProfilerOutlierView — lists detected outlier frames and shows a butterfly
    // chart comparing the selected outlier against a reference (typical) frame.
    // -------------------------------------------------------------------------
    class ProfilerOutlierView
    {
    public:
        ProfilerOutlierView();
        ~ProfilerOutlierView() = default;

        // Main entry point — creates its own window.
        void Render(const char* windowTitle, TimelineFlameGraphData* data,
                    float outlierThresholdPct, bool* isOpen = nullptr);

        // Content-only rendering (no window Begin/End).
        // Must be called between ImGui::Begin / ImGui::End.
        void RenderContent(TimelineFlameGraphData* data, float outlierThresholdPct);

        // Thread filter: when non-empty, only threads whose name is in this list
        // contribute to the butterfly cost comparison. An empty list means "all threads".
        void SetThreadFilter(const std::vector<std::string>& threadNames) { m_ThreadFilter = threadNames; }
        void ClearThreadFilter() { m_ThreadFilter.clear(); }

    private:
        // Internal content rendering (shared by Render and RenderContent)
        void RenderOutlierContent(TimelineFlameGraphData* data, float outlierThresholdPct);

        // Detect outlier frames and build the outlier list
        void DetectOutliers(TimelineFlameGraphData* data, float outlierThresholdPct);

        // Find the reference (median-duration) frame
        uint64_t FindReferenceFrame(TimelineFlameGraphData* data) const;

        // Render the outlier list panel
        void RenderOutlierList();

        // Render the butterfly comparison panel
        void RenderButterflyComparison(TimelineFlameGraphData* data);

        // CPU frequency for accurate timing calculations
        double m_CPUFrequency = 3.0e9;

        // Detected outliers
        std::vector<OutlierFrameInfo> m_Outliers;
        double m_AverageDurationMs = 0.0;
        uint64_t m_AverageDurationCycles = 0;

        // Selection state
        int      m_SelectedOutlierIndex = -1;
        uint64_t m_ReferenceFrame = 0;
        bool     m_HasReferenceFrame = false;

        // Cached threshold to detect when we need to re-detect
        float m_LastThresholdPct = -1.0f;
        const TimelineFlameGraphData* m_LastData = nullptr;

        // Butterfly chart widget
        ImGuiButterflyChart m_ButterflyChart;

        // Cost display mode (inclusive vs exclusive)
        CostMode m_CostMode = CostMode::Inclusive;

        // Thread name filter (empty = all threads)
        std::vector<std::string> m_ThreadFilter;
    };

} // namespace Profiler