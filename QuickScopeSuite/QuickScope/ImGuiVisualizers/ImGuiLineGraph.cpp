#include "ImGuiLineGraph.h"
#include <cmath>

void ImGuiLineGraph::SetData(const std::vector<PointData>& data)
{
	m_Data = data;
	UpdateDataRange();
}

void ImGuiLineGraph::SetData(const std::vector<float>& xValues, const std::vector<float>& yValues)
{
	m_Data.clear();
	size_t count = std::min(xValues.size(), yValues.size());
	m_Data.reserve(count);
	for (size_t i = 0; i < count; ++i)
	{
		m_Data.emplace_back(xValues[i], yValues[i], "Point " + std::to_string(i));
	}
	UpdateDataRange();
}

void ImGuiLineGraph::SetData(const std::vector<float>& xValues, const std::vector<float>& yValues, const std::vector<std::string>& labels)
{
	m_Data.clear();
	size_t count = std::min({ xValues.size(), yValues.size(), labels.size() });
	m_Data.reserve(count);
	for (size_t i = 0; i < count; ++i)
	{
		m_Data.emplace_back(xValues[i], yValues[i], labels[i]);
	}
	UpdateDataRange();
}

void ImGuiLineGraph::SetData(const std::vector<float>& yValues)
{
	m_Data.clear();
	m_Data.reserve(yValues.size());
	for (size_t i = 0; i < yValues.size(); ++i)
	{
		m_Data.emplace_back(static_cast<float>(i), yValues[i], "Point " + std::to_string(i));
	}
	UpdateDataRange();
}

void ImGuiLineGraph::AddPoint(float x, float y, const std::string& label)
{
	m_Data.emplace_back(x, y, label.empty() ? ("Point " + std::to_string(m_Data.size())) : label);
	UpdateDataRange();
}

void ImGuiLineGraph::Clear()
{
	m_Data.clear();
	m_DataMinX = m_DataMaxX = m_DataMinY = m_DataMaxY = 0.0f;
	m_HoveredPointIndex = -1;
}

void ImGuiLineGraph::UpdateDataRange()
{
	if (m_Data.empty())
	{
		m_DataMinX = m_DataMaxX = m_DataMinY = m_DataMaxY = 0.0f;
		return;
	}

	m_DataMinX = m_DataMaxX = m_Data[0].x;
	m_DataMinY = m_DataMaxY = m_Data[0].y;

	for (const auto& point : m_Data)
	{
		m_DataMinX = std::min(m_DataMinX, point.x);
		m_DataMaxX = std::max(m_DataMaxX, point.x);
		m_DataMinY = std::min(m_DataMinY, point.y);
		m_DataMaxY = std::max(m_DataMaxY, point.y);
	}

	// Ensure we have some range
	if (std::abs(m_DataMaxX - m_DataMinX) < 1e-6f)
	{
		m_DataMaxX = m_DataMinX + 1.0f;
	}
	if (std::abs(m_DataMaxY - m_DataMinY) < 1e-6f)
	{
		m_DataMaxY = m_DataMinY + 1.0f;
	}
}

bool ImGuiLineGraph::Render(const char* id)
{
	if (m_Data.empty())
	{
		ImGui::Text("No data to display");
		return false;
	}

	// Calculate canvas size
	ImVec2 canvasSize = m_Config.size;
	if (canvasSize.x <= 0)
		canvasSize.x = ImGui::GetContentRegionAvail().x;
	if (canvasSize.y <= 0)
		canvasSize.y = ImGui::GetContentRegionAvail().y;

	// Reserve space for title and axis labels
	float titleHeight = m_Config.title.empty() ? 0.0f : ImGui::GetTextLineHeight() + 5.0f;
	float xLabelHeight = m_Config.xAxisLabel.empty() ? 0.0f : ImGui::GetTextLineHeight() + 5.0f;
	
	// Calculate required space for Y-axis title and markers
	float yLabelWidth = 0.0f;
	float yAxisMarkerSpace = 0.0f;
	
	if (!m_Config.yAxisLabel.empty())
	{
		yLabelWidth = ImGui::CalcTextSize(m_Config.yAxisLabel.c_str()).x + 10.0f; // Add padding
	}
	
	if (m_Config.showYLabels)
	{
		// Calculate the maximum width needed for Y-axis markers
		float rangeY = GetRangeY();
		float minY = GetMinY();
		float maxYMarkerWidth = 0.0f;
		
		for (int i = 0; i <= m_Config.gridCountY; ++i)
		{
			float normalizedY = 1.0f - static_cast<float>(i) / m_Config.gridCountY;
			float dataY = minY + normalizedY * rangeY;
			std::string valueStr = FormatYValue(dataY);
			float markerWidth = ImGui::CalcTextSize(valueStr.c_str()).x;
			maxYMarkerWidth = std::max(maxYMarkerWidth, markerWidth);
		}
		yAxisMarkerSpace = maxYMarkerWidth + 10.0f; // Add padding
	}
	
	float axisLabelSpace = (m_Config.showXLabels || m_Config.showYLabels) ? 25.0f : 0.0f;

	// Adjust canvas size
	canvasSize.x = std::max(canvasSize.x - yLabelWidth - yAxisMarkerSpace - axisLabelSpace, 100.0f);
	canvasSize.y = std::max(canvasSize.y - titleHeight - xLabelHeight - axisLabelSpace, 50.0f);

	ImVec2 canvasPos = ImGui::GetCursorScreenPos();
	canvasPos.x += yLabelWidth + yAxisMarkerSpace + axisLabelSpace;
	canvasPos.y += titleHeight;

	ImVec2 canvasMin = canvasPos;
	ImVec2 canvasMax = ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y);

	// Create invisible button for interaction
	ImGui::SetCursorScreenPos(ImVec2(canvasMin.x - yLabelWidth - yAxisMarkerSpace - axisLabelSpace, canvasMin.y - titleHeight));
	ImGui::InvisibleButton(id, ImVec2(canvasSize.x + yLabelWidth + yAxisMarkerSpace + axisLabelSpace, canvasSize.y + titleHeight + xLabelHeight + axisLabelSpace));
	bool isHovered = ImGui::IsItemHovered();

	ImDrawList* drawList = ImGui::GetWindowDrawList();

	// Render components
	RenderTitle(drawList, canvasMin, canvasMax);
	RenderBackground(drawList, canvasMin, canvasMax);
	if (m_Config.showGrid)
	{
		RenderGrid(drawList, canvasMin, canvasMax);
	}
	RenderAxes(drawList, canvasMin, canvasMax);
	RenderLine(drawList, canvasMin, canvasMax);
	if (m_Config.showPoints)
	{
		RenderPoints(drawList, canvasMin, canvasMax);
	}

	// Handle interaction
	if (isHovered)
	{
		HandleInteraction(canvasMin, canvasMax);
	}
	else
	{
		m_HoveredPointIndex = -1;
	}

	// Show tooltip for hovered point
	if (m_HoveredPointIndex >= 0 && m_HoveredPointIndex < static_cast<int>(m_Data.size()))
	{
		const auto& point = m_Data[m_HoveredPointIndex];

		ImGui::BeginTooltip();
		ImGui::Text("Label: %s", point.label.c_str());
		ImGui::Text("X: %s", FormatXValue(point.x).c_str());
		ImGui::Text("Y: %s", FormatYValue(point.y).c_str());
		ImGui::Text("Index: %d", m_HoveredPointIndex);
		ImGui::EndTooltip();

		// Advance cursor
		ImVec2 finalPos(canvasMin.x - yLabelWidth - yAxisMarkerSpace - axisLabelSpace, canvasMax.y + xLabelHeight + axisLabelSpace);
		ImGui::SetCursorScreenPos(finalPos);
		return true;
	}

	// Advance cursor
	ImVec2 finalPos(canvasMin.x - yLabelWidth - yAxisMarkerSpace - axisLabelSpace, canvasMax.y + xLabelHeight + axisLabelSpace);
	ImGui::SetCursorScreenPos(finalPos);
	return false;
}

ImU32 ImGuiLineGraph::GetLineColor(size_t index, float x, float y, float normalizedX, float normalizedY) const
{
	if (m_ColorFunction)
	{
		return m_ColorFunction(index, x, y, normalizedX, normalizedY);
	}
	return m_Config.lineColor;
}

ImU32 ImGuiLineGraph::GetPointColor(size_t index, float x, float y, float normalizedX, float normalizedY) const
{
	if (m_ColorFunction)
	{
		return m_ColorFunction(index, x, y, normalizedX, normalizedY);
	}

	if (static_cast<int>(index) == m_HoveredPointIndex)
	{
		return m_Config.hoveredPointColor;
	}

	return m_Config.pointColor;
}

void ImGuiLineGraph::RenderBackground(ImDrawList* drawList, const ImVec2& canvasMin, const ImVec2& canvasMax) const
{
	drawList->AddRectFilled(canvasMin, canvasMax, m_Config.backgroundColor);
	drawList->AddRect(canvasMin, canvasMax, m_Config.gridColor);
}

void ImGuiLineGraph::RenderGrid(ImDrawList* drawList, const ImVec2& canvasMin, const ImVec2& canvasMax) const
{
	float canvasWidth = canvasMax.x - canvasMin.x;
	float canvasHeight = canvasMax.y - canvasMin.y;

	// Vertical grid lines
	for (int i = 1; i < m_Config.gridCountX; ++i)
	{
		float x = canvasMin.x + (canvasWidth * i) / m_Config.gridCountX;
		drawList->AddLine(ImVec2(x, canvasMin.y), ImVec2(x, canvasMax.y), m_Config.gridColor);
	}

	// Horizontal grid lines
	for (int i = 1; i < m_Config.gridCountY; ++i)
	{
		float y = canvasMin.y + (canvasHeight * i) / m_Config.gridCountY;
		drawList->AddLine(ImVec2(canvasMin.x, y), ImVec2(canvasMax.x, y), m_Config.gridColor);
	}
}

void ImGuiLineGraph::RenderAxes(ImDrawList* drawList, const ImVec2& canvasMin, const ImVec2& canvasMax) const
{
	if (m_Config.showXLabels)
	{
		float rangeX = GetRangeX();
		float minX = GetMinX();

		for (int i = 0; i <= m_Config.gridCountX; ++i)
		{
			float normalizedX = static_cast<float>(i) / m_Config.gridCountX;
			float x = canvasMin.x + normalizedX * (canvasMax.x - canvasMin.x);
			float dataX = minX + normalizedX * rangeX;

			std::string valueStr = FormatXValue(dataX);
			ImVec2 textSize = ImGui::CalcTextSize(valueStr.c_str());
			ImVec2 textPos(x - textSize.x * 0.5f, canvasMax.y + 2.0f);

			drawList->AddText(textPos, m_Config.textColor, valueStr.c_str());
		}

		// X-axis label
		if (!m_Config.xAxisLabel.empty())
		{
			ImVec2 textSize = ImGui::CalcTextSize(m_Config.xAxisLabel.c_str());
			ImVec2 textPos((canvasMin.x + canvasMax.x - textSize.x) * 0.5f, canvasMax.y + 25.0f);
			drawList->AddText(textPos, m_Config.textColor, m_Config.xAxisLabel.c_str());
		}
	}

	if (m_Config.showYLabels)
	{
		float rangeY = GetRangeY();
		float minY = GetMinY();

		for (int i = 0; i <= m_Config.gridCountY; ++i)
		{
			float normalizedY = 1.0f - static_cast<float>(i) / m_Config.gridCountY; // Invert Y
			float y = canvasMin.y + (1.0f - normalizedY) * (canvasMax.y - canvasMin.y);
			float dataY = minY + normalizedY * rangeY;

			std::string valueStr = FormatYValue(dataY);
			ImVec2 textSize = ImGui::CalcTextSize(valueStr.c_str());
			ImVec2 textPos(canvasMin.x - textSize.x - 5.0f, y - textSize.y * 0.5f);

			if (textPos.x >= 0)
			{
				drawList->AddText(textPos, m_Config.textColor, valueStr.c_str());
			}
		}

		// Y-axis label - positioned to the left of the markers
		if (!m_Config.yAxisLabel.empty())
		{
			// Calculate the maximum width of Y-axis markers to position the title properly
			float maxMarkerWidth = 0.0f;
			float rangeY = GetRangeY();
			float minY = GetMinY();
			
			for (int i = 0; i <= m_Config.gridCountY; ++i)
			{
				float normalizedY = 1.0f - static_cast<float>(i) / m_Config.gridCountY;
				float dataY = minY + normalizedY * rangeY;
				std::string valueStr = FormatYValue(dataY);
				float markerWidth = ImGui::CalcTextSize(valueStr.c_str()).x;
				maxMarkerWidth = std::max(maxMarkerWidth, markerWidth);
			}
			
			ImVec2 textSize = ImGui::CalcTextSize(m_Config.yAxisLabel.c_str());
			// Position the Y-axis label to the left of the markers with proper spacing
			ImVec2 textPos(canvasMin.x - maxMarkerWidth - textSize.x - 15.0f, (canvasMin.y + canvasMax.y - textSize.y) * 0.5f);
			drawList->AddText(textPos, m_Config.textColor, m_Config.yAxisLabel.c_str());
		}
	}
}

void ImGuiLineGraph::RenderLine(ImDrawList* drawList, const ImVec2& canvasMin, const ImVec2& canvasMax)
{
	if (m_Data.size() < 2) return;

	for (size_t i = 0; i < m_Data.size() - 1; ++i)
	{
		const auto& point1 = m_Data[i];
		const auto& point2 = m_Data[i + 1];

		ImVec2 canvasPos1 = DataToCanvas(point1.x, point1.y, canvasMin, canvasMax);
		ImVec2 canvasPos2 = DataToCanvas(point2.x, point2.y, canvasMin, canvasMax);

		float rangeX = GetRangeX();
		float rangeY = GetRangeY();
		float normalizedX1 = rangeX > 0 ? (point1.x - GetMinX()) / rangeX : 0.0f;
		float normalizedY1 = rangeY > 0 ? (point1.y - GetMinY()) / rangeY : 0.0f;

		ImU32 lineColor = GetLineColor(i, point1.x, point1.y, normalizedX1, normalizedY1);

		switch (m_Config.lineStyle)
		{
		case LineStyle::Solid:
			drawList->AddLine(canvasPos1, canvasPos2, lineColor, m_Config.lineThickness);
			break;
		case LineStyle::Dashed:
			DrawDashedLine(drawList, canvasPos1, canvasPos2, lineColor, m_Config.lineThickness);
			break;
		case LineStyle::Dotted:
			DrawDottedLine(drawList, canvasPos1, canvasPos2, lineColor, m_Config.lineThickness);
			break;
		}
	}
}

void ImGuiLineGraph::RenderPoints(ImDrawList* drawList, const ImVec2& canvasMin, const ImVec2& canvasMax)
{
	float rangeX = GetRangeX();
	float rangeY = GetRangeY();

	for (size_t i = 0; i < m_Data.size(); ++i)
	{
		const auto& point = m_Data[i];
		ImVec2 canvasPos = DataToCanvas(point.x, point.y, canvasMin, canvasMax);

		float normalizedX = rangeX > 0 ? (point.x - GetMinX()) / rangeX : 0.0f;
		float normalizedY = rangeY > 0 ? (point.y - GetMinY()) / rangeY : 0.0f;

		ImU32 pointColor = GetPointColor(i, point.x, point.y, normalizedX, normalizedY);

		DrawPoint(drawList, canvasPos, m_Config.pointRadius, pointColor, m_Config.pointStyle);
	}
}

void ImGuiLineGraph::RenderTitle(ImDrawList* drawList, const ImVec2& canvasMin, const ImVec2& canvasMax) const
{
	if (m_Config.title.empty()) return;

	ImVec2 textSize = ImGui::CalcTextSize(m_Config.title.c_str());
	float canvasWidth = canvasMax.x - canvasMin.x;
	ImVec2 textPos(canvasMin.x + (canvasWidth - textSize.x) * 0.5f, canvasMin.y - textSize.y - 5.0f);

	drawList->AddText(textPos, m_Config.textColor, m_Config.title.c_str());
}

void ImGuiLineGraph::HandleInteraction(const ImVec2& canvasMin, const ImVec2& canvasMax)
{
	ImVec2 mousePos = ImGui::GetIO().MousePos;

	if (mousePos.x < canvasMin.x || mousePos.x > canvasMax.x ||
		mousePos.y < canvasMin.y || mousePos.y > canvasMax.y)
	{
		m_HoveredPointIndex = -1;
		return;
	}

	float minDistance = FLT_MAX;
	int closestPoint = -1;

	for (size_t i = 0; i < m_Data.size(); ++i)
	{
		const auto& point = m_Data[i];
		ImVec2 canvasPos = DataToCanvas(point.x, point.y, canvasMin, canvasMax);

		float distance = (float)std::sqrt(std::pow(mousePos.x - canvasPos.x, 2) + std::pow(mousePos.y - canvasPos.y, 2));

		if (distance < minDistance && distance <= m_Config.pointRadius + 5.0f)
		{
			minDistance = distance;
			closestPoint = static_cast<int>(i);
		}
	}

	m_HoveredPointIndex = closestPoint;
}

ImVec2 ImGuiLineGraph::DataToCanvas(float x, float y, const ImVec2& canvasMin, const ImVec2& canvasMax) const
{
	float rangeX = GetRangeX();
	float rangeY = GetRangeY();
	float minX = GetMinX();
	float minY = GetMinY();

	float normalizedX = rangeX > 0 ? (x - minX) / rangeX : 0.0f;
	float normalizedY = rangeY > 0 ? (y - minY) / rangeY : 0.0f;

	// Invert Y coordinate (canvas Y grows downward, but we want data Y to grow upward)
	normalizedY = 1.0f - normalizedY;

	return ImVec2(
		canvasMin.x + normalizedX * (canvasMax.x - canvasMin.x),
		canvasMin.y + normalizedY * (canvasMax.y - canvasMin.y)
	);
}

void ImGuiLineGraph::DrawPoint(ImDrawList* drawList, const ImVec2& center, float radius, ImU32 color, PointStyle style) const
{
	switch (style)
	{
	case PointStyle::Circle:
		drawList->AddCircleFilled(center, radius, color);
		drawList->AddCircle(center, radius, IM_COL32(255, 255, 255, 128));
		break;
	case PointStyle::Square:
	{
		ImVec2 min(center.x - radius, center.y - radius);
		ImVec2 max(center.x + radius, center.y + radius);
		drawList->AddRectFilled(min, max, color);
		drawList->AddRect(min, max, IM_COL32(255, 255, 255, 128));
		break;
	}
	case PointStyle::Diamond:
	{
		ImVec2 points[4] = {
			ImVec2(center.x, center.y - radius),      // Top
			ImVec2(center.x + radius, center.y),      // Right
			ImVec2(center.x, center.y + radius),      // Bottom
			ImVec2(center.x - radius, center.y)       // Left
		};
		drawList->AddConvexPolyFilled(points, 4, color);
		drawList->AddPolyline(points, 4, IM_COL32(255, 255, 255, 128), true, 1.0f);
		break;
	}
	case PointStyle::None:
	default:
		break;
	}
}

void ImGuiLineGraph::DrawDashedLine(ImDrawList* drawList, const ImVec2& p1, const ImVec2& p2, ImU32 color, float thickness, float dashLength) const
{
	ImVec2 dir = ImVec2(p2.x - p1.x, p2.y - p1.y);
	float length = std::sqrt(dir.x * dir.x + dir.y * dir.y);

	if (length < 1e-6f) return;

	dir.x /= length;
	dir.y /= length;

	float currentLength = 0.0f;
	bool drawing = true;

	while (currentLength < length)
	{
		float segmentLength = std::min(dashLength, length - currentLength);
		ImVec2 start = ImVec2(p1.x + dir.x * currentLength, p1.y + dir.y * currentLength);
		ImVec2 end = ImVec2(start.x + dir.x * segmentLength, start.y + dir.y * segmentLength);

		if (drawing)
		{
			drawList->AddLine(start, end, color, thickness);
		}

		currentLength += segmentLength;
		drawing = !drawing;
	}
}

void ImGuiLineGraph::DrawDottedLine(ImDrawList* drawList, const ImVec2& p1, const ImVec2& p2, ImU32 color, float thickness, float dotSpacing) const
{
	ImVec2 dir = ImVec2(p2.x - p1.x, p2.y - p1.y);
	float length = std::sqrt(dir.x * dir.x + dir.y * dir.y);

	if (length < 1e-6f) return;

	dir.x /= length;
	dir.y /= length;

	int dotCount = static_cast<int>(length / dotSpacing) + 1;

	for (int i = 0; i <= dotCount; ++i)
	{
		float t = static_cast<float>(i) / dotCount;
		if (t > 1.0f) t = 1.0f;

		ImVec2 pos = ImVec2(p1.x + dir.x * length * t, p1.y + dir.y * length * t);
		drawList->AddCircleFilled(pos, thickness * 0.5f, color);
	}
}

std::string ImGuiLineGraph::FormatValue(float value, int precision) const
{
	if (precision < 0) precision = 2; // Default precision

	std::ostringstream oss;
	oss << std::fixed << std::setprecision(precision) << value;
	return oss.str();
}

std::string ImGuiLineGraph::FormatXValue(float value) const
{
	return FormatValue(value, m_Config.xLabelPrecision);
}

std::string ImGuiLineGraph::FormatYValue(float value) const
{
	return FormatValue(value, m_Config.yLabelPrecision);
}

float ImGuiLineGraph::GetRangeX() const
{
	return m_Config.autoScaleX ? (m_DataMaxX - m_DataMinX) : (m_Config.maxX - m_Config.minX);
}

float ImGuiLineGraph::GetRangeY() const
{
	return m_Config.autoScaleY ? (m_DataMaxY - m_DataMinY) : (m_Config.maxY - m_Config.minY);
}

float ImGuiLineGraph::GetMinX() const
{
	return m_Config.autoScaleX ? m_DataMinX : m_Config.minX;
}

float ImGuiLineGraph::GetMaxX() const
{
	return m_Config.autoScaleX ? m_DataMaxX : m_Config.maxX;
}

float ImGuiLineGraph::GetMinY() const
{
	return m_Config.autoScaleY ? m_DataMinY : m_Config.minY;
}

float ImGuiLineGraph::GetMaxY() const
{
	return m_Config.autoScaleY ? m_DataMaxY : m_Config.maxY;
}