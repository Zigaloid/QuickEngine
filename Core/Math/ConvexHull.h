#pragma once

#include "Vector3f.h"
#include <vector>
#include <memory>
#include <cstdint>

namespace Geometry {

	/// <summary>
	/// Represents a convex hull with bounding sphere for collision detection and rendering optimization
	/// </summary>
	struct ConvexHull {
		std::vector<float> vertices;      // x,y,z coordinates (kept for compatibility)
		std::vector<uint32_t> indices;    // Triangle indices
		std::vector<float> normals;       // Face normals (kept for compatibility)

		// Bounding sphere
		Vector3f sphereCenter = Vector3f::zero(); // Center position as Vector3f
		float sphereRadius = 0.0f;
	};

	/// <summary>
	/// Factory class for generating convex hulls from vertex data
	/// </summary>
	class ConvexHullGenerator {
	public:
		/// <summary>
		/// Generate a convex hull from a flat array of vertex positions
		/// </summary>
		/// <param name="vertices">Array of vertex positions (x,y,z,x,y,z,...)</param>
		/// <returns>Unique pointer to generated convex hull, or nullptr if generation failed</returns>
		static std::unique_ptr<ConvexHull> GenerateFromVertices(const std::vector<float>& vertices);

		/// <summary>
		/// Generate a convex hull from Vector3f points
		/// </summary>
		/// <param name="points">Array of 3D points</param>
		/// <returns>Unique pointer to generated convex hull, or nullptr if generation failed</returns>
		static std::unique_ptr<ConvexHull> GenerateFromPoints(const std::vector<Vector3f>& points);

		/// <summary>
		/// Compute face normals for an existing convex hull
		/// </summary>
		/// <param name="hull">Convex hull to compute normals for</param>
		static void ComputeNormals(ConvexHull& hull);

	private:
		/// <summary>
		/// Internal method to generate hull from preprocessed points
		/// </summary>
		/// <param name="points">Vector of unique, sorted points</param>
		/// <returns>Unique pointer to generated convex hull</returns>
		static std::unique_ptr<ConvexHull> GenerateHullFromPoints(const std::vector<Vector3f>& points);

		/// <summary>
		/// Remove duplicate points from a vector
		/// </summary>
		/// <param name="points">Vector of points to deduplicate</param>
		/// <returns>Vector with unique points only</returns>
		static std::vector<Vector3f> RemoveDuplicatePoints(std::vector<Vector3f> points);

		/// <summary>
		/// Calculate bounding sphere for a set of points
		/// </summary>
		/// <param name="points">Vector of points</param>
		/// <param name="outCenter">Output sphere center</param>
		/// <param name="outRadius">Output sphere radius</param>
		static void CalculateBoundingSphere(const std::vector<Vector3f>& points, Vector3f& outCenter, float& outRadius);

		/// <summary>
		/// Find extreme points (min/max in each axis) for approximate hull generation
		/// </summary>
		/// <param name="points">Vector of points to analyze</param>
		/// <returns>Vector of extreme points</returns>
		static std::vector<Vector3f> FindExtremePoints(const std::vector<Vector3f>& points);

		/// <summary>
		/// Triangulate a set of points using simple fan triangulation
		/// </summary>
		/// <param name="hullPoints">Vector of hull points</param>
		/// <param name="outIndices">Output triangle indices</param>
		static void TriangulatePoints(const std::vector<Vector3f>& hullPoints, std::vector<uint32_t>& outIndices);
	};

} // namespace Geometry