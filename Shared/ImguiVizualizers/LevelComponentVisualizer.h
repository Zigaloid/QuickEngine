#pragma once
#include "CombinedObjJson3DVisualizer.h"
#include "CSelectionManager.h"
#include "CRenderComponentSelectable.h"
#include "LevelComponent.h"
#include "MeshComponent.h"
#include "BgfxGizmoRenderer.h"
#include <vector>
#include <string>
#include <memory>

namespace ImGuiVisualizers {

/**
 * @brief An editor visualizer for level files.
 *
 * Loads a level file into a CLevelComponent and renders all
 * CRenderComponent-derived descendants each frame via the
 * embedded ImGui3DViewVisualizer.
 */
class LevelComponentVisualizer : public CombinedObjJson3DVisualizer
{
public:
    explicit LevelComponentVisualizer(const char* name = "Level Editor")
        : CombinedObjJson3DVisualizer(name)
        , m_levelComp(nullptr)
        , m_entityPanelWidth(250.0f)
        , m_gizmoMode(GizmoMode::Translate)
    {}

    ~LevelComponentVisualizer() override
    {
        ReleaseLevelComponent();
    }
    void Initialize() override
    {
        // Once, after bgfx init:
        m_selectionManager.InitializeGizmo();

        // Each frame, after submitting scene geometry:        
        CombinedObjJson3DVisualizer::Initialize();

        RegisterLevelActions();
    }
    bool Render(bool* isOpen) override;

protected:
    bool AttachMeshFromPath(const std::string& levelPath) override;

private:
    void ReleaseLevelComponent();

    /// Registers gizmo-mode actions into the editor's action manager.
    void RegisterLevelActions();

    /// Recursively walks the component hierarchy and calls Render() on any
    /// CRenderComponent-derived component that is active.
    void RenderComponentHierarchy(bgfx::ViewId viewId, ComponentSystem::Component* comp);

    /// Collects all CRenderComponent-derived components in the hierarchy.
    void CollectRenderComponents(ComponentSystem::Component* comp,
                                 std::vector<CRenderComponent*>& out);

    /// Builds CRenderComponentSelectable entries for all render components
    /// under the given root and registers them with m_selectionManager.
    void RegisterRenderComponents(ComponentSystem::Component* root);

    /// Renders a wireframe bounding-sphere highlight around the selected component.
    void RenderSelectionHighlight(bgfx::ViewId viewId, BgfxRenderPrimitives& prims);

    // Entity asset browser panel
    void RenderEntityAssetPanel();
    void RefreshEntityAssets();
    void HandleEntityDrop();

    struct EntityAssetEntry {
        std::string fileName;
        std::string fullPath;
    };

    CLevelComponent*   m_levelComp;

    CSelectionManager  m_selectionManager;

    /// Active gizmo manipulation mode (Translate / Scale / Rotate).
    GizmoMode          m_gizmoMode;

    /// Parallel list kept so we can call SyncFromComponent() and
    /// dynamic_cast back to CRenderComponent* after picking.
    std::vector<std::shared_ptr<CRenderComponentSelectable>> m_componentSelectables;

    // Viewport rect recorded each frame for picking
    ImVec2 m_viewportMin  = { 0.0f, 0.0f };
    ImVec2 m_viewportSize = { 1.0f, 1.0f };

    // Entity asset browser state
    std::vector<EntityAssetEntry> m_entityAssets;
    bool  m_entityAssetsNeedRefresh = true;
    float m_entityPanelWidth;
};

} // namespace ImGuiVisualizers