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
            m_selectablesRegistered = false;
        }

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

        auto meshRes = m_heightFieldComp->GetMeshResource();
        if (!meshRes || !meshRes->IsLoaded())
        {
            return;
        }
        auto mesh = meshRes->GetMesh();
        if (!mesh)
        {
            return;
        }

        ClearHeightFieldPoints();

        std::cout << "RegisterHeightFieldPoints: Found " << mesh->m_groups.size() << " groups" << std::endl;

        // Iterate through all groups in the mesh
        for (size_t groupIdx = 0; groupIdx < mesh->m_groups.size(); ++groupIdx)
        {
            Group& group = mesh->m_groups[groupIdx];
            std::cout << "  Group " << groupIdx << ": " << group.m_numVertices << " vertices, data="
                << (group.m_vertices ? "valid" : "NULL") << std::endl;

            // Create a selectable for each vertex in the group
            for (uint16_t vertexIdx = 0; vertexIdx < group.m_numVertices; ++vertexIdx)
            {
                auto selectable = std::make_shared<CMeshVertexSelectable>(
                    m_heightFieldComp,
                    meshRes.get(),
                    &mesh->m_groups[groupIdx],
                    vertexIdx,
                    &mesh->m_layout
                );
                m_pointSelectables.push_back(selectable);
                m_selectionManager.AddSelectable(selectable);

                // Debug: print first vertex position
                if (vertexIdx < 3)
                {
                    Vector3f pos = selectable->GetWorldPosition();
                    std::cout << "    Vertex " << vertexIdx << ": (" << pos.x << ", " << pos.y << ", " << pos.z << ")" << std::endl;
                }
            }
        }

        std::cout << "Total selectables registered: " << m_pointSelectables.size() << std::endl;
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

        // Update mesh vertex positions from selectable transforms (apply live; do not push history here)
        for (const auto& selectable : m_selectionManager.GetAllSelected())
        {
            auto vertexSel = std::dynamic_pointer_cast<CMeshVertexSelectable>(selectable);
            if (!vertexSel) continue;

            // Get the world-space position of the point (from the selectable's transform)
            const Matrix4f& worldTransform = vertexSel->GetTransform();
            Vector3f worldPos = worldTransform.ExtractTranslation();

            // Convert to local space relative to the component's world transform
            Vector3f localPos = componentLocalTransform.TransformPoint(worldPos);

            // Set the vertex position in the mesh
            vertexSel->SetLocalPosition(localPos);
            anyChanged = true;
        }

        // If any vertices changed, we need to update the vertex buffer on the GPU
        if (anyChanged)
        {
            auto meshRes = m_heightFieldComp->GetMeshResource();
            if (meshRes && meshRes->IsLoaded())
            {
                auto mesh = meshRes->GetMesh();
                if (mesh && !mesh->m_groups.empty())
                {
                    auto& group = mesh->m_groups[0];
                    if (group.m_vertices && bgfx::isValid(group.m_vbh))
                    {
                        // Recalculate normals based on the new vertex positions
                        RecalculateMeshNormals(group, mesh->m_layout);

                        // Destroy the old vertex buffer
                        bgfx::destroy(group.m_vbh);

                        // Create new vertex buffer with updated data
                        uint16_t stride = mesh->m_layout.getStride();
                        const bgfx::Memory* mem = bgfx::copy(group.m_vertices, group.m_numVertices * stride);
                        group.m_vbh = bgfx::createVertexBuffer(mem, mesh->m_layout);
                    }
                }
            }
        }
    }
    void HeightFieldMeshComponentVisualizer::RecalculateMeshNormals(Group& group, const bgfx::VertexLayout& layout)
    {
        if (!group.m_vertices || !group.m_indices || group.m_numIndices == 0)
            return;

        uint16_t stride = layout.getStride();

        // Temporary storage for accumulated normals (as floats for calculations)
        std::vector<Vector3f> accumulatedNormals(group.m_numVertices, Vector3f(0.0f, 0.0f, 0.0f));

        // Accumulate face normals to vertices
        for (uint32_t i = 0; i < group.m_numIndices; i += 3)
        {
            uint16_t idx0 = group.m_indices[i];
            uint16_t idx1 = group.m_indices[i + 1];
            uint16_t idx2 = group.m_indices[i + 2];

            // Validate indices
            if (idx0 >= group.m_numVertices || idx1 >= group.m_numVertices || idx2 >= group.m_numVertices)
                continue;

            // Get vertex positions
            uint8_t* v0Data = group.m_vertices + (idx0 * stride);
            uint8_t* v1Data = group.m_vertices + (idx1 * stride);
            uint8_t* v2Data = group.m_vertices + (idx2 * stride);

            const float* p0 = reinterpret_cast<const float*>(v0Data);
            const float* p1 = reinterpret_cast<const float*>(v1Data);
            const float* p2 = reinterpret_cast<const float*>(v2Data);

            Vector3f v0(p0[0], p0[1], p0[2]);
            Vector3f v1(p1[0], p1[1], p1[2]);
            Vector3f v2(p2[0], p2[1], p2[2]);

            // Calculate face normal
            Vector3f edge1 = v1 - v0;
            Vector3f edge2 = v2 - v0;
            Vector3f faceNormal = edge1.Cross(edge2);

            // Accumulate to all three vertices
            accumulatedNormals[idx0] += faceNormal;
            accumulatedNormals[idx1] += faceNormal;
            accumulatedNormals[idx2] += faceNormal;
        }

        // Write normalized normals back to vertex buffer, preserving other attributes
        for (uint32_t i = 0; i < group.m_numVertices; ++i)
        {
            uint8_t* vertexData = group.m_vertices + (i * stride);

            // Normalize the accumulated normal
            Vector3f normal = accumulatedNormals[i].Normalize();

            // Encode normal as RGBA8 using the same format as in RegenerateGridMesh
            // Normal format: (normal.x * 0.5 + 0.5, normal.y * 0.5 + 0.5, normal.z * 0.5 + 0.5, 1.0)
            // Then packed as ABGR uint32
            uint8_t nx = static_cast<uint8_t>((normal.x * 0.5f + 0.5f) * 255.0f);
            uint8_t ny = static_cast<uint8_t>((normal.y * 0.5f + 0.5f) * 255.0f);
            uint8_t nz = static_cast<uint8_t>((normal.z * 0.5f + 0.5f) * 255.0f);
            uint8_t nw = 0xFF;  // Alpha channel

            // Pack as ABGR (note: bgfx uses this byte order for packed normals)
            uint32_t packedNormal = (nw << 24) | (nz << 16) | (ny << 8) | nx;

            // Write to the normal attribute in the vertex data
            if (layout.has(bgfx::Attrib::Normal))
            {
                uint16_t offset = layout.getOffset(bgfx::Attrib::Normal);
                uint32_t* normalPtr = reinterpret_cast<uint32_t*>(vertexData + offset);
                *normalPtr = packedNormal;
            }
        }
    }

    void HeightFieldMeshComponentVisualizer::RenderHeightFieldPointSelection(bgfx::ViewId viewId, Rendering::BgfxRenderPrimitives& prims)
    {
        const auto& allSelected = m_selectionManager.GetAllSelected();
        if (allSelected.empty()) return;

        const std::shared_ptr<CSelectable> lastSelected = m_selectionManager.GetSelected();

        for (const auto& selectable : allSelected)
        {
            // Cast to mesh vertex selectable
            auto vertexSel = std::dynamic_pointer_cast<CMeshVertexSelectable>(selectable);
            if (!vertexSel)
            {
                // Try the old height field point selectable for backwards compatibility
                auto pointSel = std::dynamic_pointer_cast<CMeshVertexSelectable>(selectable);
                if (!pointSel) continue;
                pointSel->UpdateTransform();
            }
            else
            {
                vertexSel->UpdateTransform();
            }

            const Vector4f bs = selectable->GetBoundingSphere();
            const Matrix4f& mtx = selectable->GetTransform();
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

        // Check if we need to register selectables (deferred until mesh resource is loaded)
        if (m_heightFieldComp && m_heightFieldComp->isMeshInitialized() && !m_selectablesRegistered)
        {
            RegisterHeightFieldPoints();
            m_selectablesRegistered = true;
        }

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

        // Drag start: snapshot initial positions for selected vertices
        if (nowDragging && !m_wasGizmoDragging)
        {
            m_gizmoInitialEntries.clear();
            for (const auto& sel : m_selectionManager.GetAllSelected())
            {
                auto pointSel = std::dynamic_pointer_cast<CMeshVertexSelectable>(sel);
                if (!pointSel) continue;

                // Snapshot the current world position
                CHeightFieldEditCommand::Entry entry;
                entry.vertexIndex = pointSel->GetVertexIndex();
                entry.before = pointSel->GetWorldPosition();
                entry.after = entry.before;  // placeholder, will be updated during drag
                m_gizmoInitialEntries.push_back(std::move(entry));
            }
        }

        // Update vertices from gizmo drags (applies live changes; no history push here)
        UpdateHeightFieldFromGizmoDrag();

        // Drag end: build one command from initial -> final and push as already-executed
        if (!nowDragging && m_wasGizmoDragging)
        {
            if (!m_gizmoInitialEntries.empty())
            {
                std::vector<CHeightFieldEditCommand::Entry> entries;
                entries.reserve(m_gizmoInitialEntries.size());
                
                // Find which vertices actually changed
                for (auto& e : m_gizmoInitialEntries)
                {
                    // Look up the corresponding selectable to get the current position
                    Vector3f currentPos(0.0f, 0.0f, 0.0f);
                    for (const auto& selectable : m_pointSelectables)
                    {
                        if (selectable->GetVertexIndex() == e.vertexIndex)
                        {
                            currentPos = selectable->GetWorldPosition();
                            break;
                        }
                    }

                    // Only record if position changed significantly
                    Vector3f delta = currentPos - e.before;
                    if (delta.Length() > 0.001f)
                    {
                        CHeightFieldEditCommand::Entry out;
                        out.vertexIndex = e.vertexIndex;
                        out.before = e.before;
                        out.after = currentPos;
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
        }
        else if (m_heightFieldComp) {
            // Show the selected point info if available
            const auto& selected = m_selectionManager.GetSelected();
            if (auto pointSel = std::dynamic_pointer_cast<CMeshVertexSelectable>(selected))
            {
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Selected Point:");
                ImGui::Text("Vertex Index: %u", pointSel->GetVertexIndex());
                
                Vector3f currentPos = pointSel->GetWorldPosition();
                ImGui::Text("Position: (%.2f, %.2f, %.2f)", currentPos.x, currentPos.y, currentPos.z);

                float height = currentPos.y;
                if (ImGui::SliderFloat("Height##PointHeight", &height, -10.0f, 10.0f))
                {
                    // Build a single-entry undoable command and push it
                    Vector3f newPos = currentPos;
                    newPos.y = height;

                    CHeightFieldEditCommand::Entry e;
                    e.vertexIndex = pointSel->GetVertexIndex();
                    e.before = currentPos;
                    e.after = newPos;

                    std::vector<CHeightFieldEditCommand::Entry> entries;
                    entries.push_back(std::move(e));

                    m_history.Push(std::make_unique<CHeightFieldEditCommand>(m_heightFieldComp, std::move(entries)));

                    // Update the selectable transform to reflect the new position
                    pointSel->UpdateTransform();
                }
                ImGui::Separator();
            }

            // Update the property inspector to show the component
            m_propertyInspector.SetObject(m_heightFieldComp);
            m_propertyInspector.RenderContent();
        }
        else {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Height field component not initialized");
        }

        ImGui::EndChild();

        ImGui::End();
        return true;
    }
    void HeightFieldMeshComponentVisualizer::RegenerateGridMesh()
    {
        if (!m_heightFieldComp)
            return;

        auto meshRes = m_heightFieldComp->GetMeshResource();
        if (!meshRes || !meshRes->IsLoaded())
        {
            std::cerr << "RegenerateGridMesh: Mesh resource not loaded" << std::endl;
            return;
        }

        Mesh* mesh = meshRes->GetMesh();
        if (!mesh || mesh->m_groups.empty())
        {
            std::cerr << "RegenerateGridMesh: No mesh groups available" << std::endl;
            return;
        }

        uint32_t xSteps = m_heightFieldComp->GetXSteps();
        uint32_t zSteps = m_heightFieldComp->GetZSteps();
        float stepSize = m_heightFieldComp->GetStepSize();

        // Create flat grid of vertices
        uint32_t vertexCount = (xSteps + 1) * (zSteps + 1);
        uint32_t indexCount = xSteps * zSteps * 6; // 2 triangles per quad = 6 indices

        // Get vertex layout info
        const bgfx::VertexLayout& layout = mesh->m_layout;
        uint16_t stride = layout.getStride();

        // Allocate persistent buffers (member variables)
        uint32_t vertexDataSize = vertexCount * stride;
        m_regeneratedVertexBuffer.resize(vertexDataSize, 0);
        m_regeneratedIndexBuffer.clear();
        m_regeneratedIndexBuffer.reserve(indexCount);

        // Generate grid vertices with position and normals
        for (uint32_t z = 0; z <= zSteps; ++z)
        {
            for (uint32_t x = 0; x <= xSteps; ++x)
            {
                uint32_t vertexIdx = z * (xSteps + 1) + x;
                uint8_t* vertexPtr = m_regeneratedVertexBuffer.data() + (vertexIdx * stride);

                // Write position
                if (layout.has(bgfx::Attrib::Position))
                {
                    uint16_t offset = layout.getOffset(bgfx::Attrib::Position);
                    float* posPtr = reinterpret_cast<float*>(vertexPtr + offset);
                    posPtr[0] = x * stepSize;
                    posPtr[1] = 0.0f;
                    posPtr[2] = z * stepSize;
                }

                // Write normal pointing up (0, 1, 0) encoded as RGBA8
                if (layout.has(bgfx::Attrib::Normal))
                {
                    uint16_t offset = layout.getOffset(bgfx::Attrib::Normal);
                    uint32_t* normalPtr = reinterpret_cast<uint32_t*>(vertexPtr + offset);
                    *normalPtr = 0xFFFF7F80;  // ABGR format for normal (0, 1, 0)
                }

                // Write TexCoord0 as grid coordinates
                if (layout.has(bgfx::Attrib::TexCoord0))
                {
                    uint16_t offset = layout.getOffset(bgfx::Attrib::TexCoord0);
                    float* texCoordPtr = reinterpret_cast<float*>(vertexPtr + offset);
                    texCoordPtr[0] = x / float(xSteps);
                    texCoordPtr[1] = z / float(zSteps);
                }
            }
        }

        // Generate indices (quad tessellation)
        for (uint32_t z = 0; z < zSteps; ++z)
        {
            for (uint32_t x = 0; x < xSteps; ++x)
            {
                uint16_t v0 = (z * (xSteps + 1)) + x;
                uint16_t v1 = v0 + 1;
                uint16_t v2 = v0 + (xSteps + 1);
                uint16_t v3 = v2 + 1;

                // First triangle (CCW)
                m_regeneratedIndexBuffer.push_back(v0);
                m_regeneratedIndexBuffer.push_back(v2);
                m_regeneratedIndexBuffer.push_back(v1);

                // Second triangle (CCW)
                m_regeneratedIndexBuffer.push_back(v1);
                m_regeneratedIndexBuffer.push_back(v2);
                m_regeneratedIndexBuffer.push_back(v3);
            }
        }

        // Update the mesh group
        Group& group = mesh->m_groups[0];

        // Point to our persistent buffers
        group.m_vertices = m_regeneratedVertexBuffer.data();
        group.m_indices = m_regeneratedIndexBuffer.data();
        group.m_numVertices = vertexCount;
        group.m_numIndices = static_cast<uint32_t>(m_regeneratedIndexBuffer.size());

        // Recreate GPU vertex buffer
        if (bgfx::isValid(group.m_vbh))
        {
            bgfx::destroy(group.m_vbh);
        }
        const bgfx::Memory* vbMem = bgfx::copy(group.m_vertices, vertexDataSize);
        group.m_vbh = bgfx::createVertexBuffer(vbMem, layout);

        // Recreate GPU index buffer
        if (bgfx::isValid(group.m_ibh))
        {
            bgfx::destroy(group.m_ibh);
        }
        uint32_t indexDataSize = static_cast<uint32_t>(m_regeneratedIndexBuffer.size()) * sizeof(uint16_t);
        const bgfx::Memory* ibMem = bgfx::copy(group.m_indices, indexDataSize);
        group.m_ibh = bgfx::createIndexBuffer(ibMem);

        // Update primitives
        group.m_prims.clear();
        Primitive prim;
        prim.m_startIndex = 0;
        prim.m_numIndices = static_cast<uint32_t>(m_regeneratedIndexBuffer.size());
        prim.m_startVertex = 0;
        prim.m_numVertices = vertexCount;

        // Calculate bounding volumes
        float maxX = xSteps * stepSize;
        float maxZ = zSteps * stepSize;

        bx::Aabb aabb;
        aabb.min = { 0.0f, -0.1f, 0.0f };
        aabb.max = { maxX, 0.1f, maxZ };
        prim.m_aabb = aabb;

        bx::Sphere sphere;
        sphere.center = { maxX * 0.5f, 0.0f, maxZ * 0.5f };
        sphere.radius = bx::length({ maxX * 0.5f, 0.0f, maxZ * 0.5f });
        prim.m_sphere = sphere;

        // OBB: 4x4 transform matrix
        bx::Obb obb;
        float mtx[16];
        bx::mtxIdentity(mtx);
        mtx[0] = maxX * 0.5f;  // scale x
        mtx[5] = 0.1f;         // scale y (small, plane is flat)
        mtx[10] = maxZ * 0.5f;  // scale z
        mtx[12] = maxX * 0.5f;  // translate x
        mtx[13] = 0.0f;         // translate y
        mtx[14] = maxZ * 0.5f;  // translate z
        std::memcpy(obb.mtx, mtx, sizeof(mtx));
        prim.m_obb = obb;

        group.m_prims.push_back(prim);

        // Update group bounding volumes
        group.m_sphere = sphere;
        group.m_aabb = aabb;
        group.m_obb = obb;

        // Immediately re-register selectables with the updated mesh
        RegisterHeightFieldPoints();

        std::cout << "RegenerateGridMesh: Created grid with " << vertexCount << " vertices and "
            << prim.m_numIndices << " indices" << std::endl;
    }

    std::string HeightFieldMeshComponentVisualizer::MakeAssetPath(const std::string& absolutePath) const
    {
        // Get the working directory from AppConfig
        const Core::AppConfig& config = Core::AppConfig::Instance();

        // Normalize path separators to forward slashes and convert to lowercase
        std::string normalized = absolutePath;
        for (auto& c : normalized)
        {
            if (c == '\\') c = '/';
            c = std::tolower(c);
        }

        // Find the /assets/ substring
        const std::string assetsMarker = "/assets/";
        size_t assetsPos = normalized.find(assetsMarker);

        if (assetsPos != std::string::npos)
        {
            // Extract from /assets/ onwards (including the /assets/ part)
            // Use original path to preserve casing
            size_t originalAssetsPos = absolutePath.find("assets");
            if (originalAssetsPos != std::string::npos)
            {
                // Find the slash before "assets"
                while (originalAssetsPos > 0 && absolutePath[originalAssetsPos - 1] != '/' && absolutePath[originalAssetsPos - 1] != '\\')
                {
                    originalAssetsPos--;
                }
            }

            std::string assetPath = originalAssetsPos != std::string::npos
                ? absolutePath.substr(originalAssetsPos)
                : normalized.substr(assetsPos + 1);

            // Normalize separators in the result
            for (auto& c : assetPath)
            {
                if (c == '\\') c = '/';
            }

            // Combine with working directory
            // Remove trailing slash from working dir if present, then add the asset path
            std::string result = ".";
            result += "/" + assetPath;

            return result;
        }

        // If /assets/ not found, return path as-is with working directory prefix
        return absolutePath;
    }

    void HeightFieldMeshComponentVisualizer::RegisterHeightFieldActions()
    {
        auto& am = GetEditor().GetActionManager();

        am.RegisterAction({
            .path = "File.Save",
            .description = "Save the current height field mesh.",
            .targets = UI::ActionTarget::Toolbar | UI::ActionTarget::Menu | UI::ActionTarget::Console,
            .callback = [this]()
            {
                // Save the editor document (the component's JSON representation)
                if (GetEditor().Save())
                {
                    // Also save the mesh to binary format if we have a height field component
                    if (m_heightFieldComp)
                    {
                        // Generate a mesh file path based on the document path
                        std::string docPath = GetEditor().GetFilePath();
                        // Replace extension: .hfield.obj.json -> .mesh.bin
                        size_t pos = docPath.find(".hfield.obj.json");
                        if (pos != std::string::npos)
                        {
                            std::string meshPath = docPath.substr(0, pos) + ".mesh.bin";

                            // Convert to relative asset path for AppConfig
                            std::string relativeMeshPath = MakeAssetPath(meshPath);

                            if (m_heightFieldComp->SaveMesh(relativeMeshPath))
                            {
                                std::cout << "Successfully saved height field mesh to: " << relativeMeshPath << std::endl;
                            }
                            else
                            {
                                std::cerr << "Failed to save height field mesh to: " << relativeMeshPath << std::endl;
                            }
                        }
                    }
                }
            },
            .isEnabled = [this]() { return m_heightFieldComp != nullptr && GetEditor().IsLoaded(); },
            .sortPriority = 10
            });

        am.RegisterAction({
            .path = "Edit.RegenerateGridMesh",
            .description = "Regenerate the mesh geometry as a flat grid based on current Height Field parameters.",
            .targets = UI::ActionTarget::Toolbar | UI::ActionTarget::Menu | UI::ActionTarget::Console,
            .callback = [this]()
            {
                RegenerateGridMesh();
            },
            .isEnabled = [this]() { return m_heightFieldComp != nullptr && GetEditor().IsLoaded(); },
            .sortPriority = 5
            });
    }
} // namespace ImGuiVisualizers