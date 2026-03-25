#include "ProfilerOutlierView.h"
#include "Profiler/Profiler.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <numeric>

namespace Profiler {

    // -------------------------------------------------------------------------
    // ProfilerOutlierView
    // -------------------------------------------------------------------------

    ProfilerOutlierView::ProfilerOutlierView()
    {
        m_CPUFrequency = ViewUtils::GetCachedCPUFrequency();
    }

    // -------------------------------------------------------------------------    
    // Main render entry (self-contained window)
    // -------------------------------------------------------------------------

    void ProfilerOutlierView::Render(const char* windowTitle,
                                      TimelineFlameGraphData* data,
                                      float outlierThresholdPct,
                                      bool* isOpen)
    {
        ImGui::SetNextWindowSize(ImVec2(900, 550), ImGuiCond_FirstUseEver);

        if (ImGui::Begin(windowTitle, isOpen)) {
            RenderOutlierContent(data, outlierThresholdPct);
        }
        ImGui::End();
    }

    // -------------------------------------------------------------------------
    // Content-only rendering
    // -------------------------------------------------------------------------

    void ProfilerOutlierView::RenderContent(TimelineFlameGraphData* data,
                                             float outlierThresholdPct)
    {
        RenderOutlierContent(data, outlierThresholdPct);
    }

    // -------------------------------------------------------------------------
    // Shared content implementation
    // -------------------------------------------------------------------------

    void ProfilerOutlierView::RenderOutlierContent(TimelineFlameGraphData* data,
                                                    float outlierThresholdPct)
    {
        if (!data || data->GetThreadCount() == 0) {
            ImGui::TextDisabled("No profiler data available.");
            return;
        }

        // Re-detect outliers when data or threshold changes
        if (data != m_LastData || outlierThresholdPct != m_LastThresholdPct) {
            DetectOutliers(data, outlierThresholdPct);
            m_ReferenceFrame = FindReferenceFrame(data);
            m_HasReferenceFrame = (m_ReferenceFrame != 0);
            m_LastData = data;
            m_LastThresholdPct = outlierThresholdPct;
        }

        // Summary header
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Outlier Analysis");
        ImGui::SameLine();
        ImGui::TextDisabled("(threshold: %.0f%% above average)", outlierThresholdPct);
        ImGui::Text("Average frame: %.3f ms  |  Outliers detected: %zu",
                    m_AverageDurationMs, m_Outliers.size());
        if (m_HasReferenceFrame) {
            ImGui::SameLine();
            ImGui::TextDisabled("  |  Reference frame: %llu", m_ReferenceFrame);
        }

        // Cost mode toggle (Inclusive / Exclusive)
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - 220.0f);
        ImGui::Text("Cost Mode:");
        ImGui::SameLine();
        if (ImGui::RadioButton("Inclusive##Outlier", m_CostMode == CostMode::Inclusive)) {
            m_CostMode = CostMode::Inclusive;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Total time including time spent in child calls");
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Exclusive##Outlier", m_CostMode == CostMode::Exclusive)) {
            m_CostMode = CostMode::Exclusive;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Self time only, excluding time spent in child calls");
        }

        ImGui::Separator();

        if (m_Outliers.empty()) {
            ImGui::TextDisabled("No outlier frames detected at the current threshold.");
            return;
        }

        // Split layout: outlier list on the left, butterfly chart on the right
        float listWidth = 300.0f;
        float availWidth = ImGui::GetContentRegionAvail().x;
        float chartWidth = availWidth - listWidth - ImGui::GetStyle().ItemSpacing.x;

        // Left panel — outlier list
        if (ImGui::BeginChild("OutlierList", ImVec2(listWidth, 0), true)) {
            RenderOutlierList();
        }
        ImGui::EndChild();

        ImGui::SameLine();

        // Right panel — butterfly chart
        if (ImGui::BeginChild("OutlierChart", ImVec2(chartWidth, 0), true)) {
            if (m_SelectedOutlierIndex >= 0 &&
                m_SelectedOutlierIndex < static_cast<int>(m_Outliers.size()) &&
                m_HasReferenceFrame) {
                RenderButterflyComparison(data);
            }
            else {
                ImGui::TextDisabled("Select an outlier frame from the list to compare.");
            }
        }
        ImGui::EndChild();
    }

    // -------------------------------------------------------------------------
    // Detect outlier frames
    // -------------------------------------------------------------------------

    void ProfilerOutlierView::DetectOutliers(TimelineFlameGraphData* data,
                                              float outlierThresholdPct)
    {
        m_Outliers.clear();
        m_SelectedOutlierIndex = -1;

        auto globalFrameMarkers = ViewUtils::GatherSortedGlobalFrameMarkers(data);
        if (globalFrameMarkers.size() < 2) {
            m_AverageDurationMs = 0.0;
            m_AverageDurationCycles = 0;
            return;
        }

        // Calculate the duration for each frame interval
        struct FrameDuration {
            uint64_t frameNumber;
            uint64_t cycles;
        };
        std::vector<FrameDuration> frameDurations;
        frameDurations.reserve(globalFrameMarkers.size());

        for (size_t i = 0; i < globalFrameMarkers.size(); ++i) {
            uint64_t frameStart = globalFrameMarkers[i].clockCycles;
            uint64_t frameEnd = (i + 1 < globalFrameMarkers.size())
                ? globalFrameMarkers[i + 1].clockCycles
                : data->globalEndTime;

            uint64_t duration = frameEnd - frameStart;
            frameDurations.push_back({ globalFrameMarkers[i].frameNumber, duration });
        }

        if (frameDurations.empty()) {
            m_AverageDurationMs = 0.0;
            m_AverageDurationCycles = 0;
            return;
        }

        // Compute average frame duration
        double totalCycles = 0.0;
        for (const auto& fd : frameDurations) {
            totalCycles += static_cast<double>(fd.cycles);
        }
        double averageCycles = totalCycles / static_cast<double>(frameDurations.size());
        m_AverageDurationCycles = static_cast<uint64_t>(averageCycles);
        m_AverageDurationMs = (averageCycles / m_CPUFrequency) * 1000.0;

        // Threshold: average + (average * percentage / 100)
        double threshold = averageCycles * (1.0 + outlierThresholdPct / 100.0);

        // Collect outliers
        for (const auto& fd : frameDurations) {
            if (static_cast<double>(fd.cycles) > threshold) {
                OutlierFrameInfo info;
                info.frameNumber = fd.frameNumber;
                info.durationCycles = fd.cycles;
                info.durationMs = (static_cast<double>(fd.cycles) / m_CPUFrequency) * 1000.0;
                info.deviationPct = ((static_cast<double>(fd.cycles) - averageCycles) / averageCycles) * 100.0;
                m_Outliers.push_back(info);
            }
        }

        // Sort by deviation descending (worst outliers first)
        std::sort(m_Outliers.begin(), m_Outliers.end(),
            [](const OutlierFrameInfo& a, const OutlierFrameInfo& b) {
            return a.deviationPct > b.deviationPct;
        });
    }

    // -------------------------------------------------------------------------
    // Find a reference frame (the frame closest to median duration)
    // -------------------------------------------------------------------------

    uint64_t ProfilerOutlierView::FindReferenceFrame(TimelineFlameGraphData* data) const
    {
        auto globalFrameMarkers = ViewUtils::GatherSortedGlobalFrameMarkers(data);
        if (globalFrameMarkers.size() < 2) {
            return 0;
        }

        struct FrameDuration {
            uint64_t frameNumber;
            uint64_t cycles;
        };
        std::vector<FrameDuration> frameDurations;
        frameDurations.reserve(globalFrameMarkers.size());

        for (size_t i = 0; i < globalFrameMarkers.size(); ++i) {
            uint64_t frameStart = globalFrameMarkers[i].clockCycles;
            uint64_t frameEnd = (i + 1 < globalFrameMarkers.size())
                ? globalFrameMarkers[i + 1].clockCycles
                : data->globalEndTime;

            frameDurations.push_back({ globalFrameMarkers[i].frameNumber, frameEnd - frameStart });
        }

        if (frameDurations.empty()) {
            return 0;
        }

        // Sort by duration to find the median
        std::sort(frameDurations.begin(), frameDurations.end(),
            [](const FrameDuration& a, const FrameDuration& b) {
            return a.cycles < b.cycles;
        });

        // Return the median frame as the reference
        size_t medianIndex = frameDurations.size() / 2;
        return frameDurations[medianIndex].frameNumber;
    }

    // -------------------------------------------------------------------------
    // Render the outlier list
    // -------------------------------------------------------------------------

    void ProfilerOutlierView::RenderOutlierList()
    {
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Outlier Frames (%zu)", m_Outliers.size());
        ImGui::Separator();

        // Column headers
        if (ImGui::BeginTable("OutlierTable", 3,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable))
        {
            ImGui::TableSetupColumn("Frame", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableSetupColumn("Duration", ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ImGui::TableSetupColumn("Deviation", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableHeadersRow();

            for (int i = 0; i < static_cast<int>(m_Outliers.size()); ++i) {
                const auto& outlier = m_Outliers[i];
                ImGui::TableNextRow();

                bool isSelected = (m_SelectedOutlierIndex == i);

                // Frame number column
                ImGui::TableSetColumnIndex(0);
                char label[64];
                snprintf(label, sizeof(label), "%llu", outlier.frameNumber);
                if (ImGui::Selectable(label, isSelected,
                    ImGuiSelectableFlags_SpanAllColumns))
                {
                    m_SelectedOutlierIndex = i;
                }

                // Duration column
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.3f ms", outlier.durationMs);

                // Deviation column — color-coded by severity
                ImGui::TableSetColumnIndex(2);
                ImVec4 deviationColor;
                if (outlier.deviationPct > 200.0)
                    deviationColor = ImVec4(1.0f, 0.2f, 0.2f, 1.0f);  // Red
                else if (outlier.deviationPct > 100.0)
                    deviationColor = ImVec4(1.0f, 0.5f, 0.2f, 1.0f);  // Orange
                else
                    deviationColor = ImVec4(1.0f, 0.8f, 0.3f, 1.0f);  // Yellow

                ImGui::TextColored(deviationColor, "+%.1f%%", outlier.deviationPct);
            }

            ImGui::EndTable();
        }
    }

    // -------------------------------------------------------------------------
    // Render the butterfly comparison for the selected outlier vs reference
    // -------------------------------------------------------------------------

    void ProfilerOutlierView::RenderButterflyComparison(TimelineFlameGraphData* data)
    {
        const auto& outlier = m_Outliers[m_SelectedOutlierIndex];
        uint64_t outlierFrame = outlier.frameNumber;
        uint64_t refFrame = m_ReferenceFrame;

        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Outlier Frame %llu", outlierFrame);
        ImGui::SameLine();
        ImGui::TextDisabled("(%.3f ms, +%.1f%%)", outlier.durationMs, outlier.deviationPct);
        ImGui::SameLine();
        ImGui::Text("vs");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Reference Frame %llu", refFrame);
        ImGui::SameLine();
        ImGui::TextDisabled("(median frame)");

        ImGui::Separator();

        // Accumulate per-frame costs using shared utility
        auto outlierCosts = ViewUtils::AccumulateFrameCosts(data, outlierFrame);
        auto refCosts = ViewUtils::AccumulateFrameCosts(data, refFrame);

        const bool useExclusive = (m_CostMode == CostMode::Exclusive);

        std::map<std::string, ImGuiButterflyChart::RowData> mergeMap;
        for (const auto& cost : outlierCosts) {
            auto& row = mergeMap[cost.name];
            row.label = cost.name;
            row.leftValue = static_cast<float>(useExclusive ? cost.exclusiveCycles : cost.inclusiveCycles);
            row.leftCount = cost.callCount;
        }
        for (const auto& cost : refCosts) {
            auto& row = mergeMap[cost.name];
            row.label = cost.name;
            row.rightValue = static_cast<float>(useExclusive ? cost.exclusiveCycles : cost.inclusiveCycles);
            row.rightCount = cost.callCount;
        }

        std::vector<ImGuiButterflyChart::RowData> rows;
        rows.reserve(mergeMap.size());
        for (auto& [name, row] : mergeMap) {
            rows.push_back(std::move(row));
        }

        if (rows.empty()) {
            ImGui::TextDisabled("No events found in the selected frames.");
        }
        else {
            ImGui::Text("%zu functions compared (%s time)", rows.size(),
                useExclusive ? "exclusive" : "inclusive");

            // Configure the chart
            auto& config = m_ButterflyChart.GetConfig();
            config.leftHeader = "Outlier F" + std::to_string(outlierFrame);
            config.rightHeader = "Ref F" + std::to_string(refFrame);
            config.sortMode = ImGuiButterflyChart::SortMode::ByDiffDescending;

            // Color function consistent with other profiler views
            m_ButterflyChart.SetColorFunction(
                [](size_t, const std::string& label, float) -> ImU32 {
                    return ViewUtils::GetNodeColor(label);
                });

            // Timing formatter using shared utility
            double cpuFreq = m_CPUFrequency;
            m_ButterflyChart.SetFormatFunction(
                [cpuFreq](float cycles) -> std::string {
                    return ViewUtils::FormatCycles(cycles, cpuFreq);
                });

            // Custom tooltip
            const char* costModeLabel = useExclusive ? "Exclusive" : "Inclusive";
            m_ButterflyChart.SetTooltipFunction(
                [outlierFrame, refFrame, cpuFreq, &outlier, costModeLabel]
                (size_t, const ImGuiButterflyChart::RowData& row) {
                    ImGui::Text("Function: %s", row.label.c_str());
                    ImGui::TextDisabled("(%s time)", costModeLabel);
                    ImGui::Separator();

                    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                        "Outlier Frame %llu (+%.1f%%):", outlierFrame, outlier.deviationPct);
                    if (row.leftValue > 0.0f)
                        ImGui::Text("  Time: %s (%u call%s)",
                            ViewUtils::FormatCycles(row.leftValue, cpuFreq).c_str(),
                            row.leftCount, row.leftCount == 1 ? "" : "s");
                    else
                        ImGui::TextDisabled("  Not present");

                    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f),
                        "Reference Frame %llu:", refFrame);
                    if (row.rightValue > 0.0f)
                        ImGui::Text("  Time: %s (%u call%s)",
                            ViewUtils::FormatCycles(row.rightValue, cpuFreq).c_str(),
                            row.rightCount, row.rightCount == 1 ? "" : "s");
                    else
                        ImGui::TextDisabled("  Not present");

                    if (row.leftValue > 0.0f && row.rightValue > 0.0f) {
                        float diff = row.leftValue - row.rightValue;
                        float pct = (diff / row.rightValue) * 100.0f;
                        ImVec4 diffColor = diff > 0
                            ? ImVec4(1.0f, 0.3f, 0.3f, 1.0f)
                            : ImVec4(0.3f, 1.0f, 0.3f, 1.0f);
                        ImGui::Separator();
                        ImGui::TextColored(diffColor, "Delta: %s%s (%.1f%%)",
                            diff > 0 ? "+" : "",
                            ViewUtils::FormatCycles(std::abs(diff), cpuFreq).c_str(), pct);
                    }
                });

            m_ButterflyChart.SetData(rows);
            m_ButterflyChart.Render("OutlierButterflyChart");
        }
    }

} // namespace Profiler