#pragma once

#include "Input/MouseInterface.h"
#include "GL/glew.h"
#include "GLFW/glfw3.h"
#include <memory>

namespace Input {

	class GlfwMouse : public IMouse {
	private:
		// Mouse state
		MouseState m_currentState;
		MouseState m_previousState;

		// Configuration
		MouseConfig m_config;
		float m_sensitivity;
		float m_scrollSensitivity;

		// GLFW window reference
		GLFWwindow* m_window;

		// Cursor state
		bool m_cursorVisible;
		bool m_cursorLocked;

		// Event callbacks
		MouseButtonCallback m_buttonCallback;
		MouseMoveCallback m_moveCallback;
		MouseScrollCallback m_scrollCallback;

		// Internal state
		bool m_initialized;
		std::string m_deviceName;
		MouseButton m_lastPressedButton;

		// Position tracking
		double m_currentX, m_currentY;
		double m_previousX, m_previousY;
		float m_deltaX, m_deltaY;
		float m_scrollDeltaX, m_scrollDeltaY;

		// Button state mapping
		static constexpr int BUTTON_MAPPINGS[static_cast<size_t>(MouseButton::COUNT)] = {
			GLFW_MOUSE_BUTTON_LEFT,     // MouseButton::Left
			GLFW_MOUSE_BUTTON_RIGHT,    // MouseButton::Right
			GLFW_MOUSE_BUTTON_MIDDLE,   // MouseButton::Middle
			GLFW_MOUSE_BUTTON_4,        // MouseButton::X1
			GLFW_MOUSE_BUTTON_5         // MouseButton::X2
		};

	public:
		explicit GlfwMouse(GLFWwindow* window = nullptr);
		virtual ~GlfwMouse();

		// IMouse interface implementation
		bool Initialize() override;
		void Update() override;
		void Shutdown() override;

		// State queries
		bool IsConnected() const override;
		const MouseState& GetState() const override;
		const MouseState& GetPreviousState() const override;

		// Mouse information
		std::string GetName() const override;
		std::string GetGUID() const override;

		// Position queries
		double GetPositionX() const override;
		double GetPositionY() const override;
		void GetPosition(double& x, double& y) const override;

		// Movement queries
		float GetMovementX() const override;
		float GetMovementY() const override;
		void GetMovement(float& deltaX, float& deltaY) const override;

		// Button state queries
		bool IsButtonPressed(MouseButton button) const override;
		bool IsButtonReleased(MouseButton button) const override;
		bool IsButtonDown(MouseButton button) const override;
		bool IsButtonUp(MouseButton button) const override;
		bool WasButtonJustPressed(MouseButton button) const override;
		bool WasButtonJustReleased(MouseButton button) const override;

		// Scroll queries
		float GetScrollX() const override;
		float GetScrollY() const override;
		void GetScroll(float& deltaX, float& deltaY) const override;

		// Convenience methods
		bool IsAnyButtonPressed() const override;
		MouseButton GetLastPressedButton() const override;

		// Cursor control
		void SetPosition(double x, double y) override;
		void SetCursorVisible(bool visible) override;
		bool IsCursorVisible() const override;
		void SetCursorLocked(bool locked) override;
		bool IsCursorLocked() const override;

		// Configuration
		void SetSensitivity(float sensitivity) override;
		float GetSensitivity() const override;
		void SetScrollSensitivity(float sensitivity) override;
		float GetScrollSensitivity() const override;

		// Event callbacks
		void SetButtonCallback(MouseButtonCallback callback) override;
		void SetMoveCallback(MouseMoveCallback callback) override;
		void SetScrollCallback(MouseScrollCallback callback) override;
		const MouseButtonCallback& GetButtonCallback() const override;
		const MouseMoveCallback& GetMoveCallback() const override;
		const MouseScrollCallback& GetScrollCallback() const override;
		void ResetScrollDeltas() override;
		// GLFW-specific methods
		void SetWindow(GLFWwindow* window);
		GLFWwindow* GetWindow() const { return m_window; }

		// GLFW callback handlers
		void HandleButtonCallback(int button, int action, int mods);
		void HandleMoveCallback(double xpos, double ypos);
		void HandleScrollCallback(double xoffset, double yoffset);
		void HandleEnterCallback(int entered);

	private:
		// Internal helper methods
		void updateButtonStates();
		void updatePosition();
		void resetDeltas();
		void processButtonCallbacks();
		void setupGlfwCallbacks();
		// State comparison helpers
		bool hasStateChanged() const;
		MouseButton findPressedButton() const;

		// GLFW helper methods
		static MouseButton glfwButtonToMouseButton(int glfwButton);
		static int mouseButtonToGlfwButton(MouseButton button);
	};

} // namespace Input