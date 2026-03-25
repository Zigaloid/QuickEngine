#include "GlfwMouse.h"
#include "Window.h"
#include <iostream>
#include <algorithm>
#include <cmath>

namespace Input {

	GlfwMouse::GlfwMouse(GLFWwindow* window)
		: m_window(window)
		, m_sensitivity(0.001f)
		, m_scrollSensitivity(1.0f)
		, m_cursorVisible(true)
		, m_cursorLocked(false)
		, m_initialized(false)
		, m_lastPressedButton(MouseButton::Left)
		, m_currentX(0.0)
		, m_currentY(0.0)
		, m_previousX(0.0)
		, m_previousY(0.0)
		, m_deltaX(0.0f)
		, m_deltaY(0.0f)
		, m_scrollDeltaX(0.0f)
		, m_scrollDeltaY(0.0f)
		, m_buttonCallback(nullptr)
		, m_moveCallback(nullptr)
		, m_scrollCallback(nullptr)
	{
		// Initialize states
		m_currentState = {};
		m_previousState = {};
		m_config = {};
		m_deviceName = "GLFW Mouse";
	}

	GlfwMouse::~GlfwMouse() {
		Shutdown();
	}

	bool GlfwMouse::Initialize() {
		if (m_initialized || !m_window) {
			return m_initialized;
		}

		// Get initial cursor position
		//glfwGetCursorPos(m_window, &m_currentX, &m_currentY);

		// Initialize mouse state
		m_currentState.connected = true;
		m_currentState.positionX = m_currentX;
		m_currentState.positionY = m_currentY;

		// Update initial button states
		updateButtonStates();

		// Set up GLFW callbacks
		setupGlfwCallbacks();

		m_initialized = true;
		std::cout << "GlfwMouse initialized successfully" << std::endl;
		return true;
	}

	void GlfwMouse::Update() {
		if (!m_initialized || !m_window) {
			return;
		}

		// Store previous state BEFORE updating current state
		m_previousState = m_currentState;
		m_previousX = m_currentX;
		m_previousY = m_currentY;

		// Always update button states to ensure consistency
		// This ensures WasButtonJustPressed works correctly regardless of callback usage
		updateButtonStates();

		// Update position
		updatePosition();

		// Calculate deltas
		m_deltaX = static_cast<float>(m_currentX - m_previousX);
		m_deltaY = static_cast<float>(m_currentY - m_previousY);

		// Update mouse state structure
		m_currentState.positionX = m_currentX;
		m_currentState.positionY = m_currentY;
		m_currentState.axes[static_cast<size_t>(MouseAxis::X)] = m_deltaX * m_sensitivity;
		m_currentState.axes[static_cast<size_t>(MouseAxis::Y)] = m_deltaY * m_sensitivity;
		m_currentState.axes[static_cast<size_t>(MouseAxis::ScrollX)] = m_scrollDeltaX * m_scrollSensitivity;
		m_currentState.axes[static_cast<size_t>(MouseAxis::ScrollY)] = m_scrollDeltaY * m_scrollSensitivity;

		// Apply Y inversion if configured
		if (m_config.invertY) {
			m_currentState.axes[static_cast<size_t>(MouseAxis::Y)] *= -1.0f;
		}

		// Process button callbacks for state changes
		processButtonCallbacks();

		// Call move callback if position changed
		if (m_moveCallback && (std::abs(m_deltaX) > 0.001f || std::abs(m_deltaY) > 0.001f)) {
			m_moveCallback(m_currentX, m_currentY, m_deltaX, m_deltaY);
		}

		// Call scroll callback if scroll occurred
		if (m_scrollCallback && (std::abs(m_scrollDeltaX) > 0.001f || std::abs(m_scrollDeltaY) > 0.001f)) {
			m_scrollCallback(m_scrollDeltaX, m_scrollDeltaY);
		}

	}
	void GlfwMouse::ResetScrollDeltas()
	{
		m_scrollDeltaX = 0.0f;
		m_scrollDeltaY = 0.0f;
	}

	void GlfwMouse::setupGlfwCallbacks() {
		if (!m_window) return;

		auto* windowInstance = static_cast<Window*>(glfwGetWindowUserPointer(m_window));
		if (windowInstance) {
			// Set callbacks through the Window class
			windowInstance->setMouseButtonCallback([this](int button, int action, int mods) {
				this->HandleButtonCallback(button, action, mods);
				});

			windowInstance->setMouseMoveCallback([this](double xpos, double ypos) {
				this->HandleMoveCallback(xpos, ypos);
				});

			windowInstance->setMouseScrollCallback([this](double xoffset, double yoffset) {
				this->HandleScrollCallback(xoffset, yoffset);
				});
		}

		std::cout << "GlfwMouse callbacks set up through Window class" << std::endl;
	}

	void GlfwMouse::Shutdown() {
		if (!m_initialized) {
			return;
		}

		// Restore cursor if it was hidden or locked
		if (!m_cursorVisible) {
			SetCursorVisible(true);
		}
		if (m_cursorLocked) {
			SetCursorLocked(false);
		}

		// Clear callbacks
		m_buttonCallback = nullptr;
		m_moveCallback = nullptr;
		m_scrollCallback = nullptr;

		m_initialized = false;
		std::cout << "GlfwMouse shutdown" << std::endl;
	}

	// State queries
	bool GlfwMouse::IsConnected() const {
		return m_currentState.connected && m_window != nullptr;
	}

	const MouseState& GlfwMouse::GetState() const {
		return m_currentState;
	}

	const MouseState& GlfwMouse::GetPreviousState() const {
		return m_previousState;
	}

	// Mouse information
	std::string GlfwMouse::GetName() const {
		return m_deviceName;
	}

	std::string GlfwMouse::GetGUID() const {
		return "glfw-mouse-device";
	}

	// Position queries
	double GlfwMouse::GetPositionX() const {
		return m_currentX;
	}

	double GlfwMouse::GetPositionY() const {
		return m_currentY;
	}

	void GlfwMouse::GetPosition(double& x, double& y) const {
		x = m_currentX;
		y = m_currentY;
	}

	// Movement queries
	float GlfwMouse::GetMovementX() const {
		return m_deltaX;
	}

	float GlfwMouse::GetMovementY() const {
		return m_deltaY;
	}

	void GlfwMouse::GetMovement(float& deltaX, float& deltaY) const {
		deltaX = m_deltaX;
		deltaY = m_deltaY;
	}

	// Button state queries
	bool GlfwMouse::IsButtonPressed(MouseButton button) const {
		return m_currentState.buttons[static_cast<size_t>(button)];
	}

	bool GlfwMouse::IsButtonReleased(MouseButton button) const {
		return !m_currentState.buttons[static_cast<size_t>(button)];
	}

	bool GlfwMouse::IsButtonDown(MouseButton button) const {
		return IsButtonPressed(button);
	}

	bool GlfwMouse::IsButtonUp(MouseButton button) const {
		return IsButtonReleased(button);
	}

	bool GlfwMouse::WasButtonJustPressed(MouseButton button) const {
		size_t index = static_cast<size_t>(button);
		return m_currentState.buttons[index] && !m_previousState.buttons[index];
	}

	bool GlfwMouse::WasButtonJustReleased(MouseButton button) const {
		size_t index = static_cast<size_t>(button);
		return !m_currentState.buttons[index] && m_previousState.buttons[index];
	}

	// Scroll queries
	float GlfwMouse::GetScrollX() const {
		return m_scrollDeltaX;
	}

	float GlfwMouse::GetScrollY() const {
		return m_scrollDeltaY;
	}

	void GlfwMouse::GetScroll(float& deltaX, float& deltaY) const {
		deltaX = m_scrollDeltaX;
		deltaY = m_scrollDeltaY;
	}

	// Convenience methods
	bool GlfwMouse::IsAnyButtonPressed() const {
		for (const auto& buttonPressed : m_currentState.buttons) {
			if (buttonPressed) {
				return true;
			}
		}
		return false;
	}

	MouseButton GlfwMouse::GetLastPressedButton() const {
		return m_lastPressedButton;
	}

	// Cursor control
	void GlfwMouse::SetPosition(double x, double y) {
		if (m_window) {
			glfwSetCursorPos(m_window, x, y);
			m_currentX = x;
			m_currentY = y;
		}
	}

	void GlfwMouse::SetCursorVisible(bool visible) {
		if (visible != m_cursorVisible && m_window) {
			m_cursorVisible = visible;
			glfwSetInputMode(m_window, GLFW_CURSOR, visible ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_HIDDEN);
		}
	}

	bool GlfwMouse::IsCursorVisible() const {
		return m_cursorVisible;
	}

	void GlfwMouse::SetCursorLocked(bool locked) {
		if (locked != m_cursorLocked && m_window) {
			m_cursorLocked = locked;
			if (locked) {
				glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			}
			else {
				glfwSetInputMode(m_window, GLFW_CURSOR, m_cursorVisible ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_HIDDEN);
			}
		}
	}

	bool GlfwMouse::IsCursorLocked() const {
		return m_cursorLocked;
	}

	// Configuration
	void GlfwMouse::SetSensitivity(float sensitivity) {
		m_sensitivity = std::max(0.01f, sensitivity);
	}

	float GlfwMouse::GetSensitivity() const {
		return m_sensitivity;
	}

	void GlfwMouse::SetScrollSensitivity(float sensitivity) {
		m_scrollSensitivity = std::max(0.01f, sensitivity);
	}

	float GlfwMouse::GetScrollSensitivity() const {
		return m_scrollSensitivity;
	}

	// Event callbacks
	void GlfwMouse::SetButtonCallback(MouseButtonCallback callback) {
		m_buttonCallback = callback;
	}

	void GlfwMouse::SetMoveCallback(MouseMoveCallback callback) {
		m_moveCallback = callback;
	}

	void GlfwMouse::SetScrollCallback(MouseScrollCallback callback) {
		m_scrollCallback = callback;
	}

	const MouseButtonCallback& GlfwMouse::GetButtonCallback() const {
		return m_buttonCallback;
	}

	const MouseMoveCallback& GlfwMouse::GetMoveCallback() const {
		return m_moveCallback;
	}

	const MouseScrollCallback& GlfwMouse::GetScrollCallback() const {
		return m_scrollCallback;
	}

	// GLFW-specific methods
	void GlfwMouse::SetWindow(GLFWwindow* window) {
		m_window = window;
	}

	// GLFW callback handlers
	void GlfwMouse::HandleButtonCallback(int button, int action, int mods) {
		MouseButton mouseButton = glfwButtonToMouseButton(button);
		bool pressed = (action == GLFW_PRESS);

		// Don't update button state here - let updateButtonStates() handle it
		// This prevents timing issues with WasButtonJustPressed

		if (pressed) {
			m_lastPressedButton = mouseButton;
		}

		// Call user callback if set
		if (m_buttonCallback) {
			m_buttonCallback(mouseButton, pressed, m_currentX, m_currentY);
		}
	}

	void GlfwMouse::HandleMoveCallback(double xpos, double ypos) {
	}

	void GlfwMouse::HandleScrollCallback(double xoffset, double yoffset) {
		// Accumulate scroll deltas
		m_scrollDeltaX += static_cast<float>(xoffset);
		m_scrollDeltaY += static_cast<float>(yoffset);
	}

	void GlfwMouse::HandleEnterCallback(int entered) {
		// Handle mouse enter/leave events if needed
		// This can be used for OnMouseEnter/OnMouseLeave events in input handlers
	}

	// Private helper methods
	void GlfwMouse::updateButtonStates() {
		if (!m_window) return;

		for (size_t i = 0; i < static_cast<size_t>(MouseButton::COUNT); ++i) {
			int glfwButton = BUTTON_MAPPINGS[i];
			int state = glfwGetMouseButton(m_window, glfwButton);
			m_currentState.buttons[i] = (state == GLFW_PRESS);
		}
	}

	void GlfwMouse::updatePosition() {
		if (!m_window) return;

		glfwGetCursorPos(m_window, &m_currentX, &m_currentY);
	}

	void GlfwMouse::resetDeltas() {
		m_deltaX = 0.0f;
		m_deltaY = 0.0f;
		m_scrollDeltaX = 0.0f;
		m_scrollDeltaY = 0.0f;
	}

	void GlfwMouse::processButtonCallbacks() {
		if (!m_buttonCallback) {
			return;
		}

		for (size_t i = 0; i < static_cast<size_t>(MouseButton::COUNT); ++i) {
			bool currentPressed = m_currentState.buttons[i];
			bool previousPressed = m_previousState.buttons[i];

			// Call callback on state change
			if (currentPressed != previousPressed) {
				MouseButton button = static_cast<MouseButton>(i);
				m_buttonCallback(button, currentPressed, m_currentX, m_currentY);
			}
		}
	}

	bool GlfwMouse::hasStateChanged() const {
		return m_currentState.positionX != m_previousState.positionX ||
			m_currentState.positionY != m_previousState.positionY ||
			m_currentState.buttons != m_previousState.buttons;
	}

	MouseButton GlfwMouse::findPressedButton() const {
		for (size_t i = 0; i < static_cast<size_t>(MouseButton::COUNT); ++i) {
			if (m_currentState.buttons[i]) {
				return static_cast<MouseButton>(i);
			}
		}
		return MouseButton::Left;
	}

	// Static helper methods
	MouseButton GlfwMouse::glfwButtonToMouseButton(int glfwButton) {
		switch (glfwButton) {
		case GLFW_MOUSE_BUTTON_LEFT:   return MouseButton::Left;
		case GLFW_MOUSE_BUTTON_RIGHT:  return MouseButton::Right;
		case GLFW_MOUSE_BUTTON_MIDDLE: return MouseButton::Middle;
		case GLFW_MOUSE_BUTTON_4:      return MouseButton::X1;
		case GLFW_MOUSE_BUTTON_5:      return MouseButton::X2;
		default:                       return MouseButton::Left;
		}
	}

	int GlfwMouse::mouseButtonToGlfwButton(MouseButton button) {
		return BUTTON_MAPPINGS[static_cast<size_t>(button)];
	}

} // namespace Input