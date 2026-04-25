#pragma once
#include "Math\Vector3f.h"
#include <bx/bounds.h> // for bx::Ray

// Forward-declare Vector3f to keep header lightweight; implementation includes full math headers.
class Vector3f;

namespace ImGuiVisualizers {

/// Lightweight ray used by selection / picking utilities.
struct Ray
{
    Vector3f pos;
    Vector3f dir;
};

/// Convert internal Ray to bx::Ray for interop.
bx::Ray ToBxRay(const Ray& r);

/// Closest perpendicular distance from a ray to a finite line segment.
/// @param out_tRay receives the parameter along the ray at the nearest point
///                 (negative means the closest point is behind the ray origin).
float RaySegmentDist(const Ray&      ray,
                     const Vector3f& segA,
                     const Vector3f& segB,
                     float&          out_tRay);

/// Ray-plane intersection.
/// @return false if the plane is edge-on to the ray or the hit is behind the origin.
bool RayPlaneIntersect(const Ray&      ray,
                       const Vector3f& planePoint,
                       const Vector3f& planeNormal,
                       float&          out_t,
                       Vector3f&       out_hit);

/// Parameter `t` along an infinite line (lineOrigin + t*lineDir) at the point
/// closest to the given ray. Assumes ray.dir and lineDir are unit vectors.
float RayLineClosestT(const Ray&      ray,
                      const Vector3f& lineOrigin,
                      const Vector3f& lineDir);

} // namespace ImGuiVisualizers
