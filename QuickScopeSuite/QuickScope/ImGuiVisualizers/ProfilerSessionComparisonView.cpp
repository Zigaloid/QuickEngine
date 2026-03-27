#include "ProfilerSessionComparisonView.h"
#include "Profiler/Profiler.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <numeric>
#include <map>

namespace Profiler {

    // -------------------------------------------------------------------------
    // Session colors (visually distinct palette)
    // -------------------------------------------------------------------------

    static const ImU32 kSessionColors[] = {
        IM_COL32(100, 160, 255, 255),   // Blue
        IM_COL32(255, 140,  80, 255),   // Orange
        IM_COL32(100, 220, 100, 255),   // Green
        IM_COL32(220, 100, 220, 255),   // Purple
        IM_COL32(255, 220,  80, 255),   // Yellow
        IM_COL32(100, 220, 220, 255),   // Cyan
        IM_COL32(255, 100, 100, 255),   // Red
        IM_COL32(180, 180, 180, 255),   // Gray
    };
    static constexpr size_t kSessionColorCount = sizeof(kSessionColors) / sizeof(kSessionColors[0]);

    ImU32 ProfilerSessionComparisonView::GetSessionColor(size_t sessionIndex)
    {
        return kSessionColors[sessionIndex % kSessionColorCount];
    }

    // -------------------------------------------------------------------------
    // Construction
    // -------------------------------------------------------------------------

    ProfilerSessionComparisonView::ProfilerSessionComparisonView()
    {
        m_CPUFrequency = ViewUtils::GetCachedCPUFrequency();
    }

    // -------------------------------------------------------------------------
    // Public API
    // -------------------------------------------------------------------------

    void ProfilerSessionComparisonView::SetSessions(const std::vector<SessionInfo>& sessions)
    {
        // Detect changes
        bool changed = (sessions.size() != m_Sessions.size());
        if (!changed) {
            for (size_t i = 0; i < sessions.size(); ++i) {
                if (sessions[i].displayName != m_Sessions[i].displayName ||
                    sessions[i].data != m_Sessions[i].data) {
                    changed = true;
                    break;
                }
            }
        }

        if (changed) {
            // Preserve selection state for sessions that still exist
            std::map<std::string, bool> prevSelection;
            for (const auto& s : m_Sessions) {
                prevSelection[s.displayName] = s.selected;
            }

            m_Sessions = sessions;

            for (auto& s : m_Sessions) {
                auto it = prevSelection.find(s.displayName);
                if (it != prevSelection.end()) {
                    s.selected = it->second;
                }
            }

            m_CostsDirty = true;
        }
    }

    void ProfilerSessionComparisonView::ToggleSessionSelection(size_t index)
    {
        if (index < m_Sessions.size()) {
            m_Sessions[index].selected = !m_Sessions[index].selected;
            m_CostsDirty = true;
        }
    }

    void ProfilerSessionComparisonView::SelectAll()
    {
        for (auto& s : m_Sessions) s.selected = true;
        m_CostsDirty = true;
    }

    void ProfilerSessionComparisonView::DeselectAll()
    {
        for (auto& s : m_Sessions) s.selected = false;
        m_CostsDirty = true;
    }

    // -------------------------------------------------------------------------
    // Rendering entry points
    // -------------------------------------------------------------------------

    void ProfilerSessionComparisonView::Render(const char* windowTitle, bool* isOpen)
    {
        ImGui::SetNextWindowSize(ImVec2(900, 600), ImGuiCond_FirstUseEver);

        if (ImGui::Begin(windowTitle, isOpen)) {
            RenderComparisonContent();
        }
        ImGui::End();
    }

    void ProfilerSessionComparisonView::RenderContent()
    {
        RenderComparisonContent();
    }

    // -------------------------------------------------------------------------
    // Shared content implementation
    // -------------------------------------------------------------------------

    void ProfilerSessionComparisonView::RenderComparisonContent()
    {
        if (m_Sessions.empty()) {
            ImGui::TextDisabled("No profiler sessions available.");
            return;
        }

        // Count selected sessions with data
        int selectedCount = 0;
        for (const auto& s : m_Sessions) {
            if (s.selected && s.data && s.data->GetThreadCount() > 0)
                ++selectedCount;
        }

        // Session selector (left panel)
        float selectorWidth = 250.0f;
        float availWidth = ImGui::GetContentRegionAvail().x;
        float chartWidth = availWidth - selectorWidth - ImGui::GetStyle().ItemSpacing.x;

        if (ImGui::BeginChild("SessionSelector", ImVec2(selectorWidth, 0), true)) {
            RenderSessionSelector();
        }
        ImGui::EndChild();

        ImGui::SameLine();

        // Chart area (right panel)
        if (ImGui::BeginChild("ComparisonChart", ImVec2(chartWidth, 0), true)) {
            if (selectedCount < 2) {
                ImGui::TextDisabled("Select at least 2 sessions with data to compare.");
            }
            else {
                // Recompute if dirty
                if (m_CostsDirty) {
                    if (m_BreadcrumbStack.empty()) {
                        RecomputeThreadCosts();
                    }
                    else {
                        RecomputeFunctionCosts();
                    }
                    m_CostsDirty = false;
                }

                // Breadcrumb navigation
                RenderBreadcrumbs();

                ImGui::Separator();

                // Render the appropriate bar graph
                if (m_BreadcrumbStack.empty()) {
                    RenderThreadComparison();
                }
                else {
                    RenderFunctionComparison();
                }
            }
        }
        ImGui::EndChild();
    }

    // -------------------------------------------------------------------------
    // Session selector panel
    // -------------------------------------------------------------------------

    void ProfilerSessionComparisonView::RenderSessionSelector()
    {
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Sessions (%zu)", m_Sessions.size());

        if (ImGui::Button("Select All")) { SelectAll(); }
        ImGui::SameLine();
        if (ImGui::Button("Clear All")) { DeselectAll(); }

        ImGui::Separator();

        for (size_t i = 0; i < m_Sessions.size(); ++i) {
            auto& session = m_Sessions[i];

            ImGui::PushID(static_cast<int>(i));

            // Color indicator
            ImU32 color = GetSessionColor(i);
            ImVec2 pos = ImGui::GetCursorScreenPos();
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            float sz = ImGui::GetTextLineHeight();
            drawList->AddRectFilled(pos, ImVec2(pos.x + sz, pos.y + sz), color);
            ImGui::Dummy(ImVec2(sz + 4.0f, 0));
            ImGui::SameLine();

            bool hasData = session.data && session.data->GetThreadCount() > 0;

            if (!hasData) {
                ImGui::BeginDisabled();
            }

            if (ImGui::Checkbox(session.displayName.c_str(), &session.selected)) {
                m_CostsDirty = true;
            }

            if (!hasData) {
                ImGui::EndDisabled();
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                    ImGui::SetTooltip("No profiler data in this session");
                }
            }

            ImGui::PopID();
        }
    }

    // -------------------------------------------------------------------------
    // Breadcrumb navigation
    // -------------------------------------------------------------------------

    void ProfilerSessionComparisonView::RenderBreadcrumbs()
    {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Session Comparison");
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        // Root level
        if (m_BreadcrumbStack.empty()) {
            ImGui::Text("Threads (avg frame cost)");
        }
        else {
            // Clickable "Threads" root
            if (ImGui::SmallButton("Threads")) {
                m_BreadcrumbStack.clear();
                m_CostsDirty = true;
            }

            for (size_t i = 0; i < m_BreadcrumbStack.size(); ++i) {
                ImGui::SameLine();
                ImGui::TextDisabled(">");
                ImGui::SameLine();

                const auto& level = m_BreadcrumbStack[i];
                std::string label = level.functionName.empty()
                    ? level.threadName
                    : level.functionName;

                // Last breadcrumb is not clickable (current level)
                if (i + 1 < m_BreadcrumbStack.size()) {
                    if (ImGui::SmallButton(label.c_str())) {
                        // Pop everything after this level
                        m_BreadcrumbStack.resize(i + 1);
                        m_CostsDirty = true;
                    }
                }
                else {
                    ImGui::Text("%s", label.c_str());
                }
            }
        }
    }

    // -------------------------------------------------------------------------
    // Thread-level comparison
    // -------------------------------------------------------------------------

    void ProfilerSessionComparisonView::RenderThreadComparison()
    {
        if (m_ThreadGroups.empty()) {
            ImGui::TextDisabled("No thread data found in the selected sessions.");
            return;
        }

        // Build grouped data for the bar graph
        std::vector<std::string> groupLabels;
        std::vector<std::vector<double>> groupValues;
        std::vector<std::string> seriesNames;

        // Series names = selected session names
        if (!m_ThreadGroups.empty()) {
            seriesNames = m_ThreadGroups[0].sessionNames;
        }

        for (const auto& group : m_ThreadGroups) {
            groupLabels.push_back(group.threadName);
            groupValues.push_back(group.sessionCostsMs);
        }

        int clickedGroup = RenderGroupedBarGraph("ThreadCompGraph",
            groupLabels, groupValues, seriesNames);

        if (clickedGroup >= 0 && clickedGroup < static_cast<int>(m_ThreadGroups.size())) {
            // Drill down into the clicked thread
            DrillDownLevel level;
            level.threadName = m_ThreadGroups[clickedGroup].threadName;
            level.functionName = "";
            level.depth = 0;
            m_BreadcrumbStack.clear();
            m_BreadcrumbStack.push_back(level);
            m_CostsDirty = true;
        }
    }

    // -------------------------------------------------------------------------
    // Function-level comparison (drill-down)
    // -------------------------------------------------------------------------

    void ProfilerSessionComparisonView::RenderFunctionComparison()
    {
        if (m_FunctionGroups.empty()) {
            ImGui::TextDisabled("No function data at this call stack level.");

            // Allow going back
            if (ImGui::Button("Go Back")) {
                if (!m_BreadcrumbStack.empty()) {
                    m_BreadcrumbStack.pop_back();
                    m_CostsDirty = true;
                }
            }
            return;
        }

        std::vector<std::string> groupLabels;
        std::vector<std::vector<double>> groupValues;
        std::vector<std::string> seriesNames;

        if (!m_FunctionGroups.empty()) {
            seriesNames = m_FunctionGroups[0].sessionNames;
        }

        for (const auto& group : m_FunctionGroups) {
            groupLabels.push_back(group.functionName);
            groupValues.push_back(group.sessionCostsMs);
        }

        int clickedGroup = RenderGroupedBarGraph("FuncCompGraph",
            groupLabels, groupValues, seriesNames);

        if (clickedGroup >= 0 && clickedGroup < static_cast<int>(m_FunctionGroups.size())) {
            const auto& current = m_BreadcrumbStack.back();

            // Drill deeper: add a new level for the clicked function
            DrillDownLevel nextLevel;
            nextLevel.threadName = current.threadName;
            nextLevel.functionName = m_FunctionGroups[clickedGroup].functionName;
            nextLevel.depth = current.depth + 1;
            m_BreadcrumbStack.push_back(nextLevel);
            m_CostsDirty = true;
        }
    }

    // -------------------------------------------------------------------------
    // Grouped bar graph rendering with double-click detection
    // -------------------------------------------------------------------------

    int ProfilerSessionComparisonView::RenderGroupedBarGraph(
        const char* id,
        const std::vector<std::string>& groupLabels,
        const std::vector<std::vector<double>>& groupValues,
        const std::vector<std::string>& seriesNames)
    {
        if (groupLabels.empty() || seriesNames.empty())
            return -1;

        size_t numGroups = groupLabels.size();
        size_t numSeries = seriesNames.size();
        int doubleClickedGroup = -1;

        // Find the global max value for consistent scaling
        double maxVal = 0.0;
        for (const auto& vals : groupValues) {
            for (double v : vals) {
                maxVal = std::max(maxVal, v);
            }
        }
        if (maxVal < 1e-9) maxVal = 1.0;

        // Layout constants
        float availWidth = ImGui::GetContentRegionAvail().x;
        float availHeight = ImGui::GetContentRegionAvail().y - ImGui::GetTextLineHeightWithSpacing() * 3.0f;
        availHeight = std::max(availHeight, 150.0f);

        float groupSpacing = 20.0f;
        float barSpacing = 2.0f;
        float barWidth = std::max(8.0f,
            (availWidth - groupSpacing * numGroups) / (numGroups * numSeries) - barSpacing);
        barWidth = std::min(barWidth, 60.0f);

        float groupWidth = numSeries * (barWidth + barSpacing) - barSpacing;
        float totalWidth = numGroups * (groupWidth + groupSpacing) - groupSpacing;

        // Reserve space for labels below
        float labelHeight = ImGui::GetTextLineHeightWithSpacing();

        ImVec2 canvasPos = ImGui::GetCursorScreenPos();
        float chartHeight = availHeight - labelHeight;
        ImVec2 canvasSize = ImVec2(std::max(totalWidth + 40.0f, availWidth), chartHeight);

        // Scrollable region if content is wider than available space
        ImGui::BeginChild(id, ImVec2(availWidth, availHeight),
            false, ImGuiWindowFlags_HorizontalScrollbar);

        ImVec2 origin = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##canvas",
            ImVec2(std::max(totalWidth + 40.0f, availWidth - 20.0f), chartHeight + labelHeight));
        bool canvasHovered = ImGui::IsItemHovered();

        ImDrawList* drawList = ImGui::GetWindowDrawList();

        // Background
        ImVec2 bgMin = origin;
        ImVec2 bgMax = ImVec2(origin.x + std::max(totalWidth + 40.0f, availWidth - 20.0f),
                              origin.y + chartHeight);
        drawList->AddRectFilled(bgMin, bgMax, IM_COL32(30, 30, 30, 255));
        drawList->AddRect(bgMin, bgMax, IM_COL32(60, 60, 60, 255));

        // Grid lines
        const int gridLines = 5;
        for (int g = 1; g < gridLines; ++g) {
            float y = origin.y + chartHeight * g / gridLines;
            drawList->AddLine(ImVec2(origin.x, y),
                              ImVec2(bgMax.x, y),
                              IM_COL32(60, 60, 60, 128));

            double gridVal = maxVal * (1.0 - static_cast<double>(g) / gridLines);
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(2) << gridVal << " ms";
            ImVec2 textSize = ImGui::CalcTextSize(oss.str().c_str());
            drawList->AddText(ImVec2(origin.x + 2.0f, y - textSize.y - 1.0f),
                              IM_COL32(180, 180, 180, 200), oss.str().c_str());
        }

        // Draw grouped bars
        ImVec2 mousePos = ImGui::GetIO().MousePos;
        float xCursor = origin.x + 20.0f; // Left margin

        for (size_t gi = 0; gi < numGroups; ++gi) {
            const auto& vals = groupValues[gi];

            for (size_t si = 0; si < numSeries && si < vals.size(); ++si) {
                double val = vals[si];
                float normalizedHeight = static_cast<float>(val / maxVal);
                normalizedHeight = std::clamp(normalizedHeight, 0.0f, 1.0f);
                float barHeight = normalizedHeight * chartHeight;

                float barX = xCursor + si * (barWidth + barSpacing);
                ImVec2 barMin(barX, origin.y + chartHeight - barHeight);
                ImVec2 barMax(barX + barWidth, origin.y + chartHeight);

                ImU32 barColor = GetSessionColor(si);

                // Hover detection
                bool barHovered = canvasHovered &&
                    mousePos.x >= barMin.x && mousePos.x <= barMax.x &&
                    mousePos.y >= barMin.y && mousePos.y <= barMax.y;

                if (barHovered) {
                    // Brighten on hover
                    ImVec4 c = ImGui::ColorConvertU32ToFloat4(barColor);
                    c.x = std::min(c.x * 1.4f, 1.0f);
                    c.y = std::min(c.y * 1.4f, 1.0f);
                    c.z = std::min(c.z * 1.4f, 1.0f);
                    barColor = ImGui::ColorConvertFloat4ToU32(c);

                    // Tooltip
                    ImGui::BeginTooltip();
                    ImGui::TextColored(ImVec4(1, 1, 1, 1), "%s", groupLabels[gi].c_str());
                    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(GetSessionColor(si)),
                        "Session: %s", seriesNames[si].c_str());
                    ImGui::Text("Cost: %.3f ms", val);
                    ImGui::TextDisabled("Double-click to drill down");
                    ImGui::EndTooltip();

                    // Double-click detection
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        doubleClickedGroup = static_cast<int>(gi);
                    }
                }

                drawList->AddRectFilled(barMin, barMax, barColor);
                drawList->AddRect(barMin, barMax, IM_COL32(255, 255, 255, 60));

                // Value text above bar (if there's enough space)
                if (barHeight > 15.0f && barWidth > 20.0f) {
                    std::ostringstream oss;
                    oss << std::fixed << std::setprecision(2) << val;
                    std::string valStr = oss.str();
                    ImVec2 textSize = ImGui::CalcTextSize(valStr.c_str());
                    if (textSize.x <= barWidth) {
                        drawList->AddText(
                            ImVec2(barX + (barWidth - textSize.x) * 0.5f, barMin.y - textSize.y - 2.0f),
                            IM_COL32(220, 220, 220, 255), valStr.c_str());
                    }
                }
            }

            // Group label below
            ImVec2 labelPos(xCursor, origin.y + chartHeight + 4.0f);
            std::string truncLabel = groupLabels[gi];
            ImVec2 labelSize = ImGui::CalcTextSize(truncLabel.c_str());

            // Truncate if too wide
            while (labelSize.x > groupWidth + groupSpacing - 4.0f && truncLabel.size() > 5) {
                truncLabel = truncLabel.substr(0, truncLabel.size() - 4) + "...";
                labelSize = ImGui::CalcTextSize(truncLabel.c_str());
            }

            float labelX = xCursor + (groupWidth - labelSize.x) * 0.5f;
            drawList->AddText(ImVec2(labelX, labelPos.y),
                              IM_COL32(200, 200, 200, 255), truncLabel.c_str());

            xCursor += groupWidth + groupSpacing;
        }

        ImGui::EndChild();

        // Legend
        ImGui::Spacing();
        for (size_t si = 0; si < seriesNames.size(); ++si) {
            if (si > 0) ImGui::SameLine();

            ImVec2 pos = ImGui::GetCursorScreenPos();
            float sz = ImGui::GetTextLineHeight();
            ImGui::GetWindowDrawList()->AddRectFilled(pos, ImVec2(pos.x + sz, pos.y + sz),
                                                       GetSessionColor(si));
            ImGui::Dummy(ImVec2(sz + 2.0f, sz));
            ImGui::SameLine();
            ImGui::Text("%s", seriesNames[si].c_str());
            ImGui::SameLine();
            ImGui::Dummy(ImVec2(10.0f, 0));
        }

        return doubleClickedGroup;
    }

    // -------------------------------------------------------------------------
    // Cost computation: Thread level
    // -------------------------------------------------------------------------

    void ProfilerSessionComparisonView::RecomputeThreadCosts()
    {
        m_ThreadGroups.clear();

        // Gather selected sessions
        std::vector<size_t> selectedIndices;
        for (size_t i = 0; i < m_Sessions.size(); ++i) {
            if (m_Sessions[i].selected && m_Sessions[i].data &&
                m_Sessions[i].data->GetThreadCount() > 0) {
                selectedIndices.push_back(i);
            }
        }

        if (selectedIndices.empty()) return;

        // For each selected session, compute per-thread average frame costs
        // threadName -> (sessionIndex -> cost)
        std::map<std::string, std::map<size_t, double>> threadCostMap;

        for (size_t si : selectedIndices) {
            auto* data = m_Sessions[si].data;
            auto frames = GatherFrameIntervals(data);
            auto threadCosts = AccumulateThreadCosts(data, frames);

            for (const auto& tc : threadCosts) {
                threadCostMap[tc.threadName][si] = tc.averageFrameCostMs;
            }
        }

        // Build grouped data
        for (auto& [threadName, sessionCosts] : threadCostMap) {
            ThreadGroupData group;
            group.threadName = threadName;

            for (size_t si : selectedIndices) {
                auto it = sessionCosts.find(si);
                group.sessionCostsMs.push_back(it != sessionCosts.end() ? it->second : 0.0);
                group.sessionNames.push_back(m_Sessions[si].displayName);
            }

            m_ThreadGroups.push_back(std::move(group));
        }

        // Sort by max cost descending
        std::sort(m_ThreadGroups.begin(), m_ThreadGroups.end(),
            [](const ThreadGroupData& a, const ThreadGroupData& b) {
                double maxA = *std::max_element(a.sessionCostsMs.begin(), a.sessionCostsMs.end());
                double maxB = *std::max_element(b.sessionCostsMs.begin(), b.sessionCostsMs.end());
                return maxA > maxB;
            });
    }

    // -------------------------------------------------------------------------
    // Cost computation: Function level (drill-down)
    // -------------------------------------------------------------------------

    void ProfilerSessionComparisonView::RecomputeFunctionCosts()
    {
        m_FunctionGroups.clear();

        if (m_BreadcrumbStack.empty()) return;

        const auto& current = m_BreadcrumbStack.back();

        std::vector<size_t> selectedIndices;
        for (size_t i = 0; i < m_Sessions.size(); ++i) {
            if (m_Sessions[i].selected && m_Sessions[i].data &&
                m_Sessions[i].data->GetThreadCount() > 0) {
                selectedIndices.push_back(i);
            }
        }

        if (selectedIndices.empty()) return;

        // funcName -> (sessionIndex -> cost)
        std::map<std::string, std::map<size_t, double>> funcCostMap;

        for (size_t si : selectedIndices) {
            auto* data = m_Sessions[si].data;
            auto frames = GatherFrameIntervals(data);
            auto funcCosts = AccumulateFunctionCosts(
                data, frames, current.threadName, current.depth, current.functionName);

            for (const auto& fc : funcCosts) {
                funcCostMap[fc.functionName][si] = fc.averageCostMs;
            }
        }

        for (auto& [funcName, sessionCosts] : funcCostMap) {
            FunctionGroupData group;
            group.functionName = funcName;

            for (size_t si : selectedIndices) {
                auto it = sessionCosts.find(si);
                group.sessionCostsMs.push_back(it != sessionCosts.end() ? it->second : 0.0);
                group.sessionNames.push_back(m_Sessions[si].displayName);
            }

            m_FunctionGroups.push_back(std::move(group));
        }

        // Sort by max cost descending
        std::sort(m_FunctionGroups.begin(), m_FunctionGroups.end(),
            [](const FunctionGroupData& a, const FunctionGroupData& b) {
                double maxA = *std::max_element(a.sessionCostsMs.begin(), a.sessionCostsMs.end());
                double maxB = *std::max_element(b.sessionCostsMs.begin(), b.sessionCostsMs.end());
                return maxA > maxB;
            });
    }

    // -------------------------------------------------------------------------
    // Data gathering helpers
    // -------------------------------------------------------------------------

    std::vector<ProfilerSessionComparisonView::FrameInterval>
    ProfilerSessionComparisonView::GatherFrameIntervals(TimelineFlameGraphData* data)
    {
        std::vector<FrameInterval> intervals;
        if (!data) return intervals;

        auto markers = ViewUtils::GatherSortedGlobalFrameMarkers(data);
        if (markers.size() < 2) return intervals;

        for (size_t i = 0; i < markers.size(); ++i) {
            FrameInterval fi;
            fi.frameNumber = markers[i].frameNumber;
            fi.startCycles = markers[i].clockCycles;
            fi.endCycles = (i + 1 < markers.size())
                ? markers[i + 1].clockCycles
                : data->globalEndTime;
            intervals.push_back(fi);
        }

        return intervals;
    }

    std::vector<ThreadCostEntry>
    ProfilerSessionComparisonView::AccumulateThreadCosts(
        TimelineFlameGraphData* data,
        const std::vector<FrameInterval>& frames)
    {
        if (!data || frames.empty()) return {};

        double cpuFreq = ViewUtils::GetCachedCPUFrequency();

        // threadName -> total inclusive cycles across all frames
        std::map<std::string, uint64_t> threadTotalCycles;

        for (size_t t = 0; t < data->GetThreadCount(); ++t) {
            auto* thread = data->GetThread(t);
            if (!thread) continue;

            uint64_t totalCycles = 0;

            // Sum up only depth-0 (top-level) events within frame intervals
            for (const auto& event : thread->events) {
                if (event->depth != 0) continue;
                if (event->duration == 0) continue;

                // Check if this event falls within any frame
                for (const auto& frame : frames) {
                    if (event->startTime >= frame.startCycles &&
                        event->startTime < frame.endCycles) {
                        totalCycles += event->duration;
                        break;
                    }
                }
            }

            if (totalCycles > 0) {
                threadTotalCycles[thread->threadName] += totalCycles;
            }
        }

        std::vector<ThreadCostEntry> result;
        for (auto& [name, cycles] : threadTotalCycles) {
            ThreadCostEntry entry;
            entry.threadName = name;
            entry.totalCycles = cycles;
            entry.frameCount = frames.size();
            entry.averageFrameCostMs = (static_cast<double>(cycles) / cpuFreq / frames.size()) * 1000.0;
            result.push_back(entry);
        }

        return result;
    }

    std::vector<FunctionCostEntry>
    ProfilerSessionComparisonView::AccumulateFunctionCosts(
        TimelineFlameGraphData* data,
        const std::vector<FrameInterval>& frames,
        const std::string& threadName,
        int depth,
        const std::string& parentFunctionName)
    {
        if (!data || frames.empty()) return {};

        double cpuFreq = ViewUtils::GetCachedCPUFrequency();

        // Find the target thread
        TimelineThreadData* targetThread = nullptr;
        for (size_t t = 0; t < data->GetThreadCount(); ++t) {
            auto* thread = data->GetThread(t);
            if (thread && thread->threadName == threadName) {
                targetThread = thread;
                break;
            }
        }

        if (!targetThread) return {};

        // Sort events by start time for efficient parent-child matching
        std::vector<TimelineFlameNode*> allEvents;
        for (const auto& event : targetThread->events) {
            allEvents.push_back(event.get());
        }
        std::sort(allEvents.begin(), allEvents.end(),
            [](const TimelineFlameNode* a, const TimelineFlameNode* b) {
                if (a->startTime != b->startTime)
                    return a->startTime < b->startTime;
                return a->depth < b->depth;
            });

        // funcName -> total inclusive cycles
        std::map<std::string, uint64_t> funcCycles;
        std::map<std::string, uint32_t> funcCallCount;

        for (const auto& frame : frames) {
            // Gather events in this frame
            for (auto* event : allEvents) {
                if (event->startTime < frame.startCycles) continue;
                if (event->startTime >= frame.endCycles) break;
                if (event->duration == 0) continue;

                if (parentFunctionName.empty()) {
                    // First drill-down: show depth-0 events for this thread
                    if (event->depth == 0) {
                        funcCycles[event->name] += event->duration;
                        funcCallCount[event->name]++;
                    }
                }
                else {
                    // Subsequent drill-downs: show events at (depth) that are
                    // children of events named parentFunctionName at (depth - 1)
                    if (event->depth == depth) {
                        // Check if this event's parent is the right function
                        // A parent is a depth-1 event whose time range encloses this one
                        bool hasMatchingParent = false;
                        for (auto* candidate : allEvents) {
                            if (candidate->startTime < frame.startCycles) continue;
                            if (candidate->startTime >= frame.endCycles) break;
                            if (candidate->depth == depth - 1 &&
                                candidate->name == parentFunctionName &&
                                candidate->startTime <= event->startTime &&
                                candidate->endTime >= event->endTime) {
                                hasMatchingParent = true;
                                break;
                            }
                        }

                        if (hasMatchingParent) {
                            funcCycles[event->name] += event->duration;
                            funcCallCount[event->name]++;
                        }
                    }
                }
            }
        }

        std::vector<FunctionCostEntry> result;
        for (auto& [name, cycles] : funcCycles) {
            FunctionCostEntry entry;
            entry.functionName = name;
            entry.totalCycles = cycles;
            entry.callCount = funcCallCount[name];
            entry.averageCostMs = (static_cast<double>(cycles) / cpuFreq / frames.size()) * 1000.0;
            result.push_back(entry);
        }

        return result;
    }

} // namespace Profiler