#include "ImGuiButterflyChart.h"
#include <cmath>

// -------------------------------------------------------------------------
// Data management
// -------------------------------------------------------------------------

void ImGuiButterflyChart::SetData(const std::vector<RowData>& data)
{
    m_Data = data;
    RebuildSortedData();
    UpdateMaxValue();
}

void ImGuiButterflyChart::AddRow(const std::string& label, float leftValue, float rightValue,
                                  uint32_t leftCount, uint32_t rightCount)
{
    m_Data.emplace_back(label, leftValue, rightValue, leftCount, rightCount);
    RebuildSortedData();
    UpdateMaxValue();
}

void ImGuiButterflyChart::Clear()
{
    m_Data.clear();
    m_SortedData.clear();
    m_MaxValue = 0.0f;
    m_HoveredRowIndex = -1;
}

void ImGuiButterflyChart::RebuildSortedData()
{
    m_SortedData = m_Data;

    switch (m_Config.sortMode)
    {
    case SortMode::ByMaxDescending:
        std::sort(m_SortedData.begin(), m_SortedData.end(),
            [](const RowData& a, const RowData& b) {
                return std::max(a.leftValue, a.rightValue) > std::max(b.leftValue, b.rightValue);
            });
        break;

    case SortMode::ByMaxAscending:
        std::sort(m_SortedData.begin(), m_SortedData.end(),
            [](const RowData& a, const RowData& b) {
                return std::max(a.leftValue, a.rightValue) < std::max(b.leftValue, b.rightValue);
            });
        break;

    case SortMode::ByDiffDescending:
        std::sort(m_SortedData.begin(), m_SortedData.end(),
            [](const RowData& a, const RowData& b) {
                return std::abs(a.rightValue - a.leftValue) > std::abs(b.rightValue - b.leftValue);
            });
        break;

    case SortMode::ByLabelAscending:
        std::sort(m_SortedData.begin(), m_SortedData.end(),
            [](const RowData& a, const RowData& b) {
                return a.label < b.label;
            });
        break;

    case SortMode::None:
    default:
        break;
    }
}

void ImGuiButterflyChart::UpdateMaxValue()
{
    m_MaxValue = 0.0f;
    for (const auto& row : m_SortedData)
    {
        m_MaxValue = std::max(m_MaxValue, std::max(row.leftValue, row.rightValue));
    }

    if (m_MaxValue < 1e-6f)
    {
        m_MaxValue = 1.0f;
    }
}

// -------------------------------------------------------------------------
// Rendering entry point
// -------------------------------------------------------------------------

bool ImGuiButterflyChart::Render(const char* id)
{
    if (m_SortedData.empty())
    {
        ImGui::Text("No data to display");
        return false;
    }

    ImVec2 availRegion = ImGui::GetContentRegionAvail();

    float totalHeight = m_Config.headerHeight +
        m_SortedData.size() * (m_Config.rowHeight + m_Config.rowSpacing);

    if (ImGui::BeginChild(id, ImVec2(0, 0), false))
    {
        ImVec2 canvasPos = ImGui::GetCursorScreenPos();
        ImVec2 canvasSize = ImVec2(availRegion.x, std::max(totalHeight, availRegion.y));
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        ImVec2 canvasMax(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y);

        float centerX = canvasPos.x + canvasSize.x * 0.5f;
        float halfBarWidth = (canvasSize.x * 0.5f) - m_Config.labelWidth - 10.0f;
        halfBarWidth = std::max(halfBarWidth, 20.0f);

        float startY = canvasPos.y + m_Config.headerHeight;

        RenderBackground(drawList, canvasPos, canvasMax);

        // Center axis
        drawList->AddLine(
            ImVec2(centerX, canvasPos.y),
            ImVec2(centerX, canvasPos.y + canvasSize.y),
            m_Config.axisColor, 1.0f);

        RenderHeaders(drawList, canvasPos, canvasMax, centerX, halfBarWidth);
        RenderRows(drawList, canvasPos, canvasMax, centerX, halfBarWidth, startY);

        // Reserve space for scrolling
        ImGui::Dummy(ImVec2(canvasSize.x, totalHeight));
    }
    ImGui::EndChild();

    return m_HoveredRowIndex >= 0;
}

// -------------------------------------------------------------------------
// Rendering helpers
// -------------------------------------------------------------------------

void ImGuiButterflyChart::RenderBackground(ImDrawList* drawList,
    const ImVec2& canvasMin, const ImVec2& canvasMax) const
{
    drawList->AddRectFilled(canvasMin, canvasMax, m_Config.backgroundColor);
}

void ImGuiButterflyChart::RenderHeaders(ImDrawList* drawList,
    const ImVec2& canvasMin, const ImVec2& canvasMax,
    float centerX, float halfBarWidth) const
{
    ImVec2 leftSize = ImGui::CalcTextSize(m_Config.leftHeader.c_str());
    ImVec2 rightSize = ImGui::CalcTextSize(m_Config.rightHeader.c_str());

    drawList->AddText(
        ImVec2(centerX - halfBarWidth * 0.5f - leftSize.x * 0.5f, canvasMin.y + 2.0f),
        m_Config.headerLeftColor, m_Config.leftHeader.c_str());

    drawList->AddText(
        ImVec2(centerX + halfBarWidth * 0.5f - rightSize.x * 0.5f, canvasMin.y + 2.0f),
        m_Config.headerRightColor, m_Config.rightHeader.c_str());

    if (!m_Config.title.empty())
    {
        ImVec2 titleSize = ImGui::CalcTextSize(m_Config.title.c_str());
        drawList->AddText(
            ImVec2(centerX - titleSize.x * 0.5f, canvasMin.y - titleSize.y - 4.0f),
            m_Config.textColor, m_Config.title.c_str());
    }
}

void ImGuiButterflyChart::RenderRows(ImDrawList* drawList,
    const ImVec2& canvasMin, const ImVec2& canvasMax,
    float centerX, float halfBarWidth, float startY)
{
    ImVec2 mousePos = ImGui::GetIO().MousePos;
    m_HoveredRowIndex = -1;

    for (size_t i = 0; i < m_SortedData.size(); ++i)
    {
        const auto& row = m_SortedData[i];
        float rowY = startY + i * (m_Config.rowHeight + m_Config.rowSpacing);

        // Clip to canvas
        if (rowY + m_Config.rowHeight > canvasMax.y)
            break;

        // Alternating row background
        if (m_Config.showAlternatingRows && (i % 2 == 0))
        {
            drawList->AddRectFilled(
                ImVec2(canvasMin.x, rowY),
                ImVec2(canvasMax.x, rowY + m_Config.rowHeight),
                m_Config.altRowColor);
        }

        // Hover detection
        bool isHovered = mousePos.y >= rowY && mousePos.y <= rowY + m_Config.rowHeight &&
                         mousePos.x >= canvasMin.x && mousePos.x <= canvasMax.x;
        if (isHovered)
        {
            m_HoveredRowIndex = static_cast<int>(i);
            drawList->AddRectFilled(
                ImVec2(canvasMin.x, rowY),
                ImVec2(canvasMax.x, rowY + m_Config.rowHeight),
                m_Config.hoverColor);
        }

        ImU32 barColor = GetBarColor(i, row.label, 0.0f);

        // Left bar (grows leftward from center)
        if (row.leftValue > 0.0f)
        {
            RenderBar(drawList, centerX, rowY, m_Config.rowHeight,
                      row.leftValue, m_MaxValue, halfBarWidth, barColor, true);
        }

        // Right bar (grows rightward from center)
        if (row.rightValue > 0.0f)
        {
            RenderBar(drawList, centerX, rowY, m_Config.rowHeight,
                      row.rightValue, m_MaxValue, halfBarWidth, barColor, false);
        }

        // Diff indicator at center
        if (m_Config.showDiffIndicator && row.leftValue > 0.0f && row.rightValue > 0.0f)
        {
            std::string diffStr = FormatDiff(row.leftValue, row.rightValue);
            if (!diffStr.empty())
            {
                float diff = row.rightValue - row.leftValue;
                ImU32 diffColor = diff > 0 ? m_Config.diffPositiveColor : m_Config.diffNegativeColor;
                ImVec2 diffSize = ImGui::CalcTextSize(diffStr.c_str());
                drawList->AddText(
                    ImVec2(centerX - diffSize.x * 0.5f,
                           rowY + (m_Config.rowHeight - diffSize.y) * 0.5f),
                    diffColor, diffStr.c_str());
            }
        }

        // Row label on the far left
        {
            std::string truncated = TruncateLabel(row.label, m_Config.labelWidth - 4.0f);
            ImVec2 nameSize = ImGui::CalcTextSize(truncated.c_str());
            float nameX = centerX - halfBarWidth - nameSize.x - 8.0f;
            nameX = std::max(nameX, canvasMin.x + 2.0f);

            drawList->AddText(
                ImVec2(nameX, rowY + (m_Config.rowHeight - nameSize.y) * 0.5f),
                m_Config.textColor, truncated.c_str());
        }
    }

    // Tooltip for hovered row
    if (m_HoveredRowIndex >= 0 && m_HoveredRowIndex < static_cast<int>(m_SortedData.size()))
    {
        RenderTooltip(static_cast<size_t>(m_HoveredRowIndex),
                      m_SortedData[m_HoveredRowIndex]);
    }
}

void ImGuiButterflyChart::RenderBar(ImDrawList* drawList, float centerX, float rowY,
    float rowHeight, float value, float maxVal, float halfBarWidth,
    ImU32 color, bool growLeft) const
{
    float fraction = value / maxVal;
    float barW = fraction * halfBarWidth;

    ImVec2 barMin, barMax;
    if (growLeft)
    {
        barMin = ImVec2(centerX - barW, rowY + 1.0f);
        barMax = ImVec2(centerX, rowY + rowHeight - 1.0f);
    }
    else
    {
        barMin = ImVec2(centerX, rowY + 1.0f);
        barMax = ImVec2(centerX + barW, rowY + rowHeight - 1.0f);
    }

    drawList->AddRectFilled(barMin, barMax, color);
    drawList->AddRect(barMin, barMax, m_Config.barOutlineColor);

    // Value label
    if (m_Config.showValueLabels)
    {
        std::string label = FormatValue(value);
        ImVec2 labelSize = ImGui::CalcTextSize(label.c_str());

        if (barW > labelSize.x + 4.0f)
        {
            // Inside the bar
            float textX = growLeft ? barMin.x + 2.0f : barMax.x - labelSize.x - 2.0f;
            drawList->AddText(
                ImVec2(textX, rowY + (rowHeight - labelSize.y) * 0.5f),
                m_Config.valueLabelColor, label.c_str());
        }
        else
        {
            // Outside the bar
            float textX = growLeft
                ? barMin.x - labelSize.x - 2.0f
                : barMax.x + 2.0f;
            drawList->AddText(
                ImVec2(textX, rowY + (rowHeight - labelSize.y) * 0.5f),
                m_Config.valueLabelDimColor, label.c_str());
        }
    }
}

void ImGuiButterflyChart::RenderTooltip(size_t rowIndex, const RowData& row) const
{
    if (m_TooltipFunction)
    {
        // Delegate entirely to the caller for custom tooltip content
        ImGui::BeginTooltip();
        m_TooltipFunction(rowIndex, row);
        ImGui::EndTooltip();
    }
    else
    {
        // Default tooltip
        ImGui::BeginTooltip();
        ImGui::Text("Label: %s", row.label.c_str());
        ImGui::Separator();

        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(m_Config.headerLeftColor),
            "%s:", m_Config.leftHeader.c_str());
        if (row.leftValue > 0.0f)
        {
            ImGui::Text("  Value: %s", FormatValue(row.leftValue).c_str());
            if (row.leftCount > 0)
                ImGui::Text("  Count: %u", row.leftCount);
        }
        else
        {
            ImGui::TextDisabled("  Not present");
        }

        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(m_Config.headerRightColor),
            "%s:", m_Config.rightHeader.c_str());
        if (row.rightValue > 0.0f)
        {
            ImGui::Text("  Value: %s", FormatValue(row.rightValue).c_str());
            if (row.rightCount > 0)
                ImGui::Text("  Count: %u", row.rightCount);
        }
        else
        {
            ImGui::TextDisabled("  Not present");
        }

        if (row.leftValue > 0.0f && row.rightValue > 0.0f)
        {
            float diff = row.rightValue - row.leftValue;
            float pct = (diff / row.leftValue) * 100.0f;
            ImVec4 diffColor = diff > 0
                ? ImGui::ColorConvertU32ToFloat4(m_Config.diffPositiveColor)
                : ImGui::ColorConvertU32ToFloat4(m_Config.diffNegativeColor);

            ImGui::Separator();
            ImGui::TextColored(diffColor, "Delta: %s%s (%.1f%%)",
                diff > 0 ? "+" : "",
                FormatValue(std::abs(diff)).c_str(),
                pct);
        }

        ImGui::EndTooltip();
    }
}

// -------------------------------------------------------------------------
// Color and formatting
// -------------------------------------------------------------------------

ImU32 ImGuiButterflyChart::GetBarColor(size_t rowIndex, const std::string& label,
    float normalizedValue) const
{
    if (m_ColorFunction)
    {
        return m_ColorFunction(rowIndex, label, normalizedValue);
    }

    // Default: deterministic color based on label hash (same approach as bar graph)
    std::hash<std::string> hasher;
    size_t hash = hasher(label);

    float r = ((hash >> 0) & 0xFF) / 255.0f;
    float g = ((hash >> 8) & 0xFF) / 255.0f;
    float b = ((hash >> 16) & 0xFF) / 255.0f;

    float luminance = 0.299f * r + 0.587f * g + 0.114f * b;
    if (luminance > 0.6f)
    {
        float f = 0.4f / luminance;
        r *= f; g *= f; b *= f;
    }
    else
    {
        r = r * 0.8f + 0.2f;
        g = g * 0.8f + 0.2f;
        b = b * 0.8f + 0.2f;
    }

    return IM_COL32(
        static_cast<uint8_t>(std::clamp(r, 0.0f, 1.0f) * 255),
        static_cast<uint8_t>(std::clamp(g, 0.0f, 1.0f) * 255),
        static_cast<uint8_t>(std::clamp(b, 0.0f, 1.0f) * 255), 255);
}

std::string ImGuiButterflyChart::FormatValue(float value) const
{
    if (m_FormatFunction)
    {
        return m_FormatFunction(value);
    }

    // Default: two-decimal float
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << value;
    return oss.str();
}

std::string ImGuiButterflyChart::FormatDiff(float left, float right) const
{
    float diff = right - left;
    if (std::abs(diff) < 1e-6f)
        return {};

    float pct = (diff / left) * 100.0f;
    return (diff > 0 ? "+" : "") + std::to_string(static_cast<int>(pct)) + "%";
}

std::string ImGuiButterflyChart::TruncateLabel(const std::string& label, float maxWidth)
{
    ImVec2 size = ImGui::CalcTextSize(label.c_str());
    if (size.x <= maxWidth)
        return label;

    std::string truncated = label;
    while (truncated.length() > 3 && ImGui::CalcTextSize(truncated.c_str()).x > maxWidth)
    {
        truncated.pop_back();
    }
    if (truncated.length() > 3)
    {
        truncated = truncated.substr(0, truncated.length() - 3) + "...";
    }
    return truncated;
}