#pragma once

#include "IImGuiVisualizer.h"
#include "Bgfx3DViewport.h"
#include "Bgfx3DCamera.h"
#include "BgfxRenderPrimitives.h"

#include "imgui/imgui.h"
#include <functional>
#include <string>

class CMeshComponent;
namespace ImGuiVisualizers {
/**
 * @brief An IImGuiVisualizer that displays an interactive 3D viewport
 *        rendered with BGFX into an offscreen framebuffer and presented
 *        inside an ImGui dockable window via ImGui::Image().
 *
 * Multiple instances can coexist (each gets its own BGFX view ID from
 * BgfxViewIdAllocator). Register with ImGuiVisualizerManager as usual.
 *
 * External code can submit additional draw calls by setting a render
 * callback via SetRenderCallback().
 */
class ImGui3DViewVisualizer : public IImGuiVisualizer {
public:
    /**
     * @brief Callback invoked every frame between BeginFrame/EndFrame.
     *        Use it to submit your own BGFX draw calls into the view.
     * @param viewId   The BGFX view to submit draw calls into.
     * @param prims    Reference to the shared primitive renderer.
     */
    using RenderCallback = std::function<void(bgfx::ViewId viewId,
        Rendering::BgfxRenderPrimitives& prims)>;

    /**
     * @param name      Window title (must be unique for each instance).
     * @param shortcut  Optional menu shortcut label.
     * @param category  Optional menu category.
     */
    explicit ImGui3DViewVisualizer(const char* name     = "3D View",
                                   ImGuiKey shortcut    = ImGuiKey_None,
                                   const char* category = "Visualizers");

    ~ImGui3DViewVisualizer() override = default;

    // ── IImGuiVisualizer interface ──────────────────────────────────────

    void        Initialize() override;
    void        Shutdown() override;
    void        Update(float deltaTime) override;
    bool        Render(bool* isOpen) override;
    const char* GetName()         const override { return m_name.c_str(); }
    ImGuiKey    GetShortcut()     const override { return m_shortcutKey; }
    const char* GetMenuCategory() const override { return m_category.empty() ? nullptr : m_category.c_str(); }

    // ── Public API ─────────────────────────────────────────────────────

    void SetRenderCallback(RenderCallback cb) { m_renderCallback = std::move(cb); }
    
    Bgfx3DCamera&           GetCamera()        { return m_camera; }
    bgfx::FrameBufferHandle GetFrameBuffer()   const { return m_viewport.GetFrameBuffer(); }

    // Viewport bounds tracking (updated each frame in RenderContent)
    ImVec2 GetViewportMin() const { return m_viewportMin; }
    ImVec2 GetViewportSize() const { return m_viewportSize; }

    /// Test whether a screen-space point is inside the viewport.
    /// @param screenPos Screen-space position (e.g., from ImGui::GetMousePos()).
    /// @return true if the point is within the viewport bounds.
    bool IsPointInViewport(const ImVec2& screenPos) const
    {
        return screenPos.x >= m_viewportMin.x && screenPos.y >= m_viewportMin.y &&
               screenPos.x < m_viewportMin.x + m_viewportSize.x &&
               screenPos.y < m_viewportMin.y + m_viewportSize.y;
    }

    void SetShowGrid(bool v)   { m_showGrid = v; }
    void SetShowAxes(bool v)   { m_showAxes = v; }

    struct GridConfig {
        float    size  = 20.0f;
        float    step  = 1.0f;
        uint32_t color = 0xff808080;
    };

    void SetGridConfig(const GridConfig& cfg) { m_gridConfig = cfg; }

    // Render the viewport & toolbar content into an existing ImGui region
    // (does NOT call ImGui::Begin / ImGui::End). Pass the desired content size
    // in pixels.
    void RenderContent(const ImVec2& contentSize);

private:
    void RenderToolbar();
    // Updated signature: pass whether the ImGui item (invisible button) is hovered/active
    void HandleInput(const ImVec2& regionMin, const ImVec2& regionSize, bool itemHovered, bool itemActive);

    // Identity / menu strings
    std::string m_name;
    ImGuiKey    m_shortcutKey = ImGuiKey_None;
    std::string m_category;

    // Subsystems
    Bgfx3DViewport       m_viewport;
    Bgfx3DCamera         m_camera;
    
    // External render hook
    RenderCallback m_renderCallback;

    // UI state
    ImVec2   m_lastContentSize = { 0.0f, 0.0f };
    bool     m_showGrid        = true;
    bool     m_showAxes        = true;
    GridConfig m_gridConfig;

    // Mouse drag tracking
    bool     m_orbiting = false;
    bool     m_panning  = false;    

    // Viewport bounds (cached each frame)
    ImVec2   m_viewportMin = { 0.0f, 0.0f };
    ImVec2   m_viewportSize = { 0.0f, 0.0f };
};

} // namespace ImGuiVisualizers
