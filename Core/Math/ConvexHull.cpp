#include "ConvexHull.h"

#include <algorithm>
#include <set>
#include <functional>
#include <iostream>
#include <cmath>

namespace Geometry {

// ── GenerateFromVertices ──────────────────────────────────────────────────────

std::unique_ptr<ConvexHull> ConvexHullGenerator::GenerateFromVertices(const std::vector<float>& vertices)
{
    if (vertices.size() < 9)
        return nullptr;

    std::vector<Vector3f> points;
    points.reserve(vertices.size() / 3);

    for (size_t i = 0; i < vertices.size(); i += 3)
        points.emplace_back(vertices[i], vertices[i + 1], vertices[i + 2]);

    return GenerateFromPoints(points);
}

// ── GenerateFromPoints ────────────────────────────────────────────────────────

std::unique_ptr<ConvexHull> ConvexHullGenerator::GenerateFromPoints(const std::vector<Vector3f>& points)
{
    if (points.size() < 3)
        return nullptr;

    std::vector<Vector3f> uniquePoints = RemoveDuplicatePoints(std::vector<Vector3f>(points));

    if (uniquePoints.size() < 4)
        return nullptr;

    return GenerateHullFromPoints(uniquePoints);
}

// ── GenerateHullFromPoints ────────────────────────────────────────────────────

std::unique_ptr<ConvexHull> ConvexHullGenerator::GenerateHullFromPoints(const std::vector<Vector3f>& points)
{
    auto hull = std::make_unique<ConvexHull>();

    CalculateBoundingSphere(points, hull->sphereCenter, hull->sphereRadius);

    std::vector<Vector3f> extremePoints = FindExtremePoints(points);

    hull->vertices.clear();
    hull->vertices.reserve(extremePoints.size() * 3);

    for (const auto& point : extremePoints)
    {
        hull->vertices.push_back(point.x);
        hull->vertices.push_back(point.y);
        hull->vertices.push_back(point.z);
    }

    TriangulatePoints(extremePoints, hull->indices);
    ComputeNormals(*hull);

    return hull;
}

// ── RemoveDuplicatePoints ─────────────────────────────────────────────────────

std::vector<Vector3f> ConvexHullGenerator::RemoveDuplicatePoints(std::vector<Vector3f> points)
{
    std::sort(points.begin(), points.end(), [](const Vector3f& a, const Vector3f& b)
    {
        if (a.x != b.x) return a.x < b.x;
        if (a.y != b.y) return a.y < b.y;
        return a.z < b.z;
    });

    points.erase(std::unique(points.begin(), points.end()), points.end());
    return points;
}

// ── CalculateBoundingSphere ───────────────────────────────────────────────────

void ConvexHullGenerator::CalculateBoundingSphere(const std::vector<Vector3f>& points, Vector3f& outCenter, float& outRadius)
{
    Vector3f center = Vector3f::zero();
    for (const auto& point : points)
        center += point;
    center /= static_cast<float>(points.size());

    outCenter = center;

    float maxDistanceSquared = 0.0f;
    for (const auto& point : points)
    {
        float distanceSquared = center.distanceSquaredTo(point);
        maxDistanceSquared = std::max(maxDistanceSquared, distanceSquared);
    }
    outRadius = std::sqrt(maxDistanceSquared);
}

// ── FindExtremePoints ─────────────────────────────────────────────────────────

std::vector<Vector3f> ConvexHullGenerator::FindExtremePoints(const std::vector<Vector3f>& points)
{
    auto minX = *std::min_element(points.begin(), points.end(), [](const Vector3f& a, const Vector3f& b) { return a.x < b.x; });
    auto maxX = *std::max_element(points.begin(), points.end(), [](const Vector3f& a, const Vector3f& b) { return a.x < b.x; });
    auto minY = *std::min_element(points.begin(), points.end(), [](const Vector3f& a, const Vector3f& b) { return a.y < b.y; });
    auto maxY = *std::max_element(points.begin(), points.end(), [](const Vector3f& a, const Vector3f& b) { return a.y < b.y; });
    auto minZ = *std::min_element(points.begin(), points.end(), [](const Vector3f& a, const Vector3f& b) { return a.z < b.z; });
    auto maxZ = *std::max_element(points.begin(), points.end(), [](const Vector3f& a, const Vector3f& b) { return a.z < b.z; });

    std::set<Vector3f, std::function<bool(const Vector3f&, const Vector3f&)>> uniqueHullPoints(
        [](const Vector3f& a, const Vector3f& b)
        {
            if (a.x != b.x) return a.x < b.x;
            if (a.y != b.y) return a.y < b.y;
            return a.z < b.z;
        });

    uniqueHullPoints.insert(minX);
    uniqueHullPoints.insert(maxX);
    uniqueHullPoints.insert(minY);
    uniqueHullPoints.insert(maxY);
    uniqueHullPoints.insert(minZ);
    uniqueHullPoints.insert(maxZ);

    return std::vector<Vector3f>(uniqueHullPoints.begin(), uniqueHullPoints.end());
}

// ── TriangulatePoints ─────────────────────────────────────────────────────────

void ConvexHullGenerator::TriangulatePoints(const std::vector<Vector3f>& hullPoints, std::vector<uint32_t>& outIndices)
{
    outIndices.clear();

    size_t vertexCount = hullPoints.size();
    if (vertexCount >= 4)
    {
        for (size_t i = 1; i < vertexCount - 1; ++i)
        {
            outIndices.push_back(0);
            outIndices.push_back(static_cast<uint32_t>(i));
            outIndices.push_back(static_cast<uint32_t>(i + 1));
        }
    }
}

// ── ComputeNormals ────────────────────────────────────────────────────────────

void ConvexHullGenerator::ComputeNormals(ConvexHull& hull)
{
    hull.normals.clear();
    hull.normals.reserve(hull.indices.size());

    for (size_t i = 0; i < hull.indices.size(); i += 3)
    {
        uint32_t i0 = hull.indices[i];
        uint32_t i1 = hull.indices[i + 1];
        uint32_t i2 = hull.indices[i + 2];

        Vector3f v0(hull.vertices[i0 * 3], hull.vertices[i0 * 3 + 1], hull.vertices[i0 * 3 + 2]);
        Vector3f v1(hull.vertices[i1 * 3], hull.vertices[i1 * 3 + 1], hull.vertices[i1 * 3 + 2]);
        Vector3f v2(hull.vertices[i2 * 3], hull.vertices[i2 * 3 + 1], hull.vertices[i2 * 3 + 2]);

        Vector3f edge1 = v1 - v0;
        Vector3f edge2 = v2 - v0;
        Vector3f normal = edge1.cross(edge2);

        if (!normal.isZero())
            normal.normalize();

        hull.normals.push_back(normal.x);
        hull.normals.push_back(normal.y);
        hull.normals.push_back(normal.z);
    }
}

} // namespace Geometry
