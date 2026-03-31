#pragma once

#include "imgui.h"
#include "UnifiedActionManager.h"
#include <string>
#include <vector>
#include <unordered_set>
#include <optional>
#include <algorithm>
#include <limits>
#include <memory> // added

class HeatMapContainer;

// Forward declarations to avoid heavy includes in header
namespace ResourceSystem {
    class ResourceManager;
    class TextureResource;
}

// Forward declare the histogram visualizer
class FPSTracker;

namespace ImGuiVisualizers {

class ImGuiHeatMapVisualizer
{
public:
    struct Config
    {
        ImVec2 size = ImVec2(0, 0);        // 0 means use available content region
        float cellPadding = 0.0f;          // padding between cells in pixels
        float borderThickness = 1.0f;      // border thickness around canvas
        ImU32 borderColor = IM_COL32(120, 120, 120, 200);
        ImU32 backgroundColor = IM_COL32(40, 40, 40, 255);
        bool showLegend = true;
        bool showGrid = true;
        ImU32 gridColor = IM_COL32(80, 80, 80, 128);
        bool autoScale = true;             // auto-scale color range to data
        float manualMin = 0.0f;            // used when !autoScale
        float manualMax = 1.0f;
        bool clampValues = true;           // clamp to [min,max] before color mapping
        bool showValuesOnHover = true;     // tooltip with cell infotha
        float zoom = 1.0f;                 // zoom factor
        ImVec2 pan = ImVec2(0, 0);         // pan offset in pixels
        bool enableMouseInteraction = true; // allow pan/zoom with mouse
        float minZoom = 0.25f;
        float maxZoom = 10.0f;
        std::string title = "Heat Map";

        // Background image options
        bool showBackgroundImage = false;         // toggle background image        

        // Cell rendering options
        float cellOpacity = 128.0f / 255.0f;       // 0..1 opacity for datapoint cells
    };

    ImGuiHeatMapVisualizer();

    void SetConfig(const Config& cfg) { m_Config = cfg; }
    Config& GetConfig() { return m_Config; }

    // Provide the container to render. The visualizer does not own the container.
    void SetContainer(HeatMapContainer* container);

    // Set an existing texture as background image. Texture lifetime managed externally.
    // width/height are the original image pixel dimensions (informational).
    void SetBackgroundTexture(ImTextureID texId, int width, int height)
    {
        m_BackgroundTexId = texId;
        m_BackgroundTexSize = ImVec2(static_cast<float>(width), static_cast<float>(height));
    }

    // Request and use a ResourceSystem::TextureResource as background.
    // Returns true if the resource request was queued or already present.
    // The actual GPU texture is adopted automatically once the resource is finalized.
    bool LoadBackgroundTextureFromResource(const std::string& path);

    // Clear current background texture and pending resource.
    void ClearBackgroundTexture();

    // Render window with menu for series selection and options.
    bool RenderWindow(const char* windowTitle, bool* isOpen = nullptr);

    // Render the heatmap content (call inside a window).
    bool Render(const char* id = "HeatMapCanvas");

    // Returns currently selected series name (empty if none).
    const std::string& GetSelectedSeries() const { return m_SelectedSeries; }

    // File dialog driven save/load (mirrors ProfilerController pattern)
    void LoadHeatmapDialog();
    void SaveHeatmapAs();
    void SaveHeatmapCurrent();
    // Helpers
    void RefreshSeriesList();
private:
    // Helpers    
    std::optional<double> GetCellAverage(std::size_t col, std::size_t row, const std::string& seriesName) const;
    void ComputeDataRangeForSeries(double& outMin, double& outMax) const;
    ImU32 ValueToColor(double v, double vmin, double vmax) const;
    void HandleMouseInteraction(const ImVec2& canvasMin, const ImVec2& canvasMax);
    void RenderLegend(const ImVec2& canvasMin, const ImVec2& canvasMax, double vmin, double vmax);

    // Try to adopt GL texture from a finalized TextureResource (no-op if not ready)
    void TryAdoptBackgroundTextureFromResource();

    // Histogram helpers
    void UpdateHistogramForCell(std::size_t col, std::size_t row);
    void RenderHistogram();

    // Action manager helpers
    void RegisterActions();
    void UnregisterActions();

private:
    HeatMapContainer* m_Container = nullptr;
    Config m_Config;

    // Saved mouse wheel value captured before Begin() consumes it
    float m_SavedMouseWheel = 0.0f;

    // Cached series names existing in the container
    std::vector<std::string> m_SeriesNames;
    std::string m_SelectedSeries;

    // Cached last computed range
    double m_LastMin = 0.0;
    double m_LastMax = 1.0;

    // Background texture (ImGui)
    ImTextureID m_BackgroundTexId = 0;
    ImVec2      m_BackgroundTexSize = ImVec2(0, 0);

    // Pending/owned background resource (keeps GPU texture alive)
    std::shared_ptr<ResourceSystem::TextureResource> m_BackgroundTextureResource;
    std::string m_BackgroundTexturePath;

    // Histogram state
    std::unique_ptr<FPSTracker> m_Histogram; // created on demand
    std::optional<std::pair<std::size_t, std::size_t>> m_SelectedCellForHistogram; // last clicked cell

    // Action manager for menu/toolbar/console integration
    UI::UnifiedActionManager m_ActionManager;

    // Current heatmap file path (empty = unsaved / no file loaded)
    std::string m_CurrentHeatmapPath;
};

} // namespace ImGuiVisualizers