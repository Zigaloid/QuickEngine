#pragma once

#include "imgui.h"
#include <vector>
#include <string>
#include <functional>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cmath>

class ImGuiButterflyChart
{
public:
    // A single row in the butterfly chart: one label, two values (left and right)
    struct RowData
    {
        std::string label;
        float leftValue = 0.0f;
        float rightValue = 0.0f;
        uint32_t leftCount = 0;   // Optional auxiliary count (e.g. call count)
        uint32_t rightCount = 0;

        RowData() = default;
        RowData(const std::string& lbl, float left, float right,
                uint32_t leftCnt = 0, uint32_t rightCnt = 0)
            : label(lbl), leftValue(left), rightValue(right)
            , leftCount(leftCnt), rightCount(rightCnt) {}
    };

    // How rows are sorted before rendering
    enum class SortMode
    {
        None,               // Keep insertion order
        ByMaxDescending,    // Largest max(left, right) first
        ByMaxAscending,     // Smallest max(left, right) first
        ByDiffDescending,   // Largest absolute difference first
        ByLabelAscending    // Alphabetical by label
    };

    // Color function: (rowIndex, label, normalizedValue) -> ImU32
    using ColorFunction = std::function<ImU32(size_t rowIndex, const std::string& label, float normalizedValue)>;

    // Value formatting function: (rawValue) -> display string
    using FormatFunction = std::function<std::string(float value)>;

    // Tooltip callback: (rowIndex, row) — caller may emit arbitrary ImGui tooltip content
    using TooltipFunction = std::function<void(size_t rowIndex, const RowData& row)>;

    // Configuration
    struct Config
    {
        std::string title;
        std::string leftHeader = "Left";
        std::string rightHeader = "Right";

        float rowHeight = 20.0f;
        float rowSpacing = 2.0f;
        float labelWidth = 160.0f;
        float headerHeight = 25.0f;

        bool showDiffIndicator = true;       // Show +/-% at center axis
        bool showValueLabels = true;         // Show formatted value inside/beside bars
        bool showAlternatingRows = true;     // Subtle alternating row background

        SortMode sortMode = SortMode::ByMaxDescending;

        ImU32 backgroundColor   = IM_COL32(30,  30,  30, 255);
        ImU32 axisColor         = IM_COL32(200, 200, 200, 150);
        ImU32 textColor         = IM_COL32(220, 220, 220, 255);
        ImU32 headerLeftColor   = IM_COL32(100, 180, 255, 255);
        ImU32 headerRightColor  = IM_COL32(255, 180, 100, 255);
        ImU32 hoverColor        = IM_COL32(255, 255, 255, 20);
        ImU32 altRowColor       = IM_COL32(255, 255, 255, 8);
        ImU32 barOutlineColor   = IM_COL32(255, 255, 255, 60);
        ImU32 valueLabelColor   = IM_COL32(255, 255, 255, 220);
        ImU32 valueLabelDimColor= IM_COL32(200, 200, 200, 200);
        ImU32 diffPositiveColor = IM_COL32(255, 80,  80, 200);
        ImU32 diffNegativeColor = IM_COL32(80,  255, 80, 200);
    };

private:
    std::vector<RowData> m_Data;
    std::vector<RowData> m_SortedData;  // Sorted view used during rendering
    Config m_Config;

    ColorFunction   m_ColorFunction;
    FormatFunction  m_FormatFunction;
    TooltipFunction m_TooltipFunction;

    float m_MaxValue = 0.0f;
    int   m_HoveredRowIndex = -1;

public:
    ImGuiButterflyChart() = default;

    // Configuration
    void SetConfig(const Config& config) { m_Config = config; }
    Config& GetConfig() { return m_Config; }

    // Callbacks
    void SetColorFunction(const ColorFunction& fn)     { m_ColorFunction = fn; }
    void SetFormatFunction(const FormatFunction& fn)    { m_FormatFunction = fn; }
    void SetTooltipFunction(const TooltipFunction& fn)  { m_TooltipFunction = fn; }

    // Data management
    void SetData(const std::vector<RowData>& data);
    void AddRow(const std::string& label, float leftValue, float rightValue,
                uint32_t leftCount = 0, uint32_t rightCount = 0);
    void Clear();

    int  GetHoveredRowIndex() const { return m_HoveredRowIndex; }
    const std::vector<RowData>& GetSortedData() const { return m_SortedData; }

    // Rendering — returns true if a row is hovered
    bool Render(const char* id);

private:
    void RebuildSortedData();
    void UpdateMaxValue();

    ImU32 GetBarColor(size_t rowIndex, const std::string& label, float normalizedValue) const;
    std::string FormatValue(float value) const;
    std::string FormatDiff(float left, float right) const;

    void RenderBackground(ImDrawList* drawList, const ImVec2& canvasMin, const ImVec2& canvasMax) const;
    void RenderHeaders(ImDrawList* drawList, const ImVec2& canvasMin, const ImVec2& canvasMax,
                       float centerX, float halfBarWidth) const;
    void RenderRows(ImDrawList* drawList, const ImVec2& canvasMin, const ImVec2& canvasMax,
                    float centerX, float halfBarWidth, float startY);
    void RenderBar(ImDrawList* drawList, float centerX, float rowY, float barWidth,
                   float value, float maxVal, float halfBarWidth,
                   ImU32 color, bool growLeft) const;
    void RenderTooltip(size_t rowIndex, const RowData& row) const;
    void HandleInteraction(const ImVec2& canvasMin, const ImVec2& canvasMax,
                           float startY, size_t visibleRowCount);

    static std::string TruncateLabel(const std::string& label, float maxWidth);
};