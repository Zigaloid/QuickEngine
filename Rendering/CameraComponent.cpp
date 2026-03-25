#include "CameraComponent.h"
#include "Camera.h"
#include "CoreSystem/CoreSystem.h"
#include "Input/GamepadManager.h"
#include "Input/MouseManager.h"
#include "Profiler/profiler.h"

#include "glfw\glfw3.h"
#include <algorithm>
#include <cmath>

DebugChannels::CDebugChannel CameraDebug("CameraDebug");

namespace ComponentSystem {

	CameraComponent::CameraComponent()
		: Component()
		, Input::MouseInputHandler("CameraComponent", 100, true) // High priority, active by default
		, m_velocity(Vector3f::zero())
		, m_movingForward(false)
		, m_movingBackward(false)
		, m_movingLeft(false)
		, m_movingRight(false)
		, m_movingUp(false)
		, m_movingDown(false)
		, m_isSprinting(false)
		, m_mouseRotationEnabled(false)
		, m_lastMouseX(0.0)
		, m_lastMouseY(0.0)
		, m_firstMouse(true)
		, m_currentMovement(Vector3f::zero())
		, m_mouseManager(nullptr)
	{
	}

	bool CameraComponent::OnInitialize() {
		// Create camera with default settings
		auto camera = std::make_unique<Camera>(
			Vector3f(0, 2, 5),  // Position slightly back and up
			75.0f,              // FOV
			16.0f / 9.0f,       // Aspect ratio
			0.1f,               // Near plane
			1000.0f             // Far plane
		);

		// Create camera controller
		m_cameraController = std::make_unique<CameraController>(std::move(camera));

		// Set controller settings based on our camera settings
		CameraController::ControlSettings controlSettings;
		controlSettings.movementSpeed = m_settings.movementSpeed;
		controlSettings.rotationSpeed = m_settings.rotationSpeed;
		controlSettings.panSpeed = 1.0f;
		controlSettings.zoomSpeed = 1.0f;
		m_cameraController->setSettings(controlSettings);

		// Register with MouseManager
		RegisterWithMouseManager();

		CameraDebug.printf("CameraComponent initialized and registered with MouseManager\n");
		return true;
	}

	void CameraComponent::OnUpdate(double deltaTime)
	{
		DECLARE_FUNC_LOW();
		if (!m_cameraController) return;

		// Apply smooth movement
		const Vector3f& targetMovement = m_currentMovement;

		// Smooth velocity interpolation for movement
		float smoothFactor = 1.0f - std::exp(-m_settings.smoothing * static_cast<float>(deltaTime) * 60.0f);
		m_velocity = Vector3f::lerp(m_velocity, targetMovement, smoothFactor);

		// Apply movement
		Camera* camera = GetCamera();
		if (camera && m_velocity.magnitudeSquared() > 0.001f) {
			float speed = m_settings.movementSpeed * static_cast<float>(deltaTime);
			if (m_isSprinting) {
				speed *= m_settings.sprintMultiplier;
			}

			camera->moveRight(m_velocity.getX() * speed);
			camera->moveUp(m_velocity.getY() * speed);
			camera->moveForward(-m_velocity.getZ() * speed); // Negative for forward
		}
	}

	void CameraComponent::OnShutdown() {
		UnregisterFromMouseManager();
		m_cameraController.reset();
		CameraDebug.printf("CameraComponent shut down and unregistered from MouseManager\n");
	}

	// MouseInputHandler interface implementation
	bool CameraComponent::HandleButtonInput(Input::MouseButton button, bool pressed, bool justPressed, bool justReleased, double x, double y) {
		if (!Component::IsActive()) return false;

		// Convert MouseButton enum to GLFW constants for compatibility
		int glfwButton = GLFW_MOUSE_BUTTON_LEFT;
		switch (button) {
		case Input::MouseButton::Left:   glfwButton = GLFW_MOUSE_BUTTON_LEFT; break;
		case Input::MouseButton::Right:  glfwButton = GLFW_MOUSE_BUTTON_RIGHT; break;
		case Input::MouseButton::Middle: glfwButton = GLFW_MOUSE_BUTTON_MIDDLE; break;
		case Input::MouseButton::X1:     glfwButton = GLFW_MOUSE_BUTTON_4; break;
		case Input::MouseButton::X2:     glfwButton = GLFW_MOUSE_BUTTON_5; break;
		default: return false;
		}

		// Right mouse button controls camera rotation
		if (glfwButton == GLFW_MOUSE_BUTTON_RIGHT) {
			if (justPressed) {
				m_mouseRotationEnabled = true;
				m_firstMouse = true;
				m_lastMouseX = x;
				m_lastMouseY = y;
				CameraDebug.printf("Mouse camera rotation enabled\n");
				return true; // Consume the input
			}
			else if (justReleased) {
				m_mouseRotationEnabled = false;
				CameraDebug.printf("Mouse camera rotation disabled\n");
				return true; // Consume the input
			}
		}

		// Middle mouse button for reset camera
		if (glfwButton == GLFW_MOUSE_BUTTON_MIDDLE && justPressed) {
			CameraDebug.printf("Camera reset triggered\n");
			ResetCamera();
			return true; // Consume the input
		}

		return false; // Don't consume other button inputs
	}

	bool CameraComponent::HandleMovementInput(double x, double y, float deltaX, float deltaY) {
		if (!Component::IsActive()) {
			return false;
		}

		// Initialize on first movement
		if (m_firstMouse) {
			m_lastMouseX = x;
			m_lastMouseY = y;
			m_firstMouse = false;			
			return false;
		}

		// Only apply rotation if mouse rotation is enabled
		if (!m_mouseRotationEnabled) {
			m_lastMouseX = x;
			m_lastMouseY = y;			
			return false;
		}

		// Use the provided deltas directly - don't calculate our own!
		double actualDeltaX = deltaX;
		double actualDeltaY = deltaY;

		// Update last position for next frame
		m_lastMouseX = x;
		m_lastMouseY = y;

		// Apply rotation directly based on mouse delta
		Camera* camera = GetCamera();
		if (camera)
		{
			// Convert mouse movement to rotation with sensitivity
			float yawDelta = static_cast<float>(-actualDeltaX) * m_settings.mouseSensitivity;
			float pitchDelta = static_cast<float>(-actualDeltaY) * m_settings.mouseSensitivity;

			// Apply inversion settings
			if (m_settings.invertYaw) yawDelta = -yawDelta;
			if (m_settings.invertPitch) pitchDelta = -pitchDelta;

			// Apply rotation immediately
			camera->yaw(yawDelta);
			camera->pitch(pitchDelta);			
			return true; // Consume the input when rotating
		}
		return false;
	}

	bool CameraComponent::HandleScrollInput(float deltaX, float deltaY) {
		if (!Component::IsActive()) return false;

		// Mouse wheel for quick movement forward/backward
		Camera* camera = GetCamera();
		if (camera) {
			float scrollSpeed = 2.0f;
			camera->moveForward(-deltaY * scrollSpeed);
			CameraDebug.printf("Camera scroll: %.2f\n", deltaY);
			return true; // Consume the scroll input
		}

		return false;
	}

	Camera* CameraComponent::GetCamera() const {
		return m_cameraController ? m_cameraController->getCamera() : nullptr;
	}

	void CameraComponent::ResetCamera() {
		if (m_cameraController) {
			m_cameraController->resetCamera();
		}
		m_velocity = Vector3f::zero();
	}

	void CameraComponent::SetPosition(const Vector3f& position) {
		Camera* camera = GetCamera();
		if (camera) {
			camera->setPosition(position);
		}
	}

	void CameraComponent::SetLookAt(const Vector3f& target) {
		Camera* camera = GetCamera();
		if (camera) {
			camera->lookAt(target);
		}
	}

	void CameraComponent::UpdateAspectRatio(float aspectRatio) {
		if (m_cameraController) {
			m_cameraController->updateAspectRatio(aspectRatio);
		}
	}

	void CameraComponent::HandleKeyboard(int key, int scancode, int action, int mods) {
		if (!Component::IsActive()) return;
		if (mods)
			return;

		bool pressed = (action == GLFW_PRESS);
		bool released = (action == GLFW_RELEASE);
		bool repeat = (action == GLFW_REPEAT);

		// Handle key presses and releases
		if (pressed || released || repeat) {
			bool keyState = (pressed || repeat);

			switch (key) {
				// Movement keys (WASD)
			case GLFW_KEY_W:
				m_movingForward = keyState;
				break;
			case GLFW_KEY_S:
				m_movingBackward = keyState;
				break;
			case GLFW_KEY_A:
				m_movingLeft = keyState;
				break;
			case GLFW_KEY_D:
				m_movingRight = keyState;
				break;

				// Vertical movement keys
			case GLFW_KEY_Q:
				m_movingUp = keyState;
				break;
			case GLFW_KEY_E:
				// E for down movement (when not sprinting)
				if (!(mods & GLFW_MOD_SHIFT)) {
					m_movingDown = keyState;
				}
				break;

				// Sprint key
			case GLFW_KEY_LEFT_SHIFT:
				// Shift can be either sprint or down movement
				if (m_movingForward || m_movingBackward || m_movingLeft || m_movingRight) {
					// If moving horizontally, use shift as sprint
					m_isSprinting = keyState;
				}
				break;

				// Special actions
			case GLFW_KEY_R:
				if (pressed) {
					CameraDebug.printf("Camera reset triggered via R key\n");
					ResetCamera();
				}
				break;

			case GLFW_KEY_Z:
				if (pressed) {
					CameraDebug.printf("Camera look at origin triggered via Z key\n");
					SetLookAt(Vector3f::zero());
				}
				break;

			default:
				break;
			}

			// Update movement after key state changes
			UpdateMovementInput();
		}
	}

	void CameraComponent::UpdateMovementInput() {
		// Convert key states to movement vector
		Vector3f movement = Vector3f::zero();

		// Horizontal movement (strafe left/right)
		if (m_movingLeft) movement.setX(movement.getX() - 1.0f);
		if (m_movingRight) movement.setX(movement.getX() + 1.0f);

		// Forward/backward movement
		if (m_movingForward) movement.setZ(movement.getZ() + 1.0f);  // Positive Z for forward
		if (m_movingBackward) movement.setZ(movement.getZ() - 1.0f); // Negative Z for backward

		// Vertical movement
		if (m_movingUp) movement.setY(movement.getY() + 1.0f);
		if (m_movingDown) movement.setY(movement.getY() - 1.0f);

		// Normalize diagonal movement to prevent faster movement
		if (movement.magnitudeSquared() > 1.0f) {
			movement = movement.normalized();
		}

		m_currentMovement = movement;
	}

	void CameraComponent::RegisterWithMouseManager() {		
		auto m_mouseManager = Core::CoreSystem::GetFirstComponentOfType<Input::MouseManager>();

		if (m_mouseManager) {
			m_selfReference = std::shared_ptr<CameraComponent>(this, [](CameraComponent*) {
			// Custom deleter that does nothing - component lifetime is managed by ComponentManager
			});
			m_mouseManager->AddInputHandler(m_selfReference);
			CameraDebug.printf("CameraComponent registered with MouseManager\n");
		}
		else {
			CameraDebug.warning("No MouseManager found for CameraComponent registration\n");
		}
	}

	void CameraComponent::UnregisterFromMouseManager() {
		if (m_mouseManager && m_selfReference) {
			m_mouseManager->RemoveInputHandler(m_selfReference);
			m_selfReference.reset();
			m_mouseManager = nullptr;
			CameraDebug.printf("CameraComponent unregistered from MouseManager\n");
		}
	}

} // namespace ComponentSystem