#pragma once

#include "imgui.h"
#include <vector>
#include <string>
#include <functional>
#include <algorithm>
#include <iomanip>
#include <sstream>

class ImGuiLineGraph
{
public:
	// Point data structure
	struct PointData
	{
		float x;
		float y;
		std::string label;

		PointData(float xVal, float yVal, const std::string& lbl = "")
			: x(xVal), y(yVal), label(lbl) {
		}
	};

	// Line style options
	enum class LineStyle
	{
		Solid,          // Solid line
		Dotted,         // Dotted line
		Dashed          // Dashed line
	};

	// Point style options
	enum class PointStyle
	{
		None,           // No points drawn
		Circle,         // Circular points
		Square,         // Square points
		Diamond         // Diamond points
	};

	// Color function signature: takes (index, x, y, normalized_x, normalized_y) and returns ImU32 color
	using ColorFunction = std::function<ImU32(size_t index, float x, float y, float normalizedX, float normalizedY)>;

	// Configuration options
	struct Config
	{
		ImVec2 size = ImVec2(0, 200);              // Graph size (0 = auto-width)
		float lineThickness = 2.0f;                // Line thickness
		bool showPoints = true;                     // Show data points
		float pointRadius = 3.0f;                  // Radius of data points
		PointStyle pointStyle = PointStyle::Circle; // Style of data points
		LineStyle lineStyle = LineStyle::Solid;    // Style of connecting lines
		bool showGrid = true;                       // Show grid lines
		bool showXLabels = true;                    // Show X-axis labels
		bool showYLabels = true;                    // Show Y-axis labels
		bool autoScaleX = true;                     // Auto-scale X axis to data range
		bool autoScaleY = true;                     // Auto-scale Y axis to data range
		float minX = 0.0f;                          // Manual min X value (if !autoScaleX)
		float maxX = 100.0f;                        // Manual max X value (if !autoScaleX)
		float minY = 0.0f;                          // Manual min Y value (if !autoScaleY)
		float maxY = 100.0f;                        // Manual max Y value (if !autoScaleY)
		int gridCountX = 5;                         // Number of vertical grid lines
		int gridCountY = 5;                         // Number of horizontal grid lines
		int xLabelPrecision = 2;                    // Number of decimal places for X-axis labels
		int yLabelPrecision = 2;                    // Number of decimal places for Y-axis labels
		std::string title;                          // Graph title
		std::string xAxisLabel;                     // X-axis label
		std::string yAxisLabel;                     // Y-axis label
		ImU32 backgroundColor = IM_COL32(40, 40, 40, 255);
		ImU32 gridColor = IM_COL32(80, 80, 80, 128);
		ImU32 textColor = IM_COL32(255, 255, 255, 255);
		ImU32 lineColor = IM_COL32(100, 150, 255, 255);
		ImU32 pointColor = IM_COL32(255, 100, 100, 255);
		ImU32 hoveredPointColor = IM_COL32(255, 255, 100, 255);
	};

private:
	std::vector<PointData> m_Data;
	Config m_Config;
	ColorFunction m_ColorFunction;

	// Cached values for optimization and interaction
	float m_DataMinX = 0.0f;
	float m_DataMaxX = 0.0f;
	float m_DataMinY = 0.0f;
	float m_DataMaxY = 0.0f;
	int m_HoveredPointIndex = -1;

public:
	ImGuiLineGraph() = default;

	// Set configuration
	void SetConfig(const Config& config) { m_Config = config; }
	Config& GetConfig() { return m_Config; }

	// Set custom color function
	void SetColorFunction(const ColorFunction& colorFunc) { m_ColorFunction = colorFunc; }

	// Data management
	void SetData(const std::vector<PointData>& data);
	void SetData(const std::vector<float>& xValues, const std::vector<float>& yValues);
	void SetData(const std::vector<float>& xValues, const std::vector<float>& yValues, const std::vector<std::string>& labels);
	void SetData(const std::vector<float>& yValues); // X values will be indices
	void AddPoint(float x, float y, const std::string& label = "");
	void Clear();

	// Rendering
	bool Render(const char* id);

	// Utility functions
	size_t GetPointCount() const { return m_Data.size(); }
	const PointData& GetPoint(size_t index) const { return m_Data[index]; }

private:
	void UpdateDataRange();
	ImU32 GetLineColor(size_t index, float x, float y, float normalizedX, float normalizedY) const;
	ImU32 GetPointColor(size_t index, float x, float y, float normalizedX, float normalizedY) const;
	void RenderBackground(ImDrawList* drawList, const ImVec2& canvasMin, const ImVec2& canvasMax) const;
	void RenderGrid(ImDrawList* drawList, const ImVec2& canvasMin, const ImVec2& canvasMax) const;
	void RenderAxes(ImDrawList* drawList, const ImVec2& canvasMin, const ImVec2& canvasMax) const;
	void RenderLine(ImDrawList* drawList, const ImVec2& canvasMin, const ImVec2& canvasMax);
	void RenderPoints(ImDrawList* drawList, const ImVec2& canvasMin, const ImVec2& canvasMax);
	void RenderTitle(ImDrawList* drawList, const ImVec2& canvasMin, const ImVec2& canvasMax) const;
	void HandleInteraction(const ImVec2& canvasMin, const ImVec2& canvasMax);
	ImVec2 DataToCanvas(float x, float y, const ImVec2& canvasMin, const ImVec2& canvasMax) const;
	void DrawPoint(ImDrawList* drawList, const ImVec2& center, float radius, ImU32 color, PointStyle style) const;
	void DrawDashedLine(ImDrawList* drawList, const ImVec2& p1, const ImVec2& p2, ImU32 color, float thickness, float dashLength = 5.0f) const;
	void DrawDottedLine(ImDrawList* drawList, const ImVec2& p1, const ImVec2& p2, ImU32 color, float thickness, float dotSpacing = 3.0f) const;
	std::string FormatValue(float value, int precision = -1) const;
	std::string FormatXValue(float value) const;
	std::string FormatYValue(float value) const;
	float GetRangeX() const;
	float GetRangeY() const;
	float GetMinX() const;
	float GetMaxX() const;
	float GetMinY() const;
	float GetMaxY() const;
};