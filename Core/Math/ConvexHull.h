#pragma once

#include "Vector3f.h"

#include <vector>
#include <memory>
#include <cstdint>

namespace Geometry {

// ── ConvexHull ────────────────────────────────────────────────────────────────

/** @brief Convex hull with bounding sphere for collision detection and rendering optimisation. */
struct ConvexHull
{
    std::vector<float>    vertices;     // x,y,z coordinates
    std::vector<uint32_t> indices;      // Triangle indices
    std::vector<float>    normals;      // Face normals

    Vector3f sphereCenter = Vector3f::zero();
    float    sphereRadius = 0.0f;
};

// ── ConvexHullGenerator ───────────────────────────────────────────────────────

/** @brief Factory class for generating convex hulls from vertex data. */
class ConvexHullGenerator
{
public:
    /** @brief Generate a convex hull from a flat array of vertex positions (x,y,z,...).
     *  @param vertices Flat array of vertex positions.
     *  @return Unique pointer to generated hull, or nullptr on failure. */
    static std::unique_ptr<ConvexHull> GenerateFromVertices(const std::vector<float>& vertices);

    /** @brief Generate a convex hull from an array of 3D points.
     *  @param points Array of 3D points.
     *  @return Unique pointer to generated hull, or nullptr on failure. */
    static std::unique_ptr<ConvexHull> GenerateFromPoints(const std::vector<Vector3f>& points);

    /** @brief Compute face normals for an existing convex hull.
     *  @param hull Convex hull to compute normals for. */
    static void ComputeNormals(ConvexHull& hull);

private:
    static std::unique_ptr<ConvexHull> GenerateHullFromPoints(const std::vector<Vector3f>& points);
    static std::vector<Vector3f>       RemoveDuplicatePoints(std::vector<Vector3f> points);
    static void CalculateBoundingSphere(const std::vector<Vector3f>& points, Vector3f& outCenter, float& outRadius);
    static std::vector<Vector3f>       FindExtremePoints(const std::vector<Vector3f>& points);
    static void TriangulatePoints(const std::vector<Vector3f>& hullPoints, std::vector<uint32_t>& outIndices);
};

} // namespace Geometry
