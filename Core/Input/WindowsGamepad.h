#pragma once
#include "GamepadInterface.h"

#define NOMINMAX
#include <Windows.h>

#include <string>
#include <array>
#include <Xinput.h>

#pragma comment(lib, "xinput.lib")

namespace Input {

    class WindowsGamepad : public IGamepad {
    private:
        // XInput state
        DWORD m_gamepadId;
        XINPUT_STATE m_currentState;
        XINPUT_STATE m_previousState;
        XINPUT_VIBRATION m_vibrationState;
        
        // Internal state tracking
        GamepadState m_currentGamepadState;
        GamepadState m_previousGamepadState;
        bool m_connected;
        bool m_initialized;
        
        // Configuration
        std::array<float, static_cast<size_t>(GamepadAxis::COUNT)> m_deadzones;
        std::array<float, static_cast<size_t>(GamepadAxis::COUNT)> m_sensitivities;
        bool m_vibrationEnabled;
        
        // Callbacks
        GamepadButtonCallback m_buttonCallback;
        GamepadAxisCallback m_axisCallback;
        
        // Internal helper methods
        void updateGamepadState();
        void processButtonStates();
        void processAxisStates();
        float normalizeAxisValue(SHORT rawValue, float deadzone) const;
        float normalizeTriggerValue(BYTE rawValue, float deadzone) const;
        void mapXInputToGamepadState(const XINPUT_GAMEPAD& xinputState);
        bool hasStateChanged() const;
        
        // Button mapping
        static constexpr std::array<WORD, static_cast<size_t>(GamepadButton::COUNT)> XINPUT_BUTTON_MAP = {
            XINPUT_GAMEPAD_A,              // GamepadButton::A
            XINPUT_GAMEPAD_B,              // GamepadButton::B  
            XINPUT_GAMEPAD_X,              // GamepadButton::X
            XINPUT_GAMEPAD_Y,              // GamepadButton::Y
            XINPUT_GAMEPAD_LEFT_SHOULDER,  // GamepadButton::LeftBumper
            XINPUT_GAMEPAD_RIGHT_SHOULDER, // GamepadButton::RightBumper
            XINPUT_GAMEPAD_BACK,           // GamepadButton::Back
            XINPUT_GAMEPAD_START,          // GamepadButton::Start            
            XINPUT_GAMEPAD_LEFT_THUMB,     // GamepadButton::LeftThumb
            XINPUT_GAMEPAD_RIGHT_THUMB,    // GamepadButton::RightThumb
            XINPUT_GAMEPAD_DPAD_UP,        // GamepadButton::DPadUp
            XINPUT_GAMEPAD_DPAD_RIGHT,     // GamepadButton::DPadRight
            XINPUT_GAMEPAD_DPAD_DOWN,      // GamepadButton::DPadDown
            XINPUT_GAMEPAD_DPAD_LEFT       // GamepadButton::DPadLeft
        };

    public:
        explicit WindowsGamepad(DWORD gamepadId);
        virtual ~WindowsGamepad();

        // IGamepad interface implementation
        bool Initialize() override;
        void Update() override;
        void Shutdown() override;

        // State queries
        bool IsConnected() const override;
        const GamepadState& GetState() const override;
        const GamepadState& GetPreviousState() const override;
        
        // Gamepad information
        int GetGamepadId() const override;
        std::string GetName() const override;
        std::string GetGUID() const override;

        // Button state queries
        bool IsButtonPressed(GamepadButton button) const override;
        bool IsButtonReleased(GamepadButton button) const override;
        bool IsButtonDown(GamepadButton button) const override;
        bool IsButtonUp(GamepadButton button) const override;
        bool WasButtonJustPressed(GamepadButton button) const override;
        bool WasButtonJustReleased(GamepadButton button) const override;

        // Axis state queries
        float GetAxisValue(GamepadAxis axis) const override;
        float GetAxisDelta(GamepadAxis axis) const override;

        // Convenience methods
        bool IsAnyButtonPressed() const override;
        GamepadButton GetLastPressedButton() const override;

        // Vibration/Rumble
        bool SupportsVibration() const override;
        void SetVibration(float leftMotor, float rightMotor) override;
        void StopVibration() override;

        // Configuration
        void SetDeadzone(GamepadAxis axis, float deadzone) override;
        float GetDeadzone(GamepadAxis axis) const override;
        void SetAxisSensitivity(GamepadAxis axis, float sensitivity) override;
        float GetAxisSensitivity(GamepadAxis axis) const override;

        // Event callbacks
        void SetButtonCallback(GamepadButtonCallback callback) override;
        void SetAxisCallback(GamepadAxisCallback callback) override;
        const GamepadButtonCallback& GetButtonCallback() const override;
        const GamepadAxisCallback& GetAxisCallback() const override;

        // Windows-specific methods
        DWORD GetXInputId() const { return m_gamepadId; }
        const XINPUT_STATE& GetXInputState() const { return m_currentState; }
        bool IsXInputConnected() const;
    };

} // namespace Input