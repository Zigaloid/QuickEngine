#include "ProfilerFrameComparisonView.h"
#include <algorithm>
#include <cmath>

namespace Profiler {

    // -------------------------------------------------------------------------
    // ProfilerFrameComparisonView
    // -------------------------------------------------------------------------

    ProfilerFrameComparisonView::ProfilerFrameComparisonView()
    {
        m_CPUFrequency = ViewUtils::GetCachedCPUFrequency();
    }

    // -------------------------------------------------------------------------
    // Main render entry (self-contained window)
    // -------------------------------------------------------------------------

    void ProfilerFrameComparisonView::Render(const char* windowTitle,
                                              TimelineFlameGraphData* data,
                                              const std::set<uint64_t>& selectedFrames,
                                              bool* isOpen)
    {
        ImGui::SetNextWindowSize(ImVec2(800, 500), ImGuiCond_FirstUseEver);

        if (ImGui::Begin(windowTitle, isOpen)) {
            RenderComparisonContent(data, selectedFrames);
        }
        ImGui::End();
    }

    // -------------------------------------------------------------------------
    // Content-only rendering (called by the controller inside its window)
    // -------------------------------------------------------------------------

    void ProfilerFrameComparisonView::RenderContent(TimelineFlameGraphData* data,
                                                     const std::set<uint64_t>& selectedFrames)
    {
        RenderComparisonContent(data, selectedFrames);
    }

    // -------------------------------------------------------------------------
    // Shared content implementation
    // -------------------------------------------------------------------------

    void ProfilerFrameComparisonView::RenderComparisonContent(TimelineFlameGraphData* data,
                                                               const std::set<uint64_t>& selectedFrames)
    {
        if (!data || data->GetThreadCount() == 0) {
            ImGui::TextDisabled("No profiler data available.");
            return;
        }

        if (selectedFrames.size() < 2) {
            ImGui::TextDisabled("Select at least 2 frames to compare.");
            return;
        }

        // Use the first two selected frames
        auto it = selectedFrames.begin();
        uint64_t leftFrame = *it++;
        uint64_t rightFrame = *it;

        ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "Frame %llu", leftFrame);
        ImGui::SameLine();
        ImGui::Text("vs");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.4f, 1.0f), "Frame %llu", rightFrame);
        ImGui::SameLine();
        ImGui::TextDisabled("(sorted by cost, highest first)");

        // Cost mode toggle (Inclusive / Exclusive)
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - 220.0f);
        ImGui::Text("Cost Mode:");
        ImGui::SameLine();
        if (ImGui::RadioButton("Inclusive", m_CostMode == CostMode::Inclusive)) {
            m_CostMode = CostMode::Inclusive;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Total time including time spent in child calls");
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Exclusive", m_CostMode == CostMode::Exclusive)) {
            m_CostMode = CostMode::Exclusive;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Self time only, excluding time spent in child calls");
        }

        ImGui::Separator();

        // Accumulate per-frame costs using shared utility
        auto leftCosts = ViewUtils::AccumulateFrameCosts(data, leftFrame);
        auto rightCosts = ViewUtils::AccumulateFrameCosts(data, rightFrame);

        const bool useExclusive = (m_CostMode == CostMode::Exclusive);

        std::map<std::string, ImGuiButterflyChart::RowData> mergeMap;
        for (const auto& cost : leftCosts) {
            auto& row = mergeMap[cost.name];
            row.label = cost.name;
            row.leftValue = static_cast<float>(useExclusive ? cost.exclusiveCycles : cost.inclusiveCycles);
            row.leftCount = cost.callCount;
        }
        for (const auto& cost : rightCosts) {
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
            config.leftHeader = "F" + std::to_string(leftFrame);
            config.rightHeader = "F" + std::to_string(rightFrame);
            config.sortMode = ImGuiButterflyChart::SortMode::ByMaxDescending;

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

            // Custom tooltip with profiler-specific details
            const char* costModeLabel = useExclusive ? "Exclusive" : "Inclusive";
            m_ButterflyChart.SetTooltipFunction(
                [leftFrame, rightFrame, cpuFreq, costModeLabel]
                (size_t, const ImGuiButterflyChart::RowData& row) {
                    ImGui::Text("Function: %s", row.label.c_str());
                    ImGui::TextDisabled("(%s time)", costModeLabel);
                    ImGui::Separator();

                    ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "Frame %llu:", leftFrame);
                    if (row.leftValue > 0.0f)
                        ImGui::Text("  Time: %s (%u call%s)",
                            ViewUtils::FormatCycles(row.leftValue, cpuFreq).c_str(),
                            row.leftCount, row.leftCount == 1 ? "" : "s");
                    else
                        ImGui::TextDisabled("  Not present");

                    ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.4f, 1.0f), "Frame %llu:", rightFrame);
                    if (row.rightValue > 0.0f)
                        ImGui::Text("  Time: %s (%u call%s)",
                            ViewUtils::FormatCycles(row.rightValue, cpuFreq).c_str(),
                            row.rightCount, row.rightCount == 1 ? "" : "s");
                    else
                        ImGui::TextDisabled("  Not present");

                    if (row.leftValue > 0.0f && row.rightValue > 0.0f) {
                        float diff = row.rightValue - row.leftValue;
                        float pct = (diff / row.leftValue) * 100.0f;
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
            m_ButterflyChart.Render("FrameComparisonChart");
        }
    }

} // namespace Profiler