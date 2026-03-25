#include "ProfilerVisualizer.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <functional>
#include <chrono>
#include <thread>

#if defined(_WIN32)
#include <windows.h>
#undef min
#undef max
#endif

namespace Profiler {

// Helper function to get real CPU frequency for accurate cycle-to-time conversion
static double GetRealCPUFrequency() {
    static double cachedFrequency = 0.0;
    static bool isInitialized = false;
    
    // Return cached value if already calculated
    if (isInitialized) {
        return cachedFrequency;
    }
    
#if defined(_WIN32)
    LARGE_INTEGER frequency;
    if (QueryPerformanceFrequency(&frequency)) {
        // Estimate CPU frequency using RDTSC and high resolution timer        
        
        auto startTime = std::chrono::high_resolution_clock::now();
        uint64_t startCycles = 0;
        uint64_t endCycles = 0;

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
        // Use RDTSC to get actual CPU cycle counts
        startCycles = __rdtsc();
        
        // Sleep for a meaningful duration to get accurate measurement
        std::this_thread::sleep_for(std::chrono::milliseconds(50));        
        endCycles = __rdtsc();
#endif

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime);

        if (endCycles > startCycles && duration.count() > 0) {
            double cyclesPerNanosecond = static_cast<double>(endCycles - startCycles) / duration.count();
            cachedFrequency = cyclesPerNanosecond * 1e9; // Convert to cycles per second
            
            // Sanity check: typical CPU frequencies are between 0.5 GHz and 6 GHz
            if (cachedFrequency >= 0.5e9 && cachedFrequency <= 6.0e9) {
                isInitialized = true;
                return cachedFrequency;
            }
        }
    }
#endif
    // Fallback: Use a reasonable default for modern CPUs
    cachedFrequency = 3.0e9; // 3 GHz estimate
    isInitialized = true;
    return cachedFrequency;
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
{
    // Initialize CPU frequency on construction for better performance
    m_CPUFrequency = GetRealCPUFrequency();
}

void ProfilerVisualizer::SetFlameGraphData(std::unique_ptr<TimelineFlameGraphData> data)
{
    m_TimelineFlameGraphData = std::move(data);
    ResetView();
}

bool ProfilerVisualizer::RenderFlameGraphWindow(const char* windowTitle, bool* isOpen)
{
    ImGuiWindowFlags wflags = ImGuiWindowFlags_MenuBar;
    if (ImGui::GetIO().KeyCtrl)
        wflags |= ImGuiWindowFlags_NoScrollWithMouse;

    if (ImGui::Begin(windowTitle, isOpen, wflags))
    {
        if (!InitializeFlameGraphRendering()) {
            ImGui::End();
            return false;
        }

        RenderFlameGraphMenuBar();
        RenderFlameGraph();

        ImGui::End();
        return true;
    }
    else
    {
        ImGui::End();
        return false;
    }
}

void ProfilerVisualizer::ResetView()
{
    m_HoveredNode = nullptr;
    m_SelectedNode = nullptr;
    m_FlameGraphZoom = 1.0f;
    m_FlameGraphCenterTime = 0.0f;
    m_ViewInitialized = false;
    
    // Reset thread visibility - will be reinitialized in RenderFlameGraph
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

void ProfilerVisualizer::RenderFlameGraphMenuBar()
{
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("View")) {
            ImGui::Checkbox("Show Call Counts", &m_ShowCallCounts);
            ImGui::Checkbox("Show Frame Markers", &m_ShowFrameMarkers);
            ImGui::Checkbox("Show Time Ticks", &m_ShowTimeTicks);
            ImGui::SliderFloat("Min Display Time %", &m_MinDisplayTime, 0.000f, 5.0f, "%.3f");
            
            // Display detected CPU frequency for user information
            ImGui::Separator();
            ImGui::Text("CPU Frequency: %.2f GHz (detected)", m_CPUFrequency / 1e9);
            
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
}

void ProfilerVisualizer::RenderFlameGraph()
{
    // Initialize view with global timeline
    InitializeFlameGraphView();
    RenderNavigationControls();

    ImGui::Separator();

    // Check if any threads have events
    bool hasEvents = false;
    for (size_t i = 0; i < m_TimelineFlameGraphData->GetThreadCount(); ++i) {
        auto* thread = m_TimelineFlameGraphData->GetThread(i);
        if (thread && !thread->events.empty()) {
            hasEvents = true;
            break;
        }
    }

    // Initialize thread visibility tracking if needed
    size_t threadCount = m_TimelineFlameGraphData->GetThreadCount();
    if (m_ThreadVisibility.size() != threadCount) {
        m_ThreadVisibility.resize(threadCount, true); // Default to visible
    }

    // Canvas setup
    float timeTickHeight = m_ShowTimeTicks ? 30.0f : 0.0f;
    float threadSpacing = 0.0f; // No spacing between threads
    float nodeHeight = 18.0f;   // Height per row/depth level
    float threadPadding = 10.0f; // Padding at top and bottom of each thread

    // Calculate dynamic thread heights and total height needed
    std::vector<float> threadHeights(threadCount, 0.0f);
    float totalHeight = timeTickHeight + 50.0f;

    for (size_t i = 0; i < threadCount; ++i) {
        auto* thread = m_TimelineFlameGraphData->GetThread(i);
        if (!thread) continue;

        if (m_ThreadVisibility[i]) {
            // Calculate actual rows needed for this thread
            int actualMaxDepth = CalculateActualMaxDepth(thread);
            threadHeights[i] = (actualMaxDepth + 1) * (nodeHeight + 1.0f) + threadPadding;
            totalHeight += threadHeights[i] + threadSpacing;
        }
        else {
            threadHeights[i] = 0.0f; // Collapsed threads don't need height for events
            totalHeight += 20.0f + threadSpacing; // Just header height for collapsed threads
        }
    }

    // Get available region for the scrollable area
    ImVec2 availableRegion = ImGui::GetContentRegionAvail();
    
    // Create a scrollable child window with the calculated total height
    // Use ImGuiWindowFlags_NoScrollWithMouse when Ctrl is held to allow zoom instead of scroll
    ImGuiWindowFlags childFlags = ImGuiWindowFlags_None;
    if (ImGui::GetIO().KeyCtrl)
        childFlags |= ImGuiWindowFlags_NoScrollWithMouse;
        
    if (ImGui::BeginChild("FlameGraphScrollRegion", ImVec2(0, 0), false, childFlags))
    {
        ImVec2 canvasPos = ImGui::GetCursorScreenPos();
        ImVec2 canvasSize = ImVec2(availableRegion.x, totalHeight);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        
		// Draw the grey backdrop rectangle to cover the actual rendered area
		drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + m_actualRenderedHeight),
			IM_COL32(50, 50, 50, 255));

        m_HoveredNode = nullptr;

        // Render time ticks at the top (using global timeline)
        if (m_ShowTimeTicks) {
            RenderGlobalTimeTicks(canvasPos, canvasSize, drawList, timeTickHeight);
        }

        float currentY = canvasPos.y + timeTickHeight;
        float finalY = currentY; // Track the actual final Y position
        
        // Render each thread with dynamic height
        for (size_t i = 0; i < threadCount; ++i) {
            auto* thread = m_TimelineFlameGraphData->GetThread(i);
            if (!thread) continue;

            // Render thread header with custom drawing
            ImVec2 headerPos(canvasPos.x, currentY);
            ImVec2 headerSize(canvasSize.x, 20.0f);

            // Create unique button ID for each thread
            ImGui::PushID(static_cast<int>(i));

            // Position cursor for invisible button
            ImGui::SetCursorScreenPos(headerPos);

            // Create invisible button for interaction
            bool headerClicked = ImGui::InvisibleButton("ThreadHeader", headerSize);

            // Check if header is being hovered for visual feedback
            bool isHeaderHovered = ImGui::IsItemHovered();

            // Custom drawing of the header
            ImVec2 headerMin = headerPos;
            ImVec2 headerMax = ImVec2(headerPos.x + headerSize.x, headerPos.y + headerSize.y);

            // Choose colors based on hover state
            ImU32 backgroundColor = isHeaderHovered ? IM_COL32(80, 80, 80, 255) : IM_COL32(60, 60, 60, 255);
            ImU32 borderColor = IM_COL32(120, 120, 120, 255);
            ImU32 textColor = IM_COL32(255, 255, 255, 255);

            // Draw background rectangle
            drawList->AddRectFilled(headerMin, headerMax, backgroundColor);

            // Draw border
            drawList->AddRect(headerMin, headerMax, borderColor, 0.0f, 0, 1.0f);

            // Prepare text content
            std::string indicator = m_ThreadVisibility[i] ? "^ " : "> ";
            int actualMaxDepth = CalculateActualMaxDepth(thread);
            std::string threadText = indicator + thread->threadName + " (" + std::to_string(thread->events.size()) + " events, " + std::to_string(actualMaxDepth + 1) + " rows)";

            // Draw text
            ImVec2 textSize = ImGui::CalcTextSize(threadText.c_str());
            ImVec2 textPos(headerMin.x + 5.0f, headerMin.y + (headerSize.y - textSize.y) * 0.5f);
            drawList->AddText(textPos, textColor, threadText.c_str());

            // Handle click
            if (headerClicked) {
                m_ThreadVisibility[i] = !m_ThreadVisibility[i];
            }

            ImGui::PopID();

            currentY += 20.0f;
            finalY = currentY;
            
            // Only render thread events if the thread is visible
            if (m_ThreadVisibility[i]) {
                // Render thread events with dynamic height
                ImVec2 threadCanvasPos(canvasPos.x, currentY);
                ImVec2 threadCanvasSize(canvasSize.x, threadHeights[i]);

                RenderEvents(thread, threadCanvasPos, threadCanvasSize, drawList);

                currentY += threadHeights[i]; // Use dynamic height
                finalY = currentY;
            }
        }

        // Calculate the actual rendered height
        m_actualRenderedHeight = finalY - canvasPos.y;
        
        // Set up an invisible button for the entire rendered canvas to handle mouse interaction
        ImGui::SetCursorScreenPos(canvasPos);
        ImVec2 actualCanvasSize = ImVec2(canvasSize.x, m_actualRenderedHeight);
        ImGui::InvisibleButton("FlameGraphCanvas", actualCanvasSize);
        ImGui::SetItemAllowOverlap();
        
        // Handle mouse interaction using global timeline
        if (ImGui::IsItemHovered()) {
            HandleMouseWheelZoom(canvasPos, actualCanvasSize);
            HandleMouseDrag(actualCanvasSize);
        }
        HandleKeyboardZoom(canvasPos, actualCanvasSize);
    }
    ImGui::EndChild();

    RenderNodeInteractionUI(m_HoveredThread);
}

void ProfilerVisualizer::InitializeFlameGraphView()
{
    if (!m_ViewInitialized || m_FlameGraphZoom == 1.0f) {
        // Use global timeline duration
        m_FlameGraphCenterTime = m_TimelineFlameGraphData->globalDuration * 0.5f;
        m_FlameGraphZoom = 1.0f;
        m_ViewInitialized = true;
    }
}

void ProfilerVisualizer::RenderNavigationControls()
{
    if (ImGui::Button("Reset View")) {
        m_FlameGraphCenterTime = m_TimelineFlameGraphData->globalDuration * 0.5f;
        m_FlameGraphZoom = 1.0f;
    }
    ImGui::SameLine();
    ImGui::Text("Zoom: %.2fx (Ctrl + mouse wheel)", m_FlameGraphZoom);
}

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
    // Only handle keyboard input when the flame graph window is focused
    if (!ImGui::IsWindowFocused()) {
        return;
    }

    const ImGuiIO& io = ImGui::GetIO();
    bool zoomIn = false;
    bool zoomOut = false;

    // Check for +
    if (ImGui::IsKeyPressed('=')) { // = key (usually with Shift for +)
        zoomIn = true;
    }
    // Check for -
    else if (ImGui::IsKeyPressed('-')) { // Regular -
        zoomOut = true;
    }

    if (zoomIn || zoomOut) {
        // Use center of the view for keyboard zoom (unlike mouse zoom which uses mouse position)
        float mouseRelativeX = 0.5f; // Center of the view

        float visibleTimeRange = m_TimelineFlameGraphData->globalDuration / m_FlameGraphZoom;
        float visibleStartTime = m_FlameGraphCenterTime - (visibleTimeRange * 0.5f);
        float timeUnderCenter = visibleStartTime + (mouseRelativeX * visibleTimeRange);

        float zoomMultiplier = zoomIn ? 1.1f : 0.9f;
        m_FlameGraphZoom *= zoomMultiplier;
        m_FlameGraphZoom = std::clamp(m_FlameGraphZoom, 0.01f, 10000.0f);

        float newVisibleTimeRange = m_TimelineFlameGraphData->globalDuration / m_FlameGraphZoom;
        // Keep the center point stable when zooming with keyboard
        m_FlameGraphCenterTime = timeUnderCenter;
    }
}

void ProfilerVisualizer::HandleMouseWheelZoom(ImVec2 canvasPos, ImVec2 canvasSize)
{
    float wheel = ImGui::GetIO().MouseWheel;
    bool ctrlPressed = ImGui::GetIO().KeyCtrl;

    if (wheel != 0.0f && ctrlPressed) {
        // Consume the mouse wheel input to prevent ImGui from using it for scrolling
        ImGui::GetIO().MouseWheel = 0.0f;

        ImVec2 mousePos = ImGui::GetIO().MousePos;
        float mouseRelativeX = (mousePos.x - canvasPos.x) / canvasSize.x;

        float visibleTimeRange = m_TimelineFlameGraphData->globalDuration / m_FlameGraphZoom;
        float visibleStartTime = m_FlameGraphCenterTime - (visibleTimeRange * 0.5f);
        float timeUnderMouse = visibleStartTime + (mouseRelativeX * visibleTimeRange);

        float zoomMultiplier = wheel > 0.0f ? 1.1f : 0.9f;
        m_FlameGraphZoom *= zoomMultiplier;
        m_FlameGraphZoom = std::clamp(m_FlameGraphZoom, 0.01f, 10000.0f);

        float newVisibleTimeRange = m_TimelineFlameGraphData->globalDuration / m_FlameGraphZoom;
        m_FlameGraphCenterTime = timeUnderMouse - (mouseRelativeX - 0.5f) * newVisibleTimeRange;
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

void ProfilerVisualizer::RenderEvents(TimelineThreadData* thread, ImVec2 canvasPos, ImVec2 canvasSize, ImDrawList* drawList)
{
    if (!thread || thread->events.empty()) {
        return;
    }

    float nodeHeight = 18.0f;
    std::map<int, std::vector<TimelineFlameNode*>> eventsByDepth;

    // Group events by depth
    for (auto& event : thread->events) {
        if (event->duration > 0 && event->duration >= m_MinDisplayTime) {
            eventsByDepth[event->depth].push_back(event.get());
        }
    }

    // Render events by depth level
    for (const auto& [depth, events] : eventsByDepth) {
        float currentY = canvasPos.y + 5.0f + (depth * (nodeHeight + 1.0f));

        // Make sure we don't render outside the thread's allocated space
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

    // Calculate the visible time range using global timeline
    float visibleTimeRange = globalTotalTime / m_FlameGraphZoom;
    float visibleStartTime = m_FlameGraphCenterTime - (visibleTimeRange * 0.5f);
    float visibleEndTime = visibleStartTime + visibleTimeRange;

    // Calculate node's time position relative to global start time
    float nodeStartTime = static_cast<float>(node->startTime - m_TimelineFlameGraphData->globalStartTime);
    float nodeEndTime = static_cast<float>(node->endTime - m_TimelineFlameGraphData->globalStartTime);

    // Check if node overlaps with visible time range
    if (nodeEndTime < visibleStartTime || nodeStartTime > visibleEndTime) {
        return; // Node is outside visible range
    }

    // Calculate visible portion of the node
    float visibleNodeStartTime = std::max(nodeStartTime, visibleStartTime);
    float visibleNodeEndTime = std::min(nodeEndTime, visibleEndTime);

    // Convert to screen coordinates
    float nodeScreenStartX = ((visibleNodeStartTime - visibleStartTime) / visibleTimeRange) * containerWidth;
    float nodeScreenEndX = ((visibleNodeEndTime - visibleStartTime) / visibleTimeRange) * containerWidth;

    float nodeX = canvasPos.x + nodeScreenStartX;
    float nodeWidth = nodeScreenEndX - nodeScreenStartX;

    // Skip very small nodes
    if (nodeWidth < 1.0f) {
        nodeWidth = 1.0f; // Minimum 1 pixel
    }

    // Calculate node rectangle
    ImVec2 nodeMin(nodeX, startY);
    ImVec2 nodeMax(nodeX + nodeWidth, startY + nodeHeight);

    // Check if mouse is hovering this node
    ImVec2 mousePos = ImGui::GetIO().MousePos;
    bool isHovered = mousePos.x >= nodeMin.x && mousePos.x <= nodeMax.x &&
        mousePos.y >= nodeMin.y && mousePos.y <= nodeMax.y;

    if (isHovered) {
        m_HoveredNode = node;
        m_HoveredThread = thread; // Store which thread the hovered node belongs to

        // Handle selection on single click
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            m_SelectedNode = node;
            m_SelectedThread = thread;
        }

        // Handle zoom on double click
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            ZoomToNode(node);
        }
    }

    // Get node color
    float intensity = isHovered ? 1.2f : 1.0f;
    if (node == m_SelectedNode) intensity = 1.5f;
    ImU32 nodeColor = GetNodeColor(node->name, intensity);

    // Draw node rectangle
    drawList->AddRectFilled(nodeMin, nodeMax, nodeColor);

    // Draw node text if there's enough space
    if (nodeWidth > 30.0f) {
        std::string displayText = node->name;

        // Add timing cost to display text if there's enough space
        if (nodeWidth > 60.0f) {
            std::string timingText = " (" + FormatTiming(node->duration) + ")";
            displayText += timingText;
        }

        // Add timing information if requested and space allows
        if (m_ShowCallCounts && nodeWidth > 80.0f) {
            displayText += " (#" + std::to_string(node->eventIndex) + ")";
        }

        // Truncate text if too long
        ImVec2 textSize = ImGui::CalcTextSize(displayText.c_str());
        if (textSize.x > nodeWidth - 4.0f) {
            // Truncate and add ellipsis
            while (displayText.length() > 3 && ImGui::CalcTextSize(displayText.c_str()).x > nodeWidth - 4.0f) {
                displayText.pop_back();
            }
            if (displayText.length() > 3) {
                displayText = displayText.substr(0, displayText.length() - 3) + "...";
            }
        }

        // Center text vertically in the node
        ImVec2 textPos(nodeMin.x + 2.0f, nodeMin.y + (nodeHeight - textSize.y) * 0.5f);
        drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), displayText.c_str());
    }
}

void ProfilerVisualizer::ZoomToNode(TimelineFlameNode* node)
{
    if (!node || !m_TimelineFlameGraphData) {
        return;
    }

    // Calculate node's time position relative to global start time
    float nodeStartTime = static_cast<float>(node->startTime - m_TimelineFlameGraphData->globalStartTime);
    float nodeEndTime = static_cast<float>(node->endTime - m_TimelineFlameGraphData->globalStartTime);
    float nodeDuration = nodeEndTime - nodeStartTime;

    // Center the view on the node
    m_FlameGraphCenterTime = nodeStartTime + (nodeDuration * 0.5f);

    // Calculate zoom level to fit the node with some padding
    // We want the node to take up about 60-80% of the screen width for good visibility
    float targetVisibleRange = nodeDuration / 0.7f; // Node takes 70% of screen

    // Make sure we don't zoom in too much for very short events
    float minVisibleRange = m_TimelineFlameGraphData->globalDuration * 0.001f; // At least 0.1% of total timeline
    targetVisibleRange = std::max(targetVisibleRange, minVisibleRange);

    // Calculate the required zoom level
    m_FlameGraphZoom = m_TimelineFlameGraphData->globalDuration / targetVisibleRange;
    m_FlameGraphZoom = std::clamp(m_FlameGraphZoom, 0.01f, 10000.0f);
}

void ProfilerVisualizer::RenderNodeInteractionUI(TimelineThreadData* selectedThread)
{
    // Show node details on hover
    if (m_HoveredNode) {
        // In multi-thread mode, use the hovered thread instead of selected thread
        TimelineThreadData* threadForCalculation = selectedThread ? selectedThread : m_HoveredThread;

        if (threadForCalculation) {
            ImGui::BeginTooltip();
            ImGui::Text("Function: %s", m_HoveredNode->name.c_str());

            // Show thread name if we're in multi-thread mode
            if (!selectedThread && m_HoveredThread) {
                ImGui::Text("Thread: %s", m_HoveredThread->threadName.c_str());
            }

            ImGui::Text("Duration: %s (%.2f%%)",
                FormatTiming(m_HoveredNode->duration).c_str(),
                m_HoveredNode->GetPercentage(threadForCalculation->threadDuration));
            ImGui::Text("Start Time: %s", FormatTiming(m_HoveredNode->startTime - threadForCalculation->threadStartTime).c_str());
            ImGui::Text("End Time: %s", FormatTiming(m_HoveredNode->endTime - threadForCalculation->threadStartTime).c_str());
            ImGui::Text("Depth: %d", m_HoveredNode->depth);
            if (m_ShowCallCounts) {
                ImGui::Text("Event Index: %u", m_HoveredNode->eventIndex);
            }
            ImGui::EndTooltip();
        }
    }
}

void ProfilerVisualizer::RenderGlobalTimeTicks(ImVec2 canvasPos, ImVec2 canvasSize, ImDrawList* drawList, float tickHeight)
{
    // Similar to RenderTimeTicks but uses global timeline and frame markers
    std::vector<FrameMarker> globalFrameMarkers;

    for (size_t i = 0; i < m_TimelineFlameGraphData->GetThreadCount(); ++i) {
        auto* thread = m_TimelineFlameGraphData->GetThread(i);
        if (thread && !thread->frameMarkers.empty()) {
            globalFrameMarkers.insert(globalFrameMarkers.end(),
                thread->frameMarkers.begin(), thread->frameMarkers.end());
        }
    }

    if (globalFrameMarkers.empty()) {
        ImVec2 textPos(canvasPos.x + 10.0f, canvasPos.y + tickHeight * 0.3f);
        drawList->AddText(textPos, IM_COL32(150, 150, 150, 255), "No frame markers available");

        drawList->AddLine(
            ImVec2(canvasPos.x, canvasPos.y + tickHeight),
            ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + tickHeight),
            IM_COL32(200, 200, 200, 100), 1.0f);
        return;
    }

    // Calculate the visible time range using global timeline
    float visibleTimeRange = m_TimelineFlameGraphData->globalDuration / m_FlameGraphZoom;
    float visibleStartTime = m_FlameGraphCenterTime - (visibleTimeRange * 0.5f);
    float visibleEndTime = visibleStartTime + visibleTimeRange;

    // Sort frame markers by clock cycles
    std::sort(globalFrameMarkers.begin(), globalFrameMarkers.end(),
        [](const FrameMarker& a, const FrameMarker& b) {
            return a.clockCycles < b.clockCycles;
        });

    for (size_t i = 0; i < globalFrameMarkers.size(); ++i) {
        const auto& currentMarker = globalFrameMarkers[i];

        // Calculate marker time relative to global start
        float markerTime = static_cast<float>(currentMarker.clockCycles - m_TimelineFlameGraphData->globalStartTime);

        if (markerTime < visibleStartTime || markerTime > visibleEndTime) {
            continue;
        }

        float screenX = canvasPos.x + ((markerTime - visibleStartTime) / visibleTimeRange) * canvasSize.x;

        // Calculate time since last frame marker using real CPU frequency
        double timeSinceLastFrameMs = 0.0;
        if (i > 0) {
            const auto& previousMarker = globalFrameMarkers[i - 1];
            uint64_t cycleDiff = currentMarker.clockCycles - previousMarker.clockCycles;
            timeSinceLastFrameMs = (static_cast<double>(cycleDiff) / m_CPUFrequency) * 1000.0;
        }
        else {
            uint64_t cycleDiff = currentMarker.clockCycles - m_TimelineFlameGraphData->globalStartTime;
            timeSinceLastFrameMs = (static_cast<double>(cycleDiff) / m_CPUFrequency) * 1000.0;
        }

        ImU32 tickColor = IM_COL32(100, 200, 255, 200);
        std::string timeLabel = FormatMillisecondsWithUnit(timeSinceLastFrameMs);
        std::string fullLabel = "F" + std::to_string(currentMarker.frameNumber) + "\n" + timeLabel;

        if (screenX >= canvasPos.x - 30 && screenX <= canvasPos.x + canvasSize.x + 30) {
            ImVec2 textSize = ImGui::CalcTextSize(fullLabel.c_str());
            ImVec2 textPos(screenX - textSize.x, canvasPos.y + 2.0f);
            textPos.x = std::clamp(textPos.x, canvasPos.x, canvasPos.x + canvasSize.x - textSize.x);
            drawList->AddText(textPos, tickColor, fullLabel.c_str());
        }
    }

    drawList->AddLine(
        ImVec2(canvasPos.x, canvasPos.y + tickHeight),
        ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + tickHeight),
        IM_COL32(200, 200, 200, 100), 1.0f);
}

int ProfilerVisualizer::CalculateActualMaxDepth(TimelineThreadData* thread)
{
    if (!thread || thread->events.empty()) {
        return 0;
    }

    // Group events by depth to find the actual maximum depth with events
    std::map<int, std::vector<TimelineFlameNode*>> eventsByDepth;

    for (auto& event : thread->events) {
        if (event->duration > 0) {
            eventsByDepth[event->depth].push_back(event.get());
        }
    }

    // Return the highest depth that has events, or 0 if no events
    if (eventsByDepth.empty()) {
        return 0;
    }

    return eventsByDepth.rbegin()->first; // Get the highest depth key
}

void ProfilerVisualizer::RenderGlobalFrameMarkers(ImVec2 canvasPos, ImVec2 canvasSize, ImDrawList* drawList, float startY)
{
    std::vector<FrameMarker> globalFrameMarkers;

    for (size_t i = 0; i < m_TimelineFlameGraphData->GetThreadCount(); ++i) {
        auto* thread = m_TimelineFlameGraphData->GetThread(i);
        if (thread && !thread->frameMarkers.empty()) {
            globalFrameMarkers.insert(globalFrameMarkers.end(),
                thread->frameMarkers.begin(), thread->frameMarkers.end());
        }
    }

    if (globalFrameMarkers.empty()) {
        return;
    }

    std::sort(globalFrameMarkers.begin(), globalFrameMarkers.end(),
        [](const FrameMarker& a, const FrameMarker& b) {
            return a.clockCycles < b.clockCycles;
        });

    // Calculate the visible time range using global timeline
    float visibleTimeRange = m_TimelineFlameGraphData->globalDuration / m_FlameGraphZoom;
    float visibleStartTime = m_FlameGraphCenterTime - (visibleTimeRange * 0.5f);
    float visibleEndTime = visibleStartTime + visibleTimeRange;

    // Calculate marker height to span all threads
    float totalHeight = canvasSize.y - (startY - canvasPos.y);
    
    for (size_t i = 0; i < globalFrameMarkers.size(); ++i) {
        const auto& currentMarker = globalFrameMarkers[i];

        // Calculate marker time relative to global start
        float markerTime = static_cast<float>(currentMarker.clockCycles - m_TimelineFlameGraphData->globalStartTime);

        if (markerTime < visibleStartTime || markerTime > visibleEndTime) {
            continue;
        }

        float screenX = canvasPos.x + ((markerTime - visibleStartTime) / visibleTimeRange) * canvasSize.x;

        ImVec2 lineStart(screenX, startY);
        ImVec2 lineEnd(screenX, startY + totalHeight);

        ImU32 frameColor = IM_COL32(255, 100, 100, 200);
        drawList->AddLine(lineStart, lineEnd, frameColor, 2.0f);
    }
}

std::string ProfilerVisualizer::FormatMillisecondsWithUnit(double milliseconds)
{
    std::ostringstream oss;
    oss << std::fixed;

    if (milliseconds >= 1000.0) {
        // Show in seconds if >= 1 second
        oss << std::setprecision(2) << (milliseconds / 1000.0) << "s";
    }
    else if (milliseconds >= 1.0) {
        // Show in milliseconds with 1 decimal place
        oss << std::setprecision(1) << milliseconds << "ms";
    }
    else {
        // Show in microseconds for very small values
        oss << std::setprecision(0) << (milliseconds * 1000.0) << "us";
    }

    return oss.str();
}

ImU32 ProfilerVisualizer::GetNodeColor(const std::string& functionName, float intensity)
{
    // Generate a color based on function name hash
    std::hash<std::string> hasher;
    size_t hash = hasher(functionName);

    // Extract RGB components from hash
    uint8_t r = static_cast<uint8_t>((hash >> 0) & 0xFF);
    uint8_t g = static_cast<uint8_t>((hash >> 8) & 0xFF);
    uint8_t b = static_cast<uint8_t>((hash >> 16) & 0xFF);

    // Normalize to 0.0-1.0 range
    float fr = r / 255.0f;
    float fg = g / 255.0f;
    float fb = b / 255.0f;

    // Calculate luminance to determine if color is too light
    float luminance = 0.299f * fr + 0.587f * fg + 0.114f * fb;

    // If the color is too light (luminance > 0.6), darken it
    if (luminance > 0.6f) {
        // Scale down the brightness to ensure good contrast with white text
        float darkenFactor = 0.4f / luminance; // Target luminance of 0.4
        fr *= darkenFactor;
        fg *= darkenFactor;
        fb *= darkenFactor;
    }
    else {
        // For darker colors, ensure minimum brightness for visibility
        fr = fr * 0.8f + 0.2f; // Scale to 0.2-1.0 range
        fg = fg * 0.8f + 0.2f;
        fb = fb * 0.8f + 0.2f;
    }

    // Apply intensity multiplier
    fr *= intensity;
    fg *= intensity;
    fb *= intensity;

    // Clamp values to valid range
    fr = std::clamp(fr, 0.0f, 1.0f);
    fg = std::clamp(fg, 0.0f, 1.0f);
    fb = std::clamp(fb, 0.0f, 1.0f);

    return IM_COL32(static_cast<uint8_t>(fr * 255),
        static_cast<uint8_t>(fg * 255),
        static_cast<uint8_t>(fb * 255), 255);
}

std::string ProfilerVisualizer::FormatTiming(uint64_t cycles)
{
    // Use real detected CPU frequency instead of estimated 3 GHz
    double seconds = static_cast<double>(cycles) / m_CPUFrequency;

    std::ostringstream oss;
    oss << std::fixed;

    if (seconds >= 1.0) {
        oss << std::setprecision(3) << seconds << " s";
    }
    else if (seconds >= 1e-3) {
        oss << std::setprecision(3) << (seconds * 1e3) << " ms";
    }
    else if (seconds >= 1e-6) {
        oss << std::setprecision(3) << (seconds * 1e6) << " us";
    }
    else {
        oss << std::setprecision(0) << (seconds * 1e9) << " ns";
    }

    return oss.str();
}

} // namespace Profiler