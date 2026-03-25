#pragma once

#include "ComponentSystem\ComponentSystem.h"
#include "Input\MouseManager.h"
#include "Input\MouseInputHandler.h"
#include "CameraController.h"
#include <memory>

namespace ComponentSystem {

	class CameraComponent : public Component, public Input::MouseInputHandler {
	public:
		// Camera settings
		struct CameraSettings {
			float movementSpeed = 5.0f;
			float rotationSpeed = 2.0f;
			float sprintMultiplier = 2.5f;
			float smoothing = 0.1f;
			bool invertPitch = false;
			bool invertYaw = false;
			float mouseSensitivity = 0.005f;
		};

	private:
		std::unique_ptr<CameraController> m_cameraController;

		// Camera settings instance
		CameraSettings m_settings;

		// Current velocity for smooth movement
		Vector3f m_velocity;

		// Input state for movement (WASD + QE)
		bool m_movingForward;    // W
		bool m_movingBackward;   // S
		bool m_movingLeft;       // A
		bool m_movingRight;      // D
		bool m_movingUp;         // Q or Space
		bool m_movingDown;       // E or Left Shift
		bool m_isSprinting;      // Left Shift (when not used for down movement)

		// Mouse state for rotation
		bool m_mouseRotationEnabled;
		double m_lastMouseX;
		double m_lastMouseY;
		bool m_firstMouse;

		// Current movement
		Vector3f m_currentMovement;

		// Mouse manager reference for registration
		Input::MouseManager* m_mouseManager;
		std::shared_ptr<CameraComponent> m_selfReference; // For registration with MouseManager

	public:
		CameraComponent();
		~CameraComponent() override = default;

		// Component lifecycle
		bool OnInitialize() override;
		void OnUpdate(double deltaTime) override;
		void OnShutdown() override;

		// MouseInputHandler interface
		bool HandleButtonInput(Input::MouseButton button, bool pressed, bool justPressed, bool justReleased, double x, double y) override;
		bool HandleMovementInput(double x, double y, float deltaX, float deltaY) override;
		bool HandleScrollInput(float deltaX, float deltaY) override;

		// MouseInputHandler properties
		int GetPriority() const override { return 100; } // High priority for camera
		const char* GetName() const override { return "CameraComponent"; }

		// Camera access
		Camera* GetCamera() const;
		CameraController* GetCameraController() const { return m_cameraController.get(); }

		// Settings
		const CameraSettings& GetSettings() const { return m_settings; }
		void SetSettings(const CameraSettings& settings) { m_settings = settings; }

		void SetMovementSpeed(float speed) { m_settings.movementSpeed = speed; }
		void SetRotationSpeed(float speed) { m_settings.rotationSpeed = speed; }
		void SetSprintMultiplier(float multiplier) { m_settings.sprintMultiplier = multiplier; }
		void SetSmoothing(float smoothing) { m_settings.smoothing = smoothing; }
		void SetInvertPitch(bool invert) { m_settings.invertPitch = invert; }
		void SetInvertYaw(bool invert) { m_settings.invertYaw = invert; }
		void SetMouseSensitivity(float sensitivity) { m_settings.mouseSensitivity = sensitivity; }

		// Camera control methods
		void ResetCamera();
		void SetPosition(const Vector3f& position);
		void SetLookAt(const Vector3f& target);
		void UpdateAspectRatio(float aspectRatio);

		// Input handling methods for keyboard (called from viewport callbacks)
		void HandleKeyboard(int key, int scancode, int action, int mods);

		// Getters for movement system
		const Vector3f& GetCurrentMovement() const { return m_currentMovement; }
		bool IsSprinting() const { return m_isSprinting; }

		// Enable/disable mouse rotation
		void SetMouseRotationEnabled(bool enabled) { m_mouseRotationEnabled = enabled; }
		bool IsMouseRotationEnabled() const { return m_mouseRotationEnabled; }

	private:
		// Internal update methods
		void UpdateMovementInput();
		void RegisterWithMouseManager();
		void UnregisterFromMouseManager();
	};

} // namespace ComponentSystem