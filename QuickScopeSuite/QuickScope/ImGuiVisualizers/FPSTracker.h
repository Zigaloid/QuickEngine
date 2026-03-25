#pragma once

#include "VisualizableMetricHeuristic.h"
#include "UnifiedActionManager.h"
#include "ImGuiLineGraph.h"
#include <chrono>

/**
 * @brief Specialized FPS tracking class derived from VisualizableMetricHeuristic
 * 
 * This class provides FPS-specific functionality including automatic bucket setup,
 * performance-based coloring, and FPS-specific statistics display.
 */
class FPSTracker : public VisualizableMetricHeuristic
{
public:
    /**
     * @brief FPS performance thresholds for categorization
     */
    struct FPSThresholds
    {
        float excellent = 120.0f;   // > 120 FPS = Excellent
        float good = 60.0f;         // 60-120 FPS = Good
        float acceptable = 30.0f;   // 30-60 FPS = Acceptable
        float poor = 15.0f;         // 15-30 FPS = Poor
        // < 15 FPS = Unacceptable
    };

    /**
     * @brief Graph type for FPS visualization
     */
    enum class GraphType
    {
        BarGraph,       // Show distribution as bar graph
        LineGraph,      // Show FPS over time as line graph
        Both            // Show both graphs
    };

private:
    FPSThresholds m_Thresholds;
    std::chrono::steady_clock::time_point m_LastFrameTime;
    float m_CurrentFPS;
    bool m_FirstFrame;
    
    // Line graph support
    ImGuiLineGraph m_LineGraph;
    GraphType m_GraphType;
    std::chrono::steady_clock::time_point m_StartTime;
    bool m_LineGraphNeedsUpdate;
    size_t m_MaxLineGraphSamples;

    // Action manager for menu/toolbar/console integration
    UI::UnifiedActionManager m_ActionManager;

public:
    /**
     * @brief Constructor
     * @param maxSamples Maximum number of FPS samples to track (0 = unlimited)
     */
    explicit FPSTracker(size_t maxSamples = 10000);

    /**
     * @brief Update FPS from delta time
     * @param deltaTime Time since last frame in seconds
     */
    void UpdateFromDeltaTime(double deltaTime);

    /**
     * @brief Update FPS using high-resolution timer
     * Call this once per frame for automatic FPS calculation
     */
    void UpdateFrame();

    /**
     * @brief Get current FPS value
     */
    float GetCurrentFPS() const { return m_CurrentFPS; }

    /**
     * @brief Set FPS performance thresholds
     */
    void SetThresholds(const FPSThresholds& thresholds);

    /**
     * @brief Get FPS performance thresholds
     */
    const FPSThresholds& GetThresholds() const { return m_Thresholds; }

    /**
     * @brief Get FPS performance category as string
     */
    std::string GetPerformanceCategory(float fps) const;

    /**
     * @brief Get FPS performance category color
     */
    ImU32 GetPerformanceCategoryColor(float fps) const;

    /**
     * @brief Setup appropriate FPS buckets based on thresholds
     */
    void SetupFPSBuckets();

    /**
     * @brief Render FPS-specific statistics
     */
    void RenderFPSStatistics() const;

    /**
     * @brief Render complete FPS analysis window
     * @param windowTitle Title for the ImGui window
     * @param isOpen Pointer to bool controlling window visibility
     * @return true if window is open and rendering
     */
    bool RenderFPSAnalysisWindow(const char* windowTitle = "FPS Analysis", bool* isOpen = nullptr);

    /**
     * @brief Set the graph type for FPS visualization
     */
    void SetGraphType(GraphType type) { m_GraphType = type; }

    /**
     * @brief Get the current graph type
     */
    GraphType GetGraphType() const { return m_GraphType; }

    /**
     * @brief Set maximum number of samples for the line graph
     */
    void SetMaxLineGraphSamples(size_t maxSamples) { m_MaxLineGraphSamples = maxSamples; }

    /**
     * @brief Get line graph configuration for customization
     */
    ImGuiLineGraph::Config& GetLineGraphConfig() { return m_LineGraph.GetConfig(); }

    /**
     * @brief Render just the line graph
     */
    bool RenderLineGraphOnly(const char* id = "FPSLineGraph");
    
private:
    /**
     * @brief Setup FPS-specific visualization configuration
     */
    void InitializeFPSVisualization();

    /**
     * @brief Setup performance-based color scheme
     */
    void SetupPerformanceColors();

    /**
     * @brief Initialize line graph configuration
     */
    void InitializeLineGraph();

    /**
     * @brief Update line graph with new FPS data
     */
    void UpdateLineGraph();

    /**
     * @brief Get elapsed time since tracking started
     */
    double GetElapsedTime() const;

    /**
     * @brief Register FPS actions with the UnifiedActionManager
     */
    void RegisterFPSActions();

    /**
     * @brief Unregister FPS actions from the UnifiedActionManager
     */
    void UnregisterFPSActions();
};