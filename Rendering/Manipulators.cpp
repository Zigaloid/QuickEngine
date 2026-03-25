#include "RenderingSystem.h"
#include "Manipulators.h"
#include "RenderPrimitives.h"


namespace Rendering {

	// Tolerance constants for intersection tests
	static constexpr float TRANSLATION_TOLERANCE_SCALE = 0.4f;       // Tolerance for translation arrow cylinders
	static constexpr float TRANSLATION_HEAD_RADIUS_SCALE = 2.0f;     // Scale factor for arrow head sphere intersection
	static constexpr float ROTATION_TOLERANCE_SCALE = 0.3f;          // Tolerance for rotation rings
	static constexpr float SCALE_TOLERANCE_SCALE = 0.25f;            // Tolerance for scale manipulator cylinders
	static constexpr float SCALE_CUBE_SIZE_SCALE = 1.5f;             // Scale factor for enlarged cube intersection areas
	static constexpr float SCALE_CENTER_CUBE_SIZE_SCALE = 1.5f;      // Scale factor for enlarged center cube intersection area

	Manipulators::Manipulators() {
		state_.position = Vector3f::zero();
		state_.rotation = Vector3f::zero();
		state_.scale = Vector3f::one();
		state_.currentType = ManipulatorType::Translation;
		state_.size = 1.0f;
		state_.visible = true;
		state_.hoveredAxis = ManipulatorAxis::None;
		state_.selectedAxis = ManipulatorAxis::None;
	}

	RenderPrimitives* Manipulators::GetRenderPrimitives() const {
		return Rendering::RenderingSystem::GetRenderPrimitives();
	}

	bool Manipulators::IsInitialized() const {
		return RenderingSystem::IsInitialized() && GetRenderPrimitives() != nullptr;
	}

	// State management methods
	void Manipulators::SetPosition(const Vector3f& position) {
		state_.position = position;
	}

	void Manipulators::SetRotation(const Vector3f& rotation) {
		state_.rotation = rotation;
	}

	void Manipulators::SetScale(const Vector3f& scale) {
		state_.scale = scale;
	}

	void Manipulators::SetManipulatorType(ManipulatorType type) {
		state_.currentType = type;
	}

	void Manipulators::SetSize(float size) {
		state_.size = size;
	}

	void Manipulators::SetVisible(bool visible) {
		state_.visible = visible;
	}

	void Manipulators::SetHoveredAxis(ManipulatorAxis axis) {
		state_.hoveredAxis = axis;
	}

	void Manipulators::SetSelectedAxis(ManipulatorAxis axis) {
		state_.selectedAxis = axis;
	}

	// Getters
	const Vector3f& Manipulators::GetPosition() const {
		return state_.position;
	}

	const Vector3f& Manipulators::GetRotation() const {
		return state_.rotation;
	}

	const Vector3f& Manipulators::GetScale() const {
		return state_.scale;
	}

	Manipulators::ManipulatorType Manipulators::GetManipulatorType() const {
		return state_.currentType;
	}

	float Manipulators::GetSize() const {
		return state_.size;
	}

	bool Manipulators::IsVisible() const {
		return state_.visible;
	}

	Manipulators::ManipulatorAxis Manipulators::GetHoveredAxis() const {
		return state_.hoveredAxis;
	}

	Manipulators::ManipulatorAxis Manipulators::GetSelectedAxis() const {
		return state_.selectedAxis;
	}

	// Color methods
	Vector3f Manipulators::GetXAxisColor(bool isHovered, bool isSelected) const {
		if (isSelected) return Vector3f(1.0f, 0.8f, 0.8f);  // Light red
		if (isHovered) return Vector3f(1.0f, 0.6f, 0.6f);   // Medium red
		return Vector3f(0.8f, 0.2f, 0.2f);                  // Dark red
	}

	Vector3f Manipulators::GetYAxisColor(bool isHovered, bool isSelected) const {
		if (isSelected) return Vector3f(0.8f, 1.0f, 0.8f);  // Light green
		if (isHovered) return Vector3f(0.6f, 1.0f, 0.6f);   // Medium green
		return Vector3f(0.2f, 0.8f, 0.2f);                  // Dark green
	}

	Vector3f Manipulators::GetZAxisColor(bool isHovered, bool isSelected) const {
		if (isSelected) return Vector3f(0.8f, 0.8f, 1.0f);  // Light blue
		if (isHovered) return Vector3f(0.6f, 0.6f, 1.0f);   // Medium blue
		return Vector3f(0.2f, 0.2f, 0.8f);                  // Dark blue
	}

	Vector3f Manipulators::GetAxisColor(ManipulatorAxis axis, bool isHovered, bool isSelected) const {
		switch (axis) {
		case ManipulatorAxis::X:
			return GetXAxisColor(isHovered, isSelected);
		case ManipulatorAxis::Y:
			return GetYAxisColor(isHovered, isSelected);
		case ManipulatorAxis::Z:
			return GetZAxisColor(isHovered, isSelected);
		default:
			return Vector3f(0.7f, 0.7f, 0.7f);  // Gray for combined/other axes
		}
	}

	Vector3f Manipulators::GetPlaneColor(ManipulatorAxis plane, bool isHovered, bool isSelected) const {
		Vector3f color;
		switch (plane) {
		case ManipulatorAxis::XY:
			color = Vector3f(0.7f, 0.7f, 0.2f);  // Yellow for XY plane
			break;
		case ManipulatorAxis::XZ:
			color = Vector3f(0.7f, 0.2f, 0.7f);  // Magenta for XZ plane
			break;
		case ManipulatorAxis::YZ:
			color = Vector3f(0.2f, 0.7f, 0.7f);  // Cyan for YZ plane
			break;
		case ManipulatorAxis::Screen:
			color = Vector3f(0.9f, 0.9f, 0.9f);  // White for screen space
			break;
		default:
			color = Vector3f(0.5f, 0.5f, 0.5f);  // Gray
			break;
		}

		if (isSelected) return color * 1.2f;
		if (isHovered) return color * 1.1f;
		return color;
	}

	void Manipulators::RenderTranslationArrow(const Vector3f& direction, const Vector3f& color, float scale) const {
		RenderPrimitives* renderPrimitives = GetRenderPrimitives();
		if (!renderPrimitives) return;

		Vector3f scaledDirection = direction * (ARROW_LENGTH * scale);
		Vector3f arrowHeadPos = scaledDirection * 0.65f;  // Arrow head starts at 85% of length

		// Render arrow shaft (cylinder)
		Vector3f shaftEnd = scaledDirection * 0.8f;
		Vector3f shaftCenter = shaftEnd * 0.5f;
		float shaftLength = shaftEnd.magnitude();

		// Simple rotation calculation for cylinders based on direction
		Vector3f shaftRotation = Vector3f::zero();
		if (std::abs(direction.getX()) > 0.9f) {
			// X-axis arrow: rotate 90 degrees around Z
			shaftRotation = Vector3f(0.0f, 0.0f, -90.0f);
		}
		else if (std::abs(direction.getZ()) > 0.9f) {
			// Z-axis arrow: rotate 90 degrees around X
			shaftRotation = Vector3f(90.0f, 0.0f, 0.0f);
		}
		// Y-axis arrow needs no rotation (cylinder default is Y-aligned)

		renderPrimitives->RenderCylinder(
			shaftCenter,
			ARROW_SHAFT_RADIUS * scale,
			shaftLength,
			color,
			shaftRotation
		);

		// Render arrow head (cone)
		Vector3f arrowHeadCenter = arrowHeadPos + scaledDirection * 0.075f; // Half of head length
		renderPrimitives->RenderCone(
			arrowHeadCenter,
			ARROW_HEAD_RADIUS * scale,
			ARROW_HEAD_LENGTH * scale,
			color,
			shaftRotation
		);
	}

	void Manipulators::RenderTranslationPlane(const Vector3f& normal, const Vector3f& tangent1,
		const Vector3f& tangent2, const Vector3f& color, float scale) const {
		RenderPrimitives* renderPrimitives = GetRenderPrimitives();
		if (!renderPrimitives) return;

		Vector3f planeCenter = (tangent1 + tangent2) * (PLANE_OFFSET * scale);
		Vector3f planeSize = Vector3f(PLANE_SIZE * scale, PLANE_SIZE * scale, 0.01f * scale);

		// Calculate rotation to align plane with the normal
		Vector3f rotation = Vector3f::zero();
		
		if (std::abs(normal.getZ()) > 0.9f) {
			// XY plane - no rotation needed (box default orientation)
		}
		else if (std::abs(normal.getY()) > 0.9f) {
			// XZ plane - rotate 90 degrees around X
			rotation = Vector3f(90.0f, 0.0f, 0.0f);
		}
		else if (std::abs(normal.getX()) > 0.9f) {
			// YZ plane - rotate 90 degrees around Y
			rotation = Vector3f(0.0f, 90.0f, 0.0f);
		}

		renderPrimitives->RenderBox(planeCenter, planeSize, color, rotation);
	}

	void Manipulators::RenderTranslationManipulator() const {
		RenderPrimitives* renderPrimitives = GetRenderPrimitives();
		if (!renderPrimitives) return;

		float scale = state_.size;

		// X axis (red arrow)
		bool xHovered = state_.hoveredAxis == ManipulatorAxis::X;
		bool xSelected = state_.selectedAxis == ManipulatorAxis::X;
		RenderTranslationArrow(Vector3f(1, 0, 0), GetXAxisColor(xHovered, xSelected), scale);

		// Y axis (green arrow)  
		bool yHovered = state_.hoveredAxis == ManipulatorAxis::Y;
		bool ySelected = state_.selectedAxis == ManipulatorAxis::Y;
		RenderTranslationArrow(Vector3f(0, 1, 0), GetYAxisColor(yHovered, ySelected), scale);

		// Z axis (blue arrow)
		bool zHovered = state_.hoveredAxis == ManipulatorAxis::Z;
		bool zSelected = state_.selectedAxis == ManipulatorAxis::Z;
		RenderTranslationArrow(Vector3f(0, 0, 1), GetZAxisColor(zHovered, zSelected), scale);

		// Center cube for screen space movement
		bool screenHovered = state_.hoveredAxis == ManipulatorAxis::Screen;
		bool screenSelected = state_.selectedAxis == ManipulatorAxis::Screen;
		Vector3f centerSize = Vector3f(0.1f * scale, 0.1f * scale, 0.1f * scale);
		renderPrimitives->RenderBox(Vector3f::zero(), centerSize,
			GetPlaneColor(ManipulatorAxis::Screen, screenHovered, screenSelected));
	}

	// Rotation manipulator rendering
	void Manipulators::RenderRotationRing(const Vector3f& normal, const Vector3f& color, float scale) const {
		RenderPrimitives* renderPrimitives = GetRenderPrimitives();
		if (!renderPrimitives) return;

		float radius = ROTATION_RING_RADIUS * scale;

		// Use the new RenderCircleOutline primitive instead of manual line rendering
		renderPrimitives->RenderCircleOutline(
			Vector3f::zero(),           // Center at origin (transformations are handled by matrix stack)
			radius,                     // Scaled radius
			normal,                     // Normal vector defining the plane
			color,                      // Ring color
			3.0f,                       // Line width (same as original)
			ROTATION_RING_SEGMENTS      // Number of segments
		);
	}

	void Manipulators::RenderRotationManipulator() const {
		RenderPrimitives* renderPrimitives = GetRenderPrimitives();
		if (!renderPrimitives) return;

		float scale = state_.size;

		// X axis rotation ring (red)
		bool xHovered = state_.hoveredAxis == ManipulatorAxis::X;
		bool xSelected = state_.selectedAxis == ManipulatorAxis::X;
		RenderRotationRing(Vector3f(1, 0, 0), GetXAxisColor(xHovered, xSelected), scale);

		// Y axis rotation ring (green)  
		bool yHovered = state_.hoveredAxis == ManipulatorAxis::Y;
		bool ySelected = state_.selectedAxis == ManipulatorAxis::Y;
		RenderRotationRing(Vector3f(0, 1, 0), GetYAxisColor(yHovered, ySelected), scale);

		// Z axis rotation ring (blue)
		bool zHovered = state_.hoveredAxis == ManipulatorAxis::Z;
		bool zSelected = state_.selectedAxis == ManipulatorAxis::Z;
		RenderRotationRing(Vector3f(0, 0, 1), GetZAxisColor(zHovered, zSelected), scale);
	}

	// Scale manipulator rendering
	void Manipulators::RenderScaleAxis(const Vector3f& direction, const Vector3f& color, float scale) const {
		RenderPrimitives* renderPrimitives = GetRenderPrimitives();
		if (!renderPrimitives) return;

		Vector3f scaledDirection = direction * (SCALE_LINE_LENGTH * scale);

		// Fixed position for the cube - should not change with AutoScale
		Vector3f cubePos = scaledDirection;  // Remove scale factor from position
		
		// Line end position also at fixed distance
		Vector3f lineEnd = scaledDirection;  // Remove scale factor from position
		
		// But apply scale to visual elements (size/thickness)
		Vector3f cubeSize = Vector3f(SCALE_CUBE_SIZE * scale, SCALE_CUBE_SIZE * scale, SCALE_CUBE_SIZE * scale);

		// Render line from origin to fixed position
		renderPrimitives->RenderLine(Vector3f::zero(), lineEnd, color, 2.0f);  // Scale line thickness

		// Render cube handle at the fixed position, but scale its size
		renderPrimitives->RenderBox(cubePos, cubeSize, color);
	}

	void Manipulators::RenderScaleManipulator() const {
		RenderPrimitives* renderPrimitives = GetRenderPrimitives();
		if (!renderPrimitives) return;

		float scale = state_.size;

		// X axis (red)
		bool xHovered = state_.hoveredAxis == ManipulatorAxis::X;
		bool xSelected = state_.selectedAxis == ManipulatorAxis::X;
		RenderScaleAxis(Vector3f(1, 0, 0), GetXAxisColor(xHovered, xSelected), scale);

		// Y axis (green)
		bool yHovered = state_.hoveredAxis == ManipulatorAxis::Y;
		bool ySelected = state_.selectedAxis == ManipulatorAxis::Y;
		RenderScaleAxis(Vector3f(0, 1, 0), GetYAxisColor(yHovered, ySelected), scale);

		// Z axis (blue)
		bool zHovered = state_.hoveredAxis == ManipulatorAxis::Z;
		bool zSelected = state_.selectedAxis == ManipulatorAxis::Z;
		RenderScaleAxis(Vector3f(0, 0, 1), GetZAxisColor(zHovered, zSelected), scale);

		// Center cube for uniform scaling
		bool centerHovered = state_.hoveredAxis == ManipulatorAxis::XYZ;
		bool centerSelected = state_.selectedAxis == ManipulatorAxis::XYZ;
		Vector3f centerSize = Vector3f(SCALE_CENTER_CUBE_SIZE * scale, SCALE_CENTER_CUBE_SIZE * scale, SCALE_CENTER_CUBE_SIZE * scale);
		Vector3f centerColor = centerSelected ? Vector3f(1.0f, 1.0f, 0.8f) :
			(centerHovered ? Vector3f(0.9f, 0.9f, 0.7f) : Vector3f(0.8f, 0.8f, 0.6f));
		renderPrimitives->RenderBox(Vector3f::zero(), centerSize, centerColor);
	}

	// Main rendering methods
	void Manipulators::Render() const 
	{		
		RenderPrimitives* renderPrimitives = GetRenderPrimitives();
		if (!renderPrimitives || !state_.visible) {
			return;
		}

		// Store current depth range and set manipulator to render in near range
		GLfloat originalDepthRange[2];
		glGetFloatv(GL_DEPTH_RANGE, originalDepthRange);
		glDepthRange(0.0f, 0.01f);  // Render manipulator in very near depth range

		// Apply transformation by using the RenderPrimitives matrix stack
		renderPrimitives->PushMatrix();

		// Setup the render Matrix but don't apply user-defined scale to manipulator rendering
		renderPrimitives->SetTransform(state_.position, state_.rotation, Vector3f::one());

		switch (state_.currentType) {
		case ManipulatorType::Translation:
			RenderTranslationManipulator();
			break;
		case ManipulatorType::Rotation:
			RenderRotationManipulator();
			break;
		case ManipulatorType::Scale:
			RenderScaleManipulator();
			break;
		}

		// Restore matrix and depth range
		renderPrimitives->PopMatrix();
		glDepthRange(originalDepthRange[0], originalDepthRange[1]);
	}

	void Manipulators::Render(const Vector3f& position) const {
		Vector3f originalPos = state_.position;
		const_cast<Manipulators*>(this)->state_.position = position;
		Render();
		const_cast<Manipulators*>(this)->state_.position = originalPos;
	}

	void Manipulators::Render(const Vector3f& position, const Vector3f& rotation, const Vector3f& scale) const {
		ManipulatorState originalState = state_;
		const_cast<Manipulators*>(this)->state_.position = position;
		const_cast<Manipulators*>(this)->state_.rotation = rotation;
		const_cast<Manipulators*>(this)->state_.scale = scale;
		Render();
		const_cast<Manipulators*>(this)->state_ = originalState;
	}

	// Utility methods
	void Manipulators::CycleManipulatorType() {
		switch (state_.currentType) {
		case ManipulatorType::Translation:
			state_.currentType = ManipulatorType::Rotation;
			break;
		case ManipulatorType::Rotation:
			state_.currentType = ManipulatorType::Scale;
			break;
		case ManipulatorType::Scale:
			state_.currentType = ManipulatorType::Translation;
			break;
		}

		// Reset selection when changing types
		state_.selectedAxis = ManipulatorAxis::None;
		state_.hoveredAxis = ManipulatorAxis::None;
	}

	void Manipulators::Reset() {
		state_.position = Vector3f::zero();
		state_.rotation = Vector3f::zero();
		state_.scale = Vector3f::one();
		state_.currentType = ManipulatorType::Translation;
		state_.size = 1.0f;
		state_.visible = true;
		state_.hoveredAxis = ManipulatorAxis::None;
		state_.selectedAxis = ManipulatorAxis::None;
	}

	// Static utility methods
	Manipulators::ManipulatorAxis Manipulators::CombineAxes(ManipulatorAxis axis1, ManipulatorAxis axis2) {
		return static_cast<ManipulatorAxis>(static_cast<int>(axis1) | static_cast<int>(axis2));
	}

	bool Manipulators::HasAxis(ManipulatorAxis combined, ManipulatorAxis single) {
		return (static_cast<int>(combined) & static_cast<int>(single)) != 0;
	}

	std::string Manipulators::AxisToString(ManipulatorAxis axis) {
		switch (axis) {
		case ManipulatorAxis::None: return "None";
		case ManipulatorAxis::X: return "X";
		case ManipulatorAxis::Y: return "Y";
		case ManipulatorAxis::Z: return "Z";
		case ManipulatorAxis::XY: return "XY";
		case ManipulatorAxis::XZ: return "XZ";
		case ManipulatorAxis::YZ: return "YZ";
		case ManipulatorAxis::XYZ: return "XYZ";
		case ManipulatorAxis::Screen: return "Screen";
		default: return "Unknown";
		}
	}

	Manipulators::ManipulatorAxis Manipulators::TestRayIntersection(const Vector3f& rayOrigin, const Vector3f& rayDirection, float& distance) const {
		if (!state_.visible) {
			distance = std::numeric_limits<float>::max();
			return ManipulatorAxis::None;
		}

		// Create the complete transformation matrix for the manipulator (T * R * S)
		Matrix4f transform;
		transform.identity();

		// Apply rotations if any
		if (state_.rotation.getX() != 0.0f || state_.rotation.getY() != 0.0f || state_.rotation.getZ() != 0.0f) {
			// Convert rotation from degrees to radians
			float xRad = state_.rotation.getX() * (3.14159265f / 180.0f);
			float yRad = state_.rotation.getY() * (3.14159265f / 180.0f);
			float zRad = state_.rotation.getZ() * (3.14159265f / 180.0f);

			// Apply rotations in Z * Y * X order (standard rotation order)
			if (zRad != 0.0f) transform = Matrix4f::rotationZ(zRad) * transform;
			if (yRad != 0.0f) transform = Matrix4f::rotationY(yRad) * transform;
			if (xRad != 0.0f) transform = Matrix4f::rotationX(xRad) * transform;
		}

		transform = Matrix4f::translation(state_.position.getX(), state_.position.getY(), state_.position.getZ()) * transform;
		// Note we do not apply the user-defined scale to the intersection test
		// Get the inverse transformation matrix
		Matrix4f inverseTransform = transform.inverse();

		// Transform ray to manipulator's local space using the complete inverse transformation
		Vector3f localRayOrigin = inverseTransform.transformPoint(rayOrigin);
		Vector3f localRayDirection = inverseTransform.transformVector(rayDirection).normalized();

		ManipulatorAxis closestAxis = ManipulatorAxis::None;
		float closestDistance = std::numeric_limits<float>::max();

		switch (state_.currentType) {
		case ManipulatorType::Translation:
			closestAxis = TestTranslationIntersection(localRayOrigin, localRayDirection, closestDistance);
			break;
		case ManipulatorType::Rotation:
			closestAxis = TestRotationIntersection(localRayOrigin, localRayDirection, closestDistance);
			break;
		case ManipulatorType::Scale:
			closestAxis = TestScaleIntersection(localRayOrigin, localRayDirection, closestDistance);
			break;
		}

		distance = closestDistance;
		return closestAxis;
	}

	Manipulators::ManipulatorAxis Manipulators::TestTranslationIntersection(const Vector3f& rayOrigin, const Vector3f& rayDirection, float& distance) const {
		ManipulatorAxis closestAxis = ManipulatorAxis::None;
		float closestDistance = std::numeric_limits<float>::max();
		float currentDistance;

		float scale = state_.size;
		float arrowLength = ARROW_LENGTH * scale;
		float arrowHeadRadius = ARROW_HEAD_RADIUS * scale;
		float tolerance = TRANSLATION_TOLERANCE_SCALE * scale;

		// Test X axis arrow (cylinder along X axis)
		if (TestCylinderIntersection(rayOrigin, rayDirection, Vector3f::zero(), Vector3f(arrowLength, 0, 0), tolerance, currentDistance)) {
			if (currentDistance < closestDistance) {
				closestDistance = currentDistance;
				closestAxis = ManipulatorAxis::X;
			}
		}

		// Test Y axis arrow (cylinder along Y axis)
		if (TestCylinderIntersection(rayOrigin, rayDirection, Vector3f::zero(), Vector3f(0, arrowLength, 0), tolerance, currentDistance)) {
			if (currentDistance < closestDistance) {
				closestDistance = currentDistance;
				closestAxis = ManipulatorAxis::Y;
			}
		}

		// Test Z axis arrow (cylinder along Z axis)
		if (TestCylinderIntersection(rayOrigin, rayDirection, Vector3f::zero(), Vector3f(0, 0, arrowLength), tolerance, currentDistance)) {
			if (currentDistance < closestDistance) {
				closestDistance = currentDistance;
				closestAxis = ManipulatorAxis::Z;
			}
		}

		// Test arrow heads (spheres at the tips) - increased radius for easier picking
		Vector3f xTip = Vector3f(arrowLength, 0, 0);
		Vector3f yTip = Vector3f(0, arrowLength, 0);
		Vector3f zTip = Vector3f(0, 0, arrowLength);

		float enlargedHeadRadius = arrowHeadRadius * TRANSLATION_HEAD_RADIUS_SCALE;

		if (TestSphereIntersection(rayOrigin, rayDirection, xTip, enlargedHeadRadius, currentDistance)) {
			if (currentDistance < closestDistance) {
				closestDistance = currentDistance;
				closestAxis = ManipulatorAxis::X;
			}
		}

		if (TestSphereIntersection(rayOrigin, rayDirection, yTip, enlargedHeadRadius, currentDistance)) {
			if (currentDistance < closestDistance) {
				closestDistance = currentDistance;
				closestAxis = ManipulatorAxis::Y;
			}
		}

		if (TestSphereIntersection(rayOrigin, rayDirection, zTip, enlargedHeadRadius, currentDistance)) {
			if (currentDistance < closestDistance) {
				closestDistance = currentDistance;
				closestAxis = ManipulatorAxis::Z;
			}
		}

		distance = closestDistance;
		return closestAxis;
	}

	Manipulators::ManipulatorAxis Manipulators::TestRotationIntersection(const Vector3f& rayOrigin, const Vector3f& rayDirection, float& distance) const {
		ManipulatorAxis closestAxis = ManipulatorAxis::None;
		float closestDistance = std::numeric_limits<float>::max();
		float currentDistance;

		float scale = state_.size;
		float ringRadius = ROTATION_RING_RADIUS * scale;
		float tolerance = ROTATION_TOLERANCE_SCALE * scale;

		// Test X rotation ring (YZ plane)
		if (TestRingIntersection(rayOrigin, rayDirection, Vector3f::zero(), Vector3f(1, 0, 0), ringRadius, tolerance, currentDistance)) {
			if (currentDistance < closestDistance) {
				closestDistance = currentDistance;
				closestAxis = ManipulatorAxis::X;
			}
		}

		// Test Y rotation ring (XZ plane)
		if (TestRingIntersection(rayOrigin, rayDirection, Vector3f::zero(), Vector3f(0, 1, 0), ringRadius, tolerance, currentDistance)) {
			if (currentDistance < closestDistance) {
				closestDistance = currentDistance;
				closestAxis = ManipulatorAxis::Y;
			}
		}

		// Test Z rotation ring (XY plane)
		if (TestRingIntersection(rayOrigin, rayDirection, Vector3f::zero(), Vector3f(0, 0, 1), ringRadius, tolerance, currentDistance)) {
			if (currentDistance < closestDistance) {
				closestDistance = currentDistance;
				closestAxis = ManipulatorAxis::Z;
			}
		}

		distance = closestDistance;
		return closestAxis;
	}

	Manipulators::ManipulatorAxis Manipulators::TestScaleIntersection(const Vector3f& rayOrigin, const Vector3f& rayDirection, float& distance) const {
		ManipulatorAxis closestAxis = ManipulatorAxis::None;
		float closestDistance = std::numeric_limits<float>::max();
		float currentDistance;

		float scale = state_.size;
		// Use fixed line length for positions, but scale the visual elements
		float lineLength = SCALE_LINE_LENGTH * scale;  // Remove scale factor from position calculation
		float cubeSize = SCALE_CUBE_SIZE * scale;
		float centerCubeSize = SCALE_CENTER_CUBE_SIZE * scale;
		float tolerance = SCALE_TOLERANCE_SCALE * scale;



		// Enlarged cube size for easier picking
		Vector3f enlargedCubeSize = Vector3f(cubeSize * SCALE_CUBE_SIZE_SCALE, 
											 cubeSize * SCALE_CUBE_SIZE_SCALE, 
											 cubeSize * SCALE_CUBE_SIZE_SCALE);

		// Test X axis line and cube at fixed positions
		Vector3f xCubePos = Vector3f(lineLength, 0, 0);  // Fixed position
		if (TestCylinderIntersection(rayOrigin, rayDirection, Vector3f::zero(), xCubePos, tolerance, currentDistance) ||
			TestBoxIntersection(rayOrigin, rayDirection, xCubePos, enlargedCubeSize, currentDistance)) {
			if (currentDistance < closestDistance) {
				closestDistance = currentDistance;
				closestAxis = ManipulatorAxis::X;
			}
		}

		// Test Y axis line and cube at fixed positions
		Vector3f yCubePos = Vector3f(0, lineLength, 0);  // Fixed position
		if (TestCylinderIntersection(rayOrigin, rayDirection, Vector3f::zero(), yCubePos, tolerance, currentDistance) ||
			TestBoxIntersection(rayOrigin, rayDirection, yCubePos, enlargedCubeSize, currentDistance)) {
			if (currentDistance < closestDistance) {
				closestDistance = currentDistance;
				closestAxis = ManipulatorAxis::Y;
			}
		}

		// Test Z axis line and cube at fixed positions
		Vector3f zCubePos = Vector3f(0, 0, lineLength);  // Fixed position
		if (TestCylinderIntersection(rayOrigin, rayDirection, Vector3f::zero(), zCubePos, tolerance, currentDistance) ||
			TestBoxIntersection(rayOrigin, rayDirection, zCubePos, enlargedCubeSize, currentDistance)) {
			if (currentDistance < closestDistance) {
				closestDistance = currentDistance;
				closestAxis = ManipulatorAxis::Z;
			}
		}

		// Test center cube for uniform scaling - enlarged for easier picking
		Vector3f enlargedCenterSize = Vector3f(centerCubeSize * SCALE_CENTER_CUBE_SIZE_SCALE,
			centerCubeSize * SCALE_CENTER_CUBE_SIZE_SCALE,
			centerCubeSize * SCALE_CENTER_CUBE_SIZE_SCALE);
		if (TestBoxIntersection(rayOrigin, rayDirection, Vector3f::zero(), enlargedCenterSize, currentDistance)) {
			closestDistance = currentDistance;
			closestAxis = ManipulatorAxis::XYZ;
		}

		distance = closestDistance;
		return closestAxis;
	}

	bool Manipulators::TestSphereIntersection(const Vector3f& rayOrigin, const Vector3f& rayDirection, const Vector3f& sphereCenter, float radius, float& distance) const {
		Vector3f oc = rayOrigin - sphereCenter;
		float a = rayDirection.dot(rayDirection);
		float b = 2.0f * oc.dot(rayDirection);
		float c = oc.dot(oc) - radius * radius;
		float discriminant = b * b - 4 * a * c;

		if (discriminant < 0) {
			return false;
		}

		float t1 = (-b - std::sqrt(discriminant)) / (2.0f * a);
		float t2 = (-b + std::sqrt(discriminant)) / (2.0f * a);

		// Use the closest positive intersection
		if (t1 > 0) {
			distance = t1;
			return true;
		}
		else if (t2 > 0) {
			distance = t2;
			return true;
		}

		return false;
	}

	bool Manipulators::TestCylinderIntersection(const Vector3f& rayOrigin, const Vector3f& rayDirection, const Vector3f& cylinderStart, const Vector3f& cylinderEnd, float radius, float& distance) const {
		Vector3f cylinderAxis = cylinderEnd - cylinderStart;
		float cylinderLength = cylinderAxis.magnitude();

		if (cylinderLength < 0.001f) {
			return false;
		}

		Vector3f cylinderDir = cylinderAxis * (1.0f / cylinderLength);
		Vector3f oc = rayOrigin - cylinderStart;

		// Project ray onto cylinder axis
		float rayDotAxis = rayDirection.dot(cylinderDir);
		float ocDotAxis = oc.dot(cylinderDir);

		// Calculate perpendicular components
		Vector3f rayPerp = rayDirection - cylinderDir * rayDotAxis;
		Vector3f ocPerp = oc - cylinderDir * ocDotAxis;

		// Solve quadratic equation for cylinder intersection
		float a = rayPerp.dot(rayPerp);
		float b = 2.0f * ocPerp.dot(rayPerp);
		float c = ocPerp.dot(ocPerp) - radius * radius;

		float discriminant = b * b - 4 * a * c;
		if (discriminant < 0) {
			return false;
		}

		float t1 = (-b - std::sqrt(discriminant)) / (2.0f * a);
		float t2 = (-b + std::sqrt(discriminant)) / (2.0f * a);

		// Check if intersection points are within cylinder bounds
		for (float t : {t1, t2}) {
			if (t > 0) {
				float axisT = ocDotAxis + t * rayDotAxis;
				if (axisT >= 0 && axisT <= cylinderLength) {
					distance = t;
					return true;
				}
			}
		}

		return false;
	}

	bool Manipulators::TestBoxIntersection(const Vector3f& rayOrigin, const Vector3f& rayDirection, const Vector3f& boxCenter, const Vector3f& boxSize, float& distance) const {
		Vector3f boxMin = boxCenter - boxSize * 0.5f;
		Vector3f boxMax = boxCenter + boxSize * 0.5f;

		float tmin = 0;
		float tmax = std::numeric_limits<float>::max();

		// Test intersection with each axis-aligned slab
		for (int i = 0; i < 3; ++i) {
			float rayCompexpr = (i == 0) ? rayDirection.getX() : (i == 1) ? rayDirection.getY() : rayDirection.getZ();
			float originCompexpr = (i == 0) ? rayOrigin.getX() : (i == 1) ? rayOrigin.getY() : rayOrigin.getZ();
			float minCompexpr = (i == 0) ? boxMin.getX() : (i == 1) ? boxMin.getY() : boxMin.getZ();
			float maxCompexpr = (i == 0) ? boxMax.getX() : (i == 1) ? boxMax.getY() : boxMax.getZ();

			if (std::abs(rayCompexpr) < 0.001f) {
				// Ray is parallel to slab
				if (originCompexpr < minCompexpr || originCompexpr > maxCompexpr) {
					return false;
				}
			}
			else {
				float t1 = (minCompexpr - originCompexpr) / rayCompexpr;
				float t2 = (maxCompexpr - originCompexpr) / rayCompexpr;

				if (t1 > t2) {
					std::swap(t1, t2);
				}

				tmin = std::max(tmin, t1);
				tmax = std::min(tmax, t2);

				if (tmin > tmax) {
					return false;
				}
			}
		}

		if (tmin > 0) {
			distance = tmin;
			return true;
		}
		else if (tmax > 0) {
			distance = tmax;
			return true;
		}

		return false;
	}

	bool Manipulators::TestRingIntersection(const Vector3f& rayOrigin, const Vector3f& rayDirection, const Vector3f& ringCenter, const Vector3f& ringNormal, float radius, float tolerance, float& distance) const {
		// First, find intersection with the plane containing the ring
		float denom = rayDirection.dot(ringNormal);
		if (std::abs(denom) < 0.001f) {
			return false; // Ray is parallel to plane
		}

		Vector3f toCenter = ringCenter - rayOrigin;
		float t = toCenter.dot(ringNormal) / denom;
		if (t < 0) {
			return false; // Intersection behind ray origin
		}

		// Calculate intersection point on the plane
		Vector3f intersectionPoint = rayOrigin + rayDirection * t;
		Vector3f toIntersection = intersectionPoint - ringCenter;

		// Check if intersection is within ring tolerance
		float distanceFromCenter = toIntersection.magnitude();
		if (distanceFromCenter >= radius - tolerance && distanceFromCenter <= radius + tolerance) {
			distance = t;
			return true;
		}

		return false;
	}


} // namespace Rendering