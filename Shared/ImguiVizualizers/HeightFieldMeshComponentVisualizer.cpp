#include "HeightFieldMeshComponentVisualizer.h"
#include "HeightFieldMeshComponentVisualizer.h"
#include <iostream>
#include "imgui/imgui.h"
#include <bx/bounds.h>
#include <algorithm>
#include <cmath>

#include "HeightFieldEditCommand.h"

namespace ImGuiVisualizers {


bool HeightFieldMeshComponentVisualizer::AttachMeshFromPath(const std::string& meshPath)
{
    // Get the object currently held by the editor
    CReflectedBase* editorObject = GetEditor().GetObject();
    if (!editorObject)
    {
        std::cerr << "HeightFieldMeshComponentVisualizer: No object loaded in editor" << std::endl;
        return false;
    }

    CHeightFieldMeshComponent* newComponent = dynamic_cast<CHeightFieldMeshComponent*>(editorObject);
    if (!newComponent)
    {
        std::cerr << "HeightFieldMeshComponentVisualizer: Failed to cast loaded object to CHeightFieldMeshComponent" << std::endl;
        return false;
    }

    if (newComponent != m_heightFieldComp)
    {
        ReleaseHeightFieldComponent();
        m_heightFieldComp = newComponent;

        if (!m_heightFieldComp->IsInitialized())
        {
            m_heightFieldComp->Initialize();
        }
    }
    else
    {
        // Same instance: clear selection but keep the component
        m_selectionManager.ClearSelection();
        m_pointSelectables.clear();
    }

    // Register height field points as selectables
    RegisterHeightFieldPoints();

    // Install render callback that includes selection rendering
    Get3DView().SetRenderCallback([this](bgfx::ViewId viewId, Rendering::BgfxRenderPrimitives& prims) {
        if (!m_heightFieldComp) return;
        m_heightFieldComp->Render(viewId);
        RenderHeightFieldPointSelection(viewId, prims);

    });

    return true;
}

void HeightFieldMeshComponentVisualizer::ReleaseHeightFieldComponent()
{
    // Clear render callback to avoid stale calls
    Get3DView().SetRenderCallback(nullptr);

    // Shutdown gizmo and clear selectables
    m_selectionManager.ShutdownGizmo();
    m_selectionManager.ClearSelectables();
    ClearHeightFieldPoints();

    m_heightFieldComp = nullptr;
}

void HeightFieldMeshComponentVisualizer::RegisterHeightFieldPoints()
{
    if (!m_heightFieldComp)
        return;

    ClearHeightFieldPoints();

    uint32_t xSteps = m_heightFieldComp->GetXSteps();
    uint32_t zSteps = m_heightFieldComp->GetZSteps();

    // Register all height field points
    for (uint32_t z = 0; z <= zSteps; ++z)
    {
        for (uint32_t x = 0; x <= xSteps; ++x)
        {
            auto pointSelectable = std::make_shared<CHeightFieldPointSelectable>(m_heightFieldComp, x, z);
            m_pointSelectables.push_back(pointSelectable);
            m_selectionManager.AddSelectable(pointSelectable);
        }
    }
}

void HeightFieldMeshComponentVisualizer::ClearHeightFieldPoints()
{
    for (const auto& point : m_pointSelectables)
    {
        m_selectionManager.RemoveSelectable(point);
    }
    m_pointSelectables.clear();
}

void HeightFieldMeshComponentVisualizer::UpdateHeightFieldFromGizmoDrag()
{
    if (!m_heightFieldComp || m_selectionManager.GetAllSelected().empty())
        return;

    // Get the component's world transform to convert world positions back to local
    CTransformComponent* transformComp = m_heightFieldComp->FindSibling<CTransformComponent>();
    if (!transformComp)
    {
        auto* entity = dynamic_cast<CEntityComponent*>(m_heightFieldComp->GetParent());
        if (entity)
            transformComp = entity->FindChild<CTransformComponent>();
    }

    Matrix4f componentWorldTransform = transformComp ? transformComp->GetTransform() : Matrix4f::GetIdentity();
    Matrix4f componentLocalTransform = componentWorldTransform.Inverse();

    bool anyChanged = false;

    // Update height field data from selectable transforms (apply live; do not push history here)
    for (const auto& selectable : m_selectionManager.GetAllSelected())
    {
        auto pointSel = std::dynamic_pointer_cast<CHeightFieldPointSelectable>(selectable);
        if (!pointSel) continue;

        // Get the world-space position of the point (from the selectable's transform)
        const Matrix4f& worldTransform = pointSel->GetTransform();
        Vector3f worldPos = worldTransform.ExtractTranslation();

        // Convert to local space relative to the component's world transform
        Vector3f localPos = componentLocalTransform.TransformPoint(worldPos);

        // Extract the Y value as the new height
        float newHeight = localPos.y;

        // Get current height to check if it changed
        float oldHeight = m_heightFieldComp->GetHeightAt(pointSel->GetXIndex(), pointSel->GetZIndex());
        if (std::abs(newHeight - oldHeight) > 0.001f)  // Small epsilon to avoid floating point noise
        {
            m_heightFieldComp->SetHeightAt(pointSel->GetXIndex(), pointSel->GetZIndex(), newHeight);
            anyChanged = true;
        }
    }

    // If any heights changed, rebuild the mesh (visual update); history is handled by start/end logic elsewhere.
    if (anyChanged)
    {
        m_heightFieldComp->RebuildMesh();
        m_heightFieldComp->ForceRenderStateReset();
    }
}

void HeightFieldMeshComponentVisualizer::RenderHeightFieldPointSelection(bgfx::ViewId viewId, Rendering::BgfxRenderPrimitives& prims)
{
    const auto& allSelected = m_selectionManager.GetAllSelected();
    if (allSelected.empty()) return;

    const std::shared_ptr<CSelectable> lastSelected = m_selectionManager.GetSelected();

    for (const auto& selectable : allSelected)
    {
        auto pointSel = std::dynamic_pointer_cast<CHeightFieldPointSelectable>(selectable);
        if (!pointSel) continue;

        pointSel->UpdateTransform();

        const Vector4f bs = pointSel->GetBoundingSphere();
        const Matrix4f& mtx = pointSel->GetTransform();
        const float* m = mtx.data();

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

        // Most-recently-picked object renders in yellow; others in white
        const uint32_t color = (selectable == lastSelected)
            ? 0xff00ffff   // yellow (ABGR)
            : 0xffffffff;  // white  (ABGR)

        prims.RenderSphere(viewId, highlightMtx, color);
    }
}


bool HeightFieldMeshComponentVisualizer::Render(bool* isOpen)
{
    // Support undo/redo keyboard shortcuts
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

    // Render menu bar (from editor actions)
    if (ImGui::BeginMenuBar())
    {
        m_editor.GetActionManager().RenderMenuBar();
        ImGui::EndMenuBar();
    }

    // Render toolbar (from editor actions)
    m_editor.GetActionManager().RenderToolbar();

    // Layout: left = 3D view (approx 65%), right = inspector (35%)
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float leftW = avail.x * 0.65f;
    float rightW = avail.x - leftW;

    // Left pane (3D view)
    ImGui::BeginChild("##Left3D", ImVec2(leftW, 0), false);
    ImVec2 leftAvail = ImGui::GetContentRegionAvail();
    if (leftAvail.x < 1.0f) leftAvail.x = 1.0f;
    if (leftAvail.y < 1.0f) leftAvail.y = 1.0f;

    m_view.RenderContent(leftAvail);

    // Get viewport info for selection
    ImVec2 viewportMin = ImGui::GetItemRectMin();
    ImVec2 viewportSize = ImGui::GetItemRectSize();

    m_selectionManager.SetViewInfo(Get3DView().GetCamera(), viewportMin, viewportSize);

    // Handle viewport interactions
    {
        const ImVec2 mouse = ImGui::GetMousePos();
        const bool inViewport =
            mouse.x >= viewportMin.x && mouse.y >= viewportMin.y &&
            mouse.x < viewportMin.x + viewportSize.x &&
            mouse.y < viewportMin.y + viewportSize.y;

        // Delete key ? only when mouse is over the viewport
        if (inViewport &&
            ImGui::IsKeyPressed(ImGuiKey_Delete, /*repeat=*/false) &&
            m_heightFieldComp && !m_selectionManager.IsGizmoDragging())
        {
            // Could implement delete functionality here if needed
        }

        // Left-click pick (Alt = camera pan, skip picking)
        if (!ImGui::GetIO().KeyAlt)
        {
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
                !ImGui::IsMouseDragging(ImGuiMouseButton_Left))
            {
                if (inViewport)
                    m_selectionManager.PickAtCursor();
            }
        }
    }

    // Render the selection gizmo and handle box selection
    m_selectionManager.RenderSelectionGizmo(Get3DView().GetFrameBuffer(), GizmoMode::Translate, 2.0f);

    // Detect gizmo drag start/end and snapshot state accordingly
    const bool nowDragging = m_selectionManager.IsGizmoDragging();

    // Drag start: snapshot initial heights for selected points
    if (nowDragging && !m_wasGizmoDragging)
    {
        m_gizmoInitialEntries.clear();
        for (const auto& sel : m_selectionManager.GetAllSelected())
        {
            auto pointSel = std::dynamic_pointer_cast<CHeightFieldPointSelectable>(sel);
            if (!pointSel) continue;

            CHeightFieldEditCommand::Entry entry;
            entry.xIndex = pointSel->GetXIndex();
            entry.zIndex = pointSel->GetZIndex();
            entry.before = m_heightFieldComp->GetHeightAt(entry.xIndex, entry.zIndex);
            entry.after = entry.before; // placeholder
            m_gizmoInitialEntries.push_back(std::move(entry));
        }
    }

    // Update height field points from gizmo drags (applies live changes; no history push here)
    UpdateHeightFieldFromGizmoDrag();

    // Drag end: build one command from initial -> final and push as already-executed
    if (!nowDragging && m_wasGizmoDragging)
    {
        if (!m_gizmoInitialEntries.empty())
        {
            std::vector<CHeightFieldEditCommand::Entry> entries;
            entries.reserve(m_gizmoInitialEntries.size());
            for (auto& e : m_gizmoInitialEntries)
            {
                float after = m_heightFieldComp->GetHeightAt(e.xIndex, e.zIndex);
                if (std::abs(after - e.before) > 0.001f)
                {
                    CHeightFieldEditCommand::Entry out;
                    out.xIndex = e.xIndex;
                    out.zIndex = e.zIndex;
                    out.before = e.before;
                    out.after = after;
                    entries.push_back(std::move(out));
                }
            }

            if (!entries.empty())
            {
                // The changes have already been applied live; record them without re-executing.
                m_history.PushAlreadyExecuted(std::make_unique<CHeightFieldEditCommand>(m_heightFieldComp, std::move(entries)));
            }
        }
        m_gizmoInitialEntries.clear();
    }

    // save current dragging state for next frame
    m_wasGizmoDragging = nowDragging;

    ImGui::EndChild();

    ImGui::SameLine();

    // Right pane (property inspector)
    ImGui::BeginChild("##RightInspector", ImVec2(rightW, 0), false);

    if (!m_editor.IsLoaded()) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
            "No height field loaded. Use the Asset Browser to open a .hfield.obj.json file.");
    } else if (m_heightFieldComp) {
        // Show the selected point info if available
        const auto& selected = m_selectionManager.GetSelected();
        if (auto pointSel = std::dynamic_pointer_cast<CHeightFieldPointSelectable>(selected))
        {
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Selected Point:");
            ImGui::Text("X Index: %u", pointSel->GetXIndex());
            ImGui::Text("Z Index: %u", pointSel->GetZIndex());

            float currentHeight = m_heightFieldComp->GetHeightAt(pointSel->GetXIndex(), pointSel->GetZIndex());
            float height = currentHeight;
            if (ImGui::SliderFloat("Height##PointHeight", &height, -10.0f, 10.0f))
            {
                // Build a single-entry undoable command and push it. Push() will Execute().
                CHeightFieldEditCommand::Entry e;
                e.xIndex = pointSel->GetXIndex();
                e.zIndex = pointSel->GetZIndex();
                e.before = currentHeight;
                e.after = height;

                std::vector<CHeightFieldEditCommand::Entry> entries;
                entries.push_back(std::move(e));

                m_history.Push(std::make_unique<CHeightFieldEditCommand>(m_heightFieldComp, std::move(entries)));

                // Update the selectable transform to reflect the new height
                pointSel->UpdateTransform();
            }
            ImGui::Separator();
        }

        // Update the property inspector to show the component
        m_propertyInspector.SetObject(m_heightFieldComp);
        m_propertyInspector.RenderContent();
    } else {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Height field component not initialized");
    }

    ImGui::EndChild();

    ImGui::End();
    return true;
}

} // namespace ImGuiVisualizers
