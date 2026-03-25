#pragma once

#include "imgui.h"
#include "Profiler/ProfilerAnalyzer.h"
#include <memory>
#include <vector>
#include <string>
#include <map>

namespace Profiler {

class ProfilerVisualizer
{
public:
    ProfilerVisualizer();
    ~ProfilerVisualizer() = default;

    // Main rendering methods
    bool RenderFlameGraphWindow(const char* windowTitle, bool* isOpen = nullptr);
    void SetFlameGraphData(std::unique_ptr<TimelineFlameGraphData> data);
    
    // Configuration
    void SetShowCallCounts(bool show) { m_ShowCallCounts = show; }
    void SetShowFrameMarkers(bool show) { m_ShowFrameMarkers = show; }
    void SetShowTimeTicks(bool show) { m_ShowTimeTicks = show; }
    void SetMinDisplayTime(float minTime) { m_MinDisplayTime = minTime; }
    void SetAutoRefresh(bool autoRefresh) { m_AutoRefreshFlameGraph = autoRefresh; }

    // Getters
    bool GetShowCallCounts() const { return m_ShowCallCounts; }
    bool GetShowFrameMarkers() const { return m_ShowFrameMarkers; }
    bool GetShowTimeTicks() const { return m_ShowTimeTicks; }
    float GetMinDisplayTime() const { return m_MinDisplayTime; }
    bool GetAutoRefresh() const { return m_AutoRefreshFlameGraph; }

    // View control
    void ResetView();
    void ZoomToNode(TimelineFlameNode* node);

private:
    // Rendering methods
    void RenderFlameGraph();
    void RenderFlameGraphMenuBar();
    bool InitializeFlameGraphRendering();
    void InitializeFlameGraphView();
    void RenderNavigationControls();
    
    // Mouse and keyboard interaction
    void HandleMouseInteraction(ImVec2 canvasPos, ImVec2 canvasSize);
    void HandleMouseWheelZoom(ImVec2 canvasPos, ImVec2 canvasSize);
    void HandleMouseDrag(ImVec2 canvasSize);
    void HandleKeyboardZoom(ImVec2 canvasPos, ImVec2 canvasSize);
    
    // Event rendering
    void RenderEvents(TimelineThreadData* thread, ImVec2 canvasPos, ImVec2 canvasSize, ImDrawList* drawList);
    void RenderFlameGraphNode(TimelineFlameNode* node, TimelineThreadData* thread,
        float containerWidth, float nodeHeight, uint64_t globalTotalTime, 
        ImVec2 canvasPos, ImDrawList* drawList, float startY);
    
    // UI rendering
    void RenderGlobalTimeTicks(ImVec2 canvasPos, ImVec2 canvasSize, ImDrawList* drawList, float tickHeight);
    void RenderGlobalFrameMarkers(ImVec2 canvasPos, ImVec2 canvasSize, ImDrawList* drawList, float startY);
    void RenderNodeInteractionUI(TimelineThreadData* selectedThread);
    
    // Helper methods
    std::string FormatMillisecondsWithUnit(double milliseconds);
    ImU32 GetNodeColor(const std::string& functionName, float intensity = 1.0f);
    int CalculateActualMaxDepth(TimelineThreadData* thread);
    std::string FormatTiming(uint64_t cycles);

    // Timeline flamegraph data
    std::unique_ptr<TimelineFlameGraphData> m_TimelineFlameGraphData;

    // View state
    float m_FlameGraphZoom = 1.0f;
    float m_FlameGraphCenterTime = 0.0f;
    bool m_ViewInitialized = false;

    // Interaction state
    TimelineFlameNode* m_HoveredNode = nullptr;
    TimelineFlameNode* m_SelectedNode = nullptr;
    TimelineThreadData* m_HoveredThread = nullptr;
    TimelineThreadData* m_SelectedThread = nullptr;

    // Display options
    bool m_ShowCallCounts = false;
    bool m_ShowFrameMarkers = true;
    bool m_ShowTimeTicks = true;
    float m_MinDisplayTime = 0.1f;
    bool m_AutoRefreshFlameGraph = false;
    float m_actualRenderedHeight = 100.0f;
    // Thread visibility tracking
    std::vector<bool> m_ThreadVisibility;

    // CPU frequency for accurate timing calculations
    double m_CPUFrequency = 3.0e9; // Will be initialized with actual detected frequency
};

} // namespace Profiler