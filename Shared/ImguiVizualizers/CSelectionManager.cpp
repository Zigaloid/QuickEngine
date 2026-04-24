#include "CSelectionManager.h"
#include <bx/bounds.h>
#include <bx/math.h>
#include <cfloat>
#include <cmath>
#include <algorithm>
#include <imgui/imgui.h>

namespace ImGuiVisualizers {

// ── File-local geometry helpers ───────────────────────────────────────────

/// Closest perpendicular distance from a ray to a finite line segment.
/// @p out_tRay receives the parameter along the ray at the nearest point
/// (negative means the closest point is behind the ray origin).
static float RaySegmentDist(const bx::Ray&  ray,
                             const bx::Vec3& segA,
                             const bx::Vec3& segB,
                             float&          out_tRay)
{
    const bx::Vec3 u = bx::sub(segB, segA);    // segment direction
    const bx::Vec3 w = bx::sub(ray.pos, segA);

    // With |ray.dir| == 1:  a == 1
    const float b     = bx::dot(ray.dir, u);
    const float c     = bx::dot(u, u);
    const float d     = bx::dot(ray.dir, w);
    const float e     = bx::dot(u, w);
    const float denom = c - b * b;             // == a*c - b^2, a=1

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

    const bx::Vec3 ptOnSeg = bx::add(segA, bx::mul(u, s));
    const bx::Vec3 ptOnRay = bx::add(ray.pos, bx::mul(ray.dir, out_tRay));
    return bx::length(bx::sub(ptOnRay, ptOnSeg));
}

/// Ray-plane intersection.
/// @return false if the plane is edge-on to the ray or the hit is behind the origin.
static bool RayPlaneIntersect(const bx::Ray&  ray,
                               const bx::Vec3& planePoint,
                               const bx::Vec3& planeNormal,
                               float&          out_t,
                               bx::Vec3&       out_hit)
{
    const float denom = bx::dot(ray.dir, planeNormal);
    if (bx::abs(denom) < 1e-6f)
        return false;

    out_t = bx::dot(bx::sub(planePoint, ray.pos), planeNormal) / denom;
    if (out_t < 0.0f)
        return false;

    out_hit = bx::add(ray.pos, bx::mul(ray.dir, out_t));
    return true;
}

/// Parameter `t` along the infinite line (lineOrigin + t*lineDir) at the
/// point closest to the given ray. Assumes ray.dir and lineDir are unit vectors.
static float RayLineClosestT(const bx::Ray&  ray,
                              const bx::Vec3& lineOrigin,
                              const bx::Vec3& lineDir)
{
    const bx::Vec3 w     = bx::sub(ray.pos, lineOrigin);
    const float    b     = bx::dot(ray.dir, lineDir);
    const float    d     = bx::dot(ray.dir, w);
    const float    e     = bx::dot(lineDir, w);
    const float    denom = 1.0f - b * b;   // |ray.dir|=1, |lineDir|=1

    if (denom < 1e-8f)
        return 0.0f;

    return (e - b * d) / denom;
}

/// Builds a column-major 4×4 rotation matrix for @p angle radians
/// around the unit vector @p axis using Rodrigues' formula.
static void BuildRotationMatrix(float* out16, const bx::Vec3& axis, float angle)
{
    const float c = bx::cos(angle);
    const float s = bx::sin(angle);
    const float t = 1.0f - c;
    const float x = axis.x, y = axis.y, z = axis.z;

    out16[0]  = t*x*x + c;    out16[4]  = t*x*y - s*z; out16[8]  = t*x*z + s*y; out16[12] = 0.0f;
    out16[1]  = t*x*y + s*z;  out16[5]  = t*y*y + c;   out16[9]  = t*y*z - s*x; out16[13] = 0.0f;
    out16[2]  = t*x*z - s*y;  out16[6]  = t*y*z + s*x; out16[10] = t*z*z + c;   out16[14] = 0.0f;
    out16[3]  = 0.0f;          out16[7]  = 0.0f;         out16[11] = 0.0f;         out16[15] = 1.0f;
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

bx::Ray CSelectionManager::BuildPickRay() const
{
    const ImVec2 mouse = ImGui::GetMousePos();
    const float ndcX =  ((mouse.x - m_viewportMin.x) / m_viewportSize.x) * 2.0f - 1.0f;
    const float ndcY = 1.0f - ((mouse.y - m_viewportMin.y) / m_viewportSize.y) * 2.0f;

    const float aspect = m_viewportSize.x / m_viewportSize.y;
    float view[16], proj[16], VP[16], invVP[16];
    m_camera->GetViewMatrix(view);
    m_camera->GetProjectionMatrix(proj, aspect);
    bx::mtxMul(VP, view, proj);
    bx::mtxInverse(invVP, VP);

    return bx::makeRay(ndcX, ndcY, invVP);
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

    const bx::Ray ray = BuildPickRay();
    const bool    shiftHeld = ImGui::GetIO().KeyShift;

    std::shared_ptr<CSelectable> nearest;
    float nearestDistSq = FLT_MAX;

    for (const auto& selectable : m_selectables)
    {
        if (!selectable) continue;

        const Vector4f bs = selectable->GetBoundingSphere();
        const float*   m  = selectable->GetTransform().data(); // column-major

        // Transform sphere centre into world space
        const float cx = m[0]*bs.x + m[4]*bs.y + m[8] *bs.z + m[12];
        const float cy = m[1]*bs.x + m[5]*bs.y + m[9] *bs.z + m[13];
        const float cz = m[2]*bs.x + m[6]*bs.y + m[10]*bs.z + m[14];

        // World-space radius (largest column scale * local radius)
        const float sx = bx::sqrt(m[0]*m[0] + m[1]*m[1] + m[2]*m[2]);
        const float sy = bx::sqrt(m[4]*m[4] + m[5]*m[5] + m[6]*m[6]);
        const float sz = bx::sqrt(m[8]*m[8] + m[9]*m[9] + m[10]*m[10]);
        const float radius = bs.w * bx::max(sx, bx::max(sy, sz));

        bx::Sphere sphere;
        sphere.center = { cx, cy, cz };
        sphere.radius = radius > 0.0f ? radius : 0.5f;

        bx::Hit hit;
        if (!bx::intersect(ray, sphere, &hit)) continue;

        // Rank by squared   distance from ray origin to the object's world origin
        const float dx = m[12] - ray.pos.x;
        const float dy = m[13] - ray.pos.y;
        const float dz = m[14] - ray.pos.z;
        const float distSq = dx*dx + dy*dy + dz*dz;

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

GizmoAxis CSelectionManager::HitTestGizmo(GizmoMode    mode,
                                            const float* gizmoMtx,
                                            float        effectiveSize) const
{
    if (!m_camera)
        return GizmoAxis::None;

    const bx::Ray  ray    = BuildPickRay();
    const bx::Vec3 origin = { gizmoMtx[12], gizmoMtx[13], gizmoMtx[14] };

    // Normalise the matrix columns to get pure world-space direction vectors,
    // regardless of any scale baked into the gizmo transform.
    const bx::Vec3 axes[3] = {
        bx::normalize(bx::Vec3{ gizmoMtx[0], gizmoMtx[1], gizmoMtx[2]  }),
        bx::normalize(bx::Vec3{ gizmoMtx[4], gizmoMtx[5], gizmoMtx[6]  }),
        bx::normalize(bx::Vec3{ gizmoMtx[8], gizmoMtx[9], gizmoMtx[10] }),
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
            bx::Vec3 hitPt = { 0.0f, 0.0f, 0.0f };
            if (!RayPlaneIntersect(ray, origin, axes[i], tHit, hitPt))
                continue;

            const float distFromCenter = bx::length(bx::sub(hitPt, origin));
            const float delta          = bx::abs(distFromCenter - effectiveSize);

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
            bx::Vec3 hitPt = { 0.0f, 0.0f, 0.0f };
            if (!RayPlaneIntersect(ray, origin, axes[kNormal[i]], tHit, hitPt))
                continue;

            const bx::Vec3 rel = bx::sub(hitPt, origin);
            const float    u   = bx::dot(rel, axes[kTangentA[i]]);
            const float    v   = bx::dot(rel, axes[kTangentB[i]]);

            if (u >= planeMin && u <= planeMax && v >= planeMin && v <= planeMax)
            {
                if (tHit < bestDist)
                {
                    bestDist = tHit;
                    bestAxis = kPlaneAxes[i];
                }
            }
        }

        // Plane handles take priority — return immediately if any are hit.
        if (bestAxis != GizmoAxis::None)
            return bestAxis;
    }

    // Axis shafts
    const GizmoAxis kAxisEnum[3] = { GizmoAxis::X, GizmoAxis::Y, GizmoAxis::Z };

    for (int i = 0; i < 3; ++i)
    {
        const bx::Vec3 segEnd = bx::add(origin, bx::mul(axes[i], shaftEnd));

        float tRay;
        const float dist = RaySegmentDist(ray, origin, segEnd, tRay);

        // Ignore hits behind the camera and those outside the hit radius.
        if (tRay > 0.0f && dist < hitRadius && dist < bestDist)
        {
            bestDist = dist;
            bestAxis = kAxisEnum[i];
        }
    }

    return bestAxis;
}

// ── Gizmo drag ────────────────────────────────────────────────────────────

void CSelectionManager::BeginGizmoDrag(GizmoMode mode, const float* gizmoMtx)
{
    if (m_selection.empty())
        return;

    const bx::Ray  ray    = BuildPickRay();
    const bx::Vec3 origin = { gizmoMtx[12], gizmoMtx[13], gizmoMtx[14] };

    const bx::Vec3 axes[3] = {
        bx::normalize(bx::Vec3{ gizmoMtx[0], gizmoMtx[1], gizmoMtx[2]  }),
        bx::normalize(bx::Vec3{ gizmoMtx[4], gizmoMtx[5], gizmoMtx[6]  }),
        bx::normalize(bx::Vec3{ gizmoMtx[8], gizmoMtx[9], gizmoMtx[10] }),
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
        // The ring normal is the axis the ring is perpendicular to.
        // axisDir / axisDir2 are its two tangent axes (used to measure angle).
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
        bx::Vec3 hitPt = { 0.0f, 0.0f, 0.0f };
        RayPlaneIntersect(ray, origin, m_drag.planeNormal, tHit, hitPt);

        const bx::Vec3 rel = bx::sub(hitPt, origin);
        m_drag.angleStart  = std::atan2(bx::dot(rel, m_drag.axisDir2),
                                        bx::dot(rel, m_drag.axisDir));
        break;
    }
    }
}

void CSelectionManager::ApplyGizmoDrag()
{
    if (!m_drag.active || m_selection.empty())
        return;

    const bx::Ray ray = BuildPickRay();

    switch (m_drag.mode)
    {
    case GizmoMode::Translate: ApplyTranslateDrag(ray); break;
    case GizmoMode::Scale:     ApplyScaleDrag    (ray); break;
    case GizmoMode::Rotate:    ApplyRotateDrag   (ray); break;
    }
}

void CSelectionManager::ApplyTranslateDrag(const bx::Ray& ray)
{
    bx::Vec3 delta = { 0.0f, 0.0f, 0.0f };

    const bool isSingleAxis = (m_drag.axis == GizmoAxis::X ||
                                m_drag.axis == GizmoAxis::Y ||
                                m_drag.axis == GizmoAxis::Z);

    if (isSingleAxis)
    {
        const float tCurrent = RayLineClosestT(ray, m_drag.origin, m_drag.axisDir);
        delta = bx::mul(m_drag.axisDir, tCurrent - m_drag.tStart);
    }
    else
    {
        float    tHit;
        bx::Vec3 hitPt = { 0.0f, 0.0f, 0.0f };
        if (!RayPlaneIntersect(ray, m_drag.origin, m_drag.planeNormal, tHit, hitPt))
            return;
        delta = bx::sub(hitPt, m_drag.hitStart);
    }

    for (size_t i = 0; i < m_selection.size(); ++i)
    {
        Matrix4f* transform = m_selection[i]->GetTransformMutable();
        if (!transform) continue;

        const Matrix4f& start = m_drag.startTransforms[i];
        *transform = start;
        transform->data()[12] = start.data()[12] + delta.x;
        transform->data()[13] = start.data()[13] + delta.y;
        transform->data()[14] = start.data()[14] + delta.z;
    }
}

void CSelectionManager::ApplyScaleDrag(const bx::Ray& ray)
{
    const float tCurrent = RayLineClosestT(ray, m_drag.origin, m_drag.axisDir);
    const float absStart = bx::abs(m_drag.tStart);
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

        const Matrix4f& start = m_drag.startTransforms[i];
        *transform = start;

        // Scale the chosen axis column (which holds scale × direction).
        const int base = axisCol * 4;
        transform->data()[base + 0] = start.data()[base + 0] * factor;
        transform->data()[base + 1] = start.data()[base + 1] * factor;
        transform->data()[base + 2] = start.data()[base + 2] * factor;
    }
}

void CSelectionManager::ApplyRotateDrag(const bx::Ray& ray)
{
    float    tHit;
    bx::Vec3 hitPt = { 0.0f, 0.0f, 0.0f };
    if (!RayPlaneIntersect(ray, m_drag.origin, m_drag.planeNormal, tHit, hitPt))
        return;

    const bx::Vec3 rel        = bx::sub(hitPt, m_drag.origin);
    const float    angleCur   = std::atan2(bx::dot(rel, m_drag.axisDir2),
                                            bx::dot(rel, m_drag.axisDir));
    const float    deltaAngle = angleCur - m_drag.angleStart;

    float rotMtx[16];
    BuildRotationMatrix(rotMtx, m_drag.planeNormal, deltaAngle);

    for (size_t i = 0; i < m_selection.size(); ++i)
    {
        Matrix4f* transform = m_selection[i]->GetTransformMutable();
        if (!transform) continue;

        const Matrix4f& start = m_drag.startTransforms[i];

        // Rotate the object's position around the gizmo origin.
        const float px = start.data()[12] - m_drag.origin.x;
        const float py = start.data()[13] - m_drag.origin.y;
        const float pz = start.data()[14] - m_drag.origin.z;

        *transform = start;
        transform->data()[12] = rotMtx[0]*px + rotMtx[4]*py + rotMtx[8] *pz + m_drag.origin.x;
        transform->data()[13] = rotMtx[1]*px + rotMtx[5]*py + rotMtx[9] *pz + m_drag.origin.y;
        transform->data()[14] = rotMtx[2]*px + rotMtx[6]*py + rotMtx[10]*pz + m_drag.origin.z;

        // Rotate the upper-left 3×3 (orientation + scale columns).
        for (int col = 0; col < 3; ++col)
        {
            const float cx = start.data()[col*4 + 0];
            const float cy = start.data()[col*4 + 1];
            const float cz = start.data()[col*4 + 2];
            transform->data()[col*4 + 0] = rotMtx[0]*cx + rotMtx[4]*cy + rotMtx[8] *cz;
            transform->data()[col*4 + 1] = rotMtx[1]*cx + rotMtx[5]*cy + rotMtx[9] *cz;
            transform->data()[col*4 + 2] = rotMtx[2]*cx + rotMtx[6]*cy + rotMtx[10]*cz;
        }
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

    // ── 2. Build gizmo matrix from current (possibly just-updated) transforms
    float gizmoMtx[16];

    if (m_selection.size() == 1)
    {
        const float* m = m_selection.front()->GetTransform().data();

        // Strip scale: normalize each rotation column so the gizmo is
        // rendered at a uniform size regardless of the object's scale.
        const float scaleX = bx::sqrt(m[0]*m[0] + m[1]*m[1] + m[2]*m[2]);
        const float scaleY = bx::sqrt(m[4]*m[4] + m[5]*m[5] + m[6]*m[6]);
        const float scaleZ = bx::sqrt(m[8]*m[8] + m[9]*m[9] + m[10]*m[10]);

        const float invX = scaleX > 1e-8f ? 1.0f / scaleX : 0.0f;
        const float invY = scaleY > 1e-8f ? 1.0f / scaleY : 0.0f;
        const float invZ = scaleZ > 1e-8f ? 1.0f / scaleZ : 0.0f;

        gizmoMtx[0]  = m[0]  * invX;  gizmoMtx[1]  = m[1]  * invX;  gizmoMtx[2]  = m[2]  * invX;  gizmoMtx[3]  = 0.0f;
        gizmoMtx[4]  = m[4]  * invY;  gizmoMtx[5]  = m[5]  * invY;  gizmoMtx[6]  = m[6]  * invY;  gizmoMtx[7]  = 0.0f;
        gizmoMtx[8]  = m[8]  * invZ;  gizmoMtx[9]  = m[9]  * invZ;  gizmoMtx[10] = m[10] * invZ;  gizmoMtx[11] = 0.0f;
        gizmoMtx[12] = m[12];          gizmoMtx[13] = m[13];          gizmoMtx[14] = m[14];          gizmoMtx[15] = 1.0f;
    }
    else
    {
        float cx = 0.0f, cy = 0.0f, cz = 0.0f;
        for (const auto& sel : m_selection)
        {
            const float* m = sel->GetTransform().data();
            cx += m[12]; cy += m[13]; cz += m[14];
        }
        const float inv = 1.0f / static_cast<float>(m_selection.size());
        bx::mtxIdentity(gizmoMtx);
        gizmoMtx[12] = cx * inv;
        gizmoMtx[13] = cy * inv;
        gizmoMtx[14] = cz * inv;
    }

    // ── 3. Constant screen-size scaling ───────────────────────────────
    constexpr float kScreenFraction = 0.1f;

    float eye[3];
    m_camera->GetEyePosition(eye);

    const float dx   = gizmoMtx[12] - eye[0];
    const float dy   = gizmoMtx[13] - eye[1];
    const float dz   = gizmoMtx[14] - eye[2];
    const float dist = bx::sqrt(dx*dx + dy*dy + dz*dz);

    const float fovRad        = m_camera->GetFov() * bx::kPi / 180.0f;
    const float effectiveSize = size * dist * bx::tan(fovRad * 0.5f) * kScreenFraction;

    // ── 4. Hover detection and drag initiation (idle only) ────────────
    if (!m_drag.active)
    {
        m_hoveredGizmoAxis = HitTestGizmo(mode, gizmoMtx, effectiveSize);

        if (mousePressed && m_hoveredGizmoAxis != GizmoAxis::None && IsMouseInViewport())
            BeginGizmoDrag(mode, gizmoMtx);
    }

    // ── 5. Render ─────────────────────────────────────────────────────
    m_gizmoRenderer.RenderGizmo(viewId, gizmoMtx, mode, m_hoveredGizmoAxis, effectiveSize);
}

} // namespace ImGuiVisualizers