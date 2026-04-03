#pragma once

#include "IImGuiVisualizer.h"
#include "Bgfx3DViewport.h"
#include "Bgfx3DCamera.h"
#include "BgfxRenderPrimitives.h"

#include "imgui/imgui.h"
#include <functional>
#include <string>

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
                                              BgfxRenderPrimitives& prims)>;

    /**
     * @param name      Window title (must be unique for each instance).
     * @param shortcut  Optional menu shortcut label.
     * @param category  Optional menu category.
     */
    explicit ImGui3DViewVisualizer(const char* name     = "3D View",
                                   const char* shortcut = nullptr,
                                   const char* category = "Visualizers");

    ~ImGui3DViewVisualizer() override = default;

    // ── IImGuiVisualizer interface ──────────────────────────────────────

    void        Initialize() override;
    void        Shutdown() override;
    void        Update(float deltaTime) override;
    bool        Render(bool* isOpen) override;
    const char* GetName()         const override { return m_name.c_str(); }
    const char* GetShortcut()     const override { return m_shortcut.empty() ? nullptr : m_shortcut.c_str(); }
    const char* GetMenuCategory() const override { return m_category.empty() ? nullptr : m_category.c_str(); }

    // ── Public API ─────────────────────────────────────────────────────

    void SetRenderCallback(RenderCallback cb) { m_renderCallback = std::move(cb); }

    BgfxRenderPrimitives& GetPrimitives()  { return m_primitives; }
    Bgfx3DCamera&         GetCamera()      { return m_camera; }

    void SetShowGrid(bool v)   { m_showGrid = v; }
    void SetShowAxes(bool v)   { m_showAxes = v; }

    struct GridConfig {
        float    size  = 20.0f;
        float    step  = 1.0f;
        uint32_t color = 0xff808080;
    };

    void SetGridConfig(const GridConfig& cfg) { m_gridConfig = cfg; }

private:
    void RenderToolbar();
    void HandleInput(const ImVec2& regionMin, const ImVec2& regionSize);

    // Identity / menu strings
    std::string m_name;
    std::string m_shortcut;
    std::string m_category;

    // Subsystems
    Bgfx3DViewport       m_viewport;
    Bgfx3DCamera         m_camera;
    BgfxRenderPrimitives  m_primitives;

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
};

} // namespace ImGuiVisualizers
