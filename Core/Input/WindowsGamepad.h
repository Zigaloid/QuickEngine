#pragma once

#include "GamepadInterface.h"

#define NOMINMAX
#include <Windows.h>
#include <Xinput.h>

#include <string>
#include <array>

#pragma comment(lib, "xinput.lib")

namespace Input {

/** @brief XInput-based gamepad implementation for Windows. */
class WindowsGamepad : public IGamepad
{
private:
    // ── XInput State ─────────────────────────────────────────────────────────

    DWORD            m_gamepadId;
    XINPUT_STATE     m_currentState;
    XINPUT_STATE     m_previousState;
    XINPUT_VIBRATION m_vibrationState;

    GamepadState m_currentGamepadState;
    GamepadState m_previousGamepadState;
    bool         m_connected         = false;
    bool         m_initialized       = false;

    // ── Configuration ────────────────────────────────────────────────────────

    std::array<float, static_cast<size_t>(GamepadAxis::COUNT)> m_deadzones;
    std::array<float, static_cast<size_t>(GamepadAxis::COUNT)> m_sensitivities;
    bool m_vibrationEnabled = true;

    // ── Callbacks ────────────────────────────────────────────────────────────

    GamepadButtonCallback m_buttonCallback;
    GamepadAxisCallback   m_axisCallback;

    // ── Private Helpers ──────────────────────────────────────────────────────

    void  UpdateGamepadState();
    void  ProcessButtonStates();
    void  ProcessAxisStates();
    float NormalizeAxisValue(SHORT rawValue, float deadzone) const;
    float NormalizeTriggerValue(BYTE rawValue, float deadzone) const;
    void  MapXInputToGamepadState(const XINPUT_GAMEPAD& xinputState);
    bool  HasStateChanged() const;

    // ── Button Mapping ───────────────────────────────────────────────────────

    static constexpr std::array<WORD, static_cast<size_t>(GamepadButton::COUNT)> XINPUT_BUTTON_MAP =
    {
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



    // ── IGamepad ─────────────────────────────────────────────────────────────

    bool Initialize() override;
    void Update()     override;
    void Shutdown()   override;

    bool IsConnected()                     const override;
    const GamepadState& GetState()         const override;
    const GamepadState& GetPreviousState() const override;

    int         GetGamepadId() const override;
    std::string GetName()      const override;
    std::string GetGUID()      const override;

    bool IsButtonPressed(GamepadButton button)     const override;
    bool IsButtonReleased(GamepadButton button)    const override;
    bool IsButtonDown(GamepadButton button)        const override;
    bool IsButtonUp(GamepadButton button)          const override;
    bool WasButtonJustPressed(GamepadButton button)  const override;
    bool WasButtonJustReleased(GamepadButton button) const override;

    float GetAxisValue(GamepadAxis axis) const override;
    float GetAxisDelta(GamepadAxis axis) const override;

    bool          IsAnyButtonPressed()      const override;
    GamepadButton GetLastPressedButton()    const override;

    bool SupportsVibration()                              const override;
    void SetVibration(float leftMotor, float rightMotor)        override;
    void StopVibration()                                        override;

    void  SetDeadzone(GamepadAxis axis, float deadzone)              override;
    float GetDeadzone(GamepadAxis axis)                         const override;
    void  SetAxisSensitivity(GamepadAxis axis, float sensitivity)    override;
    float GetAxisSensitivity(GamepadAxis axis)                  const override;

    void SetButtonCallback(GamepadButtonCallback callback)       override;
    void SetAxisCallback(GamepadAxisCallback callback)           override;
    const GamepadButtonCallback& GetButtonCallback() const override;
    const GamepadAxisCallback&   GetAxisCallback()   const override;

    // ── Windows-Specific ─────────────────────────────────────────────────────

    DWORD                GetXInputId()    const { return m_gamepadId; }
    const XINPUT_STATE&  GetXInputState() const { return m_currentState; }
    bool                 IsXInputConnected() const;

    /// Debug helpers
    static void DebugListXInputSlots();
    static void DebugListRawInputDevices();
};

} // namespace Input
