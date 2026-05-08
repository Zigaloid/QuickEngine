#pragma once
#include "CombinedObjJson3DVisualizer.h"
#include "SelectionManager.h"
#include "RenderComponentSelectable.h"
#include "LevelComponent.h"
#include "MeshComponent.h"
#include "BgfxGizmoRenderer.h"
#include "CommandHistory.h"
#include "DeleteEntityCommand.h"
#include "DuplicateEntitiesCommand.h"
#include "PropertyInspector.h"
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
        m_selectionManager.SetCommandHistory(&m_history);
        m_selectionManager.SetShiftDragCallback([this]() { DuplicateSelectedEntities(); });
        CombinedObjJson3DVisualizer::Initialize();
        RegisterLevelActions();
    }

    void Shutdown() override
    {
        ReleaseLevelComponent();
        CombinedObjJson3DVisualizer::Shutdown();
    }

    bool Render(bool* isOpen) override;

protected:
    bool AttachMeshFromPath(const std::string& levelPath) override;

private:
    void ReleaseLevelComponent();
    void RegisterLevelActions();
    void RenderComponentHierarchy(bgfx::ViewId viewId, ComponentSystem::Component* comp);
    void CollectRenderComponents(ComponentSystem::Component* comp,
                                 std::vector<CRenderComponent*>& out);
    void RegisterRenderComponents(ComponentSystem::Component* root);
    void RenderSelectionHighlight(bgfx::ViewId viewId, BgfxRenderPrimitives& prims);
    void RenderEntityAssetPanel();
    void RefreshEntityAssets();
    void HandleEntityDrop();
    void RenderInspectorPanel();

    /// Same as RegisterRenderComponents but also returns the newly created selectables.
    std::vector<std::shared_ptr<CSelectable>> RegisterRenderComponentsReturning(ComponentSystem::Component* root);

    /// Duplicates the parent entity of every currently selected object,
    /// registers the new selectables, and pushes an undoable command.
    void DuplicateSelectedEntities();

    /// Removes all selectables belonging to @p entity from the manager and
    /// the local m_componentSelectables list.
    void DeregisterEntitySelectables(CEntityComponent* entity);

    /// Pushes a CDeleteEntityCommand onto the history for each currently
    /// selected entity. Clears the selection when done.
    void DeleteSelectedEntities();

    struct EntityAssetEntry {
        std::string fileName;
        std::string fullPath;
    };

    CLevelComponent*   m_levelComp;
    CSelectionManager  m_selectionManager;
    GizmoMode          m_gizmoMode;

    std::vector<std::shared_ptr<CRenderComponentSelectable>> m_componentSelectables;

    ImVec2 m_viewportMin  = { 0.0f, 0.0f };
    ImVec2 m_viewportSize = { 1.0f, 1.0f };

    std::vector<EntityAssetEntry> m_entityAssets;
    bool  m_entityAssetsNeedRefresh = true;
    float m_entityPanelWidth;
    CCommandHistory  m_history{ 100 };
    PropertyInspector m_propertyInspector;
};

} // namespace ImGuiVisualizers