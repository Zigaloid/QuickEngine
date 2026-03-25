#include "ProfilerVisualizer.h"
#include "CoreSystem/CoreSystem.h"
#include "Net/NexusClient.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <functional>

#if defined(_WIN32)
#include <windows.h>
#undef min
#undef max
#endif

static const std::string PROFILER_PIPE = "Profiler";

namespace Profiler {

    // Simple AABB point-in-rect test used by multiple render methods
    static bool IsPointInRect(const ImVec2& point, const ImVec2& rectMin, const ImVec2& rectMax)
    {
        return point.x >= rectMin.x && point.x <= rectMax.x &&
            point.y >= rectMin.y && point.y <= rectMax.y;
    }

    ProfilerVisualizer::ProfilerVisualizer()
        : m_TimelineFlameGraphData(nullptr)
        , m_FlameGraphZoom(1.0f)
        , m_FlameGraphCenterTime(0.0f)
        , m_HoveredNode(nullptr)
        , m_SelectedNode(nullptr)
        , m_HoveredThread(nullptr)
        , m_SelectedThread(nullptr)
        , m_ShowCallCounts(false)
        , m_ShowFrameMarkers(true)
        , m_ShowTimeTicks(true)
        , m_MinDisplayTime(0.0f)
        , m_AutoRefreshFlameGraph(false)
        , m_ViewInitialized(false)
        , m_HasPendingData(false)
    {
        m_CPUFrequency = ViewUtils::GetCachedCPUFrequency();
    }

    ProfilerVisualizer::~ProfilerVisualizer()
    {
    }

    // -------------------------------------------------------------------------
    // Utility helpers
    // -------------------------------------------------------------------------

    ProfilerVisualizer::VisibleTimeRange ProfilerVisualizer::GetVisibleTimeRange() const
    {
        VisibleTimeRange vtr;
        vtr.duration = m_TimelineFlameGraphData->globalDuration / m_FlameGraphZoom;
        vtr.startTime = m_FlameGraphCenterTime - (vtr.duration * 0.5f);
        vtr.endTime = vtr.startTime + vtr.duration;
        return vtr;
    }

    float ProfilerVisualizer::TimeToScreenX(float time, float canvasX, float canvasWidth, const VisibleTimeRange& vtr) const
    {
        return canvasX + ((time - vtr.startTime) / vtr.duration) * canvasWidth;
    }

    int ProfilerVisualizer::CalculateActualMaxDepth(TimelineThreadData* thread)
    {
        if (!thread || thread->events.empty()) {
            return 0;
        }

        int maxDepth = 0;
        for (const auto& event : thread->events) {
            if (event->duration > 0) {
                maxDepth = std::max(maxDepth, event->depth);
            }
        }
        return maxDepth;
    }

    // -------------------------------------------------------------------------
    // Data management
    // -------------------------------------------------------------------------

    void ProfilerVisualizer::ToggleFrameSelection(uint64_t frameNumber)
    {
        if (m_SelectedFrames.count(frameNumber) > 0) {
            m_SelectedFrames.erase(frameNumber);
        }
        else {
            m_SelectedFrames.insert(frameNumber);
        }
    }

    void ProfilerVisualizer::SetFlameGraphData(std::unique_ptr<TimelineFlameGraphData> data)
    {
        std::lock_guard<std::mutex> lock(m_PendingDataMutex);
        m_PendingFlameGraphData = std::move(data);
        m_HasPendingData = true;
    }

    void ProfilerVisualizer::ClearData()
    {
        std::lock_guard<std::mutex> lock(m_PendingDataMutex);
        m_PendingFlameGraphData.reset();
        m_HasPendingData = false;
        m_PendingClear = true;
    }

    void ProfilerVisualizer::ApplyPendingData()
    {
        if (!m_HasPendingData && !m_PendingClear) return;

        std::unique_ptr<TimelineFlameGraphData> newData;
        bool shouldClear = false;
        {
            std::lock_guard<std::mutex> lock(m_PendingDataMutex);
            shouldClear = m_PendingClear;
            m_PendingClear = false;
            if (m_HasPendingData) {
                newData = std::move(m_PendingFlameGraphData);
                m_HasPendingData = false;
            }
        }

        if (shouldClear) {
            m_TimelineFlameGraphData.reset();
            ResetView();
            // Don't return — fall through to apply newData if present
        }

        if (!newData)
            return;

        if (!m_TimelineFlameGraphData || m_TimelineFlameGraphData->GetThreadCount() == 0)
        {
            m_TimelineFlameGraphData = std::move(newData);
            ResetView();
            return;
        }

        // Merge new data into existing data
        for (auto& newThread : newData->threads)
        {
            auto* existingThread = m_TimelineFlameGraphData->FindThread(newThread->threadHash);

            if (existingThread)
            {
                for (auto& event : newThread->events)
                {
                    existingThread->events.push_back(std::move(event));
                }

                existingThread->frameMarkers.insert(
                    existingThread->frameMarkers.end(),
                    newThread->frameMarkers.begin(),
                    newThread->frameMarkers.end());

                if (newThread->threadStartTime < existingThread->threadStartTime) {
                    existingThread->threadStartTime = newThread->threadStartTime;
                }
                if (newThread->threadEndTime > existingThread->threadEndTime) {
                    existingThread->threadEndTime = newThread->threadEndTime;
                }
                existingThread->threadDuration = existingThread->threadEndTime - existingThread->threadStartTime;
                if (newThread->maxDepth > existingThread->maxDepth) {
                    existingThread->maxDepth = newThread->maxDepth;
                }

                existingThread->CalculateTimelineLayout();
            }
            else
            {
                m_TimelineFlameGraphData->threads.push_back(std::move(newThread));
            }
        }

        m_TimelineFlameGraphData->CalculateGlobalTimeline();

        m_HoveredNode = nullptr;
        m_SelectedNode = nullptr;
        m_HoveredThread = nullptr;
        m_SelectedThread = nullptr;

        size_t threadCount = m_TimelineFlameGraphData->GetThreadCount();
        if (m_ThreadVisibility.size() < threadCount) {
            m_ThreadVisibility.resize(threadCount, true);
        }
    }

    // -------------------------------------------------------------------------
    // Mouse wheel capture (called by the controller after ImGui::Begin)
    // -------------------------------------------------------------------------

    void ProfilerVisualizer::CaptureMouseWheel()
    {
        m_SavedMouseWheel = 0.0f;

        if (ImGui::GetIO().KeyCtrl && ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows))
        {
            if (ImGui::GetIO().MouseWheel != 0.0f)
            {
                m_SavedMouseWheel = ImGui::GetIO().MouseWheel;
                ImGui::GetIO().MouseWheel = 0.0f;
            }
        }
    }

    // -------------------------------------------------------------------------
    // Top-level window (self-contained, kept for standalone use)
    // -------------------------------------------------------------------------

    bool ProfilerVisualizer::RenderFlameGraphWindow(const char* windowTitle, bool* isOpen)
    {
        ApplyPendingData();

        m_SavedMouseWheel = 0.0f;

        ImGuiWindowFlags wflags = ImGuiWindowFlags_None;

        if (ImGui::Begin(windowTitle, isOpen, wflags))
        {
            CaptureMouseWheel();
            RenderFlameGraphContent();

            ImGui::End();
            return true;
        }
        else
        {
            ImGui::End();
            return false;
        }
    }

    // -------------------------------------------------------------------------
    // Content-only rendering (called by the controller inside its window)
    // -------------------------------------------------------------------------

    void ProfilerVisualizer::RenderFlameGraphContent()
    {
        if (!InitializeFlameGraphRendering()) {
            return;
        }

        RenderFlameGraph();
    }

    void ProfilerVisualizer::SelectOutlierFrames()
    {
        if (!m_TimelineFlameGraphData) {
            return;
        }

        auto globalFrameMarkers = ViewUtils::GatherSortedGlobalFrameMarkers(m_TimelineFlameGraphData.get());
        if (globalFrameMarkers.size() < 2) {
            return;
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
                : m_TimelineFlameGraphData->globalEndTime;

            uint64_t duration = frameEnd - frameStart;
            frameDurations.push_back({ globalFrameMarkers[i].frameNumber, duration });
        }

        if (frameDurations.empty()) {
            return;
        }

        double totalCycles = 0.0;
        for (const auto& fd : frameDurations) {
            totalCycles += static_cast<double>(fd.cycles);
        }
        double averageCycles = totalCycles / static_cast<double>(frameDurations.size());

        double threshold = averageCycles * (1.0 + m_OutlierThresholdPct / 100.0);

        m_SelectedFrames.clear();
        for (const auto& fd : frameDurations) {
            if (static_cast<double>(fd.cycles) > threshold) {
                m_SelectedFrames.insert(fd.frameNumber);
            }
        }
    }

    void ProfilerVisualizer::ResetView()
    {
        m_HoveredNode = nullptr;
        m_SelectedNode = nullptr;
        m_FlameGraphZoom = 1.0f;
        m_FlameGraphCenterTime = 0.0f;
        m_ViewInitialized = false;
        m_SelectedFrames.clear();
        m_ThreadVisibility.clear();
    }

    bool ProfilerVisualizer::InitializeFlameGraphRendering()
    {
        if (!m_TimelineFlameGraphData || m_TimelineFlameGraphData->GetThreadCount() == 0) {
            ImGui::Text("No timeline flame graph data available.");
            ImGui::Text("Generate timeline flame graph data using the Profiler menu.");
            return false;
        }
        return true;
    }

    // -------------------------------------------------------------------------
    // Main flame graph layout
    // -------------------------------------------------------------------------

    void ProfilerVisualizer::RenderFlameGraph()
    {
        InitializeFlameGraphView();
        ImGui::Separator();

        size_t threadCount = m_TimelineFlameGraphData->GetThreadCount();
        if (m_ThreadVisibility.size() != threadCount) {
            m_ThreadVisibility.resize(threadCount, true);
        }

        float timeTickHeight = m_ShowTimeTicks ? 30.0f : 0.0f;
        float nodeHeight = 18.0f;
        float threadPadding = 10.0f;

        std::vector<float> threadHeights(threadCount, 0.0f);
        float totalHeight = timeTickHeight + 50.0f;

        for (size_t i = 0; i < threadCount; ++i) {
            auto* thread = m_TimelineFlameGraphData->GetThread(i);
            if (!thread) continue;

            if (m_ThreadVisibility[i]) {
                int actualMaxDepth = CalculateActualMaxDepth(thread);
                threadHeights[i] = (actualMaxDepth + 1) * (nodeHeight + 1.0f) + threadPadding;
                totalHeight += threadHeights[i];
            }
            else {
                threadHeights[i] = 0.0f;
                totalHeight += 20.0f;
            }
        }

        ImVec2 availableRegion = ImGui::GetContentRegionAvail();

        ImGuiWindowFlags childFlags = ImGuiWindowFlags_None;
        if (ImGui::GetIO().KeyCtrl)
            childFlags |= ImGuiWindowFlags_NoScrollWithMouse;

        if (ImGui::BeginChild("FlameGraphScrollRegion", ImVec2(0, 0), false, childFlags))
        {
            ImVec2 canvasPos = ImGui::GetCursorScreenPos();
            ImVec2 canvasSize = ImVec2(availableRegion.x, totalHeight);
            ImDrawList* drawList = ImGui::GetWindowDrawList();

            RenderFrameBackdrops(canvasPos, canvasSize, drawList);

            m_HoveredNode = nullptr;

            if (m_ShowTimeTicks) {
                RenderGlobalTimeTicks(canvasPos, canvasSize, drawList, timeTickHeight);
            }

            float currentY = canvasPos.y + timeTickHeight;
            float finalY = currentY;

            for (size_t i = 0; i < threadCount; ++i) {
                auto* thread = m_TimelineFlameGraphData->GetThread(i);
                if (!thread) continue;

                ImVec2 headerPos(canvasPos.x, currentY);
                ImVec2 headerSize(canvasSize.x, 20.0f);
                RenderThreadHeader(i, thread, headerPos, headerSize, drawList);

                currentY += 20.0f;
                finalY = currentY;

                if (m_ThreadVisibility[i]) {
                    ImVec2 threadCanvasPos(canvasPos.x, currentY);
                    ImVec2 threadCanvasSize(canvasSize.x, threadHeights[i]);
                    RenderEvents(thread, threadCanvasPos, threadCanvasSize, drawList);

                    currentY += threadHeights[i];
                    finalY = currentY;
                }
            }

            m_actualRenderedHeight = finalY - canvasPos.y;

            ImGui::SetCursorScreenPos(canvasPos);
            ImVec2 actualCanvasSize = ImVec2(canvasSize.x, m_actualRenderedHeight);
            ImGui::InvisibleButton("FlameGraphCanvas", actualCanvasSize);
            ImGui::SetItemAllowOverlap();

            if (ImGui::IsItemHovered()) {
                HandleMouseWheelZoom(canvasPos, actualCanvasSize);
                HandleMouseDrag(actualCanvasSize);
                HandleFrameSelectionClick(canvasPos, actualCanvasSize);
            }
            HandleKeyboardZoom(canvasPos, actualCanvasSize);
        }
        ImGui::EndChild();

        RenderNodeInteractionUI(m_HoveredThread);
    }

    void ProfilerVisualizer::InitializeFlameGraphView()
    {
        if (!m_ViewInitialized || m_FlameGraphZoom == 1.0f) {
            m_FlameGraphCenterTime = m_TimelineFlameGraphData->globalDuration * 0.5f;
            m_FlameGraphZoom = 1.0f;
            m_ViewInitialized = true;
        }
    }

    // -------------------------------------------------------------------------
    // Frame backdrop rendering
    // -------------------------------------------------------------------------
    void ProfilerVisualizer::RenderFrameBackdrops(ImVec2 canvasPos, ImVec2 canvasSize, ImDrawList* drawList)
    {
        ImU32 colorLight = IM_COL32(50, 50, 50, 255);
        ImU32 colorDark = IM_COL32(35, 35, 35, 255);
        ImU32 colorSelLight = IM_COL32(40, 50, 80, 255);
        ImU32 colorSelDark = IM_COL32(28, 35, 65, 255);
        float backdropHeight = m_actualRenderedHeight;

        auto globalFrameMarkers = ViewUtils::GatherSortedGlobalFrameMarkers(m_TimelineFlameGraphData.get());

        if (globalFrameMarkers.empty()) {
            drawList->AddRectFilled(canvasPos,
                ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + backdropHeight), colorLight);
            return;
        }

        auto vtr = GetVisibleTimeRange();

        struct FrameBoundary {
            float screenX;
            size_t sortedIndex;
            uint64_t frameNumber;
        };
        std::vector<FrameBoundary> frameBoundaries;

        for (size_t i = 0; i < globalFrameMarkers.size(); i++) {
            float markerTime = static_cast<float>(
                globalFrameMarkers[i].clockCycles - m_TimelineFlameGraphData->globalStartTime);

            if (markerTime >= vtr.startTime && markerTime <= vtr.endTime) {
                float screenX = TimeToScreenX(markerTime, canvasPos.x, canvasSize.x, vtr);
                frameBoundaries.push_back({ screenX, i, globalFrameMarkers[i].frameNumber });
            }
        }

        size_t firstVisibleIdx = 0;
        uint64_t firstVisibleFrameNumber = 0;
        for (size_t i = 0; i < globalFrameMarkers.size(); ++i) {
            float markerTime = static_cast<float>(
                globalFrameMarkers[i].clockCycles - m_TimelineFlameGraphData->globalStartTime);
            if (markerTime <= vtr.startTime) {
                firstVisibleIdx = i;
                firstVisibleFrameNumber = globalFrameMarkers[i].frameNumber;
            }
            else {
                break;
            }
        }

        float canvasLeft = canvasPos.x;
        float canvasRight = canvasPos.x + canvasSize.x;
        float canvasTop = canvasPos.y;
        float canvasBot = canvasPos.y + backdropHeight;

        auto getBackdropColor = [&](size_t sortedIdx, uint64_t frameNumber) -> ImU32 {
            bool isSelected = m_SelectedFrames.count(frameNumber) > 0;
            bool isEven = (sortedIdx % 2 == 0);
            if (isSelected) {
                return isEven ? colorSelLight : colorSelDark;
            }
            return isEven ? colorLight : colorDark;
        };

        if (frameBoundaries.empty()) {
            ImU32 col = getBackdropColor(firstVisibleIdx, firstVisibleFrameNumber);
            drawList->AddRectFilled(ImVec2(canvasLeft, canvasTop),
                ImVec2(canvasRight, canvasBot), col);
        }
        else {
            if (frameBoundaries[0].screenX > canvasLeft) {
                size_t preIdx = (frameBoundaries[0].sortedIndex > 0) ? frameBoundaries[0].sortedIndex - 1 : 0;
                uint64_t preFrameNumber = globalFrameMarkers[preIdx].frameNumber;
                bool isEven = (frameBoundaries[0].sortedIndex % 2 == 0);
                bool isSelected = m_SelectedFrames.count(preFrameNumber) > 0;
                ImU32 col = isSelected
                    ? (isEven ? colorSelDark : colorSelLight)
                    : (isEven ? colorDark : colorLight);
                drawList->AddRectFilled(ImVec2(canvasLeft, canvasTop),
                    ImVec2(frameBoundaries[0].screenX, canvasBot), col);
            }

            for (size_t j = 0; j < frameBoundaries.size(); ++j) {
                float left = frameBoundaries[j].screenX;
                float right = (j + 1 < frameBoundaries.size())
                    ? frameBoundaries[j + 1].screenX
                    : canvasRight;

                ImU32 col = getBackdropColor(frameBoundaries[j].sortedIndex, frameBoundaries[j].frameNumber);
                drawList->AddRectFilled(ImVec2(left, canvasTop),
                    ImVec2(right, canvasBot), col);
            }
        }
    }

    // -------------------------------------------------------------------------
    // Thread header rendering
    // -------------------------------------------------------------------------

    void ProfilerVisualizer::RenderThreadHeader(size_t threadIndex, TimelineThreadData* thread,
        ImVec2 headerPos, ImVec2 headerSize, ImDrawList* drawList)
    {
        ImGui::PushID(static_cast<int>(threadIndex));

        ImGui::SetCursorScreenPos(headerPos);
        bool headerClicked = ImGui::InvisibleButton("ThreadHeader", headerSize);
        bool isHeaderHovered = ImGui::IsItemHovered();

        ImVec2 headerMin = headerPos;
        ImVec2 headerMax = ImVec2(headerPos.x + headerSize.x, headerPos.y + headerSize.y);

        ImU32 backgroundColor = isHeaderHovered ? IM_COL32(80, 80, 80, 255) : IM_COL32(60, 60, 60, 255);
        ImU32 borderColor = IM_COL32(120, 120, 120, 255);
        ImU32 textColor = IM_COL32(255, 255, 255, 255);

        drawList->AddRectFilled(headerMin, headerMax, backgroundColor);
        drawList->AddRect(headerMin, headerMax, borderColor, 0.0f, 0, 1.0f);

        std::string indicator = m_ThreadVisibility[threadIndex] ? "^ " : "> ";
        int actualMaxDepth = CalculateActualMaxDepth(thread);
        std::string threadText = indicator + thread->threadName + " (" +
            std::to_string(thread->events.size()) + " events, " +
            std::to_string(actualMaxDepth + 1) + " rows)";

        ImVec2 textSize = ImGui::CalcTextSize(threadText.c_str());
        ImVec2 textPos(headerMin.x + 5.0f, headerMin.y + (headerSize.y - textSize.y) * 0.5f);
        drawList->AddText(textPos, textColor, threadText.c_str());

        if (headerClicked) {
            m_ThreadVisibility[threadIndex] = !m_ThreadVisibility[threadIndex];
        }

        ImGui::PopID();
    }

    // -------------------------------------------------------------------------
    // Mouse and keyboard interaction
    // -------------------------------------------------------------------------

    void ProfilerVisualizer::HandleMouseInteraction(ImVec2 canvasPos, ImVec2 canvasSize)
    {
        ImGui::InvisibleButton("FlameGraphCanvas", canvasSize);
        ImGui::SetItemAllowOverlap();
        if (ImGui::IsItemHovered()) {
            HandleMouseWheelZoom(canvasPos, canvasSize);
            HandleMouseDrag(canvasSize);
        }
        HandleKeyboardZoom(canvasPos, canvasSize);
    }

    void ProfilerVisualizer::HandleKeyboardZoom(ImVec2 canvasPos, ImVec2 canvasSize)
    {
        if (!ImGui::IsWindowFocused()) {
            return;
        }

        bool zoomIn = ImGui::IsKeyPressed('=');
        bool zoomOut = !zoomIn && ImGui::IsKeyPressed('-');

        if (zoomIn || zoomOut) {
            auto vtr = GetVisibleTimeRange();
            float timeUnderCenter = vtr.startTime + (0.5f * vtr.duration);

            float zoomMultiplier = zoomIn ? 1.1f : 0.9f;
            m_FlameGraphZoom *= zoomMultiplier;
            m_FlameGraphZoom = std::clamp(m_FlameGraphZoom, 0.01f, 10000.0f);

            m_FlameGraphCenterTime = timeUnderCenter;
        }
    }

    void ProfilerVisualizer::HandleMouseWheelZoom(ImVec2 canvasPos, ImVec2 canvasSize)
    {
        float wheel = m_SavedMouseWheel;

        if (wheel != 0.0f) {
            ImVec2 mousePos = ImGui::GetIO().MousePos;
            float mouseRelativeX = (mousePos.x - canvasPos.x) / canvasSize.x;

            auto vtr = GetVisibleTimeRange();
            float timeUnderMouse = vtr.startTime + (mouseRelativeX * vtr.duration);

            float zoomMultiplier = wheel > 0.0f ? 1.1f : 0.9f;
            m_FlameGraphZoom *= zoomMultiplier;
            m_FlameGraphZoom = std::clamp(m_FlameGraphZoom, 0.01f, 10000.0f);

            float newVisibleTimeRange = m_TimelineFlameGraphData->globalDuration / m_FlameGraphZoom;
            m_FlameGraphCenterTime = timeUnderMouse - (mouseRelativeX - 0.5f) * newVisibleTimeRange;

            m_SavedMouseWheel = 0.0f;
        }
    }

    void ProfilerVisualizer::HandleMouseDrag(ImVec2 canvasSize)
    {
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            ImVec2 delta = ImGui::GetIO().MouseDelta;
            float visibleTimeRange = m_TimelineFlameGraphData->globalDuration / m_FlameGraphZoom;
            float timeDelta = -(delta.x / canvasSize.x) * visibleTimeRange;
            m_FlameGraphCenterTime += timeDelta;
        }
    }

    void ProfilerVisualizer::HandleFrameSelectionClick(ImVec2 canvasPos, ImVec2 canvasSize)
    {
        if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            return;
        }

        if (m_HoveredNode) {
            return;
        }

        auto globalFrameMarkers = ViewUtils::GatherSortedGlobalFrameMarkers(m_TimelineFlameGraphData.get());
        if (globalFrameMarkers.empty()) {
            return;
        }

        auto vtr = GetVisibleTimeRange();
        ImVec2 mousePos = ImGui::GetIO().MousePos;

        float mouseRelativeX = (mousePos.x - canvasPos.x) / canvasSize.x;
        float mouseTime = vtr.startTime + (mouseRelativeX * vtr.duration);

        uint64_t clickedFrameNumber = 0;
        bool foundFrame = false;

        for (size_t i = 0; i < globalFrameMarkers.size(); ++i) {
            float markerTime = static_cast<float>(
                globalFrameMarkers[i].clockCycles - m_TimelineFlameGraphData->globalStartTime);

            if (markerTime <= mouseTime) {
                clickedFrameNumber = globalFrameMarkers[i].frameNumber;
                foundFrame = true;
            }
            else {
                break;
            }
        }

        if (!foundFrame) {
            return;
        }

        if (ImGui::GetIO().KeyCtrl) {
            ToggleFrameSelection(clickedFrameNumber);
        }
        else {
            m_SelectedFrames.clear();
            m_SelectedFrames.insert(clickedFrameNumber);
        }
    }

    // -------------------------------------------------------------------------
    // Event / node rendering
    // -------------------------------------------------------------------------

    void ProfilerVisualizer::RenderEvents(TimelineThreadData* thread, ImVec2 canvasPos, ImVec2 canvasSize, ImDrawList* drawList)
    {
        if (!thread || thread->events.empty()) {
            return;
        }

        float nodeHeight = 18.0f;
        std::map<int, std::vector<TimelineFlameNode*>> eventsByDepth;

        for (auto& event : thread->events) {
            if (event->duration > 0 && event->duration >= m_MinDisplayTime) {
                eventsByDepth[event->depth].push_back(event.get());
            }
        }

        for (const auto& [depth, events] : eventsByDepth) {
            float currentY = canvasPos.y + 5.0f + (depth * (nodeHeight + 1.0f));

            if (currentY + nodeHeight > canvasPos.y + canvasSize.y) {
                break;
            }

            for (auto* event : events) {
                RenderFlameGraphNode(event, thread, canvasSize.x, nodeHeight,
                    m_TimelineFlameGraphData->globalDuration, canvasPos, drawList, currentY);
            }
        }
    }

    void ProfilerVisualizer::RenderFlameGraphNode(TimelineFlameNode* node, TimelineThreadData* thread,
        float containerWidth, float nodeHeight, uint64_t globalTotalTime, ImVec2 canvasPos, ImDrawList* drawList, float startY)
    {
        if (!node || !node->sourceEvent || node->duration == 0 || globalTotalTime == 0) {
            return;
        }

        auto vtr = GetVisibleTimeRange();

        float nodeStartTime = static_cast<float>(node->startTime - m_TimelineFlameGraphData->globalStartTime);
        float nodeEndTime = static_cast<float>(node->endTime - m_TimelineFlameGraphData->globalStartTime);

        if (nodeEndTime < vtr.startTime || nodeStartTime > vtr.endTime) {
            return;
        }

        float visibleNodeStartTime = std::max(nodeStartTime, vtr.startTime);
        float visibleNodeEndTime = std::min(nodeEndTime, vtr.endTime);

        float nodeX = TimeToScreenX(visibleNodeStartTime, canvasPos.x, containerWidth, vtr);
        float nodeEndX = TimeToScreenX(visibleNodeEndTime, canvasPos.x, containerWidth, vtr);
        float nodeWidth = std::max(nodeEndX - nodeX, 1.0f);

        ImVec2 nodeMin(nodeX, startY);
        ImVec2 nodeMax(nodeX + nodeWidth, startY + nodeHeight);

        ImVec2 mousePos = ImGui::GetIO().MousePos;
        bool isHovered = IsPointInRect(mousePos, nodeMin, nodeMax);

        if (isHovered) {
            m_HoveredNode = node;
            m_HoveredThread = thread;

            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                m_SelectedNode = node;
                m_SelectedThread = thread;
            }

            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                ZoomToNode(node);
            }
        }

        float intensity = isHovered ? 1.2f : 1.0f;
        if (node == m_SelectedNode) intensity = 1.5f;
        ImU32 nodeColor = ViewUtils::GetNodeColor(node->name, intensity);

        drawList->AddRectFilled(nodeMin, nodeMax, nodeColor);

        if (nodeWidth > 30.0f) {
            std::string displayText = node->name;

            if (nodeWidth > 60.0f) {
                displayText += " (" + ViewUtils::FormatCycles(node->duration, m_CPUFrequency) + ")";
            }

            if (m_ShowCallCounts && nodeWidth > 80.0f) {
                displayText += " (#" + std::to_string(node->eventIndex) + ")";
            }

            ImVec2 textSize = ImGui::CalcTextSize(displayText.c_str());
            if (textSize.x > nodeWidth - 4.0f) {
                while (displayText.length() > 3 && ImGui::CalcTextSize(displayText.c_str()).x > nodeWidth - 4.0f) {
                    displayText.pop_back();
                }
                if (displayText.length() > 3) {
                    displayText = displayText.substr(0, displayText.length() - 3) + "...";
                }
            }

            ImVec2 textPos(nodeMin.x + 2.0f, nodeMin.y + (nodeHeight - textSize.y) * 0.5f);
            drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), displayText.c_str());
        }
    }

    void ProfilerVisualizer::ZoomToNode(TimelineFlameNode* node)
    {
        if (!node || !m_TimelineFlameGraphData) {
            return;
        }

        float nodeStartTime = static_cast<float>(node->startTime - m_TimelineFlameGraphData->globalStartTime);
        float nodeEndTime = static_cast<float>(node->endTime - m_TimelineFlameGraphData->globalStartTime);
        float nodeDuration = nodeEndTime - nodeStartTime;

        m_FlameGraphCenterTime = nodeStartTime + (nodeDuration * 0.5f);

        float targetVisibleRange = nodeDuration / 0.7f;
        float minVisibleRange = m_TimelineFlameGraphData->globalDuration * 0.001f;
        targetVisibleRange = std::max(targetVisibleRange, minVisibleRange);

        m_FlameGraphZoom = m_TimelineFlameGraphData->globalDuration / targetVisibleRange;
        m_FlameGraphZoom = std::clamp(m_FlameGraphZoom, 0.01f, 10000.0f);
    }

    // -------------------------------------------------------------------------
    // Tooltip / interaction UI
    // -------------------------------------------------------------------------

    void ProfilerVisualizer::RenderNodeInteractionUI(TimelineThreadData* selectedThread)
    {
        if (m_HoveredNode) {
            TimelineThreadData* threadForCalculation = selectedThread ? selectedThread : m_HoveredThread;

            if (threadForCalculation) {
                ImGui::BeginTooltip();
                ImGui::Text("Function: %s", m_HoveredNode->name.c_str());

                if (!selectedThread && m_HoveredThread) {
                    ImGui::Text("Thread: %s", m_HoveredThread->threadName.c_str());
                }

                ImGui::Text("Duration: %s (%.2f%%)",
                    ViewUtils::FormatCycles(m_HoveredNode->duration, m_CPUFrequency).c_str(),
                    m_HoveredNode->GetPercentage(threadForCalculation->threadDuration));
                ImGui::Text("Start Time: %s", ViewUtils::FormatCycles(m_HoveredNode->startTime - threadForCalculation->threadStartTime, m_CPUFrequency).c_str());
                ImGui::Text("End Time: %s", ViewUtils::FormatCycles(m_HoveredNode->endTime - threadForCalculation->threadStartTime, m_CPUFrequency).c_str());
                ImGui::Text("Depth: %d", m_HoveredNode->depth);
                if (m_ShowCallCounts) {
                    ImGui::Text("Event Index: %u", m_HoveredNode->eventIndex);
                }
                ImGui::EndTooltip();
            }
        }
    }

    // -------------------------------------------------------------------------
    // Time ticks and frame markers
    // -------------------------------------------------------------------------

    void ProfilerVisualizer::RenderGlobalTimeTicks(ImVec2 canvasPos, ImVec2 canvasSize, ImDrawList* drawList, float tickHeight)
    {
        auto globalFrameMarkers = ViewUtils::GatherSortedGlobalFrameMarkers(m_TimelineFlameGraphData.get());

        if (globalFrameMarkers.empty()) {
            ImVec2 textPos(canvasPos.x + 10.0f, canvasPos.y + tickHeight * 0.3f);
            drawList->AddText(textPos, IM_COL32(150, 150, 150, 255), "No frame markers available");

            drawList->AddLine(
                ImVec2(canvasPos.x, canvasPos.y + tickHeight),
                ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + tickHeight),
                IM_COL32(200, 200, 200, 100), 1.0f);
            return;
        }

        auto vtr = GetVisibleTimeRange();

        // Minimum horizontal gap (in pixels) between consecutive tick labels
        constexpr float kMinLabelSpacing = 8.0f;
        // Tracks the right-most pixel used by the last drawn label
        float nextAllowedX = canvasPos.x;

        for (size_t i = 0; i < globalFrameMarkers.size(); ++i) {
            const auto& currentMarker = globalFrameMarkers[i];

            float markerTime = static_cast<float>(currentMarker.clockCycles - m_TimelineFlameGraphData->globalStartTime);

            if (markerTime < vtr.startTime || markerTime > vtr.endTime) {
                continue;
            }

            float screenX = TimeToScreenX(markerTime, canvasPos.x, canvasSize.x, vtr);

            if (screenX < canvasPos.x - 30 || screenX > canvasPos.x + canvasSize.x + 30) {
                continue;
            }

            // Skip this label if it would overlap the previous one
            if (screenX < nextAllowedX) {
                continue;
            }

            // Calculate time since last frame marker using real CPU frequency
            double timeSinceLastFrameMs = 0.0;
            if (i > 0) {
                uint64_t cycleDiff = currentMarker.clockCycles - globalFrameMarkers[i - 1].clockCycles;
                timeSinceLastFrameMs = (static_cast<double>(cycleDiff) / m_CPUFrequency) * 1000.0;
            }
            else {
                uint64_t cycleDiff = currentMarker.clockCycles - m_TimelineFlameGraphData->globalStartTime;
                timeSinceLastFrameMs = (static_cast<double>(cycleDiff) / m_CPUFrequency) * 1000.0;
            }

            // Calculate the duration of THIS frame (from this marker to the next)
            double frameDurationMs = 0.0;
            if (i + 1 < globalFrameMarkers.size()) {
                uint64_t cycleDiff = globalFrameMarkers[i + 1].clockCycles - currentMarker.clockCycles;
                frameDurationMs = (static_cast<double>(cycleDiff) / m_CPUFrequency) * 1000.0;
            }
            else {
                // Last frame: measure from this marker to the end of the capture
                uint64_t cycleDiff = m_TimelineFlameGraphData->globalEndTime - currentMarker.clockCycles;
                frameDurationMs = (static_cast<double>(cycleDiff) / m_CPUFrequency) * 1000.0;
            }

            std::string timeLabel = FormatMillisecondsWithUnit(frameDurationMs);
            std::string fullLabel = "F" + std::to_string(currentMarker.frameNumber) + "\n" + timeLabel;

            ImVec2 textSize = ImGui::CalcTextSize(fullLabel.c_str());
            ImVec2 textPos(screenX, canvasPos.y + 2.0f);
            textPos.x = std::clamp(textPos.x, canvasPos.x, canvasPos.x + canvasSize.x - textSize.x);

            drawList->AddText(textPos, IM_COL32(100, 200, 255, 200), fullLabel.c_str());

            // Advance the "occupied" watermark past this label
            nextAllowedX = textPos.x + textSize.x + kMinLabelSpacing;
        }

        drawList->AddLine(
            ImVec2(canvasPos.x, canvasPos.y + tickHeight),
            ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + tickHeight),
            IM_COL32(200, 200, 200, 100), 1.0f);
    }

    void ProfilerVisualizer::RenderGlobalFrameMarkers(ImVec2 canvasPos, ImVec2 canvasSize, ImDrawList* drawList, float startY)
    {
        auto globalFrameMarkers = ViewUtils::GatherSortedGlobalFrameMarkers(m_TimelineFlameGraphData.get());
        if (globalFrameMarkers.empty()) {
            return;
        }

        auto vtr = GetVisibleTimeRange();
        float totalHeight = canvasSize.y - (startY - canvasPos.y);

        for (const auto& currentMarker : globalFrameMarkers) {
            float markerTime = static_cast<float>(currentMarker.clockCycles - m_TimelineFlameGraphData->globalStartTime);

            if (markerTime < vtr.startTime || markerTime > vtr.endTime) {
                continue;
            }

            float screenX = TimeToScreenX(markerTime, canvasPos.x, canvasSize.x, vtr);
            drawList->AddLine(ImVec2(screenX, startY), ImVec2(screenX, startY + totalHeight),
                IM_COL32(255, 100, 100, 200), 2.0f);
        }
    }

    // -------------------------------------------------------------------------
    // Formatting helpers
    // -------------------------------------------------------------------------

    std::string ProfilerVisualizer::FormatMillisecondsWithUnit(double milliseconds)
    {
        std::ostringstream oss;
        oss << std::fixed;

        if (milliseconds >= 1000.0) {
            oss << std::setprecision(2) << (milliseconds / 1000.0) << "s";
        }
        else if (milliseconds >= 1.0) {
            oss << std::setprecision(1) << milliseconds << "ms";
        }
        else {
            oss << std::setprecision(0) << (milliseconds * 1000.0) << "us";
        }

        return oss.str();
    }

} // namespace Profiler