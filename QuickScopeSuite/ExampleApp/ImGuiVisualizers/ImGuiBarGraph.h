#pragma once

#include "imgui.h"
#include <vector>
#include <string>
#include <functional>
#include <algorithm>
#include <iomanip>
#include <sstream>

class ImGuiBarGraph
{
public:
    // Bar data structure
    struct BarData
    {
        float value;
        std::string label;
        
        BarData(float val, const std::string& lbl = "") 
            : value(val), label(lbl) {}
    };
    
    // Value display mode enum
    enum class ValueDisplayMode
    {
        None,           // No values displayed
        Values,         // Show raw values
        Percentages,    // Show percentages
        Both            // Show both values and percentages
    };
    
    // Color function signature: takes (index, value, normalized_value) and returns ImU32 color
    using ColorFunction = std::function<ImU32(size_t index, float value, float normalizedValue)>;
    
    // Configuration options
    struct Config
    {
        ImVec2 size = ImVec2(0, 200);              // Graph size (0 = auto-width)
        float barSpacing = 2.0f;                    // Space between bars
        float minBarWidth = 1.0f;                   // Minimum bar width
        float maxBarWidth = 50.0f;                  // Maximum bar width
        bool showValues = true;                     // Show value labels on bars (deprecated - use valueDisplayMode)
        ValueDisplayMode valueDisplayMode = ValueDisplayMode::Values; // What to display above bars
        bool showLabels = true;                     // Show labels below bars
        bool showGrid = true;                       // Show horizontal grid lines
        bool autoScale = true;                      // Auto-scale to data range
        float minValue = 0.0f;                      // Manual min value (if !autoScale)
        float maxValue = 100.0f;                    // Manual max value (if !autoScale)
        std::string title;                          // Graph title
        ImU32 backgroundColor = IM_COL32(40, 40, 40, 255);
        ImU32 gridColor = IM_COL32(80, 80, 80, 128);
        ImU32 textColor = IM_COL32(255, 255, 255, 255);
        ImU32 defaultBarColor = IM_COL32(100, 150, 255, 255);
    };

private:
    std::vector<BarData> m_Data;
    Config m_Config;
    ColorFunction m_ColorFunction;
    
    // Cached values for interaction
    float m_DataMin = 0.0f;
    float m_DataMax = 0.0f;
    float m_DataSum = 0.0f;  // Total sum for percentage calculations
    int m_HoveredBarIndex = -1;
    
public:
    ImGuiBarGraph() = default;
    
    // Set configuration
    void SetConfig(const Config& config) { m_Config = config; }
    Config& GetConfig() { return m_Config; }
    
    // Set custom color function
    void SetColorFunction(const ColorFunction& colorFunc) { m_ColorFunction = colorFunc; }
    
    // Data management
    void SetData(const std::vector<BarData>& data);
    void SetData(const std::vector<float>& values);
    void SetData(const std::vector<float>& values, const std::vector<std::string>& labels);
    void AddBar(float value, const std::string& label = "");
    void Clear();
    
    // Rendering
    bool Render(const char* id);
    
private:
    void UpdateDataRange();
    ImU32 GetBarColor(size_t index, float value, float normalizedValue) const;
    void RenderBackground(ImDrawList* drawList, const ImVec2& canvasMin, const ImVec2& canvasMax) const;
    void RenderGrid(ImDrawList* drawList, const ImVec2& canvasMin, const ImVec2& canvasMax) const;
    void RenderBars(ImDrawList* drawList, const ImVec2& canvasMin, const ImVec2& canvasMax);
    void RenderLabels(ImDrawList* drawList, const ImVec2& canvasMin, const ImVec2& canvasMax) const;
    void RenderTitle(ImDrawList* drawList, const ImVec2& canvasMin, const ImVec2& canvasMax) const;
    void HandleInteraction(const ImVec2& canvasMin, const ImVec2& canvasMax);
    std::string FormatValue(float value) const;
    std::string FormatPercentage(float value, float total) const;
    std::string GetDisplayText(float value) const;
};