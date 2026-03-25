#pragma once

#include "ComponentSystem/ComponentSystem.h"
#include "Manipulators.h"
#include "math/Vector3f.h"
#include "math/Matrix4f.h"
#include "CoreSystem/CoreSystem.h"
#include "Input/MouseInputHandler.h"
#include "Input/MouseManager.h"
#include <memory>
#include <functional>

// Forward declaration
class Viewport;

namespace Rendering {

	class ManipulatorComponent : public ComponentSystem::Component, public Input::MouseInputHandler {
	public:
		// Transform change callback type - called when manipulation occurs
		using TransformChangeCallback = std::function<void(const Vector3f& position, const Vector3f& rotation, const Vector3f& scale)>;

		// Interaction state
		struct InteractionState {
			bool isDragging = false;
			bool isHovering = false;
			Rendering::Manipulators::ManipulatorAxis dragAxis = Rendering::Manipulators::ManipulatorAxis::None;
			Vector3f dragStartPosition = Vector3f::zero();
			Vector3f dragStartMousePos = Vector3f::zero();
			Vector3f initialTransform = Vector3f::zero();
			double lastMouseX = 0.0;
			double lastMouseY = 0.0;
		};

		// Settings for manipulator behavior
		struct ManipulatorSettings {
			float mouseSensitivity = 0.001f;
			float snapThreshold = 0.1f;
			bool enableSnapping = false;
			bool enableKeyboardShortcuts = true;
			float manipulatorScale = 1.0f;
			bool autoScale = true;  // Scale manipulator based on distance to camera
			float autoScaleFactor = 0.1f;
		};

	private:
		std::unique_ptr<Rendering::Manipulators> manipulator_;
		InteractionState interactionState_;
		ManipulatorSettings settings_;
		TransformChangeCallback transformChangeCallback_;

		// Transform data
		Vector3f targetPosition_;
		Vector3f targetRotation_;
		Vector3f targetScale_;

		// Camera reference for auto-scaling and ray casting
		Vector3f cameraPosition_;
		Matrix4f viewMatrix_;
		Matrix4f projectionMatrix_;
		int viewportWidth_;
		int viewportHeight_;

		Input::MouseManager* m_mouseManager = nullptr;
		std::shared_ptr<ManipulatorComponent> m_selfReference; // For registration with MouseManager

		// Input methods - now viewport-aware
		Vector3f ScreenToWorldRay(double mouseX, double mouseY, const Viewport* viewport = nullptr) const;
		float CalculateAutoScale() const;

		// Manipulation calculations
		Vector3f CalculateTranslationDelta(double mouseX, double mouseY, Rendering::Manipulators::ManipulatorAxis axis) const;
		Vector3f CalculateRotationDelta(double mouseX, double mouseY, Rendering::Manipulators::ManipulatorAxis axis) const;
		Vector3f CalculateScaleDelta(double mouseX, double mouseY, Rendering::Manipulators::ManipulatorAxis axis) const;

		// Constraint helpers
		Vector3f ApplySnapping(const Vector3f& value) const;
		void RegisterWithMouseManager();
		void UnregisterFromMouseManager();

	protected:
		bool OnInitialize() override;
		void OnUpdate(double deltaTime) override;
		void OnShutdown() override;

	public:
		ManipulatorComponent();
		~ManipulatorComponent() = default;

		// Transform management
		void SetTargetTransform(const Vector3f& position, const Vector3f& rotation = Vector3f::zero(), const Vector3f& scale = Vector3f::one());
		void SetTargetPosition(const Vector3f& position);
		void SetTargetRotation(const Vector3f& rotation);
		void SetTargetScale(const Vector3f& scale);
		void UpdateManipulatorScale();

		const Vector3f& GetTargetPosition() const { return targetPosition_; }
		const Vector3f& GetTargetRotation() const { return targetRotation_; }
		const Vector3f& GetTargetScale() const { return targetScale_; }

		// Manipulator control
		void SetManipulatorType(Rendering::Manipulators::ManipulatorType type);
		Rendering::Manipulators::ManipulatorType GetManipulatorType() const;
		void CycleManipulatorType();

		void SetVisible(bool visible);
		bool IsVisible() const;

		// Camera and viewport setup (required for proper interaction)
		void SetCameraInfo(const Vector3f& cameraPosition, const Matrix4f& viewMatrix, const Matrix4f& projMatrix);
		void SetViewportSize(int width, int height);

		// Update camera information from viewport - should be called each frame
		void UpdateCameraInfoFromViewport(Viewport* viewport);

		// Input handling - now supports viewport-aware ray casting		
		bool HandleButtonInput(Input::MouseButton button, bool pressed, bool justPressed, bool justReleased, double x, double y) override;
		bool HandleMovementInput(double x, double y, float deltaX, float deltaY) override;
		bool HandleScrollInput(float deltaX, float deltaY) override;
			
		// Settings
		void SetSettings(const ManipulatorSettings& settings) { settings_ = settings; }
		const ManipulatorSettings& GetSettings() const { return settings_; }
		ManipulatorSettings& GetSettings() { return settings_; }

		// Callback management
		void SetTransformChangeCallback(const TransformChangeCallback& callback) { transformChangeCallback_ = callback; }

		// Interaction state
		bool IsDragging() const { return interactionState_.isDragging; }
		bool IsHovering() const { return interactionState_.isHovering; }
		Rendering::Manipulators::ManipulatorAxis GetActiveAxis() const { return interactionState_.dragAxis; }

		// Rendering
		void Render() const;

		// Utility methods
		void Reset();
		bool TestRayIntersection(const Vector3f& rayOrigin, const Vector3f& rayDirection, float& distance) const;

		// Input priority - should be higher than selector to handle manipulator interactions first
		int GetPriority() const override { return 10; }

	};

} // namespace ComponentSystem