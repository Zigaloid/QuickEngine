
#include "MathUtils.h"

#include <algorithm>
#include <cmath>
#include <cfloat>
#include "../Core/Math/Vector3f.h" // Adjust include path if your project's include dirs differ

namespace ImGuiVisualizers {

// Convert internal Ray to bx::Ray
bx::Ray ToBxRay(const Ray& r)
{
    return bx::Ray{ { r.pos.x, r.pos.y, r.pos.z }, { r.dir.x, r.dir.y, r.dir.z } };
}

float RaySegmentDist(const Ray&      ray,
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

bool RayPlaneIntersect(const Ray&      ray,
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

float RayLineClosestT(const Ray&      ray,
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

} // namespace ImGuiVisualizers