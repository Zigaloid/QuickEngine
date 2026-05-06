#include "EntityComponentVisualizer.h"
#include "CoreSystem/CoreSystem.h"
#include <bx/bounds.h>
#include <algorithm>
#include <cfloat>

// include physics body header to detect and call DebugRender
#include "PhysicsBodyComponent.h"
#include "PhysicsBodySelectable.h"

namespace ImGuiVisualizers {

bool EntityComponentVisualizer::AttachMeshFromPath(const std::string& entityPath)
{
    // Get the object currently held by the editor
    CEntityComponent* newEntity = dynamic_cast<CEntityComponent*>(GetEditor().GetObject());
    if (!newEntity)
    {
        std::cerr << "EntityComponentVisualizer: Failed to cast loaded object to CEntityComponent" << std::endl;
        return false;
    }

    if (newEntity != m_entityComp)
    {
        ReleaseEntityComponent();
        m_entityComp = newEntity;

        if (!m_entityComp->IsInitialized())
        {
            m_entityComp->Initialize();
        }
    }
    else
    {
        // Same instance: clear selection/render state but do not Shutdown the editor-owned instance./
        m_selectionManager.ClearSelectables();
        m_componentSelectables.clear();
    }

    // Install render callback
    Get3DView().SetRenderCallback([this](bgfx::ViewId viewId, BgfxRenderPrimitives& prims) {
        if (!m_entityComp) return;
        // Pass primitives through so non-render components (physics bodies) can draw debug visuals.
        RenderComponentHierarchy(viewId, prims, m_entityComp);
        m_selectionManager.RenderSelectionGizmo(Get3DView().GetFrameBuffer(), m_gizmoMode, 2.0f);
    });

    // Register physics body components as selectables (we only want to select physics bodies)
    RegisterPhysicsComponents(m_entityComp);

    return true;
}

void EntityComponentVisualizer::ReleaseEntityComponent()
{
    Get3DView().SetRenderCallback(nullptr);
    m_selectionManager.ShutdownGizmo();
    m_selectionManager.ClearSelectables();
    m_componentSelectables.clear();

    if (m_entityComp)
    {
        auto* manager = Core::CoreSystem::GetComponentManager();
        if (manager && manager->IsInitialized() && m_entityComp->IsInitialized())
        {
            m_entityComp->Shutdown();
        }
        m_entityComp = nullptr;
    }
}

void EntityComponentVisualizer::RegisterEntityActions()
{
    auto& am = m_actionManager;

    // Save action
    am.RegisterAction({
        .path = "File.Save",
        .description = "Save the currently opened object to disk",
        .targets = UI::ActionTarget::Toolbar | UI::ActionTarget::Menu | UI::ActionTarget::Console,
        .callback = [this]() {
            m_editor.Save();
            // Also save any referenced physics body resources that may have been
            // modified via the gizmo or the property inspector.
            SavePhysicsBodyResources();
        },
        .isEnabled = [this]() { return m_editor.IsLoaded(); },
        .sortPriority = 10
        });

    // Reload action
    am.RegisterAction({
        .path = "File.Reload",
        .description = "Reload the object from disk (discard changes)",
        .targets = UI::ActionTarget::Toolbar | UI::ActionTarget::Menu | UI::ActionTarget::Console,
        .callback = [this]() { m_editor.Reload(); },
        .isEnabled = [this]() { return m_editor.IsLoaded(); },
        .sortPriority = 20
        });

    am.RegisterAction({
        .path = "View.GizmoMode.Translate",
        .description = "Set gizmo to Translate mode",
        .targets = UI::ActionTarget::Menu | UI::ActionTarget::Console,
        .callback = [this]() { m_gizmoMode = GizmoMode::Translate; },
        .isEnabled = [this]() { return m_entityComp != nullptr; },
        .isChecked = [this]() { return m_gizmoMode == GizmoMode::Translate; },
        .sortPriority = 10
        });

    am.RegisterAction({
        .path = "View.GizmoMode.Scale",
        .description = "Set gizmo to Scale mode",
        .targets = UI::ActionTarget::Menu | UI::ActionTarget::Console,
        .callback = [this]() { m_gizmoMode = GizmoMode::Scale; },
        .isEnabled = [this]() { return m_entityComp != nullptr; },
        .isChecked = [this]() { return m_gizmoMode == GizmoMode::Scale; },
        .sortPriority = 20
        });

    am.RegisterAction({
        .path = "View.GizmoMode.Rotate",
        .description = "Set gizmo to Rotate mode",
        .targets = UI::ActionTarget::Menu | UI::ActionTarget::Console,
        .callback = [this]() { m_gizmoMode = GizmoMode::Rotate; },
        .isEnabled = [this]() { return m_entityComp != nullptr; },
        .isChecked = [this]() { return m_gizmoMode == GizmoMode::Rotate; },
        .sortPriority = 30
        });
}

void EntityComponentVisualizer::SavePhysicsBodyResources()
{
    if (!m_entityComp) return;

    std::vector<CPhysicsBodyComponent*> physComps;
    CollectPhysicsComponents(m_entityComp, physComps);

    for (CPhysicsBodyComponent* pc : physComps)
    {
        if (!pc) continue;        
        auto& resRef = pc->GetBodyResourceRef();
        auto resource = resRef.GetResourceAs<CPhysicsBodyResource>();        

        if (resource && !resource->GetPath().empty())
        {
            resource->Write(resource->GetPath().c_str());
        }
    }
}

void EntityComponentVisualizer::RegisterPhysicsComponents(ComponentSystem::Component* root)
{
    std::vector<CPhysicsBodyComponent*> physComps;
    CollectPhysicsComponents(root, physComps);

    for (CPhysicsBodyComponent* pc : physComps)
    {
        auto selectable = std::make_shared<CPhysicsBodySelectable>(pc);
        // Initialize selectable from the component state
        selectable->UpdateFromComponent();
        m_componentSelectables.push_back(selectable);
        m_selectionManager.AddSelectable(selectable);
    }
}

void EntityComponentVisualizer::CollectPhysicsComponents(ComponentSystem::Component* comp,
    std::vector<CPhysicsBodyComponent*>& out)
{
    if (!comp) return;

    if (auto* pc = dynamic_cast<CPhysicsBodyComponent*>(comp))
    {
        out.push_back(pc);
    }

    for (auto* child : comp->GetChildren())
    {
        CollectPhysicsComponents(child, out);
    }
}

void EntityComponentVisualizer::RenderComponentHierarchy(bgfx::ViewId viewId, BgfxRenderPrimitives& prims, ComponentSystem::Component* comp)
{
    if (!comp) return;

    // First, if this is a render component call its Render() as before.
    if (auto* renderComp = dynamic_cast<CRenderComponent*>(comp))
    {
        if (renderComp->IsActive())
        {
            renderComp->Render(viewId);
        }
    }

    // If this is a physics body component, call its DebugRender so it can draw its collision shape.
    if (auto* physComp = dynamic_cast<CPhysicsBodyComponent*>(comp))
    {
        if (physComp->IsActive())
        {
            CPhysicsBodyResource* res = physComp->GetBodyResource();
            if (res)
            {
                Matrix4f transform = res->GetTransform();

                // If the owning entity has a CRenderComponent, multiply by its model matrix
                ComponentSystem::Component* parent = comp->GetParent();
                if (parent)
                {
                    for (auto* sibling : parent->GetChildren())
                    {
                        if (auto* renderComp = dynamic_cast<CRenderComponent*>(sibling))
                        {
                            transform = (*renderComp->GetModelMatrix()) * transform;
                            break;
                        }
                    }
                }

                physComp->DebugRender(viewId, transform);
            }
        }
    }

    // Update any selectable associated with this physics component so the
    // selection system has up-to-date transform / bounding-sphere data.
    if (auto* physComp2 = dynamic_cast<CPhysicsBodyComponent*>(comp))
    {
        for (auto& sel : m_componentSelectables)
        {
            if (!sel) continue;
            if (sel->GetOwner() == physComp2)
            {
                // Try to cast to our physics selectable and update
                auto physSel = std::dynamic_pointer_cast<CPhysicsBodySelectable>(sel);
                if (physSel)
                    physSel->UpdateFromComponent();
                break;
            }
        }
    }

    for (auto* child : comp->GetChildren())
    {
        RenderComponentHierarchy(viewId, prims, child);
    }
}

void EntityComponentVisualizer::RenderSelectionHighlight(bgfx::ViewId viewId,
    BgfxRenderPrimitives& prims)
{
    const auto& allSelected = m_selectionManager.GetAllSelected();
    if (allSelected.empty()) return;

    const std::shared_ptr<CSelectable> lastSelected = m_selectionManager.GetSelected();

    for (const auto& selectable : allSelected)
    {
        if (!selectable) continue;

        // Skip inactive owners (if the owner is a Component)
        CReflectedBase* owner = selectable->GetOwner();
        if (owner)
        {
            if (auto* compOwner = dynamic_cast<ComponentSystem::Component*>(owner))
            {
                if (!compOwner->IsActive())
                    continue;
            }
        }

        const Vector4f bs = selectable->GetBoundingSphere();
        const float* m = selectable->GetTransform().data();

        const float cx = m[0] * bs.x + m[4] * bs.y + m[8] * bs.z + m[12];
        const float cy = m[1] * bs.x + m[5] * bs.y + m[9] * bs.z + m[13];
        const float cz = m[2] * bs.x + m[6] * bs.y + m[10] * bs.z + m[14];

        const float sx = bx::sqrt(m[0] * m[0] + m[1] * m[1] + m[2] * m[2]);
        const float sy = bx::sqrt(m[4] * m[4] + m[5] * m[5] + m[6] * m[6]);
        const float sz = bx::sqrt(m[8] * m[8] + m[9] * m[9] + m[10] * m[10]);
        const float radius = bs.w * bx::max(sx, bx::max(sy, sz));
        const float r = radius > 0.0f ? radius : 0.5f;

        float highlightMtx[16];
        bx::mtxSRT(highlightMtx, r, r, r, 0.0f, 0.0f, 0.0f, cx, cy, cz);

        const uint32_t color = (selectable == lastSelected)
            ? 0xff00ffff   // yellow (ABGR)
            : 0xffffffff;  // white  (ABGR)

        prims.RenderSphere(viewId, highlightMtx, color);
    }
}

void EntityComponentVisualizer::RenderInspectorPanel()
{
    if (!m_editor.IsLoaded())
    {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No entity loaded");
        return;
    }

    // Sync the property inspector to the most-recently selected object.
    // When a physics body is selected, point the inspector at the underlying
    // PhysicsBodyResource so that gizmo transform edits are reflected directly.
    const auto& lastSelected = m_selectionManager.GetSelected();
    CReflectedBase* inspectorTarget = nullptr;

    if (lastSelected)
    {
        auto physSel = std::dynamic_pointer_cast<CPhysicsBodySelectable>(lastSelected);
        if (physSel && physSel->GetComponent())
        {
            CPhysicsBodyResource* res = physSel->GetComponent()->GetBodyResource();
            if (res)
                inspectorTarget = res;
        }
        if (!inspectorTarget)
            inspectorTarget = lastSelected->GetOwner();
    }

    if (inspectorTarget != m_propertyInspector.GetObject())
        m_propertyInspector.SetObject(inspectorTarget);

    if (ImGui::BeginTabBar("##InspectorTabs"))
    {
        if (ImGui::BeginTabItem("Properties"))
        {
            if (inspectorTarget)
                m_propertyInspector.RenderContent();
            else
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No object selected");
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Object"))
        {
            m_editor.RenderInspectorInline();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
}

bool EntityComponentVisualizer::Render(bool* isOpen)
{
    // Undo/Redo shortcuts
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Z)) m_history.Undo();
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Y)) m_history.Redo();

    // Build window title
    std::string title = m_windowName;
    if (!m_fileName.empty()) {
        title += " - " + m_fileName;
    }

    if (!ImGui::Begin(title.c_str(), isOpen, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        ImGui::End();
        return false;
    }

    if (ImGui::BeginMenuBar())
    {
        m_actionManager.RenderMenuBar();
        ImGui::EndMenuBar();
    }

    m_actionManager.RenderToolbar();

    // Gizmo combo
    ImGui::SameLine();
    ImGui::SetNextItemWidth(110.0f);
    static constexpr const char* k_gizmoModeLabels[] = { "Translate", "Scale", "Rotate" };
    int gizmoModeIdx = static_cast<int>(m_gizmoMode);
    if (ImGui::Combo("##GizmoMode", &gizmoModeIdx, k_gizmoModeLabels, 3))
    {
        m_gizmoMode = static_cast<GizmoMode>(gizmoModeIdx);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Gizmo Mode");
    }

    ImVec2 avail = ImGui::GetContentRegionAvail();
    float leftW = avail.x * 0.65f;
    float rightW = avail.x - leftW;

    ImGui::BeginChild("##Left3D", ImVec2(leftW, 0), false);
    ImVec2 leftAvail = ImGui::GetContentRegionAvail();
    if (leftAvail.x < 1.0f) leftAvail.x = 1.0f;
    if (leftAvail.y < 1.0f) leftAvail.y = 1.0f;

    m_view.RenderContent(leftAvail);

    m_viewportMin = ImGui::GetItemRectMin();
    m_viewportSize = ImGui::GetItemRectSize();

    m_selectionManager.SetViewInfo(Get3DView().GetCamera(), m_viewportMin, m_viewportSize);

    if (!ImGui::GetIO().KeyAlt)
    {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
            !ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            const ImVec2 mouse = ImGui::GetMousePos();
            const bool inViewport = mouse.x >= m_viewportMin.x &&
                mouse.y >= m_viewportMin.y &&
                mouse.x < m_viewportMin.x + m_viewportSize.x &&
                mouse.y < m_viewportMin.y + m_viewportSize.y;
            if (inViewport)
            {
                m_selectionManager.PickAtCursor();
            }
        }
    }

    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("##RightInspector", ImVec2(rightW, 0), false);
    RenderInspectorPanel();
    ImGui::EndChild();

    ImGui::End();
    return true;
}

} // namespace ImGuiVisualizers