#pragma once
#include "math\Vector3f.h"
#include "math\Matrix4f.h"
#include <memory>
#include <string>
#include <limits>
#include <cmath>

namespace Rendering {

	class RenderPrimitives;

	class Manipulators {
	public:
		enum class ManipulatorType {
			Translation,
			Rotation,
			Scale
		};

		enum class ManipulatorAxis {
			None = 0,
			X = 1,
			Y = 2,
			Z = 4,
			XY = 3,
			XZ = 5,
			YZ = 6,
			XYZ = 7,
			Screen = 8  // For center square in translation manipulator
		};

		struct ManipulatorState {
			Vector3f position = Vector3f::zero();
			Vector3f rotation = Vector3f::zero();
			Vector3f scale = Vector3f::one();
			ManipulatorType currentType = ManipulatorType::Translation;
			float size = 1.0f;
			bool visible = true;
			ManipulatorAxis hoveredAxis = ManipulatorAxis::None;
			ManipulatorAxis selectedAxis = ManipulatorAxis::None;
		};

	private:
		ManipulatorState state_;

		// Visual parameters
		static constexpr float ARROW_LENGTH = 2.0f;
		static constexpr float ARROW_HEAD_LENGTH = 0.3f;
		static constexpr float ARROW_HEAD_RADIUS = 0.1f;
		static constexpr float ARROW_SHAFT_RADIUS = 0.03f;
		static constexpr float PLANE_SIZE = 0.5f;
		static constexpr float PLANE_OFFSET = 0.8f;

		static constexpr float ROTATION_RING_RADIUS = 1.5f;
		static constexpr float ROTATION_RING_THICKNESS = 0.05f;
		static constexpr int ROTATION_RING_SEGMENTS = 64;

		static constexpr float SCALE_CUBE_SIZE = 0.25f;
		static constexpr float SCALE_LINE_LENGTH = 1.8f;
		static constexpr float SCALE_CENTER_CUBE_SIZE = 0.2f;

		// Helper to get RenderPrimitives from CoreSystem
		RenderPrimitives* GetRenderPrimitives() const;

		// Colors
		Vector3f GetAxisColor(ManipulatorAxis axis, bool isHovered = false, bool isSelected = false) const;
		Vector3f GetXAxisColor(bool isHovered = false, bool isSelected = false) const;
		Vector3f GetYAxisColor(bool isHovered = false, bool isSelected = false) const;
		Vector3f GetZAxisColor(bool isHovered = false, bool isSelected = false) const;
		Vector3f GetPlaneColor(ManipulatorAxis plane, bool isHovered = false, bool isSelected = false) const;

		// Translation manipulator rendering
		void RenderTranslationArrow(const Vector3f& direction, const Vector3f& color, float scale = 1.0f) const;
		void RenderTranslationPlane(const Vector3f& normal, const Vector3f& tangent1, const Vector3f& tangent2,
			const Vector3f& color, float scale = 1.0f) const;
		void RenderTranslationManipulator() const;

		// Rotation manipulator rendering
		void RenderRotationRing(const Vector3f& normal, const Vector3f& color, float scale = 1.0f) const;
		void RenderRotationManipulator() const;

		// Scale manipulator rendering
		void RenderScaleAxis(const Vector3f& direction, const Vector3f& color, float scale = 1.0f) const;
		void RenderScaleManipulator() const;

	public:
		Manipulators();
		~Manipulators() = default;

		bool IsInitialized() const;

		// State management
		void SetPosition(const Vector3f& position);
		void SetRotation(const Vector3f& rotation);
		void SetScale(const Vector3f& scale);
		void SetManipulatorType(ManipulatorType type);
		void SetSize(float size);
		void SetVisible(bool visible);
		void SetHoveredAxis(ManipulatorAxis axis);
		void SetSelectedAxis(ManipulatorAxis axis);

		// Getters
		const Vector3f& GetPosition() const;
		const Vector3f& GetRotation() const;
		const Vector3f& GetScale() const;
		ManipulatorType GetManipulatorType() const;
		float GetSize() const;
		bool IsVisible() const;
		ManipulatorAxis GetHoveredAxis() const;
		ManipulatorAxis GetSelectedAxis() const;

		// Rendering
		void Render() const;
		void Render(const Vector3f& position) const;
		void Render(const Vector3f& position, const Vector3f& rotation, const Vector3f& scale) const;

		// Utility methods
		void CycleManipulatorType();
		void Reset();

		// Ray intersection testing (for mouse picking)
		ManipulatorAxis TestRayIntersection(const Vector3f& rayOrigin, const Vector3f& rayDirection,
			float& distance) const;
		
		ManipulatorAxis TestTranslationIntersection(const Vector3f& rayOrigin, const Vector3f& rayDirection, float& distance) const;
		ManipulatorAxis TestRotationIntersection(const Vector3f& rayOrigin, const Vector3f& rayDirection, float& distance) const;
		ManipulatorAxis TestScaleIntersection(const Vector3f& rayOrigin, const Vector3f& rayDirection, float& distance) const;

		// Primitive intersection tests
		bool TestSphereIntersection(const Vector3f& rayOrigin, const Vector3f& rayDirection, const Vector3f& sphereCenter, float radius, float& distance) const;
		bool TestCylinderIntersection(const Vector3f& rayOrigin, const Vector3f& rayDirection, const Vector3f& cylinderStart, const Vector3f& cylinderEnd, float radius, float& distance) const;
		bool TestBoxIntersection(const Vector3f& rayOrigin, const Vector3f& rayDirection, const Vector3f& boxCenter, const Vector3f& boxSize, float& distance) const;
		bool TestRingIntersection(const Vector3f& rayOrigin, const Vector3f& rayDirection, const Vector3f& ringCenter, const Vector3f& ringNormal, float radius, float tolerance, float& distance) const;

		// Conversion utilities
		static ManipulatorAxis CombineAxes(ManipulatorAxis axis1, ManipulatorAxis axis2);
		static bool HasAxis(ManipulatorAxis combined, ManipulatorAxis single);
		static std::string AxisToString(ManipulatorAxis axis);
	};

} // namespace Rendering