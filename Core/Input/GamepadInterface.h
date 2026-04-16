#pragma once

#include <string>
#include <functional>
#include <array>

namespace Input {

// ── Enumerations ──────────────────────────────────────────────────────────────

/** @brief Gamepad button enumeration. */
enum class GamepadButton : int
{
    A           = 0,    // Cross on PlayStation
    B           = 1,    // Circle on PlayStation
    X           = 2,    // Square on PlayStation
    Y           = 3,    // Triangle on PlayStation
    LeftBumper  = 4,    // L1 on PlayStation
    RightBumper = 5,    // R1 on PlayStation
    Back        = 6,    // Share on PlayStation
    Start       = 7,    // Options on PlayStation
    Guide       = 8,    // PS button on PlayStation
    LeftThumb   = 9,    // L3 on PlayStation
    RightThumb  = 10,   // R3 on PlayStation
    DPadUp      = 11,
    DPadRight   = 12,
    DPadDown    = 13,
    DPadLeft    = 14,
    COUNT       = 15
};

/** @brief Gamepad axis enumeration. */
enum class GamepadAxis : int
{
    LeftX        = 0,   // Left stick X axis
    LeftY        = 1,   // Left stick Y axis
    RightX       = 2,   // Right stick X axis
    RightY       = 3,   // Right stick Y axis
    LeftTrigger  = 4,   // L2 on PlayStation
    RightTrigger = 5,   // R2 on PlayStation
    COUNT        = 6
};

// ── State Structures ──────────────────────────────────────────────────────────

/** @brief Snapshot of all gamepad input state for a single frame. */
struct GamepadState
{
    std::array<bool,  static_cast<size_t>(GamepadButton::COUNT)> buttons{};
    std::array<float, static_cast<size_t>(GamepadAxis::COUNT)>   axes{};
    bool connected = false;

    bool IsButtonPressed(GamepadButton button) const { return buttons[static_cast<size_t>(button)]; }
    float GetAxis(GamepadAxis axis) const { return axes[static_cast<size_t>(axis)]; }

    bool IsAPressed() const { return IsButtonPressed(GamepadButton::A); }
    bool IsBPressed() const { return IsButtonPressed(GamepadButton::B); }
    bool IsXPressed() const { return IsButtonPressed(GamepadButton::X); }
    bool IsYPressed() const { return IsButtonPressed(GamepadButton::Y); }

    float GetLeftStickX()    const { return GetAxis(GamepadAxis::LeftX); }
    float GetLeftStickY()    const { return GetAxis(GamepadAxis::LeftY); }
    float GetRightStickX()   const { return GetAxis(GamepadAxis::RightX); }
    float GetRightStickY()   const { return GetAxis(GamepadAxis::RightY); }
    float GetLeftTrigger()   const { return GetAxis(GamepadAxis::LeftTrigger); }
    float GetRightTrigger()  const { return GetAxis(GamepadAxis::RightTrigger); }
};

// ── Callbacks ─────────────────────────────────────────────────────────────────

using GamepadConnectedCallback    = std::function<void(int gamepadId)>;
using GamepadDisconnectedCallback = std::function<void(int gamepadId)>;
using GamepadButtonCallback       = std::function<void(int gamepadId, GamepadButton button, bool pressed)>;
using GamepadAxisCallback         = std::function<void(int gamepadId, GamepadAxis axis, float value)>;

// ── Interface ─────────────────────────────────────────────────────────────────

/** @brief Abstract gamepad interface. */
class IGamepad
{
public:
    virtual ~IGamepad() = default;

    // ── Lifecycle ────────────────────────────────────────────────────────────

    virtual bool Initialize() = 0;
    virtual void Update()     = 0;
    virtual void Shutdown()   = 0;

    // ── State Queries ────────────────────────────────────────────────────────

    virtual bool IsConnected()                    const = 0;
    virtual const GamepadState& GetState()        const = 0;
    virtual const GamepadState& GetPreviousState() const = 0;

    virtual int         GetGamepadId() const = 0;
    virtual std::string GetName()      const = 0;
    virtual std::string GetGUID()      const = 0;

    virtual bool IsButtonPressed(GamepadButton button)     const = 0;
    virtual bool IsButtonReleased(GamepadButton button)    const = 0;
    virtual bool IsButtonDown(GamepadButton button)        const = 0;
    virtual bool IsButtonUp(GamepadButton button)          const = 0;
    virtual bool WasButtonJustPressed(GamepadButton button)  const = 0;
    virtual bool WasButtonJustReleased(GamepadButton button) const = 0;

    virtual float GetAxisValue(GamepadAxis axis) const = 0;
    virtual float GetAxisDelta(GamepadAxis axis) const = 0;

    virtual bool          IsAnyButtonPressed()       const = 0;
    virtual GamepadButton GetLastPressedButton()      const = 0;

    // ── Vibration ────────────────────────────────────────────────────────────

    virtual bool SupportsVibration()                              const = 0;
    virtual void SetVibration(float leftMotor, float rightMotor)        = 0;
    virtual void StopVibration()                                        = 0;

    // ── Configuration ────────────────────────────────────────────────────────

    virtual void  SetDeadzone(GamepadAxis axis, float deadzone)              = 0;
    virtual float GetDeadzone(GamepadAxis axis)                         const = 0;
    virtual void  SetAxisSensitivity(GamepadAxis axis, float sensitivity)    = 0;
    virtual float GetAxisSensitivity(GamepadAxis axis)                  const = 0;

    // ── Callbacks ────────────────────────────────────────────────────────────

    virtual void SetButtonCallback(GamepadButtonCallback callback) = 0;
    virtual void SetAxisCallback(GamepadAxisCallback callback)     = 0;
    virtual const GamepadButtonCallback& GetButtonCallback() const = 0;
    virtual const GamepadAxisCallback&   GetAxisCallback()   const = 0;

    // ── Convenience ──────────────────────────────────────────────────────────

    virtual float GetLeftStickX()   const { return GetAxisValue(GamepadAxis::LeftX); }
    virtual float GetLeftStickY()   const { return GetAxisValue(GamepadAxis::LeftY); }
    virtual float GetRightStickX()  const { return GetAxisValue(GamepadAxis::RightX); }
    virtual float GetRightStickY()  const { return GetAxisValue(GamepadAxis::RightY); }
    virtual float GetLeftTrigger()  const { return GetAxisValue(GamepadAxis::LeftTrigger); }
    virtual float GetRightTrigger() const { return GetAxisValue(GamepadAxis::RightTrigger); }
};

// ── Configuration ─────────────────────────────────────────────────────────────

/** @brief Configuration parameters for gamepad input. */
struct GamepadConfig
{
    float deadzone            = 0.1f;   // Default deadzone for analog sticks
    float triggerDeadzone     = 0.05f;  // Default deadzone for triggers
    float sensitivity         = 1.0f;   // Default sensitivity multiplier
    bool  enableVibration     = true;   // Enable vibration if supported
    bool  autoDetectControllers = true; // Automatically detect new controllers
    int   maxControllers      = 4;      // Maximum number of controllers to support

    std::array<float, static_cast<size_t>(GamepadAxis::COUNT)> axisDeadzones{};
    std::array<float, static_cast<size_t>(GamepadAxis::COUNT)> axisSensitivities{};

    GamepadConfig()
    {
        for (size_t i = 0; i < static_cast<size_t>(GamepadAxis::COUNT); ++i)
        {
            if (i == static_cast<size_t>(GamepadAxis::LeftTrigger) ||
                i == static_cast<size_t>(GamepadAxis::RightTrigger))
            {
                axisDeadzones[i] = triggerDeadzone;
            }
            else
            {
                axisDeadzones[i] = deadzone;
            }
            axisSensitivities[i] = sensitivity;
        }
    }
};

// ── Helper Functions ──────────────────────────────────────────────────────────

const char* GetButtonName(GamepadButton button);
const char* GetAxisName(GamepadAxis axis);
float ApplyDeadzone(float value, float deadzone);
float ApplySensitivity(float value, float sensitivity);

} // namespace Input
