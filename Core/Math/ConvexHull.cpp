#include "ConvexHull.h"
#include <algorithm>
#include <set>
#include <functional>
#include <iostream>
#include <cmath>

namespace Geometry {

	std::unique_ptr<ConvexHull> ConvexHullGenerator::GenerateFromVertices(const std::vector<float>& vertices) {
		if (vertices.size() < 9) { // Need at least 3 vertices (9 floats) for a hull
			return nullptr;
		}

		// Convert to Vector3f points array
		std::vector<Vector3f> points;
		points.reserve(vertices.size() / 3);

		for (size_t i = 0; i < vertices.size(); i += 3) {
			points.emplace_back(vertices[i], vertices[i + 1], vertices[i + 2]);
		}

		return GenerateFromPoints(points);
	}

	std::unique_ptr<ConvexHull> ConvexHullGenerator::GenerateFromPoints(const std::vector<Vector3f>& points) {
		if (points.size() < 3) {
			return nullptr;
		}

		// Remove duplicate points
		std::vector<Vector3f> uniquePoints = RemoveDuplicatePoints(std::vector<Vector3f>(points));

		if (uniquePoints.size() < 4) {
			return nullptr;
		}

		return GenerateHullFromPoints(uniquePoints);
	}

	std::unique_ptr<ConvexHull> ConvexHullGenerator::GenerateHullFromPoints(const std::vector<Vector3f>& points) {
		auto hull = std::make_unique<ConvexHull>();

		// Calculate bounding sphere
		CalculateBoundingSphere(points, hull->sphereCenter, hull->sphereRadius);

		// Find extreme points for approximate convex hull
		std::vector<Vector3f> extremePoints = FindExtremePoints(points);

		// Convert extreme points to vertex array
		hull->vertices.clear();
		hull->vertices.reserve(extremePoints.size() * 3);

		for (const auto& point : extremePoints) {
			hull->vertices.push_back(point.x);
			hull->vertices.push_back(point.y);
			hull->vertices.push_back(point.z);
		}

		// Triangulate the extreme points
		TriangulatePoints(extremePoints, hull->indices);

		// Compute face normals
		ComputeNormals(*hull);

		return hull;
	}

	std::vector<Vector3f> ConvexHullGenerator::RemoveDuplicatePoints(std::vector<Vector3f> points) {
		// Sort points for duplicate removal
		std::sort(points.begin(), points.end(), [](const Vector3f& a, const Vector3f& b) {
			if (a.x != b.x) return a.x < b.x;
			if (a.y != b.y) return a.y < b.y;
			return a.z < b.z;
			});

		// Remove duplicates
		points.erase(std::unique(points.begin(), points.end()), points.end());

		return points;
	}

	void ConvexHullGenerator::CalculateBoundingSphere(const std::vector<Vector3f>& points, Vector3f& outCenter, float& outRadius) {
		// Calculate bounding sphere center (centroid of all points)
		Vector3f center = Vector3f::zero();
		for (const auto& point : points) {
			center += point;
		}
		center /= static_cast<float>(points.size());

		outCenter = center;

		// Calculate bounding sphere radius (maximum distance from center to any point)
		float maxDistanceSquared = 0.0f;
		for (const auto& point : points) {
			float distanceSquared = center.distanceSquaredTo(point);
			maxDistanceSquared = std::max(maxDistanceSquared, distanceSquared);
		}
		outRadius = std::sqrt(maxDistanceSquared);
	}

	std::vector<Vector3f> ConvexHullGenerator::FindExtremePoints(const std::vector<Vector3f>& points) {
		// Find extreme points along each axis
		auto minX = *std::min_element(points.begin(), points.end(),
			[](const Vector3f& a, const Vector3f& b) { return a.x < b.x; });
		auto maxX = *std::max_element(points.begin(), points.end(),
			[](const Vector3f& a, const Vector3f& b) { return a.x < b.x; });
		auto minY = *std::min_element(points.begin(), points.end(),
			[](const Vector3f& a, const Vector3f& b) { return a.y < b.y; });
		auto maxY = *std::max_element(points.begin(), points.end(),
			[](const Vector3f& a, const Vector3f& b) { return a.y < b.y; });
		auto minZ = *std::min_element(points.begin(), points.end(),
			[](const Vector3f& a, const Vector3f& b) { return a.z < b.z; });
		auto maxZ = *std::max_element(points.begin(), points.end(),
			[](const Vector3f& a, const Vector3f& b) { return a.z < b.z; });

		// Add extreme points to set (avoiding duplicates)
		std::set<Vector3f, std::function<bool(const Vector3f&, const Vector3f&)>> uniqueHullPoints(
			[](const Vector3f& a, const Vector3f& b) {
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

		// Convert to vector
		std::vector<Vector3f> extremePoints(uniqueHullPoints.begin(), uniqueHullPoints.end());
		return extremePoints;
	}

	void ConvexHullGenerator::TriangulatePoints(const std::vector<Vector3f>& hullPoints, std::vector<uint32_t>& outIndices) {
		outIndices.clear();

		size_t vertexCount = hullPoints.size();
		if (vertexCount >= 4) {
			// Simple fan triangulation from first vertex
			for (size_t i = 1; i < vertexCount - 1; ++i) {
				outIndices.push_back(0);
				outIndices.push_back(static_cast<uint32_t>(i));
				outIndices.push_back(static_cast<uint32_t>(i + 1));
			}
		}
	}

	void ConvexHullGenerator::ComputeNormals(ConvexHull& hull) {
		hull.normals.clear();
		hull.normals.reserve(hull.indices.size()); // One normal per vertex of each triangle

		for (size_t i = 0; i < hull.indices.size(); i += 3) {
			uint32_t i0 = hull.indices[i];
			uint32_t i1 = hull.indices[i + 1];
			uint32_t i2 = hull.indices[i + 2];

			// Get vertex positions using Vector3f
			Vector3f v0(hull.vertices[i0 * 3], hull.vertices[i0 * 3 + 1], hull.vertices[i0 * 3 + 2]);
			Vector3f v1(hull.vertices[i1 * 3], hull.vertices[i1 * 3 + 1], hull.vertices[i1 * 3 + 2]);
			Vector3f v2(hull.vertices[i2 * 3], hull.vertices[i2 * 3 + 1], hull.vertices[i2 * 3 + 2]);

			// Calculate edges
			Vector3f edge1 = v1 - v0;
			Vector3f edge2 = v2 - v0;

			// Calculate normal via cross product
			Vector3f normal = edge1.cross(edge2);

			// Normalize
			if (!normal.isZero()) {
				normal.normalize();
			}

			// Add normal for each vertex of the triangle
			hull.normals.push_back(normal.x);
			hull.normals.push_back(normal.y);
			hull.normals.push_back(normal.z);
		}
	}

} // namespace Geometry