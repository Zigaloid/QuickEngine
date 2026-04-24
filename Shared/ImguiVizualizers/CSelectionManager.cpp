#include "CSelectionManager.h"
#include <bx/bounds.h>
#include <cfloat>
#include <cmath>
#include <algorithm>
#include <imgui/imgui.h>

namespace ImGuiVisualizers {

// ── File-local geometry helpers ───────────────────────────────────────────

/// Converts the internal Ray to a bx::Ray for use with bx::intersect only.
static bx::Ray ToBxRay(const Ray& r)
{
    return bx::Ray{ { r.pos.x, r.pos.y, r.pos.z }, { r.dir.x, r.dir.y, r.dir.z } };
}

/// Closest perpendicular distance from a ray to a finite line segment.
/// @p out_tRay receives the parameter along the ray at the nearest point
/// (negative means the closest point is behind the ray origin).
static float RaySegmentDist(const Ray&      ray,
                             const Vector3f& segA,
                             const Vector3f& segB,
                             float&          out_tRay)
{
    const Vector3f u = segB - segA;
    const Vector3f w = ray.pos - segA;

    const float b     = ray.dir.Dot(u);
    const float c     = u.Dot(u);
    const float d     = ray.dir.Dot(w);
    const float e     = u.Dot(w);
    const float denom = c - b * b;     // == a*c - b^2, a=1 (|ray.dir|==1)

    float s; // parameter along segment [0,1]
    if (denom < 1e-8f)
    {
        // Ray and segment are parallel
        s        = 0.0f;
        out_tRay = d;
    }
    else
    {
        s        = std::clamp((e - b * d) / denom, 0.0f, 1.0f);
        out_tRay = b * s - d;
    }

    const Vector3f ptOnSeg = segA + u * s;
    const Vector3f ptOnRay = ray.pos + ray.dir * out_tRay;
    return (ptOnRay - ptOnSeg).Length();
}

/// Ray-plane intersection.
/// @return false if the plane is edge-on to the ray or the hit is behind the origin.
static bool RayPlaneIntersect(const Ray&      ray,
                               const Vector3f& planePoint,
                               const Vector3f& planeNormal,
                               float&          out_t,
                               Vector3f&       out_hit)
{
    const float denom = ray.dir.Dot(planeNormal);
    if (std::abs(denom) < 1e-6f)
        return false;

    out_t = (planePoint - ray.pos).Dot(planeNormal) / denom;
    if (out_t < 0.0f)
        return false;

    out_hit = ray.pos + ray.dir * out_t;
    return true;
}

/// Parameter `t` along the infinite line (lineOrigin + t*lineDir) at the
/// point closest to the given ray. Assumes ray.dir and lineDir are unit vectors.
static float RayLineClosestT(const Ray&      ray,
                              const Vector3f& lineOrigin,
                              const Vector3f& lineDir)
{
    const Vector3f w     = ray.pos - lineOrigin;
    const float    b     = ray.dir.Dot(lineDir);
    const float    d     = ray.dir.Dot(w);
    const float    e     = lineDir.Dot(w);
    const float    denom = 1.0f - b * b;   // |ray.dir|=1, |lineDir|=1

    if (denom < 1e-8f)
        return 0.0f;

    return (e - b * d) / denom;
}

// ── CSelectionManager ─────────────────────────────────────────────────────

void CSelectionManager::SetViewInfo(Bgfx3DCamera& camera,
                                    const ImVec2& viewportMin,
                                    const ImVec2& viewportSize)
{
    m_camera       = &camera;
    m_viewportMin  = viewportMin;
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

    const Ray  ray       = BuildPickRay();
    const bool shiftHeld = ImGui::GetIO().KeyShift;

    std::shared_ptr<CSelectable> nearest;
    float nearestDistSq = FLT_MAX;

    for (const auto& selectable : m_selectables)
    {
        if (!selectable) continue;

        const Vector4f    bs     = selectable->GetBoundingSphere();
        const Matrix4f&   mtx    = selectable->GetTransform();

        // Transform sphere centre into world space using the full TRS matrix.
        const Vector3f worldCentre = mtx.TransformPoint(Vector3f(bs.x, bs.y, bs.z));

        // World-space radius: local radius scaled by the largest column magnitude.
        const Vector3f scale  = mtx.ExtractScale();
        const float    radius = bs.w * std::max({ scale.x, scale.y, scale.z });

        bx::Sphere sphere;
        sphere.center = { worldCentre.x, worldCentre.y, worldCentre.z };
        sphere.radius = radius > 0.0f ? radius : 0.5f;

        bx::Hit hit;
        if (!bx::intersect(ToBxRay(ray), sphere, &hit)) continue;

        // Rank by squared distance from ray origin to the object's world origin.
        const float distSq = mtx.ExtractTranslation().DistanceSquaredTo(ray.pos);

        if (distSq < nearestDistSq)
        {
            nearestDistSq = distSq;
            nearest       = selectable;
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

    const Ray      ray    = BuildPickRay();
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
        float     bestT    = FLT_MAX;

        for (int i = 0; i < 3; ++i)
        {
            float    tHit;
            Vector3f hitPt;
            if (!RayPlaneIntersect(ray, origin, axes[i], tHit, hitPt))
                continue;

            const float distFromCenter = (hitPt - origin).Length();
            const float delta          = std::abs(distFromCenter - effectiveSize);

            if (delta < effectiveSize * kRingHalfWidth && tHit < bestT)
            {
                bestT    = tHit;
                bestAxis = kRingAxes[i];
            }
        }

        return bestAxis;
    }

    // ── Translate / Scale ─────────────────────────────────────────────
    const float shaftEnd  = effectiveSize * (mode == GizmoMode::Scale ? 0.92f : 1.0f);
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
        constexpr int kNormal  [3] = { 2, 0, 1 };

        const GizmoAxis kPlaneAxes[3] = { GizmoAxis::XY, GizmoAxis::YZ, GizmoAxis::XZ };

        for (int i = 0; i < 3; ++i)
        {
            float    tHit;
            Vector3f hitPt;
            if (!RayPlaneIntersect(ray, origin, axes[kNormal[i]], tHit, hitPt))
                continue;

            const Vector3f rel = hitPt - origin;
            const float    u   = rel.Dot(axes[kTangentA[i]]);
            const float    v   = rel.Dot(axes[kTangentB[i]]);

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

    const Ray      ray    = BuildPickRay();
    const Vector3f origin = gizmoMtx.ExtractTranslation();

    const Vector3f axes[3] = {
        gizmoMtx.GetColumn(0).Normalized(),
        gizmoMtx.GetColumn(1).Normalized(),
        gizmoMtx.GetColumn(2).Normalized(),
    };

    m_drag.active = true;
    m_drag.mode   = mode;
    m_drag.axis   = m_hoveredGizmoAxis;
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
            m_drag.tStart  = RayLineClosestT(ray, origin, axes[0]);
            break;
        case GizmoAxis::Y:
            m_drag.axisDir = axes[1];
            m_drag.tStart  = RayLineClosestT(ray, origin, axes[1]);
            break;
        case GizmoAxis::Z:
            m_drag.axisDir = axes[2];
            m_drag.tStart  = RayLineClosestT(ray, origin, axes[2]);
            break;
        case GizmoAxis::XY:
            m_drag.axisDir     = axes[0];
            m_drag.axisDir2    = axes[1];
            m_drag.planeNormal = axes[2];
            { float t; RayPlaneIntersect(ray, origin, axes[2], t, m_drag.hitStart); }
            break;
        case GizmoAxis::YZ:
            m_drag.axisDir     = axes[1];
            m_drag.axisDir2    = axes[2];
            m_drag.planeNormal = axes[0];
            { float t; RayPlaneIntersect(ray, origin, axes[0], t, m_drag.hitStart); }
            break;
        case GizmoAxis::XZ:
            m_drag.axisDir     = axes[0];
            m_drag.axisDir2    = axes[2];
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
            m_drag.axisDir     = axes[1];
            m_drag.axisDir2    = axes[2];
            break;
        case GizmoAxis::Y:
            m_drag.planeNormal = axes[1];
            m_drag.axisDir     = axes[0];
            m_drag.axisDir2    = axes[2];
            break;
        case GizmoAxis::Z:
            m_drag.planeNormal = axes[2];
            m_drag.axisDir     = axes[0];
            m_drag.axisDir2    = axes[1];
            break;
        default: break;
        }

        float    tHit;
        Vector3f hitPt;
        RayPlaneIntersect(ray, origin, m_drag.planeNormal, tHit, hitPt);

        const Vector3f rel = hitPt - origin;
        m_drag.angleStart  = std::atan2(rel.Dot(m_drag.axisDir2), rel.Dot(m_drag.axisDir));
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
    case GizmoMode::Scale:     ApplyScaleDrag    (ray); break;
    case GizmoMode::Rotate:    ApplyRotateDrag   (ray); break;
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
    if      (m_drag.axis == GizmoAxis::X) axisCol = 0;
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

    const Vector3f rel        = hitPt - m_drag.origin;
    const float    angleCur   = std::atan2(rel.Dot(m_drag.axisDir2), rel.Dot(m_drag.axisDir));
    const float    deltaAngle = angleCur - m_drag.angleStart;

    // Rotate around the gizmo origin: T(origin) * R * T(-origin) * startTransform
    const Matrix4f R          = Matrix4f::Rotation(m_drag.planeNormal, deltaAngle);
    const Matrix4f fromOrigin = Matrix4f::Translation( m_drag.origin);
    const Matrix4f toOrigin   = Matrix4f::Translation(-m_drag.origin);

    for (size_t i = 0; i < m_selection.size(); ++i)
    {
        Matrix4f* transform = m_selection[i]->GetTransformMutable();
        if (!transform) continue;

        *transform = fromOrigin * R * toOrigin * m_drag.startTransforms[i];
    }
}

// ── RenderSelectionGizmo ──────────────────────────────────────────────────

void CSelectionManager::RenderSelectionGizmo(bgfx::ViewId viewId,
                                              GizmoMode    mode,
                                              float        size)
{
    if (m_selection.empty() || !m_camera || !m_gizmoRenderer.IsInitialized())
    {
        m_hoveredGizmoAxis = GizmoAxis::None;
        return;
    }

    const bool mouseDown     = ImGui::IsMouseDown    (ImGuiMouseButton_Left);
    const bool mousePressed  = ImGui::IsMouseClicked (ImGuiMouseButton_Left);
    const bool mouseReleased = ImGui::IsMouseReleased(ImGuiMouseButton_Left);

    // ── 1. Apply ongoing drag (before recomputing gizmo position) ─────
    if (m_drag.active)
    {
        if (mouseReleased)
        {
            m_drag.active = false;
            m_drag.startTransforms.clear();
        }
        else if (mouseDown)
        {
            ApplyGizmoDrag();
        }
    }

    // ── 2. Build gizmo matrix from current (possibly just-updated) transforms ─
    Matrix4f gizmo;

    if (m_selection.size() == 1)
    {
        const Matrix4f& objMtx = m_selection.front()->GetTransform();
        const Vector3f  scale  = objMtx.ExtractScale();

        // Strip scale: normalize each rotation column so the gizmo renders
        // at a uniform size regardless of the object's scale.
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
        // Multi-selection: identity rotation at centroid of all selected objects.
        Vector3f centroid;
        for (const auto& sel : m_selection)
            centroid += sel->GetTransform().ExtractTranslation();
        centroid = centroid * (1.0f / static_cast<float>(m_selection.size()));

        // gizmo is already identity from default constructor
        gizmo.SetTranslation(centroid);
    }

    // ── 3. Constant screen-size scaling ───────────────────────────────
    constexpr float kScreenFraction = 0.1f;

    float eye[3];
    m_camera->GetEyePosition(eye);

    const float dist          = gizmo.ExtractTranslation().DistanceTo(Vector3f(eye[0], eye[1], eye[2]));
    const float fovRad        = m_camera->GetFov() * static_cast<float>(M_PI) / 180.0f;
    const float effectiveSize = size * dist * std::tan(fovRad * 0.5f) * kScreenFraction;

    // ── 4. Hover detection and drag initiation (idle only) ────────────
    if (!m_drag.active)
    {
        m_hoveredGizmoAxis = HitTestGizmo(mode, gizmo, effectiveSize);

        if (mousePressed && m_hoveredGizmoAxis != GizmoAxis::None && IsMouseInViewport())
            BeginGizmoDrag(mode, gizmo);
    }

    // ── 5. Render ─────────────────────────────────────────────────────
    m_gizmoRenderer.RenderGizmo(viewId, gizmo.data(), mode, m_hoveredGizmoAxis, effectiveSize);
}

} // namespace ImGuiVisualizers