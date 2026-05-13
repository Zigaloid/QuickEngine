#include "TransformCommand.h"
#include "../Utils/MathUtils.h"
#include <cfloat>
#include <cmath>
#include <algorithm>
#include <functional>
#include <imgui/imgui.h>
#include "SelectionManager.h"

namespace ImGuiVisualizers {

    // ── CSelectionManager ─────────────────────────────────────────────────────

    void CSelectionManager::SetViewInfo(Bgfx3DCamera& camera,
        const ImVec2& viewportMin,
        const ImVec2& viewportSize)
    {
        m_camera = &camera;
        m_viewportMin = viewportMin;
        m_viewportSize = viewportSize;
    }

    bool CSelectionManager::IsMouseInViewport() const
    {
        const ImVec2 mouse = ImGui::GetMousePos();
        return mouse.x >= m_viewportMin.x
            && mouse.x <= m_viewportMin.x + m_viewportSize.x
            && mouse.y >= m_viewportMin.y
            && mouse.y <= m_viewportMin.y + m_viewportSize.y;
    }

    void CSelectionManager::AddSelectable(std::shared_ptr<CSelectable> selectable)
    {
        if (selectable)
            m_selectables.push_back(std::move(selectable));
    }

    void CSelectionManager::RemoveSelectable(const std::shared_ptr<CSelectable>& selectable)
    {
        auto it = std::find(m_selectables.begin(), m_selectables.end(), selectable);
        if (it != m_selectables.end())
            m_selectables.erase(it);

        RemoveFromSelection(selectable);

        if (m_lastSelected == selectable)
            m_lastSelected = m_selection.empty() ? nullptr : m_selection.back();
    }

    void CSelectionManager::ClearSelectables()
    {
        m_selectables.clear();
        ClearSelection();
    }

    void CSelectionManager::ClearSelection()
    {
        m_selection.clear();
        m_lastSelected = nullptr;
    }

    void CSelectionManager::SetSelected(std::shared_ptr<CSelectable> selectable)
    {
        m_selection.clear();
        if (selectable)
            m_selection.push_back(selectable);
        m_lastSelected = std::move(selectable);
    }

    void CSelectionManager::SetAllSelected(const std::vector<std::shared_ptr<CSelectable>>& selectables)
    {
        m_selection.clear();
        for (const auto& sel : selectables)
        {
            if (sel)
                m_selection.push_back(sel);
        }
        m_lastSelected = m_selection.empty() ? nullptr : m_selection.back();
    }

    bool CSelectionManager::IsSelected(const std::shared_ptr<CSelectable>& selectable) const
    {
        return std::find(m_selection.begin(), m_selection.end(), selectable) != m_selection.end();
    }

    void CSelectionManager::AddToSelection(const std::shared_ptr<CSelectable>& selectable)
    {
        if (!IsSelected(selectable))
            m_selection.push_back(selectable);
    }

    void CSelectionManager::RemoveFromSelection(const std::shared_ptr<CSelectable>& selectable)
    {
        auto it = std::find(m_selection.begin(), m_selection.end(), selectable);
        if (it != m_selection.end())
            m_selection.erase(it);
    }

    Ray CSelectionManager::BuildPickRay() const
    {
        const ImVec2 mouse = ImGui::GetMousePos();
        const float  ndcX = ((mouse.x - m_viewportMin.x) / m_viewportSize.x) * 2.0f - 1.0f;
        const float  ndcY = 1.0f - ((mouse.y - m_viewportMin.y) / m_viewportSize.y) * 2.0f;

        const float aspect = m_viewportSize.x / m_viewportSize.y;

        Matrix4f view, proj;
        m_camera->GetViewMatrix(view.data());
        m_camera->GetProjectionMatrix(proj.data(), aspect);

        // bx is row-major; Matrix4f is column-major. Loading the same float[16]
        // makes each matrix appear as its transpose. To cancel this out, the
        // multiplication order must be reversed: (A*B)^T = B^T * A^T.
        // Also handle bgfx NDC depth: 0..1 (DX/Vulkan) vs -1..1 (GL).
        const Matrix4f invVP = (proj * view).Inverse();

        const float    nearZ = bgfx::getCaps()->homogeneousDepth ? -1.0f : 0.0f;
        const Vector3f nearPt = invVP.TransformPoint(Vector3f(ndcX, ndcY, nearZ));
        const Vector3f farPt = invVP.TransformPoint(Vector3f(ndcX, ndcY, 1.0f));

        return Ray{ nearPt, (farPt - nearPt).Normalized() };
    }
    std::shared_ptr<CSelectable> CSelectionManager::PickAtCursor()
    {
        if (!m_camera || m_viewportSize.x < 1.0f || m_viewportSize.y < 1.0f)
            return nullptr;

        // Do not pick objects while a gizmo drag is active, or if the mouse was
        // just pressed on a highlighted gizmo handle (which will start a drag).
        if (m_drag.active)
            return nullptr;
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && m_hoveredGizmoAxis != GizmoAxis::None)
            return nullptr;

        const bool shiftHeld = ImGui::GetIO().KeyShift;

        // ── Build view-projection matrix ──────────────────────────────────
        const float aspect = m_viewportSize.x / m_viewportSize.y;

        Matrix4f view, proj;
        m_camera->GetViewMatrix(view.data());
        m_camera->GetProjectionMatrix(proj.data(), aspect);

        const Matrix4f viewProj = proj * view;

        // Helper: projects a world-space point to viewport pixel coordinates.
        // Performs the full homogeneous multiply without the automatic perspective
        // divide so that we can inspect w and detect behind-camera points.
        // Returns false when the point is behind the camera (w <= 0).
        auto WorldToScreen = [&](const Vector3f& worldPt, ImVec2& outScreen) -> bool
            {
                const float x = worldPt.x, y = worldPt.y, z = worldPt.z;
                const float cx = viewProj(0, 0) * x + viewProj(0, 1) * y + viewProj(0, 2) * z + viewProj(0, 3);
                const float cy = viewProj(1, 0) * x + viewProj(1, 1) * y + viewProj(1, 2) * z + viewProj(1, 3);
                const float cw = viewProj(3, 0) * x + viewProj(3, 1) * y + viewProj(3, 2) * z + viewProj(3, 3);

                if (cw <= 0.0f)
                    return false;

                const float ndcX = cx / cw;
                const float ndcY = cy / cw;

                outScreen.x = (ndcX * 0.5f + 0.5f) * m_viewportSize.x + m_viewportMin.x;
                outScreen.y = (0.5f - ndcY * 0.5f) * m_viewportSize.y + m_viewportMin.y;
                return true;
            };

        // ── Pick by 2D screen-space distance to object centre ─────────────
        const ImVec2 mouse = ImGui::GetMousePos();

        std::shared_ptr<CSelectable> nearest;
        float nearestDist2D = FLT_MAX;

        for (const auto& selectable : m_selectables)
        {
            if (!selectable) continue;

            const Vector4f  bs = selectable->GetBoundingSphere();
            const Matrix4f& mtx = selectable->GetTransform();
            const Vector3f  worldCentre = mtx.TransformPoint(Vector3f(bs.x, bs.y, bs.z));

            // Project the object centre into screen space.
            ImVec2 screenCentre;
            if (!WorldToScreen(worldCentre, screenCentre))
                continue;   // Behind the camera – skip.

            // Estimate the screen-space radius by projecting a point displaced by
            // the world-space radius along the camera's right axis (view matrix row 0).
            const Vector3f  scale = mtx.ExtractScale();
            const float     worldRadius = bs.w * std::max({ scale.x, scale.y, scale.z, 0.5f });

            const Vector3f  camRight = Vector3f(view(0, 0), view(0, 1), view(0, 2)).Normalized();
            ImVec2          screenEdge;
            const float     screenRadius = [&]() -> float
                {
                    if (WorldToScreen(worldCentre + camRight * worldRadius, screenEdge))
                    {
                        const float dx = screenEdge.x - screenCentre.x;
                        const float dy = screenEdge.y - screenCentre.y;
                        return std::sqrt(dx * dx + dy * dy);
                    }
                    return 20.0f;   // Fallback when the edge point is off-screen.
                }();

            // 2D pixel distance from cursor to the projected object centre.
            const float dx = mouse.x - screenCentre.x;
            const float dy = mouse.y - screenCentre.y;
            const float dist2D = std::sqrt(dx * dx + dy * dy);

            // Only consider objects whose screen-space sphere contains the cursor.
            if (dist2D > screenRadius)
                continue;

            if (dist2D < nearestDist2D)
            {
                nearestDist2D = dist2D;
                nearest = selectable;
            }
        }

        if (shiftHeld)
        {
            if (nearest)
            {
                // Shift+click a selected object → deselect it
                // Shift+click an unselected object → add it
                if (IsSelected(nearest))
                    RemoveFromSelection(nearest);
                else
                    AddToSelection(nearest);

                m_lastSelected = nearest;
            }
            // Shift+click on empty space → leave selection unchanged
        }
        else
        {
            // Plain click → replace selection
            m_selection.clear();
            if (nearest)
                m_selection.push_back(nearest);

            m_lastSelected = nearest;
        }

        return nearest;
    }

    // ── Gizmo hover detection ─────────────────────────────────────────────────

    GizmoAxis CSelectionManager::HitTestGizmo(GizmoMode       mode,
        const Matrix4f& gizmoMtx,
        float           effectiveSize) const
    {
        if (!m_camera)
            return GizmoAxis::None;

        const Ray      ray = BuildPickRay();
        const Vector3f origin = gizmoMtx.ExtractTranslation();

        // Normalise the matrix columns to get pure world-space direction vectors,
        // regardless of any scale baked into the gizmo transform.
        const Vector3f axes[3] = {
            gizmoMtx.GetColumn(0).Normalized(),
            gizmoMtx.GetColumn(1).Normalized(),
            gizmoMtx.GetColumn(2).Normalized(),
        };

        // ── Rotate: ring band test ────────────────────────────────────────
        if (mode == GizmoMode::Rotate)
        {
            constexpr float kRingHalfWidth = 0.12f; // fraction of effectiveSize

            const GizmoAxis kRingAxes[3] = { GizmoAxis::X, GizmoAxis::Y, GizmoAxis::Z };
            GizmoAxis bestAxis = GizmoAxis::None;
            float     bestT = FLT_MAX;

            for (int i = 0; i < 3; ++i)
            {
                float    tHit;
                Vector3f hitPt;
                if (!RayPlaneIntersect(ray, origin, axes[i], tHit, hitPt))
                    continue;

                const float distFromCenter = (hitPt - origin).Length();
                const float delta = std::abs(distFromCenter - effectiveSize);

                if (delta < effectiveSize * kRingHalfWidth && tHit < bestT)
                {
                    bestT = tHit;
                    bestAxis = kRingAxes[i];
                }
            }

            return bestAxis;
        }

        // ── Translate / Scale ─────────────────────────────────────────────
        const float shaftEnd = effectiveSize * (mode == GizmoMode::Scale ? 0.92f : 1.0f);
        const float hitRadius = effectiveSize * 0.1f;

        GizmoAxis bestAxis = GizmoAxis::None;
        float     bestDist = FLT_MAX;

        // Plane handles take priority (Translate only)
        if (mode == GizmoMode::Translate)
        {
            const float planeMin = effectiveSize * 0.15f;
            const float planeMax = effectiveSize * 0.35f;

            constexpr int kTangentA[3] = { 0, 1, 0 };
            constexpr int kTangentB[3] = { 1, 2, 2 };
            constexpr int kNormal[3] = { 2, 0, 1 };

            const GizmoAxis kPlaneAxes[3] = { GizmoAxis::XY, GizmoAxis::YZ, GizmoAxis::XZ };

            for (int i = 0; i < 3; ++i)
            {
                float    tHit;
                Vector3f hitPt;
                if (!RayPlaneIntersect(ray, origin, axes[kNormal[i]], tHit, hitPt))
                    continue;

                const Vector3f rel = hitPt - origin;
                const float    u = rel.Dot(axes[kTangentA[i]]);
                const float    v = rel.Dot(axes[kTangentB[i]]);

                if (u >= planeMin && u <= planeMax && v >= planeMin && v <= planeMax)
                {
                    if (tHit < bestDist)
                    {
                        bestDist = tHit;
                        bestAxis = kPlaneAxes[i];
                    }
                }
            }

            if (bestAxis != GizmoAxis::None)
                return bestAxis;
        }

        // Axis shafts
        const GizmoAxis kAxisEnum[3] = { GizmoAxis::X, GizmoAxis::Y, GizmoAxis::Z };

        for (int i = 0; i < 3; ++i)
        {
            const Vector3f segEnd = origin + axes[i] * shaftEnd;

            float tRay;
            const float dist = RaySegmentDist(ray, origin, segEnd, tRay);

            if (tRay > 0.0f && dist < hitRadius && dist < bestDist)
            {
                bestDist = dist;
                bestAxis = kAxisEnum[i];
            }
        }

        return bestAxis;
    }

    // ── Gizmo drag ────────────────────────────────────────────────────────────

    void CSelectionManager::BeginGizmoDrag(GizmoMode mode, const Matrix4f& gizmoMtx)
    {
        if (m_selection.empty())
            return;

        // If SHIFT is held, invoke the duplicate callback so the caller can
        // replace the selection with fresh duplicates before the drag starts.
        // The callback is expected to update m_selection via SetAllSelected().
        if (ImGui::GetIO().KeyShift && m_shiftDragCallback)
        {
            m_shiftDragCallback();
            // If duplication left us with no selectables, abort the drag.
            if (m_selection.empty())
                return;
        }

        const Ray      ray = BuildPickRay();
        const Vector3f origin = gizmoMtx.ExtractTranslation();

        const Vector3f axes[3] = {
            gizmoMtx.GetColumn(0).Normalized(),
            gizmoMtx.GetColumn(1).Normalized(),
            gizmoMtx.GetColumn(2).Normalized(),
        };

        m_drag.active = true;
        m_drag.mode = mode;
        m_drag.axis = m_hoveredGizmoAxis;
        m_drag.origin = origin;

        // Snapshot each selected object's transform so we can apply absolute deltas.
        m_drag.startTransforms.clear();
        for (const auto& sel : m_selection)
            m_drag.startTransforms.push_back(sel->GetTransform());

        switch (mode)
        {
        case GizmoMode::Translate:
        case GizmoMode::Scale:
        {
            switch (m_hoveredGizmoAxis)
            {
            case GizmoAxis::X:
                m_drag.axisDir = axes[0];
                m_drag.tStart = RayLineClosestT(ray, origin, axes[0]);
                break;
            case GizmoAxis::Y:
                m_drag.axisDir = axes[1];
                m_drag.tStart = RayLineClosestT(ray, origin, axes[1]);
                break;
            case GizmoAxis::Z:
                m_drag.axisDir = axes[2];
                m_drag.tStart = RayLineClosestT(ray, origin, axes[2]);
                break;
            case GizmoAxis::XY:
                m_drag.axisDir = axes[0];
                m_drag.axisDir2 = axes[1];
                m_drag.planeNormal = axes[2];
                { float t; RayPlaneIntersect(ray, origin, axes[2], t, m_drag.hitStart); }
                break;
            case GizmoAxis::YZ:
                m_drag.axisDir = axes[1];
                m_drag.axisDir2 = axes[2];
                m_drag.planeNormal = axes[0];
                { float t; RayPlaneIntersect(ray, origin, axes[0], t, m_drag.hitStart); }
                break;
            case GizmoAxis::XZ:
                m_drag.axisDir = axes[0];
                m_drag.axisDir2 = axes[2];
                m_drag.planeNormal = axes[1];
                { float t; RayPlaneIntersect(ray, origin, axes[1], t, m_drag.hitStart); }
                break;
            default: break;
            }
            break;
        }

        case GizmoMode::Rotate:
        {
            switch (m_hoveredGizmoAxis)
            {
            case GizmoAxis::X:
                m_drag.planeNormal = axes[0];
                m_drag.axisDir = axes[1];
                m_drag.axisDir2 = axes[2];
                break;
            case GizmoAxis::Y:
                m_drag.planeNormal = axes[1];
                m_drag.axisDir = axes[0];
                m_drag.axisDir2 = axes[2];
                break;
            case GizmoAxis::Z:
                m_drag.planeNormal = axes[2];
                m_drag.axisDir = axes[0];
                m_drag.axisDir2 = axes[1];
                break;
            default: break;
            }

            float    tHit;
            Vector3f hitPt;
            RayPlaneIntersect(ray, origin, m_drag.planeNormal, tHit, hitPt);

            const Vector3f rel = hitPt - origin;
            m_drag.angleStart = std::atan2(rel.Dot(m_drag.axisDir2), rel.Dot(m_drag.axisDir));
            break;
        }
        }
    }

    void CSelectionManager::ApplyGizmoDrag()
    {
        if (!m_drag.active || m_selection.empty())
            return;

        const Ray ray = BuildPickRay();

        switch (m_drag.mode)
        {
        case GizmoMode::Translate: ApplyTranslateDrag(ray); break;
        case GizmoMode::Scale:     ApplyScaleDrag(ray); break;
        case GizmoMode::Rotate:    ApplyRotateDrag(ray); break;
        }
    }

    void CSelectionManager::ApplyTranslateDrag(const Ray& ray)
    {
        Vector3f delta;

        const bool isSingleAxis = (m_drag.axis == GizmoAxis::X ||
            m_drag.axis == GizmoAxis::Y ||
            m_drag.axis == GizmoAxis::Z);

        if (isSingleAxis)
        {
            const float tCurrent = RayLineClosestT(ray, m_drag.origin, m_drag.axisDir);
            delta = m_drag.axisDir * (tCurrent - m_drag.tStart);
        }
        else
        {
            float    tHit;
            Vector3f hitPt;
            if (!RayPlaneIntersect(ray, m_drag.origin, m_drag.planeNormal, tHit, hitPt))
                return;
            delta = hitPt - m_drag.hitStart;
        }

        for (size_t i = 0; i < m_selection.size(); ++i)
        {
            Matrix4f* transform = m_selection[i]->GetTransformMutable();
            if (!transform) continue;

            *transform = m_drag.startTransforms[i];
            transform->SetTranslation(m_drag.startTransforms[i].ExtractTranslation() + delta);
        }
    }

    void CSelectionManager::ApplyScaleDrag(const Ray& ray)
    {
        const float tCurrent = RayLineClosestT(ray, m_drag.origin, m_drag.axisDir);
        const float absStart = std::abs(m_drag.tStart);
        if (absStart < 1e-6f)
            return;

        // Clamp to a small positive value to prevent zero or negative scale.
        const float factor = std::max(tCurrent / absStart, 0.001f);

        int axisCol = -1;
        if (m_drag.axis == GizmoAxis::X) axisCol = 0;
        else if (m_drag.axis == GizmoAxis::Y) axisCol = 1;
        else if (m_drag.axis == GizmoAxis::Z) axisCol = 2;
        if (axisCol < 0) return;

        for (size_t i = 0; i < m_selection.size(); ++i)
        {
            Matrix4f* transform = m_selection[i]->GetTransformMutable();
            if (!transform) continue;

            *transform = m_drag.startTransforms[i];
            transform->SetColumn(axisCol, m_drag.startTransforms[i].GetColumn(axisCol) * factor);
        }
    }

    void CSelectionManager::ApplyRotateDrag(const Ray& ray)
    {
        float    tHit;
        Vector3f hitPt;
        if (!RayPlaneIntersect(ray, m_drag.origin, m_drag.planeNormal, tHit, hitPt))
            return;

        const Vector3f rel = hitPt - m_drag.origin;
        const float    angleCur = std::atan2(rel.Dot(m_drag.axisDir2), rel.Dot(m_drag.axisDir));
        const float    deltaAngle = angleCur - m_drag.angleStart;

        // Rotate around the gizmo origin: T(origin) * R * T(-origin) * startTransform
        const Matrix4f R = Matrix4f::Rotation(m_drag.planeNormal, deltaAngle);
        const Matrix4f fromOrigin = Matrix4f::Translation(m_drag.origin);
        const Matrix4f toOrigin = Matrix4f::Translation(-m_drag.origin);

        for (size_t i = 0; i < m_selection.size(); ++i)
        {
            Matrix4f* transform = m_selection[i]->GetTransformMutable();
            if (!transform) continue;

            *transform = fromOrigin * R * toOrigin * m_drag.startTransforms[i];
        }
    }

    // ── RenderSelectionGizmo ──────────────────────────────────────────────────

    void CSelectionManager::EndGizmoDrag()
    {
        if (!m_drag.active)
            return;

        // Record undo/redo command if a history was provided.
        if (m_commandHistory && !m_selection.empty())
        {
            std::vector<CTransformCommand::Entry> entries;
            entries.reserve(m_selection.size());

            for (size_t i = 0; i < m_selection.size(); ++i)
            {
                CTransformCommand::Entry e;
                e.selectable = m_selection[i];
                e.before = m_drag.startTransforms[i];
                e.after = m_selection[i]->GetTransform();  // current = result of drag
                entries.push_back(std::move(e));
            }

            // Push without re-executing — the drag already applied the final state.
            // We bypass Push() to avoid a redundant Execute() call by directly
            // inserting the command in its already-applied state.
            // Instead, wrap: temporarily make Execute() a no-op on first call.
            // Simplest: just call the history's internals — but since we own the
            // abstraction, we expose a PushAlreadyExecuted() helper below.
            m_commandHistory->PushAlreadyExecuted(
                std::make_unique<CTransformCommand>(std::move(entries),
                    m_drag.mode == GizmoMode::Translate ? "Move"
                    : m_drag.mode == GizmoMode::Scale ? "Scale"
                    : "Rotate"));
        }

        m_drag.active = false;
        m_drag.startTransforms.clear();
    }

    void CSelectionManager::RenderSelectionGizmo(bgfx::FrameBufferHandle fbh,
        GizmoMode               mode,
        float                   size)
    {
        // Always allow box-selection even when nothing is selected. Only early-out
        // if we don't have a valid camera.
        if (!m_camera)
        {
            m_hoveredGizmoAxis = GizmoAxis::None;
            return;
        }

        // Lazy-initialize the gizmo renderer here so its view ID is always
        // allocated AFTER the scene viewport's view ID (bgfx renders views in
        // ascending ID order – a lower gizmo ID would cause the scene to overwrite it).
        if (!m_gizmoRenderer.IsInitialized())
        {
            if (!m_gizmoRenderer.Initialize())
            {
                m_hoveredGizmoAxis = GizmoAxis::None;
                return;
            }
        }

        const bool mouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
        const bool mousePressed = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        const bool mouseReleased = ImGui::IsMouseReleased(ImGuiMouseButton_Left);

        // Track whether the most recent mouse-down started inside the viewport.
        if (mousePressed)
            m_mouseDownStartedInViewport = IsMouseInViewport();
        if (mouseReleased)
            m_mouseDownStartedInViewport = false;

        // ── 1. Apply ongoing drag (before recomputing gizmo position) ─────
        if (m_drag.active)
        {
            if (mouseReleased)
            {
                ApplyGizmoDrag();
                EndGizmoDrag();
            }
            else if (mouseDown)
            {
                ApplyGizmoDrag();
            }
        }

        // ── 2. Build gizmo matrix from current (possibly just-updated) transforms ─
        // Only compute gizmo/hover when there is a selection. Always compute
        // view/proj (needed for box selection projection) even when empty.
        Matrix4f gizmo;
        const bool hasSelection = !m_selection.empty();

        // ── 3. Constant screen-size scaling ───────────────────────────────
        constexpr float kScreenFraction = 0.1f;

        const float aspect = m_viewportSize.x / m_viewportSize.y;

        Matrix4f view, proj;
        m_camera->GetViewMatrix(view.data());
        m_camera->GetProjectionMatrix(proj.data(), aspect);

        float eye[3];
        m_camera->GetEyePosition(eye);

        float effectiveSize = 0.0f;
        if (hasSelection)
        {
            if (m_selection.size() == 1)
            {
                const Matrix4f& objMtx = m_selection.front()->GetTransform();
                const Vector3f  scale = objMtx.ExtractScale();

                const float invX = scale.x > 1e-8f ? 1.0f / scale.x : 0.0f;
                const float invY = scale.y > 1e-8f ? 1.0f / scale.y : 0.0f;
                const float invZ = scale.z > 1e-8f ? 1.0f / scale.z : 0.0f;

                gizmo.SetColumn(0, objMtx.GetColumn(0) * invX);
                gizmo.SetColumn(1, objMtx.GetColumn(1) * invY);
                gizmo.SetColumn(2, objMtx.GetColumn(2) * invZ);
                gizmo.SetTranslation(objMtx.ExtractTranslation());
            }
            else
            {
                Vector3f centroid;
                for (const auto& sel : m_selection)
                    centroid += sel->GetTransform().ExtractTranslation();
                centroid = centroid * (1.0f / static_cast<float>(m_selection.size()));

                gizmo.SetTranslation(centroid);
            }

            const float dist = gizmo.ExtractTranslation().DistanceTo(Vector3f(eye[0], eye[1], eye[2]));
            const float fovRad = m_camera->GetFov() * static_cast<float>(M_PI) / 180.0f;
            effectiveSize = size * dist * std::tan(fovRad * 0.5f) * kScreenFraction;
        }

        // Prepare view-proj for box selection projection
        const Matrix4f viewProj = proj * view;
        auto WorldToScreen = [&](const Vector3f& worldPt, ImVec2& outScreen) -> bool
            {
                const float x = worldPt.x, y = worldPt.y, z = worldPt.z;
                const float cx = viewProj(0, 0) * x + viewProj(0, 1) * y + viewProj(0, 2) * z + viewProj(0, 3);
                const float cy = viewProj(1, 0) * x + viewProj(1, 1) * y + viewProj(1, 2) * z + viewProj(1, 3);
                const float cw = viewProj(3, 0) * x + viewProj(3, 1) * y + viewProj(3, 2) * z + viewProj(3, 3);

                if (cw <= 0.0f)
                    return false;

                const float ndcX = cx / cw;
                const float ndcY = cy / cw;

                outScreen.x = (ndcX * 0.5f + 0.5f) * m_viewportSize.x + m_viewportMin.x;
                outScreen.y = (0.5f - ndcY * 0.5f) * m_viewportSize.y + m_viewportMin.y;
                return true;
            };

        // ── 4. Hover detection and drag initiation (idle only) ────────────
        if (!m_drag.active)
        {
            if (hasSelection)
            {
                m_hoveredGizmoAxis = HitTestGizmo(mode, gizmo, effectiveSize);

                if (mousePressed && m_hoveredGizmoAxis != GizmoAxis::None && IsMouseInViewport())
                    BeginGizmoDrag(mode, gizmo);
                else if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) && m_hoveredGizmoAxis == GizmoAxis::None && m_mouseDownStartedInViewport && IsMouseInViewport())
                {
                    // Begin a selection box drag only once dragging is detected (prevents click->clear).
                    ImVec2 current = ImGui::GetMousePos();
                    ImVec2 delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.0f);
                    m_boxDrag.start = ImVec2{ current.x - delta.x, current.y - delta.y };
                    m_boxDrag.current = current;
                    m_boxDrag.active = true;
                }
            }
            else
            {
                // No gizmo when nothing is selected – allow box drag when dragging only.
                m_hoveredGizmoAxis = GizmoAxis::None;
                if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) && m_mouseDownStartedInViewport && IsMouseInViewport())
                {
                    ImVec2 current = ImGui::GetMousePos();
                    ImVec2 delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.0f);
                    m_boxDrag.start = ImVec2{ current.x - delta.x, current.y - delta.y };
                    m_boxDrag.current = current;
                    m_boxDrag.active = true;
                }
            }
        }

        // If a box drag is active, update/draw it and commit on release
        if (m_boxDrag.active)
        {
            if (mouseDown)
            {
                m_boxDrag.current = ImGui::GetMousePos();
            }

            // Draw selection rectangle overlay
            ImVec2 a = m_boxDrag.start;
            ImVec2 b = m_boxDrag.current;
            ImVec2 rmin = { std::min(a.x, b.x), std::min(a.y, b.y) };
            ImVec2 rmax = { std::max(a.x, b.x), std::max(a.y, b.y) };

            ImDrawList* dl = ImGui::GetForegroundDrawList();
            const ImU32 fillCol = IM_COL32(0, 122, 204, 64);
            const ImU32 borderCol = IM_COL32(0, 122, 204, 192);
            dl->AddRectFilled(rmin, rmax, fillCol);
            dl->AddRect(rmin, rmax, borderCol, 0.0f, 0, 2.0f);

            if (mouseReleased)
            {
                // Compute contained selectables
                std::vector<std::shared_ptr<CSelectable>> inside;

                for (const auto& selectable : m_selectables)
                {
                    if (!selectable) continue;

                    const Vector4f bs = selectable->GetBoundingSphere();
                    const Matrix4f& mtx = selectable->GetTransform();
                    const Vector3f worldCentre = mtx.TransformPoint(Vector3f(bs.x, bs.y, bs.z));

                    ImVec2 screenPt;
                    if (!WorldToScreen(worldCentre, screenPt))
                        continue; // behind camera

                    if (screenPt.x >= rmin.x && screenPt.x <= rmax.x &&
                        screenPt.y >= rmin.y && screenPt.y <= rmax.y)
                    {
                        inside.push_back(selectable);
                    }
                }

                const bool shiftHeld = ImGui::GetIO().KeyShift;
                if (shiftHeld)
                {
                    // Add inside items to current selection (if not already present)
                    for (const auto& s : inside)
                        AddToSelection(s);
                }
                else
                {
                    m_selection = inside;
                }

                m_lastSelected = m_selection.empty() ? nullptr : m_selection.back();

                m_boxDrag.active = false;
            }
        }

        // ── 5. Render ─────────────────────────────────────────────────────
        m_gizmoRenderer.BeginFrame(fbh,
            static_cast<uint16_t>(m_viewportSize.x),
            static_cast<uint16_t>(m_viewportSize.y),
            view.data(),
            proj.data());
        m_gizmoRenderer.RenderGizmo(gizmo.data(), mode, m_hoveredGizmoAxis, effectiveSize);
    }

} // namespace ImGuiVisualizers
