#include "WindowsGamepad.h"
#include <iostream>
#include <algorithm>
#include <cmath>

namespace Input {
    WindowsGamepad::WindowsGamepad(DWORD gamepadId)
        : m_gamepadId(gamepadId)
        , m_connected(false)
        , m_initialized(false)
        , m_vibrationEnabled(true) {
        
        // Initialize XInput states
        ZeroMemory(&m_currentState, sizeof(XINPUT_STATE));
        ZeroMemory(&m_previousState, sizeof(XINPUT_STATE));
        ZeroMemory(&m_vibrationState, sizeof(XINPUT_VIBRATION));
        
        // Initialize deadzones with default values
        for (size_t i = 0; i < static_cast<size_t>(GamepadAxis::COUNT); ++i) {
            if (i == static_cast<size_t>(GamepadAxis::LeftTrigger) || 
                i == static_cast<size_t>(GamepadAxis::RightTrigger)) {
                m_deadzones[i] = 0.05f; // Lower deadzone for triggers
            } else {
                m_deadzones[i] = 0.1f; // Standard deadzone for sticks
            }
            m_sensitivities[i] = 1.0f;
        }

    }

    WindowsGamepad::~WindowsGamepad() {
        Shutdown();
    }

    bool WindowsGamepad::Initialize() {
        if (m_initialized) {
            return true;
        }

        // Check if the gamepad is connected
        XINPUT_STATE state;
        DWORD result = XInputGetState(m_gamepadId, &state);
        
        if (result == ERROR_SUCCESS) {
            m_connected = true;
            m_currentState = state;
            m_previousState = state;
            
            // Initialize gamepad states
            mapXInputToGamepadState(state.Gamepad);
            m_previousGamepadState = m_currentGamepadState;
            
            m_initialized = true;
            
            std::cout << "WindowsGamepad " << m_gamepadId << " initialized successfully" << std::endl;
            return true;
        } else {
            m_connected = false;
            std::cout << "WindowsGamepad " << m_gamepadId << " failed to initialize (not connected)" << std::endl;
            return false;
        }
    }

    void WindowsGamepad::Update() {
        if (!m_initialized) {
            return;
        }

        // Store previous state
        m_previousState = m_currentState;
        m_previousGamepadState = m_currentGamepadState;

        // Get current XInput state
        DWORD result = XInputGetState(m_gamepadId, &m_currentState);
        
        if (result == ERROR_SUCCESS) {
            if (!m_connected) {
                m_connected = true;
                std::cout << "WindowsGamepad " << m_gamepadId << " reconnected" << std::endl;
            }
            
            // Update gamepad state
            updateGamepadState();
            
            // Process button and axis changes
            processButtonStates();
            processAxisStates();
            
        } else if (result == ERROR_DEVICE_NOT_CONNECTED) {
            if (m_connected) {
                m_connected = false;
                std::cout << "WindowsGamepad " << m_gamepadId << " disconnected" << std::endl;
                
                // Clear state when disconnected
                ZeroMemory(&m_currentState, sizeof(XINPUT_STATE));
                m_currentGamepadState = GamepadState{};
            }
        }
    }

    void WindowsGamepad::Shutdown() {
        if (!m_initialized) {
            return;
        }

        // Stop vibration
        StopVibration();
        
        m_connected = false;
        m_initialized = false;
        
        // Clear callbacks
        m_buttonCallback = nullptr;
        m_axisCallback = nullptr;
        
        std::cout << "WindowsGamepad " << m_gamepadId << " shutdown" << std::endl;
    }

    void WindowsGamepad::updateGamepadState() {
        mapXInputToGamepadState(m_currentState.Gamepad);
        m_currentGamepadState.connected = m_connected;
    }

    void WindowsGamepad::mapXInputToGamepadState(const XINPUT_GAMEPAD& xinputState) {
        // Map buttons
        for (size_t i = 0; i < static_cast<size_t>(GamepadButton::COUNT); ++i) {
            GamepadButton button = static_cast<GamepadButton>(i);
            
            // Handle the Guide button specially as it may not be available in all XInput versions
            if (button == GamepadButton::Guide) {
                #ifdef XINPUT_GAMEPAD_GUIDE
                m_currentGamepadState.buttons[i] = (xinputState.wButtons & XINPUT_GAMEPAD_GUIDE) != 0;
                #else
                m_currentGamepadState.buttons[i] = false; // Guide button not supported
                #endif
            } else {
                m_currentGamepadState.buttons[i] = (xinputState.wButtons & XINPUT_BUTTON_MAP[i]) != 0;
            }
        }

        // Map axes
        m_currentGamepadState.axes[static_cast<size_t>(GamepadAxis::LeftX)] = 
            normalizeAxisValue(xinputState.sThumbLX, m_deadzones[static_cast<size_t>(GamepadAxis::LeftX)]);
        
        m_currentGamepadState.axes[static_cast<size_t>(GamepadAxis::LeftY)] = 
            normalizeAxisValue(xinputState.sThumbLY, m_deadzones[static_cast<size_t>(GamepadAxis::LeftY)]);
        
        m_currentGamepadState.axes[static_cast<size_t>(GamepadAxis::RightX)] = 
            normalizeAxisValue(xinputState.sThumbRX, m_deadzones[static_cast<size_t>(GamepadAxis::RightX)]);
        
        m_currentGamepadState.axes[static_cast<size_t>(GamepadAxis::RightY)] = 
            normalizeAxisValue(xinputState.sThumbRY, m_deadzones[static_cast<size_t>(GamepadAxis::RightY)]);
        
        m_currentGamepadState.axes[static_cast<size_t>(GamepadAxis::LeftTrigger)] = 
            normalizeTriggerValue(xinputState.bLeftTrigger, m_deadzones[static_cast<size_t>(GamepadAxis::LeftTrigger)]);
        
        m_currentGamepadState.axes[static_cast<size_t>(GamepadAxis::RightTrigger)] = 
            normalizeTriggerValue(xinputState.bRightTrigger, m_deadzones[static_cast<size_t>(GamepadAxis::RightTrigger)]);

        // Apply sensitivity
        for (size_t i = 0; i < static_cast<size_t>(GamepadAxis::COUNT); ++i) {
            m_currentGamepadState.axes[i] *= m_sensitivities[i];
            m_currentGamepadState.axes[i] = std::clamp(m_currentGamepadState.axes[i], -1.0f, 1.0f);
        }
    }

    float WindowsGamepad::normalizeAxisValue(SHORT rawValue, float deadzone) const {
        // Convert from SHORT range to float range
        float normalized = static_cast<float>(rawValue) / 32767.0f;
        
        // Apply deadzone
        return ApplyDeadzone(normalized, deadzone);
    }

    float WindowsGamepad::normalizeTriggerValue(BYTE rawValue, float deadzone) const {
        // Convert from BYTE range to float range (0-1)
        float normalized = static_cast<float>(rawValue) / 255.0f;
        
        // Apply deadzone
        if (normalized < deadzone) {
            return 0.0f;
        }
        
        // Scale to use full range after deadzone
        return (normalized - deadzone) / (1.0f - deadzone);
    }

    void WindowsGamepad::processButtonStates() {
        if (!m_buttonCallback) {
            return;
        }

        for (size_t i = 0; i < static_cast<size_t>(GamepadButton::COUNT); ++i) {
            GamepadButton button = static_cast<GamepadButton>(i);
            bool currentPressed = m_currentGamepadState.buttons[i];
            bool previousPressed = m_previousGamepadState.buttons[i];
            
            // Call callback on state change
            if (currentPressed != previousPressed) {
                m_buttonCallback(static_cast<int>(m_gamepadId), button, currentPressed);
            }
        }
    }

    void WindowsGamepad::processAxisStates() {
        if (!m_axisCallback) {
            return;
        }

        for (size_t i = 0; i < static_cast<size_t>(GamepadAxis::COUNT); ++i) {
            GamepadAxis axis = static_cast<GamepadAxis>(i);
            float currentValue = m_currentGamepadState.axes[i];
            float previousValue = m_previousGamepadState.axes[i];
            
            // Call callback if value changed significantly
            float delta = std::abs(currentValue - previousValue);
            if (delta > 0.001f) {
                m_axisCallback(static_cast<int>(m_gamepadId), axis, currentValue);
            }
        }
    }

    bool WindowsGamepad::hasStateChanged() const {
        return m_currentState.dwPacketNumber != m_previousState.dwPacketNumber;
    }

    // IGamepad interface implementation
    bool WindowsGamepad::IsConnected() const {
        return m_connected;
    }

    const GamepadState& WindowsGamepad::GetState() const {
        return m_currentGamepadState;
    }

    const GamepadState& WindowsGamepad::GetPreviousState() const {
        return m_previousGamepadState;
    }

    int WindowsGamepad::GetGamepadId() const {
        return static_cast<int>(m_gamepadId);
    }

    std::string WindowsGamepad::GetName() const {
        return "XInput Controller " + std::to_string(m_gamepadId);
    }

    std::string WindowsGamepad::GetGUID() const {
        // Generate a simple GUID based on XInput ID
        return "xinput-" + std::to_string(m_gamepadId);
    }

    bool WindowsGamepad::IsButtonPressed(GamepadButton button) const {
        size_t index = static_cast<size_t>(button);
        return index < static_cast<size_t>(GamepadButton::COUNT) ? 
               m_currentGamepadState.buttons[index] : false;
    }

    bool WindowsGamepad::IsButtonReleased(GamepadButton button) const {
        return !IsButtonPressed(button);
    }

    bool WindowsGamepad::IsButtonDown(GamepadButton button) const {
        return IsButtonPressed(button);
    }

    bool WindowsGamepad::IsButtonUp(GamepadButton button) const {
        return !IsButtonPressed(button);
    }

    bool WindowsGamepad::WasButtonJustPressed(GamepadButton button) const {
        size_t index = static_cast<size_t>(button);
        if (index >= static_cast<size_t>(GamepadButton::COUNT)) {
            return false;
        }
        return m_currentGamepadState.buttons[index] && !m_previousGamepadState.buttons[index];
    }

    bool WindowsGamepad::WasButtonJustReleased(GamepadButton button) const {
        size_t index = static_cast<size_t>(button);
        if (index >= static_cast<size_t>(GamepadButton::COUNT)) {
            return false;
        }
        return !m_currentGamepadState.buttons[index] && m_previousGamepadState.buttons[index];
    }

    float WindowsGamepad::GetAxisValue(GamepadAxis axis) const {
        size_t index = static_cast<size_t>(axis);
        return index < static_cast<size_t>(GamepadAxis::COUNT) ? 
               m_currentGamepadState.axes[index] : 0.0f;
    }

    float WindowsGamepad::GetAxisDelta(GamepadAxis axis) const {
        size_t index = static_cast<size_t>(axis);
        if (index >= static_cast<size_t>(GamepadAxis::COUNT)) {
            return 0.0f;
        }
        return m_currentGamepadState.axes[index] - m_previousGamepadState.axes[index];
    }

    bool WindowsGamepad::IsAnyButtonPressed() const {
        for (size_t i = 0; i < static_cast<size_t>(GamepadButton::COUNT); ++i) {
            if (m_currentGamepadState.buttons[i]) {
                return true;
            }
        }
        return false;
    }

    GamepadButton WindowsGamepad::GetLastPressedButton() const {
        // Return the first pressed button found (could be enhanced to track actual last pressed)
        for (size_t i = 0; i < static_cast<size_t>(GamepadButton::COUNT); ++i) {
            if (WasButtonJustPressed(static_cast<GamepadButton>(i))) {
                return static_cast<GamepadButton>(i);
            }
        }
        return GamepadButton::COUNT; // Invalid button
    }

    bool WindowsGamepad::SupportsVibration() const {
        return m_vibrationEnabled;
    }

    void WindowsGamepad::SetVibration(float leftMotor, float rightMotor) {
        if (!m_vibrationEnabled || !m_connected) {
            return;
        }

        // Clamp values to valid range
        leftMotor = std::clamp(leftMotor, 0.0f, 1.0f);
        rightMotor = std::clamp(rightMotor, 0.0f, 1.0f);

        // Convert to XInput format (0-65535)
        m_vibrationState.wLeftMotorSpeed = static_cast<WORD>(leftMotor * 65535.0f);
        m_vibrationState.wRightMotorSpeed = static_cast<WORD>(rightMotor * 65535.0f);

        XInputSetState(m_gamepadId, &m_vibrationState);
    }

    void WindowsGamepad::StopVibration() {
        m_vibrationState.wLeftMotorSpeed = 0;
        m_vibrationState.wRightMotorSpeed = 0;
        if (m_connected) {
            XInputSetState(m_gamepadId, &m_vibrationState);
        }
    }

    void WindowsGamepad::SetDeadzone(GamepadAxis axis, float deadzone) {
        size_t index = static_cast<size_t>(axis);
        if (index < static_cast<size_t>(GamepadAxis::COUNT)) {
            m_deadzones[index] = std::clamp(deadzone, 0.0f, 1.0f);
        }
    }

    float WindowsGamepad::GetDeadzone(GamepadAxis axis) const {
        size_t index = static_cast<size_t>(axis);
        return index < static_cast<size_t>(GamepadAxis::COUNT) ? 
               m_deadzones[index] : 0.0f;
    }

    void WindowsGamepad::SetAxisSensitivity(GamepadAxis axis, float sensitivity) {
        size_t index = static_cast<size_t>(axis);
        if (index < static_cast<size_t>(GamepadAxis::COUNT)) {
            m_sensitivities[index] = std::max(0.0f, sensitivity);
        }
    }

    float WindowsGamepad::GetAxisSensitivity(GamepadAxis axis) const {
        size_t index = static_cast<size_t>(axis);
        return index < static_cast<size_t>(GamepadAxis::COUNT) ? 
               m_sensitivities[index] : 1.0f;
    }

    void WindowsGamepad::SetButtonCallback(GamepadButtonCallback callback) {
        m_buttonCallback = callback;
    }

    void WindowsGamepad::SetAxisCallback(GamepadAxisCallback callback) {
        m_axisCallback = callback;
    }

    const GamepadButtonCallback& WindowsGamepad::GetButtonCallback() const {
        return m_buttonCallback;
    }

    const GamepadAxisCallback& WindowsGamepad::GetAxisCallback() const {
        return m_axisCallback;
    }

    bool WindowsGamepad::IsXInputConnected() const {
        XINPUT_STATE state;
        return XInputGetState(m_gamepadId, &state) == ERROR_SUCCESS;
    }

} // namespace Input