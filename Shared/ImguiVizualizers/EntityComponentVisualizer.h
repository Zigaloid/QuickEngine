#pragma once

#include "CombinedObjJson3DVisualizer.h"
#include "SelectionManager.h"
#include "RenderComponentSelectable.h"
#include "PhysicsBodySelectable.h"
#include "EntityComponent.h"
#include "BgfxGizmoRenderer.h"
#include "CommandHistory.h"
#include "PropertyInspector.h"

// Include primitives header (we now pass primitives through the hierarchy)
#include "BgfxRenderPrimitives.h"

#include <vector>
#include <string>
#include <memory>

namespace ImGuiVisualizers {

/**
 * @brief Editor visualizer for a single entity file.
 *
 * Loads a `CEntityComponent` instance via the embedded `ObjJsonEditor`
 * and renders any `CRenderComponent` descendants in the 3D view.
 */
class EntityComponentVisualizer : public CombinedObjJson3DVisualizer
{
public:
    explicit EntityComponentVisualizer(const char* name = "Entity Editor")
        : CombinedObjJson3DVisualizer(name)
        , m_entityComp(nullptr)
        , m_gizmoMode(GizmoMode::Translate)
    {}

    ~EntityComponentVisualizer() override
    {
        ReleaseEntityComponent();
    }

    void Initialize() override
    {
        m_selectionManager.SetCommandHistory(&m_history);
        CombinedObjJson3DVisualizer::Initialize();
        RegisterEntityActions();
    }

    void Shutdown() override
    {
        ReleaseEntityComponent();
        CombinedObjJson3DVisualizer::Shutdown();
    }

    bool Render(bool* isOpen) override;

protected:
    bool AttachMeshFromPath(const std::string& entityPath) override;

private:
    void ReleaseEntityComponent();
    void RegisterEntityActions();
    void SavePhysicsBodyResources();

    void RegisterPhysicsComponents(ComponentSystem::Component* root);
    void CollectPhysicsComponents(ComponentSystem::Component* comp,
                                 std::vector<CPhysicsBodyComponent*>& out);
    // Updated signature: pass primitives so components that are not CRenderComponent
    // (e.g. physics bodies) can render debug visuals.
    void RenderComponentHierarchy(bgfx::ViewId viewId, BgfxRenderPrimitives& prims, ComponentSystem::Component* comp);

    void RenderSelectionHighlight(bgfx::ViewId viewId, BgfxRenderPrimitives& prims);
    void RenderInspectorPanel();

    CEntityComponent* m_entityComp;
    CSelectionManager  m_selectionManager;
    GizmoMode          m_gizmoMode;

    std::vector<std::shared_ptr<CSelectable>> m_componentSelectables;

    ImVec2 m_viewportMin  = { 0.0f, 0.0f };
    ImVec2 m_viewportSize = { 1.0f, 1.0f };

    CCommandHistory  m_history{ 100 };
    PropertyInspector m_propertyInspector;
    UI::UnifiedActionManager m_actionManager;
};

} // namespace ImGuiVisualizers