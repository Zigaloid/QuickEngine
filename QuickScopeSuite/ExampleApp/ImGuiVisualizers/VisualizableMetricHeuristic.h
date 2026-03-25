#pragma once

#include "MetricBucketHeuristic.h"
#include "ImGuiBarGraph.h"
#include <sstream>
#include <iomanip>

/**
 * @brief A visualizable metric heuristic that uses ImGuiBarGraph for rendering
 * 
 * This class extends MetricBucketHeuristic to provide immediate visualization
 * capabilities using the ImGuiBarGraph widget. It can display bucket counts,
 * percentages, or averages with customizable appearance.
 */
class VisualizableMetricHeuristic : public MetricBucketHeuristic
{
public:
    /**
     * @brief Display mode for the visualization
     */
    enum class DisplayMode
    {
        Count,       // Show raw counts in each bucket
        Percentage,  // Show percentage of total samples in each bucket
        Average      // Show average value in each bucket
    };

    /**
     * @brief Configuration for the visualization
     */
    struct VisualizationConfig
    {
        DisplayMode displayMode = DisplayMode::Count;
        bool showStatistics = true;           // Show statistics panel
        bool showBucketDetails = true;        // Show bucket details on hover
        bool autoUpdateGraph = true;          // Update graph automatically when samples are added
        std::string title = "Metric Distribution";
        ImGuiBarGraph::Config graphConfig;    // Underlying graph configuration
        
        VisualizationConfig()
        {
            // Set up default graph configuration
            graphConfig.size = ImVec2(0, 200);
            graphConfig.showValues = true;
            graphConfig.showLabels = true;
            graphConfig.showGrid = true;
            graphConfig.autoScale = true;
        }
    };

private:
    ImGuiBarGraph m_BarGraph;
    VisualizationConfig m_VisConfig;
    bool m_GraphNeedsUpdate;

public:
    /**
     * @brief Constructor
     * @param maxSamples Maximum number of samples to track (0 = unlimited)
     * @param autoUpdateStats Whether to automatically update statistics on each sample
     */
    explicit VisualizableMetricHeuristic(size_t maxSamples = 0, bool autoUpdateStats = true)
        : MetricBucketHeuristic(maxSamples, autoUpdateStats), m_GraphNeedsUpdate(true)
    {
        SetupDefaultColorFunction();
    }

    /**
     * @brief Set visualization configuration
     */
    void SetVisualizationConfig(const VisualizationConfig& config)
    {
        m_VisConfig = config;
        m_BarGraph.SetConfig(config.graphConfig);
        m_GraphNeedsUpdate = true;
    }

    /**
     * @brief Get visualization configuration
     */
    VisualizationConfig& GetVisualizationConfig() { return m_VisConfig; }
    const VisualizationConfig& GetVisualizationConfig() const { return m_VisConfig; }

    /**
     * @brief Override AddSample to trigger graph updates
     */
    int AddSample(float value) override
    {
        int result = MetricBucketHeuristic::AddSample(value);
        
        if (m_VisConfig.autoUpdateGraph)
        {
            m_GraphNeedsUpdate = true;
        }
        
        return result;
    }

    /**
     * @brief Override Reset to trigger graph updates
     */
    void Reset() override
    {
        MetricBucketHeuristic::Reset();
        m_GraphNeedsUpdate = true;
    }

    /**
     * @brief Manually trigger graph update
     */
    void UpdateGraph()
    {
        UpdateGraphData();
        m_GraphNeedsUpdate = false;
    }

    /**
     * @brief Render the visualization using ImGui
     * @param id Unique identifier for the ImGui widgets
     * @return true if user interacted with the graph
     */
    bool Render(const char* id = "MetricHeuristic")
    {
        if (m_GraphNeedsUpdate)
        {
            UpdateGraph();
        }

        bool interacted = false;

        // Create a unique window or use current context
        std::string windowId = std::string(id) + "_Window";
        
        if (ImGui::Begin(windowId.c_str()))
        {
            // Render controls
            if (RenderControls(id))
            {
                interacted = true;
            }

            ImGui::Separator();

            // Render statistics if enabled
            if (m_VisConfig.showStatistics)
            {
                RenderStatistics();
                ImGui::Separator();
            }

            // Render the bar graph
            std::string graphId = std::string(id) + "_Graph";
            if (m_BarGraph.Render(graphId.c_str()))
            {
                interacted = true;
            }

            // Render bucket details if enabled
            if (m_VisConfig.showBucketDetails)
            {
                ImGui::Separator();
                RenderBucketDetails();
            }
        }
        ImGui::End();

        return interacted;
    }

    /**
     * @brief Render just the graph (without window wrapper)
     * @param id Unique identifier for the graph
     * @return true if user interacted with the graph
     */
    bool RenderGraphOnly(const char* id = "MetricGraph")
    {
        if (m_GraphNeedsUpdate)
        {
            UpdateGraph();
        }

        return m_BarGraph.Render(id);
    }

    /**
     * @brief Set custom color function for the bars
     */
    void SetColorFunction(const ImGuiBarGraph::ColorFunction& colorFunc)
    {
        m_BarGraph.SetColorFunction(colorFunc);
    }

    /**
     * @brief Set up a performance-based color function (green = good, red = bad)
     * @param goodThreshold Values below this are colored green
     * @param badThreshold Values above this are colored red
     */
    void SetPerformanceColorFunction(float goodThreshold, float badThreshold)
    {
        m_BarGraph.SetColorFunction([goodThreshold, badThreshold](size_t index, float value, float normalizedValue) -> ImU32 {
            if (value <= goodThreshold)
                return IM_COL32(0, 255, 0, 255);    // Green for good performance
            else if (value <= badThreshold)
                return IM_COL32(255, 255, 0, 255);  // Yellow for acceptable
            else
                return IM_COL32(255, 0, 0, 255);    // Red for bad performance
        });
    }

    /**
     * @brief Set display mode and update graph
     */
    void SetDisplayMode(DisplayMode mode)
    {
        if (m_VisConfig.displayMode != mode)
        {
            m_VisConfig.displayMode = mode;
            m_GraphNeedsUpdate = true;
        }
    }

    /**
     * @brief Get current display mode
     */
    DisplayMode GetDisplayMode() const { return m_VisConfig.displayMode; }

    /**
     * @brief Forward DefineStepBuckets to base class for convenience
     */
    void DefineStepBuckets(float minValue, float maxValue, float stepValue)
    {
        MetricBucketHeuristic::DefineStepBuckets(minValue, maxValue, stepValue);
        m_GraphNeedsUpdate = true;
    }

    /**
     * @brief Forward DefinePerformanceBuckets to base class for convenience
     */
    void DefinePerformanceBuckets(float targetFrameTime = 16.67f)
    {
        MetricBucketHeuristic::DefinePerformanceBuckets(targetFrameTime);
        m_GraphNeedsUpdate = true;
    }

private:
    /**
     * @brief Update the graph data based on current buckets and display mode
     */
    void UpdateGraphData()
    {
        std::vector<float> values;
        std::vector<std::string> labels;

        values.reserve(m_Buckets.size());
        labels.reserve(m_Buckets.size());

        for (const auto& bucket : m_Buckets)
        {
            float displayValue = 0.0f;
            
            switch (m_VisConfig.displayMode)
            {
                case DisplayMode::Count:
                    displayValue = static_cast<float>(bucket.count);
                    break;
                    
                case DisplayMode::Percentage:
                    displayValue = bucket.GetPercentage(m_Stats.totalSamples);
                    break;
                    
                case DisplayMode::Average:
                    displayValue = bucket.GetAverage();
                    break;
            }

            values.push_back(displayValue);
            labels.push_back(bucket.label);
        }

        m_BarGraph.SetData(values, labels);

        // Update graph title based on display mode
        auto& config = m_BarGraph.GetConfig();
        config.title = m_VisConfig.title;
        
        switch (m_VisConfig.displayMode)
        {
            case DisplayMode::Count:
                config.title += " (Counts)";
                break;
            case DisplayMode::Percentage:
                config.title += " (Percentages)";
                break;
            case DisplayMode::Average:
                config.title += " (Averages)";
                break;
        }
    }

    /**
     * @brief Render control widgets
     */
    bool RenderControls(const char* id)
    {
        bool changed = false;

        // Display mode selection
        const char* displayModeNames[] = { "Count", "Percentage", "Average" };
        int currentMode = static_cast<int>(m_VisConfig.displayMode);
        
        if (ImGui::Combo("Display Mode", &currentMode, displayModeNames, IM_ARRAYSIZE(displayModeNames)))
        {
            SetDisplayMode(static_cast<DisplayMode>(currentMode));
            changed = true;
        }

        ImGui::SameLine();
        if (ImGui::Button("Reset Data"))
        {
            Reset();
            changed = true;
        }

        // Toggles
        ImGui::Checkbox("Show Statistics", &m_VisConfig.showStatistics);
        ImGui::SameLine();
        ImGui::Checkbox("Show Bucket Details", &m_VisConfig.showBucketDetails);
        ImGui::SameLine();
        ImGui::Checkbox("Auto Update", &m_VisConfig.autoUpdateGraph);

        return changed;
    }

    /**
     * @brief Render statistics panel
     */
    void RenderStatistics()
    {
        const auto& stats = GetStatistics();

        ImGui::Text("Statistics:");
        ImGui::Columns(2, "StatisticsColumns", false);
        
        ImGui::Text("Total Samples:"); ImGui::NextColumn();
        ImGui::Text("%zu", stats.totalSamples); ImGui::NextColumn();
        
        if (stats.totalSamples > 0)
        {
            ImGui::Text("Min Value:"); ImGui::NextColumn();
            ImGui::Text("%.3f", stats.minValue); ImGui::NextColumn();
            
            ImGui::Text("Max Value:"); ImGui::NextColumn();
            ImGui::Text("%.3f", stats.maxValue); ImGui::NextColumn();
            
            ImGui::Text("Average:"); ImGui::NextColumn();
            ImGui::Text("%.3f", stats.average); ImGui::NextColumn();            
        }
        
        ImGui::Columns(1);
    }

    /**
     * @brief Render detailed bucket information
     */
    void RenderBucketDetails()
    {
        if (m_Buckets.empty()) return;

        ImGui::Text("Bucket Details:");
        
        if (ImGui::BeginTable("BucketTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
        {
            ImGui::TableSetupColumn("Label");
            ImGui::TableSetupColumn("Range");
            ImGui::TableSetupColumn("Count");
            ImGui::TableSetupColumn("Percentage");
            ImGui::TableSetupColumn("Average");
            ImGui::TableHeadersRow();

            for (size_t i = 0; i < m_Buckets.size(); ++i)
            {
                const auto& bucket = m_Buckets[i];
                ImGui::TableNextRow();
                
                ImGui::TableNextColumn();
                ImGui::Text("%s", bucket.label.c_str());
                
                ImGui::TableNextColumn();
                ImGui::Text("%.2f", bucket.minValue);
                
                ImGui::TableNextColumn();
                ImGui::Text("%zu", bucket.count);
                
                ImGui::TableNextColumn();
                ImGui::Text("%.1f%%", bucket.GetPercentage(m_Stats.totalSamples));
                
                ImGui::TableNextColumn();
                if (bucket.count > 0)
                    ImGui::Text("%.3f", bucket.GetAverage());
                else
                    ImGui::Text("N/A");
            }
            
            ImGui::EndTable();
        }
    }

    /**
     * @brief Set up default color function based on bucket index
     */
    void SetupDefaultColorFunction()
    {
        m_BarGraph.SetColorFunction([](size_t index, float value, float normalizedValue) -> ImU32 {
            // Generate colors based on bucket index for consistent coloring
            float hue = (static_cast<float>(index) * 137.5f) / 360.0f; // Golden angle for good distribution
            hue = hue - std::floor(hue); // Keep in [0,1] range
            
            // Convert HSV to RGB (simplified)
            float r, g, b;
            int i = static_cast<int>(hue * 6);
            float f = hue * 6 - i;
            float p = 0.4f; // Low value for darker colors
            float q = 0.8f * (1 - f);
            float t = 0.8f * f;
            
            switch(i % 6)
            {
                case 0: r = 0.8f; g = t; b = p; break;
                case 1: r = q; g = 0.8f; b = p; break;
                case 2: r = p; g = 0.8f; b = t; break;
                case 3: r = p; g = q; b = 0.8f; break;
                case 4: r = t; g = p; b = 0.8f; break;
                case 5: r = 0.8f; g = p; b = q; break;
                default: r = g = b = 0.5f; break;
            }
            
            return IM_COL32(static_cast<uint8_t>(r * 255), 
                           static_cast<uint8_t>(g * 255), 
                           static_cast<uint8_t>(b * 255), 255);
        });
    }
};