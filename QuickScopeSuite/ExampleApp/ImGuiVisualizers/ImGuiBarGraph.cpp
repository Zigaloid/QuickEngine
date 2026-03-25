#include "ImGuiBarGraph.h"
#include <cmath>

void ImGuiBarGraph::SetData(const std::vector<BarData>& data)
{
    m_Data = data;
    UpdateDataRange();
}

void ImGuiBarGraph::SetData(const std::vector<float>& values)
{
    m_Data.clear();
    m_Data.reserve(values.size());
    for (size_t i = 0; i < values.size(); ++i)
    {
        m_Data.emplace_back(values[i], "Bar " + std::to_string(i));
    }
    UpdateDataRange();
}

void ImGuiBarGraph::SetData(const std::vector<float>& values, const std::vector<std::string>& labels)
{
    m_Data.clear();
    size_t count = std::min(values.size(), labels.size());
    m_Data.reserve(count);
    for (size_t i = 0; i < count; ++i)
    {
        m_Data.emplace_back(values[i], labels[i]);
    }
    UpdateDataRange();
}

void ImGuiBarGraph::AddBar(float value, const std::string& label)
{
    m_Data.emplace_back(value, label.empty() ? ("Bar " + std::to_string(m_Data.size())) : label);
    UpdateDataRange();
}

void ImGuiBarGraph::Clear()
{
    m_Data.clear();
    m_DataMin = 0.0f;
    m_DataMax = 0.0f;
    m_DataSum = 0.0f;
    m_HoveredBarIndex = -1;
}

void ImGuiBarGraph::UpdateDataRange()
{
    if (m_Data.empty())
    {
        m_DataMin = m_DataMax = m_DataSum = 0.0f;
        return;
    }
    
    m_DataMin = m_DataMax = m_Data[0].value;
    m_DataSum = 0.0f;
    
    for (const auto& bar : m_Data)
    {
        m_DataMin = std::min(m_DataMin, bar.value);
        m_DataMax = std::max(m_DataMax, bar.value);
        m_DataSum += bar.value;
    }
    
    // Ensure we have some range
    if (std::abs(m_DataMax - m_DataMin) < 1e-6f)
    {
        m_DataMax = m_DataMin + 1.0f;
    }
}

bool ImGuiBarGraph::Render(const char* id)
{
    if (m_Data.empty())
    {
        ImGui::Text("No data to display");
        return false;
    }
    
    // Calculate canvas size
    ImVec2 canvasSize = m_Config.size;
    canvasSize.x = ImGui::GetContentRegionAvail().x;
    canvasSize.y = ImGui::GetContentRegionAvail().y;
    
    // Reserve space for title
    float titleHeight = m_Config.title.empty() ? 0.0f : ImGui::GetTextLineHeight() + 5.0f;
    
    // Reserve space for labels
    float labelHeight = m_Config.showLabels ? ImGui::GetTextLineHeight() + 5.0f : 0.0f;
    
    // Reserve space for Y-axis labels if grid is shown
    float yAxisLabelWidth = 0.0f;
    if (m_Config.showGrid)
    {
        // Calculate the maximum width needed for Y-axis labels
        float dataRange = m_Config.autoScale ? (m_DataMax - m_DataMin) : (m_Config.maxValue - m_Config.minValue);
        float minVal = m_Config.autoScale ? m_DataMin : m_Config.minValue;
        
        const int gridLines = 5;
        float maxLabelWidth = 0.0f;
        
        for (int i = 1; i < gridLines; ++i)
        {
            float gridValue = minVal + dataRange * (1.0f - static_cast<float>(i) / gridLines);
            std::string valueStr = FormatValue(gridValue);
            ImVec2 textSize = ImGui::CalcTextSize(valueStr.c_str());
            maxLabelWidth = std::max(maxLabelWidth, textSize.x);
        }
        
        yAxisLabelWidth = maxLabelWidth + 10.0f; // Add some padding
    }
    
    // Adjust canvas size to account for title, labels, and Y-axis labels
    canvasSize.x = std::max(canvasSize.x - yAxisLabelWidth, 100.0f);
    canvasSize.y = std::max(canvasSize.y - titleHeight - labelHeight, 50.0f); // Ensure minimum height
    
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    canvasPos.x += yAxisLabelWidth; // Offset by Y-axis label width
    canvasPos.y += titleHeight;
    
    ImVec2 canvasMin = canvasPos;
    ImVec2 canvasMax = ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y);
    
    // Create invisible button for interaction
    ImGui::SetCursorScreenPos(ImVec2(canvasMin.x - yAxisLabelWidth, canvasMin.y - titleHeight));
    ImGui::InvisibleButton(id, ImVec2(canvasSize.x + yAxisLabelWidth, canvasSize.y + titleHeight + labelHeight));
    bool isHovered = ImGui::IsItemHovered();
    
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    
    // Render components
    RenderTitle(drawList, canvasMin, canvasMax);
    RenderBackground(drawList, canvasMin, canvasMax);
    if (m_Config.showGrid)
    {
        RenderGrid(drawList, canvasMin, canvasMax);
    }
    RenderBars(drawList, canvasMin, canvasMax);
    if (m_Config.showLabels)
    {
        RenderLabels(drawList, canvasMin, canvasMax);
    }
    
    // Handle interaction
    if (isHovered)
    {
        HandleInteraction(canvasMin, canvasMax);
    }
    else
    {
        m_HoveredBarIndex = -1;
    }
    
    // Show tooltip for hovered bar
    if (m_HoveredBarIndex >= 0 && m_HoveredBarIndex < static_cast<int>(m_Data.size()))
    {
        const auto& bar = m_Data[m_HoveredBarIndex];
        float percentage = m_DataSum > 0 ? (bar.value / m_DataSum) * 100.0f : 0.0f;
        
        ImGui::BeginTooltip();
        ImGui::Text("Label: %s", bar.label.c_str());
        ImGui::Text("Value: %s", FormatValue(bar.value).c_str());
        ImGui::Text("Percentage: %.1f%%", percentage);
        ImGui::Text("Index: %d", m_HoveredBarIndex);
        ImGui::EndTooltip();
        
        // Advance cursor to account for our custom drawing
        ImGui::SetCursorScreenPos(ImVec2(canvasMin.x - yAxisLabelWidth, canvasMax.y + labelHeight));
        return true;
    }
    
    // Advance cursor to account for our custom drawing
    ImGui::SetCursorScreenPos(ImVec2(canvasMin.x, canvasMax.y + labelHeight));
    return false;
}

ImU32 ImGuiBarGraph::GetBarColor(size_t index, float value, float normalizedValue) const
{
    if (m_ColorFunction)
    {
        return m_ColorFunction(index, value, normalizedValue);
    }
    
    // Default color based on value intensity
    float intensity = std::clamp(normalizedValue, 0.0f, 1.0f);
    uint8_t r = static_cast<uint8_t>(100 + intensity * 155);
    uint8_t g = static_cast<uint8_t>(150 + intensity * 105);
    uint8_t b = static_cast<uint8_t>(255);
    
    return IM_COL32(r, g, b, 255);
}

void ImGuiBarGraph::RenderBackground(ImDrawList* drawList, const ImVec2& canvasMin, const ImVec2& canvasMax) const
{
    drawList->AddRectFilled(canvasMin, canvasMax, m_Config.backgroundColor);
    drawList->AddRect(canvasMin, canvasMax, m_Config.gridColor);
}

void ImGuiBarGraph::RenderGrid(ImDrawList* drawList, const ImVec2& canvasMin, const ImVec2& canvasMax) const
{
    const int gridLines = 5;
    float canvasHeight = canvasMax.y - canvasMin.y;
    
    for (int i = 1; i < gridLines; ++i)
    {
        float y = canvasMin.y + (canvasHeight * i) / gridLines;
        drawList->AddLine(ImVec2(canvasMin.x, y), ImVec2(canvasMax.x, y), m_Config.gridColor);
        
        // Add value labels on the left
        float dataRange = m_Config.autoScale ? (m_DataMax - m_DataMin) : (m_Config.maxValue - m_Config.minValue);
        float minVal = m_Config.autoScale ? m_DataMin : m_Config.minValue;
        float gridValue = minVal + dataRange * (1.0f - static_cast<float>(i) / gridLines);
        
        std::string valueStr = FormatValue(gridValue);
        ImVec2 textSize = ImGui::CalcTextSize(valueStr.c_str());
        ImVec2 textPos(canvasMin.x - textSize.x - 5.0f, y - textSize.y * 0.5f);
        
        if (textPos.x >= 0) // Only draw if there's space
        {
            drawList->AddText(textPos, m_Config.textColor, valueStr.c_str());
        }
    }
}

void ImGuiBarGraph::RenderBars(ImDrawList* drawList, const ImVec2& canvasMin, const ImVec2& canvasMax)
{
    if (m_Data.empty()) return;
    
    float canvasWidth = canvasMax.x - canvasMin.x;
    float canvasHeight = canvasMax.y - canvasMin.y;
    
    // Calculate bar dimensions
    float totalSpacing = m_Config.barSpacing * (m_Data.size() - 1);
    float availableWidth = canvasWidth - totalSpacing;
    float barWidth = std::clamp(availableWidth / m_Data.size(), m_Config.minBarWidth, m_Config.maxBarWidth);
    
    // If bars don't fit, adjust spacing
    if (barWidth * m_Data.size() + totalSpacing > canvasWidth)
    {
        barWidth = m_Config.minBarWidth;
        totalSpacing = canvasWidth - (barWidth * m_Data.size());
        if (totalSpacing < 0) totalSpacing = 0;
    }
    
    float spacing = m_Data.size() > 1 ? totalSpacing / (m_Data.size() - 1) : 0;
    
    // Calculate value range
    float dataRange = m_Config.autoScale ? (m_DataMax - m_DataMin) : (m_Config.maxValue - m_Config.minValue);
    float minVal = m_Config.autoScale ? m_DataMin : m_Config.minValue;
    
    for (size_t i = 0; i < m_Data.size(); ++i)
    {
        const auto& bar = m_Data[i];
        
        // Calculate bar position and height
        float x = canvasMin.x + i * (barWidth + spacing);
        float normalizedValue = (bar.value - minVal) / dataRange;
        normalizedValue = std::clamp(normalizedValue, 0.0f, 1.0f);
        float barHeight = normalizedValue * canvasHeight;
        
        // Bar rectangle (bottom-up)
        ImVec2 barMin(x, canvasMax.y - barHeight);
        ImVec2 barMax(x + barWidth, canvasMax.y);
        
        // Get bar color        
        ImU32 barColor = GetBarColor(i, bar.value, normalizedValue);
        
        // Highlight if hovered
        if (static_cast<int>(i) == m_HoveredBarIndex)
        {
            // Lighten the color for hover effect
            ImVec4 color = ImGui::ColorConvertU32ToFloat4(barColor);
            color.x = std::min(color.x * 1.3f, 1.0f);
            color.y = std::min(color.y * 1.3f, 1.0f);
            color.z = std::min(color.z * 1.3f, 1.0f);
            barColor = ImGui::ColorConvertFloat4ToU32(color);
        }
        
        // Draw bar
        drawList->AddRectFilled(barMin, barMax, barColor);
        drawList->AddRect(barMin, barMax, IM_COL32(255, 255, 255, 100));
        
        // Draw value/percentage on top of bar if enabled and there's space
        bool shouldShowText = (m_Config.showValues || m_Config.valueDisplayMode != ValueDisplayMode::None) && 
                             m_Config.valueDisplayMode != ValueDisplayMode::None && barHeight > 1.0f;
        
        if (shouldShowText)
        {
            std::string displayText = GetDisplayText(bar.value);
            ImVec2 textSize = ImGui::CalcTextSize(displayText.c_str());
            
            if (textSize.x <= barWidth - 4.0f)
            {
                ImVec2 textPos(x + (barWidth - textSize.x) * 0.5f, barMin.y - textSize.y - 2.0f);
                drawList->AddText(textPos, m_Config.textColor, displayText.c_str());
            }
        }
    }
}

void ImGuiBarGraph::RenderLabels(ImDrawList* drawList, const ImVec2& canvasMin, const ImVec2& canvasMax) const
{
    if (m_Data.empty()) return;
    
    float canvasWidth = canvasMax.x - canvasMin.x;
    float totalSpacing = m_Config.barSpacing * (m_Data.size() - 1);
    float availableWidth = canvasWidth - totalSpacing;
    float barWidth = std::clamp(availableWidth / m_Data.size(), m_Config.minBarWidth, m_Config.maxBarWidth);
    
    if (barWidth * m_Data.size() + totalSpacing > canvasWidth)
    {
        barWidth = m_Config.minBarWidth;
        totalSpacing = canvasWidth - (barWidth * m_Data.size());
        if (totalSpacing < 0) totalSpacing = 0;
    }
    
    float spacing = m_Data.size() > 1 ? totalSpacing / (m_Data.size() - 1) : 0;
    
    for (size_t i = 0; i < m_Data.size(); ++i)
    {
        const auto& bar = m_Data[i];
        
        float x = canvasMin.x + i * (barWidth + spacing);
        
        std::string label = bar.label;
        ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
        
        // Truncate label if too long
        while (textSize.x > barWidth && label.length() > 3)
        {
            label = label.substr(0, label.length() - 4) + "...";
            textSize = ImGui::CalcTextSize(label.c_str());
        }
        
        ImVec2 textPos(x + (barWidth - textSize.x) * 0.5f, canvasMax.y + 2.0f);
        drawList->AddText(textPos, m_Config.textColor, label.c_str());
    }
}

void ImGuiBarGraph::RenderTitle(ImDrawList* drawList, const ImVec2& canvasMin, const ImVec2& canvasMax) const
{
    if (m_Config.title.empty()) return;
    
    ImVec2 textSize = ImGui::CalcTextSize(m_Config.title.c_str());
    float canvasWidth = canvasMax.x - canvasMin.x;
    ImVec2 textPos(canvasMin.x + (canvasWidth - textSize.x) * 0.5f, canvasMin.y - textSize.y - 5.0f);
    
    drawList->AddText(textPos, m_Config.textColor, m_Config.title.c_str());
}

void ImGuiBarGraph::HandleInteraction(const ImVec2& canvasMin, const ImVec2& canvasMax)
{
    ImVec2 mousePos = ImGui::GetIO().MousePos;
    
    if (mousePos.y < canvasMin.y || mousePos.y > canvasMax.y)
    {
        m_HoveredBarIndex = -1;
        return;
    }
    
    float canvasWidth = canvasMax.x - canvasMin.x;
    float totalSpacing = m_Config.barSpacing * (m_Data.size() - 1);
    float availableWidth = canvasWidth - totalSpacing;
    float barWidth = std::clamp(availableWidth / m_Data.size(), m_Config.minBarWidth, m_Config.maxBarWidth);
    
    if (barWidth * m_Data.size() + totalSpacing > canvasWidth)
    {
        barWidth = m_Config.minBarWidth;
        totalSpacing = canvasWidth - (barWidth * m_Data.size());
        if (totalSpacing < 0) totalSpacing = 0;
    }
    
    float spacing = m_Data.size() > 1 ? totalSpacing / (m_Data.size() - 1) : 0;
    
    for (size_t i = 0; i < m_Data.size(); ++i)
    {
        float x = canvasMin.x + i * (barWidth + spacing);
        
        if (mousePos.x >= x && mousePos.x <= x + barWidth)
        {
            m_HoveredBarIndex = static_cast<int>(i);
            return;
        }
    }
    
    m_HoveredBarIndex = -1;
}

std::string ImGuiBarGraph::FormatValue(float value) const
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << value;
    return oss.str();
}

std::string ImGuiBarGraph::FormatPercentage(float value, float total) const
{
    if (total <= 0.0f) return "0.0%";
    
    float percentage = (value / total) * 100.0f;
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << percentage << "%";
    return oss.str();
}

std::string ImGuiBarGraph::GetDisplayText(float value) const
{
    switch (m_Config.valueDisplayMode)
    {
        case ValueDisplayMode::None:
            return "";
            
        case ValueDisplayMode::Values:
            return FormatValue(value);
            
        case ValueDisplayMode::Percentages:
            return FormatPercentage(value, m_DataSum);
            
        case ValueDisplayMode::Both:
            return FormatValue(value) + "\n" + FormatPercentage(value, m_DataSum);
            
        default:
            // Fallback to old behavior for backward compatibility
            return m_Config.showValues ? FormatValue(value) : "";
    }
}