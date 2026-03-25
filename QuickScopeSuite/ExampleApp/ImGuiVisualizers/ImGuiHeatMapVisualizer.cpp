#include "ImGuiHeatMapVisualizer.h"
#include "Analysis/HeatMapContainer.h" // adjust include path as needed
#include "ResourceManager/ResourceManager.h"
#include <cmath>
#include <algorithm>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include "TextureResource.h"
#include "CoreSystem/CoreSystem.h"

// Add visualizer include
#include "FPSTracker.h"

namespace ImGuiVisualizers {

void ImGuiHeatMapVisualizer::SetContainer(HeatMapContainer* container)
{
    m_Container = container;
    RefreshSeriesList();
}

bool ImGuiHeatMapVisualizer::LoadBackgroundTextureFromResource(const std::string& path)
{
    // Request resource (duplicated by ResourceManager)   
    m_BackgroundTextureResource = Core::CoreSystem::GetResourceManager()->RequestResource<ResourceSystem::TextureResource>(path);
    m_BackgroundTexturePath = path;
    m_Config.showBackgroundImage = true; // turn on by default
    // If already finalized (cached), adopt immediately
    TryAdoptBackgroundTextureFromResource();
    return static_cast<bool>(m_BackgroundTextureResource);
}

void ImGuiHeatMapVisualizer::ClearBackgroundTexture()
{
    m_BackgroundTextureResource.reset();
    m_BackgroundTexturePath.clear();
    m_BackgroundTexId = nullptr;
    m_BackgroundTexSize = ImVec2(0, 0);
    m_Config.showBackgroundImage = false;
}

void ImGuiHeatMapVisualizer::TryAdoptBackgroundTextureFromResource()
{
    if (!m_BackgroundTextureResource) return;
    // Only adopt after the resource finalized GPU upload
    if (!m_BackgroundTextureResource->IsFinalized()) return;

    const GLuint glId = m_BackgroundTextureResource->GetTextureId();
    if (glId == 0) return;

    const int w = m_BackgroundTextureResource->GetWidth();
    const int h = m_BackgroundTextureResource->GetHeight();

    // ImGui OpenGL backends use ImTextureID as a GL id cast to void*
    ImTextureID texId = reinterpret_cast<ImTextureID>(static_cast<intptr_t>(glId));
    // If already set to same id and size, skip
    if (m_BackgroundTexId == texId && static_cast<int>(m_BackgroundTexSize.x) == w && static_cast<int>(m_BackgroundTexSize.y) == h)
        return;

    SetBackgroundTexture(texId, w, h);
}

void ImGuiHeatMapVisualizer::RefreshSeriesList()
{
    m_SeriesNames.clear();
    if (!m_Container) return;

    // Scan all cells and collect unique series names.
    std::unordered_set<std::string> uniqueNames;

    const std::size_t cols = m_Container->GetColumns();
    const std::size_t rows = m_Container->GetRows();
    for (std::size_t r = 0; r < rows; ++r) {
        for (std::size_t c = 0; c < cols; ++c) {
            const HeatMapContainer::Cell* cell = m_Container->TryGetCell(c, r);
            if (!cell) continue;
            for (const auto& kv : cell->seriesByName) {
                uniqueNames.insert(kv.first);
            }
        }
    }

    m_SeriesNames.assign(uniqueNames.begin(), uniqueNames.end());
    std::sort(m_SeriesNames.begin(), m_SeriesNames.end());

    if (!m_SeriesNames.empty()) {
        if (m_SelectedSeries.empty() ||
            std::find(m_SeriesNames.begin(), m_SeriesNames.end(), m_SelectedSeries) == m_SeriesNames.end()) {
            m_SelectedSeries = m_SeriesNames[0];
        }
    } else {
        m_SelectedSeries.clear();
    }
}

bool ImGuiHeatMapVisualizer::RenderWindow(const char* windowTitle, bool* isOpen)
{
    if (!ImGui::Begin(windowTitle, isOpen, ImGuiWindowFlags_MenuBar))
    {
        ImGui::End();
        return false;
    }

    // Menu bar
    if (ImGui::BeginMenuBar()) {
        // File menu for HeatMapContainer save/load
        if (ImGui::BeginMenu("File")) {
            // Path buffer persists across frames
            static char heatmapPathBuffer[512] = "HeatMap.hmap";
            ImGui::InputText("HeatMap Path", heatmapPathBuffer, sizeof(heatmapPathBuffer));

            const bool canOperate = (m_Container != nullptr);

            if (ImGui::MenuItem("Save HeatMap", "Ctrl+S", false, canOperate)) {
                std::string savePath(heatmapPathBuffer);
                // Default directory if empty
                if (savePath.empty()) {
                    savePath = "HeatMap.hmap";
                }
                // Ensure .hmap extension if none provided
                if (savePath.find('.') == std::string::npos) {
                    savePath += ".hmap";
                }
                // Attempt save
                if (!m_Container->Save(savePath)) {
                    ImGui::OpenPopup("HeatMap Save Error");
                } else {
                    ImGui::OpenPopup("HeatMap Save Success");
                }
            }

            if (ImGui::MenuItem("Load HeatMap", "Ctrl+L", false, canOperate)) {
                std::string loadPath(heatmapPathBuffer);
                if (loadPath.empty()) {
                    ImGui::OpenPopup("HeatMap Load Error");
                } else {
                    if (!m_Container->Load(loadPath)) {
                        ImGui::OpenPopup("HeatMap Load Error");
                    } else {
                        // After a successful load, refresh series selection
                        RefreshSeriesList();
                        ImGui::OpenPopup("HeatMap Load Success");
                    }
                }
            }

            // Notifications
            if (ImGui::BeginPopup("HeatMap Save Error")) {
                ImGui::TextUnformatted("Failed to save HeatMapContainer.");
                ImGui::EndPopup();
            }
            if (ImGui::BeginPopup("HeatMap Load Error")) {
                ImGui::TextUnformatted("Failed to load HeatMapContainer. Check the path and file format.");
                ImGui::EndPopup();
            }
            if (ImGui::BeginPopup("HeatMap Save Success")) {
                ImGui::TextUnformatted("Heat map saved successfully.");
                ImGui::EndPopup();
            }
            if (ImGui::BeginPopup("HeatMap Load Success")) {
                ImGui::TextUnformatted("Heat map loaded successfully.");
                ImGui::EndPopup();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::Checkbox("Show Grid", &m_Config.showGrid);
            ImGui::Checkbox("Show Legend", &m_Config.showLegend);
            ImGui::Checkbox("Auto Scale", &m_Config.autoScale);
            if (!m_Config.autoScale) {
                ImGui::InputFloat("Min", &m_Config.manualMin, 0.1f, 1.0f, "%.3f");
                ImGui::InputFloat("Max", &m_Config.manualMax, 0.1f, 1.0f, "%.3f");
                if (m_Config.manualMin > m_Config.manualMax) {
                    std::swap(m_Config.manualMin, m_Config.manualMax);
                }
            }
            ImGui::Separator();
            ImGui::Checkbox("Mouse Pan/Zoom", &m_Config.enableMouseInteraction);
            ImGui::SliderFloat("Zoom", &m_Config.zoom, m_Config.minZoom, m_Config.maxZoom, "%.2fx");
            ImGui::Separator();
            ImGui::Checkbox("Show Background Image", &m_Config.showBackgroundImage);
            ImGui::Separator();
            ImGui::SliderFloat("Cell Opacity", &m_Config.cellOpacity, 0.0f, 1.0f, "%.2f");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Series")) {
            if (ImGui::Button("Refresh")) {
                RefreshSeriesList();
            }
            if (m_SeriesNames.empty()) {
                ImGui::TextDisabled("No series found");
            } else {
                // Combo for selecting the series
                int currentIndex = 0;
                for (int i = 0; i < (int)m_SeriesNames.size(); ++i) {
                    if (m_SeriesNames[i] == m_SelectedSeries) {
                        currentIndex = i;
                        break;
                    }
                }
                if (ImGui::BeginCombo("DataSeries", m_SelectedSeries.empty() ? "<none>" : m_SelectedSeries.c_str())) {
                    for (int i = 0; i < (int)m_SeriesNames.size(); ++i) {
                        bool selected = (i == currentIndex);
                        if (ImGui::Selectable(m_SeriesNames[i].c_str(), selected)) {
                            m_SelectedSeries = m_SeriesNames[i];
                        }
                        if (selected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    // Title
    if (!m_Config.title.empty()) {
        ImGui::Text("%s", m_Config.title.c_str());
    }

    bool result = Render("HeatMapCanvas");
    ImGui::End();
    return result;
}

bool ImGuiHeatMapVisualizer::Render(const char* id)
{
    // If a background texture resource is pending, adopt it when finalized
    TryAdoptBackgroundTextureFromResource();

    if (!m_Container) {
        ImGui::TextDisabled("No HeatMapContainer set.");
        return false;
    }
    if (m_SelectedSeries.empty()) {
        RefreshSeriesList();
        if (m_SelectedSeries.empty()) {
            ImGui::TextDisabled("No DataSeries available to render.");
            return false;
        }
    }

    // Determine canvas size
    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImVec2 size = m_Config.size;
    if (size.x <= 0) size.x = avail.x;
    if (size.y <= 0) size.y = avail.y;

    // Reserve space with an invisible button for interaction
    ImGui::InvisibleButton(id, size);
    ImVec2 canvasMin = ImGui::GetItemRectMin();
    ImVec2 canvasMax = ImGui::GetItemRectMax();

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Background and border
    dl->AddRectFilled(canvasMin, canvasMax, m_Config.backgroundColor);

    if (m_Config.borderThickness > 0.0f) {
        dl->AddRect(canvasMin, canvasMax, m_Config.borderColor, 0.0f, 0, m_Config.borderThickness);
    }

    const std::size_t cols = m_Container->GetColumns();
    const std::size_t rows = m_Container->GetRows();
    if (cols == 0 || rows == 0) {
        ImGui::TextDisabled("Empty HeatMapContainer (0x0).");
        return false;
    }

    // Compute color scaling range
    double vmin = 0.0, vmax = 1.0;
    if (m_Config.autoScale) {
        ComputeDataRangeForSeries(vmin, vmax);
        m_LastMin = vmin; m_LastMax = vmax;
    } else {
        vmin = m_Config.manualMin;
        vmax = m_Config.manualMax;
        m_LastMin = vmin; m_LastMax = vmax;
    }
    if (vmax <= vmin) {
        vmax = vmin + 1.0;
    }

    // Compute cell pixel size accounting for zoom and pan
    const float totalW = (canvasMax.x - canvasMin.x);
    const float totalH = (canvasMax.y - canvasMin.y);
    const float cellW = (totalW / static_cast<float>(cols)) * m_Config.zoom;
    const float cellH = (totalH / static_cast<float>(rows)) * m_Config.zoom;

    // Origin with pan
    const float originX = canvasMin.x + m_Config.pan.x;
    const float originY = canvasMin.y + m_Config.pan.y;

    // Optional background image (aligned to the heatmap area, panned and zoomed)
    if (m_Config.showBackgroundImage && m_BackgroundTexId != nullptr) {
        const ImVec2 heatmapMin(originX, originY);
        const ImVec2 heatmapMax(originX + static_cast<float>(cols) * cellW,
                                originY + static_cast<float>(rows) * cellH);
        
        dl->AddImage(m_BackgroundTexId, heatmapMin, heatmapMax, ImVec2(0, 1), ImVec2(1, 0));
    }

    // Draw grid optionally
    if (m_Config.showGrid) {
        // Vertical lines
        for (std::size_t c = 0; c <= cols; ++c) {
            float x = originX + static_cast<float>(c) * cellW;
            dl->AddLine(ImVec2(x, originY), ImVec2(x, originY + rows * cellH), m_Config.gridColor, 1.0f);
        }
        // Horizontal lines
        for (std::size_t r = 0; r <= rows; ++r) {
            float y = originY + static_cast<float>(r) * cellH;
            dl->AddLine(ImVec2(originX, y), ImVec2(originX + cols * cellW, y), m_Config.gridColor, 1.0f);
        }
    }

    // Draw cells
    ImVec2 mousePos = ImGui::GetIO().MousePos;
    std::optional<std::pair<std::size_t, std::size_t>> hoveredCell;

    const int cellAlphaU8 = static_cast<int>(std::round(std::clamp(m_Config.cellOpacity, 0.0f, 1.0f) * 255.0f));

    for (std::size_t r = 0; r < rows; ++r) {
        for (std::size_t c = 0; c < cols; ++c) {
            ImVec2 p0(originX + c * cellW + m_Config.cellPadding, originY + r * cellH + m_Config.cellPadding);
            ImVec2 p1(originX + (c + 1) * cellW - m_Config.cellPadding, originY + (r + 1) * cellH - m_Config.cellPadding);

            // Skip if outside canvas for performance
            if (p1.x < canvasMin.x || p0.x > canvasMax.x || p1.y < canvasMin.y || p0.y > canvasMax.y)
                continue;

            const auto avgOpt = GetCellAverage(c, r, m_SelectedSeries);
            if (!avgOpt.has_value()) {
                // Do not render a rectangle for cells without values
                continue;
            }

            double v = avgOpt.value();
            if (m_Config.clampValues) {
                v = std::min(std::max(v, vmin), vmax);
            }

            // Color with configured alpha
            ImU32 rgb = ValueToColor(v, vmin, vmax);
            const int r8 = (rgb & 0xFF);
            const int g8 = ((rgb >> 8) & 0xFF);
            const int b8 = ((rgb >> 16) & 0xFF);
            ImU32 color = IM_COL32(r8, g8, b8, cellAlphaU8);

            dl->AddRectFilled(p0, p1, color);

            // Hover detection only for rendered cells
            if (mousePos.x >= p0.x && mousePos.x <= p1.x &&
                mousePos.y >= p0.y && mousePos.y <= p1.y)
            {
                hoveredCell = std::make_pair(c, r);
            }
        }
    }

    // Tooltip
    if (hoveredCell.has_value() && m_Config.showValuesOnHover) {
        auto [hc, hr] = hoveredCell.value();
        auto avgOpt = GetCellAverage(hc, hr, m_SelectedSeries);
        ImGui::BeginTooltip();
        ImGui::Text("Cell: (%zu, %zu)", hc, hr);
        ImGui::Text("Series: %s", m_SelectedSeries.c_str());
        if (avgOpt.has_value()) {
            ImGui::Text("Average: %.6f", avgOpt.value());
        } else {
            ImGui::TextDisabled("Average: <no data>");
        }
        ImGui::Text("Range: [%.6f, %.6f]", m_LastMin, m_LastMax);
        ImGui::EndTooltip();
    }

    // Handle clicks to populate histogram
    if (hoveredCell.has_value() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        auto [cc, rr] = hoveredCell.value();
        m_SelectedCellForHistogram = hoveredCell;
        UpdateHistogramForCell(cc, rr);
    }

    // Mouse interaction (pan/zoom/reset)
    if (m_Config.enableMouseInteraction) {
        HandleMouseInteraction(canvasMin, canvasMax);
    }

    // Legend
    if (m_Config.showLegend) {
        RenderLegend(canvasMin, canvasMax, vmin, vmax);
    }

    // Render histogram window if available
    RenderHistogram();

    return true;
}

std::optional<double> ImGuiHeatMapVisualizer::GetCellAverage(std::size_t col, std::size_t row, const std::string& seriesName) const
{
    if (!m_Container) return std::nullopt;
    return m_Container->GetAverageAtIndex(col, row, seriesName);
}

void ImGuiHeatMapVisualizer::ComputeDataRangeForSeries(double& outMin, double& outMax) const
{
    outMin = std::numeric_limits<double>::infinity();
    outMax = -std::numeric_limits<double>::infinity();

    if (!m_Container || m_SelectedSeries.empty()) {
        outMin = 0.0; outMax = 1.0;
        return;
    }

    const std::size_t cols = m_Container->GetColumns();
    const std::size_t rows = m_Container->GetRows();
    bool anyValue = false;

    for (std::size_t r = 0; r < rows; ++r) {
        for (std::size_t c = 0; c < cols; ++c) {
            const HeatMapContainer::Cell* cell = m_Container->TryGetCell(c, r);
            if (!cell) continue;
            auto it = cell->seriesByName.find(m_SelectedSeries);
            if (it != cell->seriesByName.end() && it->second.sampleCount > 0) {
                double v = it->second.Average();
                outMin = std::min(outMin, v);
                outMax = std::max(outMax, v);
                anyValue = true;
            }
        }
    }

    if (!anyValue) {
        outMin = 0.0;
        outMax = 1.0;
    } else {
        if (outMin == outMax) {
            // Ensure visible gradient even if all values identical
            const double epsilon = (outMin == 0.0) ? 1.0 : std::abs(outMin) * 0.01;
            outMin -= epsilon;
            outMax += epsilon;
        }
    }
}

// Simple blue->cyan->yellow->red colormap
ImU32 ImGuiHeatMapVisualizer::ValueToColor(double v, double vmin, double vmax) const
{
    double t = (v - vmin) / (vmax - vmin);
    t = std::clamp(t, 0.0, 1.0);

    // 4-stop gradient: blue (0), cyan (0.33), yellow (0.66), red (1)
    float r = 0.0f, g = 0.0f, b = 0.0f;
    if (t < 0.33) {
        double k = t / 0.33;
        r = 0.0f;
        g = static_cast<float>(k);
        b = 1.0f;
    } else if (t < 0.66) {
        double k = (t - 0.33) / 0.33;
        r = static_cast<float>(k);
        g = 1.0f;
        b = static_cast<float>(1.0 - k);
    } else {
        double k = (t - 0.66) / 0.34;
        r = 1.0f;
        g = static_cast<float>(1.0 - k * 0.5);
        b = 0.0f;
    }
    // Return opaque RGB; alpha is applied by caller based on m_Config.cellOpacity
    return IM_COL32(
        static_cast<int>(std::round(r * 255.0f)),
        static_cast<int>(std::round(g * 255.0f)),
        static_cast<int>(std::round(b * 255.0f)),
        255);
}

void ImGuiHeatMapVisualizer::HandleMouseInteraction(const ImVec2& canvasMin, const ImVec2& canvasMax)
{
    ImVec2 mousePos = ImGui::GetIO().MousePos;
    bool hovered = (mousePos.x >= canvasMin.x && mousePos.x <= canvasMax.x &&
        mousePos.y >= canvasMin.y && mousePos.y <= canvasMax.y);

    // Zoom with Ctrl + mouse wheel (anchor under cursor)
    float wheel = ImGui::GetIO().MouseWheel;
    if (hovered && (ImGui::GetIO().KeyCtrl) && wheel != 0.0f) {
        const float oldZoom = m_Config.zoom;
        const float zoomMul = wheel > 0 ? 1.1f : 0.9f;
        const float newZoom = std::clamp(oldZoom * zoomMul, m_Config.minZoom, m_Config.maxZoom);

        if (newZoom != oldZoom && oldZoom > 0.0f) {
            const float k = newZoom / oldZoom;

            // Keep the point under the mouse stationary in screen space:
            // screen = canvasMin + pan + local * zoom
            // Solve for pan' so that screen remains constant when zoom changes.
            const float dx = mousePos.x - canvasMin.x;
            const float dy = mousePos.y - canvasMin.y;

            m_Config.pan.x = dx - (dx - m_Config.pan.x) * k;
            m_Config.pan.y = dy - (dy - m_Config.pan.y) * k;
            m_Config.zoom = newZoom;
        }

        // Consume wheel to avoid scrolling the parent
        ImGui::GetIO().MouseWheel = 0.0f;
    }

    // Pan with left mouse drag
    if (hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        ImVec2 delta = ImGui::GetIO().MouseDelta;
        m_Config.pan.x += delta.x;
        m_Config.pan.y += delta.y;
    }

    // Reset with middle-click
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
        m_Config.pan = ImVec2(0, 0);
        m_Config.zoom = 1.0f;
    }
}

void ImGuiHeatMapVisualizer::RenderLegend(const ImVec2& canvasMin, const ImVec2& canvasMax, double vmin, double vmax)
{
    const float legendWidth = 140.0f;
    const float legendHeight = 12.0f;
    const float padding = 8.0f;

    ImVec2 legendMin(canvasMax.x - legendWidth - padding, canvasMin.y + padding);
    ImVec2 legendMax(canvasMax.x - padding, canvasMin.y + padding + legendHeight);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    // Gradient legend
    const int steps = 64;
    const float stepW = (legendMax.x - legendMin.x) / steps;
    for (int i = 0; i < steps; ++i) {
        float x0 = legendMin.x + i * stepW;
        float x1 = x0 + stepW;
        double t0 = static_cast<double>(i) / (steps - 1);
        double v = vmin + t0 * (vmax - vmin);
        ImU32 col = ValueToColor(v, vmin, vmax);
        // Apply the same cell opacity to the legend for consistency
        const int a = static_cast<int>(std::round(std::clamp(m_Config.cellOpacity, 0.0f, 1.0f) * 255.0f));
        const int r8 = (col & 0xFF);
        const int g8 = ((col >> 8) & 0xFF);
        const int b8 = ((col >> 16) & 0xFF);
        dl->AddRectFilled(ImVec2(x0, legendMin.y), ImVec2(x1, legendMax.y), IM_COL32(r8, g8, b8, a));
    }
    dl->AddRect(legendMin, legendMax, IM_COL32(255, 255, 255, 160));

    // Labels
    char bufMin[64], bufMax[64];
    snprintf(bufMin, sizeof(bufMin), "%.3f", vmin);
    snprintf(bufMax, sizeof(bufMax), "%.3f", vmax);
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
    ImGui::SetCursorScreenPos(ImVec2(legendMin.x, legendMax.y + 2.0f));
    ImGui::Text("%s", bufMin);
    ImGui::SameLine();
    ImGui::SetCursorScreenPos(ImVec2(legendMax.x - ImGui::CalcTextSize(bufMax).x, legendMax.y + 2.0f));
    ImGui::Text("%s", bufMax);
    ImGui::PopStyleColor();

    // Series name
    ImGui::SetCursorScreenPos(ImVec2(legendMin.x, legendMin.y - ImGui::GetTextLineHeight() - 2.0f));
    ImGui::Text("Series: %s", m_SelectedSeries.c_str());
}

// -------- Histogram integration --------

void ImGuiHeatMapVisualizer::UpdateHistogramForCell(std::size_t col, std::size_t row)
{
    if (!m_Container || m_SelectedSeries.empty()) return;

    // Pull the full history for this cell/series
    std::vector<double> valuesD;
    if (!m_Container->GetLastValuesAtIndex(col, row, m_SelectedSeries, valuesD, HeatMapContainer::MaxHistory)) {
        // No data for this cell/series
        m_Histogram.reset();
        return;
    }

    // Convert to float for MetricBucketHeuristic
    std::vector<float> values;
    values.reserve(valuesD.size());
    float minV = std::numeric_limits<float>::infinity();
    float maxV = -std::numeric_limits<float>::infinity();
    for (double d : valuesD) {
        float f = static_cast<float>(d);
        values.push_back(f);
        minV = std::min(minV, f);
        maxV = std::max(maxV, f);
    }

    if (!std::isfinite(minV) || !std::isfinite(maxV) || values.empty()) {
        m_Histogram.reset();
        return;
    }

    // If all equal, expand range slightly to make uniform buckets meaningful
    if (minV == maxV) {
        const float eps = (minV == 0.0f) ? 1.0f : std::abs(minV) * 0.01f;
        minV -= eps;
        maxV += eps;
    }

    // Create or reuse the histogram visualizer
    if (!m_Histogram) 
    {
        m_Histogram = std::make_unique<FPSTracker>();
    }
    // Feed samples
    m_Histogram->Reset();
    m_Histogram->AddSamples(values);

    // Update title to reflect cell and series
    auto& visCfg = m_Histogram->GetVisualizationConfig();
    {
        char titleBuf[256];
        snprintf(titleBuf, sizeof(titleBuf), "Histogram: %s @ Cell (%zu,%zu)", m_SelectedSeries.c_str(), col, row);
        visCfg.title = titleBuf;
        m_Histogram->SetVisualizationConfig(visCfg); // push title to graph config
    }
}

void ImGuiHeatMapVisualizer::RenderHistogram()
{
    if (!m_Histogram || !m_SelectedCellForHistogram.has_value())
        return;

    m_Histogram->Render("Histogram of Cell History");
    
}

} // namespace ImGuiVisualizers