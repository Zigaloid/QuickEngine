#include "WindowsMouse.h"
#include <iostream>
#include <algorithm>
#include <cmath>

namespace Input {

	WindowsMouse::WindowsMouse(HWND windowHandle)
		: m_windowHandle(windowHandle)
		, m_sensitivity(1.0f)
		, m_scrollSensitivity(1.0f)
		, m_cursorVisible(true)
		, m_cursorLocked(false)
		, m_rawInputEnabled(false)
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

		// Initialize lock area
		m_lockArea = {};

		// Initialize raw input device
		m_rawInputDevice = {};
	}

	WindowsMouse::~WindowsMouse() {
		Shutdown();
	}

	bool WindowsMouse::Initialize() {
		if (m_initialized) {
			return true;
		}

		// Update device name
		updateDeviceName();

		// Setup raw input if enabled
		if (m_config.enableRawInput) {
			setupRawInput();
		}

		// Get initial cursor position
		POINT cursorPos;
		if (GetCursorPos(&cursorPos)) {
			if (m_windowHandle) {
				ScreenToClient(m_windowHandle, &cursorPos);
			}
			m_currentX = m_previousX = static_cast<double>(cursorPos.x);
			m_currentY = m_previousY = static_cast<double>(cursorPos.y);
		}

		// Initialize mouse state
		m_currentState.connected = true;
		m_currentState.positionX = m_currentX;
		m_currentState.positionY = m_currentY;

		m_initialized = true;
		std::cout << "WindowsMouse initialized successfully" << std::endl;
		return true;
	}

	void WindowsMouse::Update() {
		if (!m_initialized) {
			return;
		}

		// Store previous state BEFORE updating current state
		m_previousState = m_currentState;
		m_previousX = m_currentX;
		m_previousY = m_currentY;

		// Update button states only if not using window messages
		// Window messages will handle button state updates directly
		if (!m_windowHandle) {
			updateButtonStates();
		}

		// Update position if not using raw input
		if (!m_rawInputEnabled) {
			updatePosition();
		}

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

		// Process callbacks (but don't call them here as they're called in ProcessWindowMessage)
		// Only process if we're not using window messages
		if (!m_windowHandle) {
			processButtonCallbacks();
		}

		// Call move callback if position changed
		if (m_moveCallback && (std::abs(m_deltaX) > 0.001f || std::abs(m_deltaY) > 0.001f)) {
			m_moveCallback(m_currentX, m_currentY, m_deltaX, m_deltaY);
		}

		// Call scroll callback if scroll occurred
		if (m_scrollCallback && (std::abs(m_scrollDeltaX) > 0.001f || std::abs(m_scrollDeltaY) > 0.001f)) {
			m_scrollCallback(m_scrollDeltaX, m_scrollDeltaY);
		}

		// Reset scroll delta for next frame
		resetScrollDelta();
	}

	void WindowsMouse::Shutdown() {
		if (!m_initialized) {
			return;
		}

		// Cleanup raw input
		cleanupRawInput();

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
		std::cout << "WindowsMouse shutdown" << std::endl;
	}

	// State queries
	bool WindowsMouse::IsConnected() const {
		return m_currentState.connected;
	}

	const MouseState& WindowsMouse::GetState() const {
		return m_currentState;
	}

	const MouseState& WindowsMouse::GetPreviousState() const {
		return m_previousState;
	}

	// Mouse information
	std::string WindowsMouse::GetName() const {
		return m_deviceName.empty() ? "Windows Mouse" : m_deviceName;
	}

	std::string WindowsMouse::GetGUID() const {
		return "windows-mouse-device";
	}

	// Position queries
	double WindowsMouse::GetPositionX() const {
		return m_currentX;
	}

	double WindowsMouse::GetPositionY() const {
		return m_currentY;
	}

	void WindowsMouse::GetPosition(double& x, double& y) const {
		x = m_currentX;
		y = m_currentY;
	}

	// Movement queries
	float WindowsMouse::GetMovementX() const {
		return m_deltaX;
	}

	float WindowsMouse::GetMovementY() const {
		return m_deltaY;
	}

	void WindowsMouse::GetMovement(float& deltaX, float& deltaY) const {
		deltaX = m_deltaX;
		deltaY = m_deltaY;
	}

	// Button state queries
	bool WindowsMouse::IsButtonPressed(MouseButton button) const {
		return m_currentState.buttons[static_cast<size_t>(button)];
	}

	bool WindowsMouse::IsButtonReleased(MouseButton button) const {
		return !m_currentState.buttons[static_cast<size_t>(button)];
	}

	bool WindowsMouse::IsButtonDown(MouseButton button) const {
		return IsButtonPressed(button);
	}

	bool WindowsMouse::IsButtonUp(MouseButton button) const {
		return IsButtonReleased(button);
	}

	bool WindowsMouse::WasButtonJustPressed(MouseButton button) const {
		size_t index = static_cast<size_t>(button);
		return m_currentState.buttons[index] && !m_previousState.buttons[index];
	}

	bool WindowsMouse::WasButtonJustReleased(MouseButton button) const {
		size_t index = static_cast<size_t>(button);
		return !m_currentState.buttons[index] && m_previousState.buttons[index];
	}

	// Scroll queries
	float WindowsMouse::GetScrollX() const {
		return m_scrollDeltaX;
	}

	float WindowsMouse::GetScrollY() const {
		return m_scrollDeltaY;
	}
	
	void WindowsMouse::ResetScrollDeltas()
	{
		m_scrollDeltaX = 0.0f;
		m_scrollDeltaY = 0.0f;
	}

	void WindowsMouse::GetScroll(float& deltaX, float& deltaY) const {
		deltaX = m_scrollDeltaX;
		deltaY = m_scrollDeltaY;
	}

	// Convenience methods
	bool WindowsMouse::IsAnyButtonPressed() const {
		for (const auto& buttonPressed : m_currentState.buttons) {
			if (buttonPressed) {
				return true;
			}
		}
		return false;
	}

	MouseButton WindowsMouse::GetLastPressedButton() const {
		return m_lastPressedButton;
	}

	// Cursor control
	void WindowsMouse::SetPosition(double x, double y) {
		POINT newPos = { static_cast<LONG>(x), static_cast<LONG>(y) };

		if (m_windowHandle) {
			ClientToScreen(m_windowHandle, &newPos);
		}

		SetCursorPos(newPos.x, newPos.y);

		// Update internal position immediately
		m_currentX = x;
		m_currentY = y;
	}

	void WindowsMouse::SetCursorVisible(bool visible) {
		if (visible != m_cursorVisible) {
			m_cursorVisible = visible;
			ShowCursor(visible ? TRUE : FALSE);
		}
	}

	bool WindowsMouse::IsCursorVisible() const {
		return m_cursorVisible;
	}

	void WindowsMouse::SetCursorLocked(bool locked) {
		if (locked != m_cursorLocked) {
			m_cursorLocked = locked;
			if (locked) {
				lockCursorToWindow();
			}
			else {
				unlockCursor();
			}
		}
	}

	bool WindowsMouse::IsCursorLocked() const {
		return m_cursorLocked;
	}

	// Configuration
	void WindowsMouse::SetSensitivity(float sensitivity) {
		m_sensitivity = std::max(0.01f, sensitivity);
	}

	float WindowsMouse::GetSensitivity() const {
		return m_sensitivity;
	}

	void WindowsMouse::SetScrollSensitivity(float sensitivity) {
		m_scrollSensitivity = std::max(0.01f, sensitivity);
	}

	float WindowsMouse::GetScrollSensitivity() const {
		return m_scrollSensitivity;
	}

	// Event callbacks
	void WindowsMouse::SetButtonCallback(MouseButtonCallback callback) {
		m_buttonCallback = callback;
	}

	void WindowsMouse::SetMoveCallback(MouseMoveCallback callback) {
		m_moveCallback = callback;
	}

	void WindowsMouse::SetScrollCallback(MouseScrollCallback callback) {
		m_scrollCallback = callback;
	}

	const MouseButtonCallback& WindowsMouse::GetButtonCallback() const {
		return m_buttonCallback;
	}

	const MouseMoveCallback& WindowsMouse::GetMoveCallback() const {
		return m_moveCallback;
	}

	const MouseScrollCallback& WindowsMouse::GetScrollCallback() const {
		return m_scrollCallback;
	}

	// Windows-specific methods
	void WindowsMouse::SetWindowHandle(HWND windowHandle) {
		m_windowHandle = windowHandle;
	}

	bool WindowsMouse::ProcessRawInput(LPARAM lParam) {
		if (!m_rawInputEnabled) {
			return false;
		}

		UINT dwSize = 0;
		GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, nullptr, &dwSize, sizeof(RAWINPUTHEADER));

		if (dwSize == 0) {
			return false;
		}

		std::unique_ptr<BYTE[]> lpb = std::make_unique<BYTE[]>(dwSize);
		if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, lpb.get(), &dwSize, sizeof(RAWINPUTHEADER)) != dwSize) {
			return false;
		}

		RAWINPUT* raw = reinterpret_cast<RAWINPUT*>(lpb.get());
		if (raw->header.dwType == RIM_TYPEMOUSE) {
			// Update delta from raw input
			m_deltaX = static_cast<float>(raw->data.mouse.lLastX);
			m_deltaY = static_cast<float>(raw->data.mouse.lLastY);

			// Update absolute position
			if (!(raw->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE)) {
				m_currentX += m_deltaX;
				m_currentY += m_deltaY;
			}
			else {
				m_currentX = static_cast<double>(raw->data.mouse.lLastX);
				m_currentY = static_cast<double>(raw->data.mouse.lLastY);
			}

			return true;
		}

		return false;
	}

	bool WindowsMouse::ProcessWindowMessage(UINT message, WPARAM wParam, LPARAM lParam) {
		switch (message) {
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_XBUTTONDOWN:
		case WM_XBUTTONUP:
		{
			MouseButton button = getMouseButtonFromMessage(message, wParam);
			bool pressed = (message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN ||
				message == WM_MBUTTONDOWN || message == WM_XBUTTONDOWN);

			// Update button state immediately
			m_currentState.buttons[static_cast<size_t>(button)] = pressed;

			if (pressed) {
				m_lastPressedButton = button;
			}

			// Get cursor position for callback
			POINT cursorPos;
			GetCursorPos(&cursorPos);
			if (m_windowHandle) {
				ScreenToClient(m_windowHandle, &cursorPos);
			}

			if (m_buttonCallback) {
				m_buttonCallback(button, pressed, static_cast<double>(cursorPos.x), static_cast<double>(cursorPos.y));
			}

			return true;
		}

		case WM_MOUSEMOVE:
		{
			if (!m_rawInputEnabled) {
				POINT cursorPos;
				GetCursorPos(&cursorPos);
				if (m_windowHandle) {
					ScreenToClient(m_windowHandle, &cursorPos);
				}

				double newX = static_cast<double>(cursorPos.x);
				double newY = static_cast<double>(cursorPos.y);

				m_deltaX = static_cast<float>(newX - m_currentX);
				m_deltaY = static_cast<float>(newY - m_currentY);

				m_currentX = newX;
				m_currentY = newY;
			}
			return true;
		}

		case WM_MOUSEWHEEL:
		{
			m_scrollDeltaY += GET_WHEEL_DELTA_WPARAM(wParam) / static_cast<float>(WHEEL_DELTA);
			return true;
		}

		case WM_MOUSEHWHEEL:
		{
			m_scrollDeltaX += GET_WHEEL_DELTA_WPARAM(wParam) / static_cast<float>(WHEEL_DELTA);
			return true;
		}

		case WM_INPUT:
		{
			return ProcessRawInput(lParam);
		}
		}

		return false;
	}

	void WindowsMouse::SetRawInputEnabled(bool enabled) {
		if (enabled != m_rawInputEnabled) {
			m_rawInputEnabled = enabled;
			if (m_initialized) {
				if (enabled) {
					setupRawInput();
				}
				else {
					cleanupRawInput();
				}
			}
		}
	}

	// Private helper methods
	void WindowsMouse::updateButtonStates() {
		// Update button states from Windows API
		m_currentState.buttons[static_cast<size_t>(MouseButton::Left)] = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
		m_currentState.buttons[static_cast<size_t>(MouseButton::Right)] = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
		m_currentState.buttons[static_cast<size_t>(MouseButton::Middle)] = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;
		m_currentState.buttons[static_cast<size_t>(MouseButton::X1)] = (GetAsyncKeyState(VK_XBUTTON1) & 0x8000) != 0;
		m_currentState.buttons[static_cast<size_t>(MouseButton::X2)] = (GetAsyncKeyState(VK_XBUTTON2) & 0x8000) != 0;
	}

	void WindowsMouse::updatePosition() {
		POINT cursorPos;
		if (GetCursorPos(&cursorPos)) {
			if (m_windowHandle) {
				ScreenToClient(m_windowHandle, &cursorPos);
			}

			double newX = static_cast<double>(cursorPos.x);
			double newY = static_cast<double>(cursorPos.y);

			m_deltaX = static_cast<float>(newX - m_currentX);
			m_deltaY = static_cast<float>(newY - m_currentY);

			m_currentX = newX;
			m_currentY = newY;
		}
	}

	void WindowsMouse::updateScrollDelta() {
		// This method can be used to update scroll deltas from accumulated values
		// Currently, scroll updates are handled directly in ProcessWindowMessage
		// This method is here for completeness and future use
	}

	void WindowsMouse::resetScrollDelta() {
		m_scrollDeltaX = 0.0f;
		m_scrollDeltaY = 0.0f;
	}

	void WindowsMouse::processButtonCallbacks() {
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

	void WindowsMouse::lockCursorToWindow() {
		if (m_windowHandle) {
			GetClientRect(m_windowHandle, &m_lockArea);
			ClientToScreen(m_windowHandle, reinterpret_cast<POINT*>(&m_lockArea.left));
			ClientToScreen(m_windowHandle, reinterpret_cast<POINT*>(&m_lockArea.right));
			ClipCursor(&m_lockArea);
		}
	}

	void WindowsMouse::unlockCursor() {
		ClipCursor(nullptr);
	}

	bool WindowsMouse::setupRawInput() {
		m_rawInputDevice.usUsagePage = 0x01;  // Generic Desktop Controls
		m_rawInputDevice.usUsage = 0x02;      // Mouse
		m_rawInputDevice.dwFlags = 0;         // Default flags
		m_rawInputDevice.hwndTarget = m_windowHandle;

		if (RegisterRawInputDevices(&m_rawInputDevice, 1, sizeof(RAWINPUTDEVICE)) == FALSE) {
			std::cerr << "Failed to register raw input device for mouse" << std::endl;
			return false;
		}

		m_rawInputEnabled = true;
		return true;
	}

	void WindowsMouse::cleanupRawInput() {
		if (m_rawInputEnabled) {
			m_rawInputDevice.dwFlags = RIDEV_REMOVE;
			m_rawInputDevice.hwndTarget = nullptr;
			RegisterRawInputDevices(&m_rawInputDevice, 1, sizeof(RAWINPUTDEVICE));
			m_rawInputEnabled = false;
		}
	}

	void WindowsMouse::updateDeviceName() {
		m_deviceName = "Windows System Mouse";

		// Try to get more specific device information
		UINT deviceCount = 0;
		if (GetRawInputDeviceList(nullptr, &deviceCount, sizeof(RAWINPUTDEVICELIST)) != -1 && deviceCount > 0) {
			std::vector<RAWINPUTDEVICELIST> deviceList(deviceCount);
			if (GetRawInputDeviceList(deviceList.data(), &deviceCount, sizeof(RAWINPUTDEVICELIST)) != -1) {
				for (const auto& device : deviceList) {
					if (device.dwType == RIM_TYPEMOUSE) {
						UINT nameSize = 0;
						if (GetRawInputDeviceInfo(device.hDevice, RIDI_DEVICENAME, nullptr, &nameSize) != -1 && nameSize > 0) {
							std::wstring deviceName(nameSize, L'\0');
							if (GetRawInputDeviceInfo(device.hDevice, RIDI_DEVICENAME, deviceName.data(), &nameSize) != -1) {
								// Properly convert wide string to narrow string using WideCharToMultiByte
								int requiredSize = WideCharToMultiByte(CP_UTF8, 0, deviceName.c_str(), -1, nullptr, 0, nullptr, nullptr);
								if (requiredSize > 0) {
									std::string name(requiredSize - 1, '\0'); // -1 to exclude null terminator
									if (WideCharToMultiByte(CP_UTF8, 0, deviceName.c_str(), -1, name.data(), requiredSize, nullptr, nullptr) > 0) {
										if (!name.empty()) {
											m_deviceName = name;
											break;
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	// Static helper methods
	MouseButton WindowsMouse::getMouseButtonFromMessage(UINT message, WPARAM wParam) {
		switch (message) {
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
			return MouseButton::Left;
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
			return MouseButton::Right;
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
			return MouseButton::Middle;
		case WM_XBUTTONDOWN:
		case WM_XBUTTONUP:
			return (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) ? MouseButton::X1 : MouseButton::X2;
		default:
			return MouseButton::Left;
		}
	}

	bool WindowsMouse::isButtonPressed(WPARAM wParam, MouseButton button) {
		DWORD buttonFlag = BUTTON_MAPPINGS[static_cast<size_t>(button)];
		return (wParam & buttonFlag) != 0;
	}

	// Helper functions implementation
	const char* GetButtonName(MouseButton button) {
		switch (button) {
		case MouseButton::Left: return "Left";
		case MouseButton::Right: return "Right";
		case MouseButton::Middle: return "Middle";
		case MouseButton::X1: return "X1";
		case MouseButton::X2: return "X2";
		default: return "Unknown";
		}
	}

	const char* GetAxisName(MouseAxis axis) {
		switch (axis) {
		case MouseAxis::X: return "Mouse X";
		case MouseAxis::Y: return "Mouse Y";
		case MouseAxis::ScrollX: return "Scroll X";
		case MouseAxis::ScrollY: return "Scroll Y";
		default: return "Unknown";
		}
	}

} // namespace Input