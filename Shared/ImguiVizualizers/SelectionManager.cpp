#include "TransformCommand.h"
#include "../Utils/MathUtils.h"
#include <cfloat>
#include <cmath>
#include <algorithm>
#include <functional>
#include <imgui/imgui.h>
#include "SelectionManager.h"

namespace ImGuiVisualizers {

    // ── Local 2D helpers (anonymous) ──────────────────────────────────────────

    namespace {

    /// Closest distance from point @p p to the finite segment a→b (2D, pixels).
    float PointSegmentDist2D(const ImVec2& p, const ImVec2& a, const ImVec2& b)
    {
        const float dx = b.x - a.x;
        const float dy = b.y - a.y;
        const float len2 = dx * dx + dy * dy;
        if (len2 < 1e-12f)
        {
            const float ex = p.x - a.x;
            const float ey = p.y - a.y;
            return std::sqrt(ex * ex + ey * ey);
        }
        float t = ((p.x - a.x) * dx + (p.y - a.y) * dy) / len2;
        t = std::clamp(t, 0.0f, 1.0f);
        const float cx = a.x + t * dx;
        const float cy = a.y + t * dy;
        return std::sqrt((p.x - cx) * (p.x - cx) + (p.y - cy) * (p.y - cy));
    }

    } // anonymous namespace

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

        const Vector3f dir = farPt - nearPt;
        if (dir.MagnitudeSquared() < 1e-12f)
            return Ray{ nearPt, Vector3f(0.0f, 0.0f, -1.0f) };

        return Ray{ nearPt, dir.Normalized() };
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

        const bool shiftHeld = ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift);

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

            const Matrix4f& mtx = selectable->GetTransform();

            ImVec2 screenCentre;
            float  screenRadius = 20.0f;

            if (selectable->GetSpace() == SpaceKind::Screen)
            {
                // UI elements: translation is already NDC; project to pixels
                // and derive the half-extent from the model-matrix column lengths.
                const Vector3f t = mtx.ExtractTranslation();
                screenCentre = NdcToScreen(Vector2f(t.GetX(), t.GetY()));

                const float halfXpx = mtx.GetColumn(0).Length() * m_viewportSize.x * 0.5f;
                const float halfYpx = mtx.GetColumn(1).Length() * m_viewportSize.y * 0.5f;
                screenRadius = std::max({ halfXpx, halfYpx, 8.0f });
            }
            else
            {
                const Vector4f  bs = selectable->GetBoundingSphere();
                const Vector3f  worldCentre = mtx.TransformPoint(Vector3f(bs.x, bs.y, bs.z));

                // Project the object centre into screen space.
                if (!WorldToScreen(worldCentre, screenCentre))
                    continue;   // Behind the camera – skip.

                // Estimate the screen-space radius by projecting a point displaced by
                // the world-space radius along the camera's right axis (view matrix row 0).
                const Vector3f  scale = mtx.ExtractScale();
                const float     worldRadius = bs.w * std::max({ scale.x, scale.y, scale.z, 0.5f });

                const Vector3f  camRight = Vector3f(view(0, 0), view(0, 1), view(0, 2)).Normalized();
                ImVec2          screenEdge;
                screenRadius = [&]() -> float
                    {
                        if (WorldToScreen(worldCentre + camRight * worldRadius, screenEdge))
                        {
                            const float dx = screenEdge.x - screenCentre.x;
                            const float dy = screenEdge.y - screenCentre.y;
                            return std::sqrt(dx * dx + dy * dy);
                        }
                        return 20.0f;   // Fallback when the edge point is off-screen.
                    }();
            }

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
        if ((ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift)) && m_shiftDragCallback)
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
        m_drag.space = SpaceKind::World;
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

        // Screen-space (UI) drags use their own pixel→NDC math.
        if (m_drag.space == SpaceKind::Screen)
        {
            ApplyGizmoDrag2D();
            return;
        }

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

        // UI elements render in NDC/screen space and use a dedicated 2D gizmo
        // path (ImGui overlay) instead of the 3D world-space manipulator.
        if (IsSelectionScreenSpace())
        {
            RenderSelectionGizmo2D(mode, size);
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
        const bool altHeld = ImGui::IsKeyDown(ImGuiKey_LeftAlt) || ImGui::IsKeyDown(ImGuiKey_RightAlt);
        if (!m_drag.active && !altHeld)
        {
            if (hasSelection)
            {
                m_hoveredGizmoAxis = HitTestGizmo(mode, gizmo, effectiveSize);

                if (mousePressed && m_hoveredGizmoAxis != GizmoAxis::None && IsMouseInViewport())
                {
                    BeginGizmoDrag(mode, gizmo);
                }
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

                        const Matrix4f& mtx = selectable->GetTransform();

                        ImVec2 screenPt;
                        if (selectable->GetSpace() == SpaceKind::Screen)
                        {
                            // UI elements: translation is NDC; project to pixels.
                            const Vector3f t = mtx.ExtractTranslation();
                            screenPt = NdcToScreen(Vector2f(t.GetX(), t.GetY()));
                        }
                        else
                        {
                            const Vector4f bs = selectable->GetBoundingSphere();
                            const Vector3f worldCentre = mtx.TransformPoint(Vector3f(bs.x, bs.y, bs.z));
                            if (!WorldToScreen(worldCentre, screenPt))
                                continue; // behind camera
                        }

                        if (screenPt.x >= rmin.x && screenPt.x <= rmax.x &&
                            screenPt.y >= rmin.y && screenPt.y <= rmax.y)
                        {
                            inside.push_back(selectable);
                        }
                    }

                const bool shiftHeld = ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift);
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

    // ── Camera focus ──────────────────────────────────────────────────────────

    void CSelectionManager::FocusCameraOnSelection()
    {
        if (!m_camera || m_selection.empty())
            return;

        // Compute the gizmo centre (centroid of selected objects).
        Vector3f centroid;
        for (const auto& sel : m_selection)
            centroid += sel->GetTransform().ExtractTranslation();
        centroid = centroid * (1.0f / static_cast<float>(m_selection.size()));

        // Compute the bounding radius: largest distance from centroid to
        // the far edge (centre offset + world-space bounding radius) of any selected object.
        float maxRadius = 0.0f;
        for (const auto& sel : m_selection)
        {
            const Vector4f  bs  = sel->GetBoundingSphere();
            const Matrix4f& mtx = sel->GetTransform();
            const Vector3f  worldCentre = mtx.TransformPoint(Vector3f(bs.x, bs.y, bs.z));
            const Vector3f  scale       = mtx.ExtractScale();
            const float     worldRadius = bs.w * std::max({ scale.x, scale.y, scale.z, 0.5f });
            const float     dist        = (worldCentre - centroid).Length() + worldRadius;
            if (dist > maxRadius)
                maxRadius = dist;
        }

        if (maxRadius < 0.001f)
            maxRadius = 1.0f;

        m_camera->SetTarget(centroid.x, centroid.y, centroid.z);
        m_camera->SetDistance(maxRadius * 2.0f);
    }

    // ── Screen-space (UI) gizmo path ────────────────────────────────────────

    bool CSelectionManager::IsSelectionScreenSpace() const
    {
        if (m_selection.empty())
            return false;
        for (const auto& sel : m_selection)
            if (sel->GetSpace() != SpaceKind::Screen)
                return false;
        return true;
    }

    ImVec2 CSelectionManager::NdcToScreen(const Vector2f& ndc) const
    {
        // Must match BgfxUIView's aspect-compensated kUIViewID view matrix
        // exactly: 1 NDC unit X -> width/2 px, 1 NDC unit Y -> width/2 px
        // (i.e. ndc.y is pre-multiplied by aspect before the standard
        // [-1,1] -> [0,sizeY] viewport mapping). Keeping this in sync with
        // the rendered UI quad keeps the gizmo glued to the quad's visual
        // position on non-square viewports.
        const float aspect = (m_viewportSize.x > 0.0f && m_viewportSize.y > 0.0f)
            ? m_viewportSize.x / m_viewportSize.y : 1.0f;
        ImVec2 r;
        r.x = (ndc.x * 0.5f + 0.5f) * m_viewportSize.x + m_viewportMin.x;
        r.y = (0.5f - ndc.y * aspect * 0.5f) * m_viewportSize.y + m_viewportMin.y;
        return r;
    }

    Vector2f CSelectionManager::ScreenToNdc(const ImVec2& s) const
    {
        // Inverse of NdcToScreen — see above for the aspect rationale.
        const float aspect = (m_viewportSize.x > 0.0f && m_viewportSize.y > 0.0f)
            ? m_viewportSize.x / m_viewportSize.y : 1.0f;
        Vector2f r;
        r.x = ((s.x - m_viewportMin.x) / m_viewportSize.x) * 2.0f - 1.0f;
        r.y = (1.0f - ((s.y - m_viewportMin.y) / m_viewportSize.y) * 2.0f) / aspect;
        return r;
    }

    GizmoAxis CSelectionManager::HitTestGizmo2D(GizmoMode      mode,
                                                 const ImVec2&  centre,
                                                 float          size) const
    {
        const ImVec2 mouse = ImGui::GetMousePos();

        // ── Rotate: single ring around the centre ──────────────────────────
        if (mode == GizmoMode::Rotate)
        {
            const float dx   = mouse.x - centre.x;
            const float dy   = mouse.y - centre.y;
            const float d    = std::sqrt(dx * dx + dy * dy);
            const float band = size * 0.12f;
            return (std::abs(d - size) < band) ? GizmoAxis::Z : GizmoAxis::None;
        }

        // ── Centre handle: free move / uniform scale ───────────────────────
        const ImVec2 endX = ImVec2(centre.x + size, centre.y);
        const ImVec2 endY = ImVec2(centre.x, centre.y + size);

        // Centre square hit (Translate = free XY, Scale = uniform).
        if (mode == GizmoMode::Translate)
        {
            const float half = size * 0.12f;
            if (std::abs(mouse.x - centre.x) <= half && std::abs(mouse.y - centre.y) <= half)
                return GizmoAxis::XY;
        }
        else if (mode == GizmoMode::Scale)
        {
            const float half = size * 0.18f;
            if (std::abs(mouse.x - centre.x) <= half && std::abs(mouse.y - centre.y) <= half)
                return GizmoAxis::XY;
        }

        // ── Axis shafts (X right, Y down on screen) ────────────────────────
        const float hitR = size * 0.10f;
        const float dX = PointSegmentDist2D(mouse, centre, endX);
        const float dY = PointSegmentDist2D(mouse, centre, endY);

        if (dX < hitR && dX <= dY) return GizmoAxis::X;
        if (dY < hitR)             return GizmoAxis::Y;
        return GizmoAxis::None;
    }

    void CSelectionManager::BeginGizmoDrag2D(GizmoMode      mode,
                                              const Vector2f& centreNdc,
                                              const ImVec2&   centrePx)
    {
        if (m_selection.empty())
            return;

        // SHIFT+drag duplicates (same contract as the world-space path).
        if ((ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift)) && m_shiftDragCallback)
        {
            m_shiftDragCallback();
            if (m_selection.empty())
                return;
        }

        m_drag.active       = true;
        m_drag.space        = SpaceKind::Screen;
        m_drag.mode         = mode;
        m_drag.axis         = m_hoveredGizmoAxis;
        m_drag.origin       = Vector3f(centreNdc.x, centreNdc.y, 0.0f);
        m_drag.centrePx     = centrePx;
        m_drag.originNdc    = centreNdc;
        m_drag.startScreenPt = ImGui::GetMousePos();

        m_drag.startTransforms.clear();
        for (const auto& sel : m_selection)
            m_drag.startTransforms.push_back(sel->GetTransform());

        // Rotate uses start/current screen angles computed in ApplyGizmoDrag2D,
        // so there is nothing to seed here. m_drag.angleStart is left untouched
        // (it remains zero — it is only used by the world-space Rotate path).
    }

    void CSelectionManager::ApplyGizmoDrag2D()
    {
        const ImVec2 mp = ImGui::GetMousePos();

        for (size_t i = 0; i < m_selection.size(); ++i)
        {
            Matrix4f* cur = m_selection[i]->GetTransformMutable();
            if (!cur) continue;

            const Matrix4f& start = m_drag.startTransforms[i];
            *cur = start;

            switch (m_drag.mode)
            {
            case GizmoMode::Translate:
                {
                    // Convert mouse delta from screen px to NDC and add to the
                    // start translation. Axis masks keep a 1D drag clamped.
                    Vector2f startNdc = ScreenToNdc(m_drag.startScreenPt);
                    Vector2f curNdc   = ScreenToNdc(mp);
                    Vector2f deltaNdc = curNdc - startNdc;
                    if (m_drag.axis == GizmoAxis::X) deltaNdc.y = 0.0f;
                    else if (m_drag.axis == GizmoAxis::Y) deltaNdc.x = 0.0f;

                    const Vector3f t = start.ExtractTranslation();
                    cur->SetTranslation(Vector3f(t.GetX() + deltaNdc.x,
                                                 t.GetY() + deltaNdc.y,
                                                 t.GetZ()));
                }
                break;

            case GizmoMode::Scale:
                {
                    // Use the mouse-to-centre distance ratio in screen space.
                    const float sdx = m_drag.startScreenPt.x - m_drag.centrePx.x;
                    const float sdy = m_drag.startScreenPt.y - m_drag.centrePx.y;
                    const float cdx = mp.x - m_drag.centrePx.x;
                    const float cdy = mp.y - m_drag.centrePx.y;
                    constexpr float kMin = 1e-3f;

                    if (m_drag.axis == GizmoAxis::X)
                    {
                        const float s = std::max(std::abs(sdx), kMin);
                        const float f = std::max(std::abs(cdx) / s, kMin);
                        cur->SetColumn(0, start.GetColumn(0) * f);
                    }
                    else if (m_drag.axis == GizmoAxis::Y)
                    {
                        const float s = std::max(std::abs(sdy), kMin);
                        const float f = std::max(std::abs(cdy) / s, kMin);
                        cur->SetColumn(1, start.GetColumn(1) * f);
                    }
                    else // XY / uniform
                    {
                        const float sStart = std::max(std::sqrt(sdx * sdx + sdy * sdy), kMin);
                        const float sCur   = std::sqrt(cdx * cdx + cdy * cdy);
                        const float f = std::max(sCur / sStart, kMin);
                        cur->SetColumn(0, start.GetColumn(0) * f);
                        cur->SetColumn(1, start.GetColumn(1) * f);
                    }
                }
                break;

	case GizmoMode::Rotate:
				{
					// Mouse angle about the gizmo centre in screen px (Y down).
					// NDC is Y up; negate so a clockwise screen drag produces a
					// clockwise visual rotation about the Z axis.
					const float sdx = m_drag.startScreenPt.x - m_drag.centrePx.x;
					const float sdy = m_drag.startScreenPt.y - m_drag.centrePx.y;
					const float cdx = mp.x - m_drag.centrePx.x;
					const float cdy = mp.y - m_drag.centrePx.y;

					const float angleStart = -std::atan2(sdy, sdx);
					const float angleCur   = -std::atan2(cdy, cdx);
					float       deltaRad   = angleCur - angleStart;
					float       deltaDeg   = deltaRad * 180.0f / static_cast<float>(M_PI);

					// CTRL snaps to 15° increments.
					if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl))
						deltaDeg = std::round(deltaDeg / 15.0f) * 15.0f;
					deltaRad = deltaDeg * static_cast<float>(M_PI) / 180.0f;

                    // ── Rebuild the XY basis directly from the start columns ──
                    // The model matrix stores canonical NDC transforms and
                    // BgfxUIView::kUIViewID applies an aspect-compensated
                    // scale(1, aspect, 1) view matrix at render time, so
                    // rotated quads stay visually square on non-square
                    // viewports without any viewport compensation inside the
                    // stored matrix itself.
                    const Vector3f col0 = start.GetColumn(0);
                    const Vector3f col1 = start.GetColumn(1);

                    const float sx      = std::max(col0.Length(), 1e-6f);
                    const float syLen   = std::max(col1.Length(), 1e-6f);
                    const float theta   = std::atan2(col0.GetY(), col0.GetX());

                    const float crossZ  = col0.GetX() * col1.GetY()
                                        - col0.GetY() * col1.GetX();
                    const float sySign  = (crossZ >= 0.0f) ? 1.0f : -1.0f;
                    const float sy      = sySign * syLen;

                    const float newTheta = theta + deltaRad;
                    const float cosN     = std::cos(newTheta);
                    const float sinN     = std::sin(newTheta);

                    *cur = start;   // preserve col2, translation, and w-row
                    cur->SetColumn(0, Vector3f(sx  *  cosN, sx  *  sinN, 0.0f));
                    cur->SetColumn(1, Vector3f(sy  * -sinN, sy  *  cosN, 0.0f));

                    // ── Translation ──────────────────────────────────────────
                    const Vector3f t   = start.ExtractTranslation();
                    const Vector3f ctr(m_drag.originNdc.x, m_drag.originNdc.y, 0.0f);
                    if ((t - ctr).MagnitudeSquared() > 1e-8f)
                    {
                        const Matrix4f Tc  = Matrix4f::Translation(ctr);
                        const Matrix4f Tc2 = Matrix4f::Translation(-ctr);
                        const Vector3f newT =
                            (Tc * Matrix4f::RotationZ(deltaRad) * Tc2).TransformPoint(t);
                        cur->SetTranslation(newT);
                    }
                }
                break;
            }
        }
    }

    void CSelectionManager::DrawGizmo2D(ImDrawList*   dl,
                                         GizmoMode     mode,
                                         const ImVec2& centre,
                                         float         size,
                                         GizmoAxis     highlighted) const
    {
        if (!dl) return;

        const ImU32 colX = (highlighted == GizmoAxis::X)  ? IM_COL32(255, 255, 0, 255) : IM_COL32(255,  60,  60, 255);
        const ImU32 colY = (highlighted == GizmoAxis::Y)  ? IM_COL32(255, 255, 0, 255) : IM_COL32( 60, 255,  60, 255);
        const ImU32 colC = (highlighted == GizmoAxis::XY) ? IM_COL32(255, 255, 0, 255) : IM_COL32(255, 255, 255, 255);
        const float thick = (highlighted != GizmoAxis::None) ? 4.0f : 3.0f;

        const ImVec2 endX = ImVec2(centre.x + size, centre.y);
        const ImVec2 endY = ImVec2(centre.x, centre.y + size);

        if (mode == GizmoMode::Rotate)
        {
            // Single ring centred on the element centre.
            dl->AddCircle(centre, size,
                          (highlighted == GizmoAxis::Z) ? IM_COL32(255, 255, 0, 255) : IM_COL32(255, 255, 255, 255),
                          48, thick);
            // Direction tick so the user sees the "0°" reference.
            dl->AddLine(centre, endX, IM_COL32(255, 255, 255, 160), 2.0f);
            return;
        }

        // Translate / Scale: two orthogonal shafts from the centre.
        dl->AddLine(centre, endX, colX, thick);
        dl->AddLine(centre, endY, colY, thick);

        if (mode == GizmoMode::Translate)
        {
            // Arrow heads.
            dl->AddTriangleFilled(endX,
                                   ImVec2(endX.x - 10.0f, endX.y - 6.0f),
                                   ImVec2(endX.x - 10.0f, endX.y + 6.0f), colX);
            dl->AddTriangleFilled(endY,
                                   ImVec2(endY.x - 6.0f, endY.y - 10.0f),
                                   ImVec2(endY.x + 6.0f, endY.y - 10.0f), colY);
            // Centre free-move square.
            dl->AddRectFilled(ImVec2(centre.x - size * 0.12f, centre.y - size * 0.12f),
                              ImVec2(centre.x + size * 0.12f, centre.y + size * 0.12f), colC);
        }
        else // Scale
        {
            // Box caps at the shaft ends.
            const float b = 6.0f;
            dl->AddRectFilled(ImVec2(endX.x - b, endX.y - b), ImVec2(endX.x + b, endX.y + b), colX);
            dl->AddRectFilled(ImVec2(endY.x - b, endY.y - b), ImVec2(endY.x + b, endY.y + b), colY);
            // Centre uniform-scale square.
            const float c = size * 0.18f;
            dl->AddRectFilled(ImVec2(centre.x - c, centre.y - c), ImVec2(centre.x + c, centre.y + c), colC);
        }
    }

    void CSelectionManager::RenderSelectionGizmo2D(GizmoMode mode, float /*size*/)
    {
        // The UI gizmo is drawn entirely in screen pixels via ImGui's
        // foreground draw list, so its size is a fixed fraction of the
        // viewport rather than a camera-distance-scaled world size.
        constexpr float kUISize = 80.0f;

        const bool mouseDown     = ImGui::IsMouseDown(ImGuiMouseButton_Left);
        const bool mousePressed  = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        const bool mouseReleased = ImGui::IsMouseReleased(ImGuiMouseButton_Left);

        if (mousePressed)  m_mouseDownStartedInViewport = IsMouseInViewport();
        if (mouseReleased) m_mouseDownStartedInViewport = false;

        // ── 1. Apply ongoing drag ──────────────────────────────────────────
        if (m_drag.active)
        {
            if (mouseReleased) { ApplyGizmoDrag2D(); EndGizmoDrag(); }
            else if (mouseDown) { ApplyGizmoDrag2D(); }
        }

        // ── 2. Compute gizmo centre (NDC → screen) ─────────────────────────
        const bool hasSelection = !m_selection.empty();
        Vector2f centreNdc(0.0f, 0.0f);
        if (hasSelection)
        {
            for (const auto& sel : m_selection)
            {
                const Vector3f t = sel->GetTransform().ExtractTranslation();
                centreNdc.x += t.GetX();
                centreNdc.y += t.GetY();
            }
            centreNdc = centreNdc * (1.0f / static_cast<float>(m_selection.size()));
        }
        const ImVec2 centrePx = NdcToScreen(centreNdc);

        // ── 3. Hover detection / drag initiation (idle) ────────────────────
        const bool altHeld = ImGui::IsKeyDown(ImGuiKey_LeftAlt) || ImGui::IsKeyDown(ImGuiKey_RightAlt);
        if (!m_drag.active && !altHeld)
        {
            if (hasSelection)
            {
                m_hoveredGizmoAxis = HitTestGizmo2D(mode, centrePx, kUISize);

                if (mousePressed && m_hoveredGizmoAxis != GizmoAxis::None && IsMouseInViewport())
                {
                    BeginGizmoDrag2D(mode, centreNdc, centrePx);
                }
                else if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) &&
                         m_hoveredGizmoAxis == GizmoAxis::None &&
                         m_mouseDownStartedInViewport && IsMouseInViewport())
                {
                    ImVec2 current = ImGui::GetMousePos();
                    ImVec2 delta   = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.0f);
                    m_boxDrag.start   = ImVec2{ current.x - delta.x, current.y - delta.y };
                    m_boxDrag.current = current;
                    m_boxDrag.active  = true;
                }
            }
            else
            {
                m_hoveredGizmoAxis = GizmoAxis::None;
                if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) &&
                    m_mouseDownStartedInViewport && IsMouseInViewport())
                {
                    ImVec2 current = ImGui::GetMousePos();
                    ImVec2 delta   = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.0f);
                    m_boxDrag.start   = ImVec2{ current.x - delta.x, current.y - delta.y };
                    m_boxDrag.current = current;
                    m_boxDrag.active  = true;
                }
            }
        }

        // ── 4. Box-drag selection (same overlay as 3D path) ────────────────
        if (m_boxDrag.active)
        {
            if (mouseDown)
                m_boxDrag.current = ImGui::GetMousePos();

            ImVec2 a = m_boxDrag.start;
            ImVec2 b = m_boxDrag.current;
            ImVec2 rmin = { std::min(a.x, b.x), std::min(a.y, b.y) };
            ImVec2 rmax = { std::max(a.x, b.x), std::max(a.y, b.y) };

            ImDrawList* dl = ImGui::GetForegroundDrawList();
            if (dl)
            {
                dl->AddRectFilled(rmin, rmax, IM_COL32(0, 122, 204, 64));
                dl->AddRect(rmin, rmax, IM_COL32(0, 122, 204, 192), 0.0f, 0, 2.0f);
            }

            if (mouseReleased)
            {
                std::vector<std::shared_ptr<CSelectable>> inside;
                for (const auto& selectable : m_selectables)
                {
                    if (!selectable) continue;
                    if (selectable->GetSpace() != SpaceKind::Screen) continue;

                    const Matrix4f& mtx = selectable->GetTransform();
                    const Vector3f  t   = mtx.ExtractTranslation();
                    const ImVec2    sp  = NdcToScreen(Vector2f(t.GetX(), t.GetY()));

                    if (sp.x >= rmin.x && sp.x <= rmax.x &&
                        sp.y >= rmin.y && sp.y <= rmax.y)
                        inside.push_back(selectable);
                }

                const bool shiftHeld = ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift);
                if (shiftHeld)
                {
                    for (const auto& s : inside) AddToSelection(s);
                }
                else
                {
                    m_selection = inside;
                }
                m_lastSelected = m_selection.empty() ? nullptr : m_selection.back();
                m_boxDrag.active = false;
            }
        }

        // ── 5. Draw the 2D gizmo overlay ───────────────────────────────────
        if (hasSelection)
        {
            ImDrawList* dl = ImGui::GetForegroundDrawList();
            DrawGizmo2D(dl, mode, centrePx, kUISize, m_hoveredGizmoAxis);
        }
    }

    } // namespace ImGuiVisualizers
