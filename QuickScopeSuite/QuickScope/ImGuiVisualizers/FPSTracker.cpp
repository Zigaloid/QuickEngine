#include "FPSTracker.h"
#include "UnifiedActionManager.h"
#include <algorithm>
#include <sstream>
#include <iomanip>

static const float MAX_FPS = 120.0f;

FPSTracker::FPSTracker(size_t maxSamples)
    : VisualizableMetricHeuristic(maxSamples, true)
    , m_CurrentFPS(0.0f)
    , m_FirstFrame(true)
    , m_GraphType(GraphType::Both)  // Changed default to Both
    , m_LineGraphNeedsUpdate(false)
    , m_MaxLineGraphSamples(1000)
{
    m_StartTime = std::chrono::steady_clock::now();
    InitializeFPSVisualization();
    InitializeLineGraph();
    SetupFPSBuckets();
    SetupPerformanceColors();
    RegisterFPSActions();
}

void FPSTracker::UpdateFromDeltaTime(double deltaTime)
{
    if (deltaTime > 0.0)
    {
        m_CurrentFPS = static_cast<float>(1.0 / deltaTime);
        // Anything > MAX_FPS is clamped to just below MAX_FPS for visualization purposes.
        if (m_CurrentFPS >= MAX_FPS)
            m_CurrentFPS = MAX_FPS - 1.0f;

        AddSample(m_CurrentFPS);
        UpdateLineGraph();
    }
}

void FPSTracker::UpdateFrame()
{
    auto currentTime = std::chrono::steady_clock::now();

    if (!m_FirstFrame)
    {
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - m_LastFrameTime);
        double deltaTime = duration.count() / 1000000.0; // Convert to seconds

        if (deltaTime > 0.0)
        {
            m_CurrentFPS = static_cast<float>(1.0 / deltaTime);
            AddSample(m_CurrentFPS);
            UpdateLineGraph();
        }
    }
    else
    {
        m_FirstFrame = false;
    }

    m_LastFrameTime = currentTime;
}

void FPSTracker::SetThresholds(const FPSThresholds& thresholds)
{
    m_Thresholds = thresholds;
    SetupFPSBuckets();
    SetupPerformanceColors();
}

std::string FPSTracker::GetPerformanceCategory(float fps) const
{
    if (fps >= m_Thresholds.excellent)
        return "Excellent";
    else if (fps >= m_Thresholds.good)
        return "Good";
    else if (fps >= m_Thresholds.acceptable)
        return "Acceptable";
    else if (fps >= m_Thresholds.poor)
        return "Poor";
    else
        return "Unacceptable";
}

ImU32 FPSTracker::GetPerformanceCategoryColor(float fps) const
{
    if (fps >= m_Thresholds.excellent)
        return IM_COL32(0, 255, 0, 255);      // Bright Green - Excellent
    else if (fps >= m_Thresholds.good)
        return IM_COL32(0, 255, 0, 255);      // Bright Green - Good.
    else if (fps >= m_Thresholds.acceptable)
        return IM_COL32(255, 255, 0, 255);    // Yellow - Acceptable
    else if (fps >= m_Thresholds.poor)
        return IM_COL32(255, 128, 0, 255);    // Orange - Poor
    else
        return IM_COL32(255, 0, 0, 255);      // Red - Unacceptable
}

void FPSTracker::SetupFPSBuckets()
{
    DefineStepBuckets(0, MAX_FPS, 5);
}

void FPSTracker::RenderFPSStatistics() const
{
    if (GetVisualizationConfig().showStatistics == false)
        return;

    const auto& stats = GetStatistics();

    if (stats.totalSamples == 0)
    {
        ImGui::Text("No FPS data collected yet");
        return;
    }

    // Current FPS with color coding
    ImU32 currentColor = GetPerformanceCategoryColor(m_CurrentFPS);
    ImVec4 colorVec = ImGui::ColorConvertU32ToFloat4(currentColor);

    ImGui::Text("Current FPS: ");
    ImGui::SameLine();
    ImGui::TextColored(colorVec, "%.1f (%s)", m_CurrentFPS, GetPerformanceCategory(m_CurrentFPS).c_str());

    // Statistics in two columns
    ImGui::Columns(2, "FPSStatsColumns", false);

    ImGui::Text("Average FPS:"); ImGui::NextColumn();
    ImGui::Text("%.1f", stats.average); ImGui::NextColumn();

    ImGui::Text("Min FPS:"); ImGui::NextColumn();
    ImU32 minColor = GetPerformanceCategoryColor(stats.minValue);
    ImVec4 minColorVec = ImGui::ColorConvertU32ToFloat4(minColor);
    ImGui::TextColored(minColorVec, "%.1f", stats.minValue); ImGui::NextColumn();

    ImGui::Text("Max FPS:"); ImGui::NextColumn();
    ImU32 maxColor = GetPerformanceCategoryColor(stats.maxValue);
    ImVec4 maxColorVec = ImGui::ColorConvertU32ToFloat4(maxColor);
    ImGui::TextColored(maxColorVec, "%.1f", stats.maxValue); ImGui::NextColumn();

    ImGui::Text("Total Samples:"); ImGui::NextColumn();
    ImGui::Text("%zu", stats.totalSamples); ImGui::NextColumn();

    ImGui::Columns(1);
}

void FPSTracker::RegisterFPSActions()
{    
    // FPS.Display.ShowStatistics (toggle)
    m_ActionManager.RegisterAction({
        .path = "Frame Analysis.Display.Show Statistics",
        .description = "Toggle FPS statistics display",
        .targets = UI::ActionTarget::Menu | UI::ActionTarget::Console | UI::ActionTarget::Toolbar,
        .callback = [this]() {
            auto& visConfig = GetVisualizationConfig();
            visConfig.showStatistics = !visConfig.showStatistics;
        },
        .isChecked = [this]() {
            return GetVisualizationConfig().showStatistics;
        },
        .sortPriority = 10
        });

    // FPS.Display.BarGraph
    m_ActionManager.RegisterAction({
        .path = "Frame Analysis.Display.Bar Graph",
        .description = "Show FPS distribution as bar graph",
        .targets = UI::ActionTarget::Menu | UI::ActionTarget::Console | UI::ActionTarget::Toolbar,
        .callback = [this]() { m_GraphType = GraphType::BarGraph; },
        .isChecked = [this]() { return m_GraphType == GraphType::BarGraph; },
        .sortPriority = 20
        });

    // FPS.Display.LineGraph
    m_ActionManager.RegisterAction({
        .path = "Frame Analysis.Display.Line Graph",
        .description = "Show FPS over time as line graph",
        .targets = UI::ActionTarget::Menu | UI::ActionTarget::Console | UI::ActionTarget::Toolbar,
        .callback = [this]() { m_GraphType = GraphType::LineGraph; },
        .isChecked = [this]() { return m_GraphType == GraphType::LineGraph; },
        .sortPriority = 30
        });

    // FPS.Display.Both
    m_ActionManager.RegisterAction({
        .path = "Frame Analysis.Display.Both Graphs",
        .description = "Show both bar graph and line graph",
        .targets = UI::ActionTarget::Menu | UI::ActionTarget::Console | UI::ActionTarget::Toolbar,
        .callback = [this]() { m_GraphType = GraphType::Both; },
        .isChecked = [this]() { return m_GraphType == GraphType::Both; },
        .sortPriority = 40
        });

    // FPS.Actions.ResetData
    m_ActionManager.RegisterAction({
        .path = "Frame Analysis.Actions.Reset Data",
        .description = "Reset all FPS tracking data",
        .targets = UI::ActionTarget::Menu | UI::ActionTarget::Console | UI::ActionTarget::Toolbar,
        .callback = [this]() {
            Reset();
            m_LineGraph.Clear();
            m_StartTime = std::chrono::steady_clock::now();
        },
        .sortPriority = 10
        });
}

void FPSTracker::UnregisterFPSActions()
{
    m_ActionManager.UnregisterAction("Frame Analysis.Display.Show Statistics");
    m_ActionManager.UnregisterAction("Frame Analysis.Display.Bar Graph");
    m_ActionManager.UnregisterAction("Frame Analysis.Display.Line Graph");
    m_ActionManager.UnregisterAction("Frame Analysis.Display.Both Graphs");
    m_ActionManager.UnregisterAction("Frame Analysis.Actions.Reset Data");
}
bool FPSTracker::RenderFPSAnalysisWindow(const char* windowTitle, bool* isOpen)
{
    ImGuiWindowFlags flags = ImGuiWindowFlags_MenuBar;
    bool localOpen = true;
    bool* pOpen = isOpen ? isOpen : &localOpen;

    if (ImGui::Begin(windowTitle, pOpen, flags))
    {
        // Menu bar for display options
        if (ImGui::BeginMenuBar())
        {
            m_ActionManager.RenderMenuBar();
            ImGui::EndMenuBar();
        }

        m_ActionManager.RenderToolbar();

        // FPS Statistics
        RenderFPSStatistics();
        ImGui::Separator();

        // Render graphs based on selected type
        switch (m_GraphType)
        {
        case GraphType::BarGraph:
            RenderGraphOnly("FPSBarGraph");
            break;

        case GraphType::LineGraph:
            RenderLineGraphOnly("FPSLineGraph");
            break;

        case GraphType::Both:
        {
            // Calculate available space for graphs more accurately
            float availableHeight = ImGui::GetContentRegionAvail().y;

            // Account for all UI elements that will consume space
            float textLineHeight = ImGui::GetTextLineHeightWithSpacing();
            float separatorHeight = ImGui::GetStyle().ItemSpacing.y;

            // Space consumed by titles and separators
            float title1Height = textLineHeight + separatorHeight;  // "FPS Distribution" + separator
            float middleSpacing = separatorHeight * 3;              // Spacing + Separator + Spacing
            float title2Height = textLineHeight + separatorHeight;  // "FPS Over Time" + separator

            float totalOverhead = title1Height + middleSpacing + title2Height;
            float availableForGraphs = availableHeight - totalOverhead;

            // Split remaining space equally between graphs
            float halfHeight = availableForGraphs * 0.5f;

            // Ensure minimum height for graphs
            halfHeight = std::max(halfHeight, 100.0f);

            // Store original sizes
            auto& barConfig = GetVisualizationConfig().graphConfig;
            auto& lineConfig = m_LineGraph.GetConfig();

            ImVec2 originalBarSize = barConfig.size;
            ImVec2 originalLineSize = lineConfig.size;

            // Set fixed heights for both graphs
            barConfig.size.y = halfHeight;
            lineConfig.size.y = halfHeight;

            // Render bar graph in first half
            ImGui::Text("FPS Distribution");
            ImGui::Separator();

            // Create a child window for the bar graph to constrain its size
            if (ImGui::BeginChild("BarGraphRegion", ImVec2(0, halfHeight), false))
            {
                RenderGraphOnly("FPSBarGraph");
            }
            ImGui::EndChild();

            // Separator between graphs
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Render line graph in second half
            ImGui::Text("FPS Over Time");
            ImGui::Separator();

            // Create a child window for the line graph to constrain its size
            if (ImGui::BeginChild("LineGraphRegion", ImVec2(0, halfHeight), false))
            {
                RenderLineGraphOnly("FPSLineGraph");
            }
            ImGui::EndChild();

            // Restore original sizes
            barConfig.size = originalBarSize;
            lineConfig.size = originalLineSize;

            break;
        }
        }

        ImGui::End();
        return true;
    }
    else
    {
        // Window is collapsed or closed - still need to call End()
        ImGui::End();
        return false;
    }
}

bool FPSTracker::RenderLineGraphOnly(const char* id)
{
    return m_LineGraph.Render(id);
}

void FPSTracker::InitializeFPSVisualization()
{
    auto& visConfig = GetVisualizationConfig();

    // Configure visualization settings
    visConfig.title = "FPS Distribution";
    visConfig.displayMode = DisplayMode::Percentage;
    visConfig.showStatistics = true;
    visConfig.autoUpdateGraph = true;

    // Configure graph appearance - use auto-height (0) so it adapts to container
    visConfig.graphConfig.size = ImVec2(0, 0);  // Auto-size both width and height
    visConfig.graphConfig.showValues = true;
    visConfig.graphConfig.showLabels = true;
    visConfig.graphConfig.showGrid = true;
    visConfig.graphConfig.autoScale = true;
    visConfig.graphConfig.barSpacing = 3.0f;
    visConfig.graphConfig.title = "FPS Performance Distribution";
}

void FPSTracker::InitializeLineGraph()
{
    auto& config = m_LineGraph.GetConfig();

    // Configure line graph appearance - use auto-height (0) so it adapts to container
    config.size = ImVec2(0, 0);  // Auto-size both width and height
    config.title = "FPS Over Time";
    config.xAxisLabel = "Time (seconds)";
    config.yAxisLabel = "FPS";
    config.showGrid = true;
    config.showPoints = true;
    config.showXLabels = true;
    config.showYLabels = true;
    config.autoScaleX = true;
    config.autoScaleY = false; // We want to control Y scale for FPS
    config.minY = 0.0f;
    config.maxY = MAX_FPS;
    config.lineThickness = 2.0f;
    config.pointRadius = 2.0f;
    config.pointStyle = ImGuiLineGraph::PointStyle::Circle;
    config.lineStyle = ImGuiLineGraph::LineStyle::Solid;
    config.lineColor = IM_COL32(100, 150, 255, 255);
    config.pointColor = IM_COL32(255, 100, 100, 255);
    config.gridCountX = 10;
    config.gridCountY = 8;

    // Set precision for axis labels to match FPS statistics (1 decimal place)
    config.xLabelPrecision = 0;  // Time in seconds with 2 decimal places
    config.yLabelPrecision = 0;  // FPS with 1 decimal place to match statistics

    // Set up performance-based color function for the line graph
    m_LineGraph.SetColorFunction([this](size_t index, float x, float y, float normalizedX, float normalizedY) -> ImU32 {
        return GetPerformanceCategoryColor(y);
    });
}

void FPSTracker::UpdateLineGraph()
{
    double elapsedTime = GetElapsedTime();

    // Add the current FPS sample with timestamp
    m_LineGraph.AddPoint(static_cast<float>(elapsedTime), m_CurrentFPS,
        "FPS: " + std::to_string(static_cast<int>(m_CurrentFPS)));

    // Limit the number of points to keep performance good
    if (m_LineGraph.GetPointCount() > m_MaxLineGraphSamples)
    {
        // Remove oldest points by recreating the data
        std::vector<ImGuiLineGraph::PointData> currentData;
        currentData.reserve(m_MaxLineGraphSamples);

        size_t startIndex = m_LineGraph.GetPointCount() - m_MaxLineGraphSamples;
        for (size_t i = startIndex; i < m_LineGraph.GetPointCount(); ++i)
        {
            currentData.push_back(m_LineGraph.GetPoint(i));
        }

        m_LineGraph.SetData(currentData);
    }
}

double FPSTracker::GetElapsedTime() const
{
    auto currentTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - m_StartTime);
    return duration.count() / 1000000.0; // Convert to seconds
}

void FPSTracker::SetupPerformanceColors()
{
    // Set performance-based color function
    SetColorFunction([this](size_t index, float value, float normalizedValue) -> ImU32 {
        // Use bucket-based coloring for consistency
        if (index < m_Buckets.size())
        {
            float barValue = m_Buckets[index].maxValue;
            if (barValue <= 20) return IM_COL32(255, 0, 0, 255);      // Unacceptable - Red
            if (barValue <= 40) return IM_COL32(255, 128, 0, 255);    // Poor - Orange
            if (barValue <= 60) return IM_COL32(255, 255, 0, 255);    // Acceptable - Yellow
            if (barValue <= 120) return IM_COL32(128, 255, 0, 255);    // Good - Green-Yellow
        }
        // Fallback to default coloring
        return IM_COL32(0, 255, 0, 255);      // Excellent - Green
    });
}