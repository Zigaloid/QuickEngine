#pragma once

#include "MouseInterface.h"
#include "GamepadInterface.h"

#include <string>
#include <vector>

namespace Input {

// ?? Source Type ???????????????????????????????????????????????????????????????

/** @brief Discriminator for the physical input source of a binding. */
enum class InputSourceType
{
    MouseButton,
    MouseAxis,
    GamepadButton,
    GamepadAxis,
};

// ?? Trigger Type ??????????????????????????????????????????????????????????????

/** @brief How the bound source causes the action to fire.
 *
 *  | Trigger    | Started          | Ongoing            | Completed                   | Canceled                     |
 *  |------------|------------------|--------------------|-----------------------------|------------------------------|
 *  | Pressed    | justActivated    | —                  | —                           | —                            |
 *  | Released   | —                | —                  | justDeactivated             | —                            |
 *  | Continuous | justActivated    | every frame active | justDeactivated             | —                            |
 *  | Held       | justActivated    | each frame (timer) | holdDuration reached        | released before holdDuration |
 *  | DoubleTap  | —                | —                  | 2nd tap within tapWindow    | —                            |
 *  | Combo      | all mods held on justActivated | —   | —                           | —                            |
 */
enum class TriggerType
{
    Pressed,    ///< Fires Started on the first frame the source is active.
    Released,   ///< Fires Completed on the first frame the source becomes inactive.
    /** Fires Started on activation, Ongoing every frame while active (carrying the current
     *  axis/button value), and Completed on deactivation.
     *  The natural choice for analogue axes (joystick, mouse movement). */
    Continuous,
    Held,       ///< Fires Started immediately, Ongoing each frame, Completed after holdDuration.
    DoubleTap,  ///< Fires Completed when the source is activated twice within tapWindow seconds.
    Combo,      ///< Fires Started when primary source is activated while all modifiers are active.
};

// ?? Input Binding ?????????????????????????????????????????????????????????????

/** @brief Maps a physical input (+ trigger behaviour) to a named action.
 *
 *  Use the static factory helpers for the most readable construction:
 *  @code
 *  auto b = InputBinding::GamepadButton_("Jump",   GamepadButton::A);
 *  auto h = InputBinding::GamepadHeld   ("Crouch", GamepadButton::B, 0.4f);
 *  auto d = InputBinding::MouseDoubleTap("Dodge",  MouseButton::Left);
 *  auto c = InputBinding::MouseCombo    ("Block",  MouseButton::Right,
 *                { { InputSourceType::MouseButton, MouseButton::Left } });
 *  @endcode
 */
struct InputBinding
{
    // ?? Action ????????????????????????????????????????????????????????????????

    std::string actionName;

    // ?? Source ????????????????????????????????????????????????????????????????

    InputSourceType sourceType    = InputSourceType::MouseButton;

    MouseButton     mouseButton   = MouseButton::Left;
    MouseAxis       mouseAxis     = MouseAxis::X;
    GamepadButton   gamepadButton = GamepadButton::A;
    GamepadAxis     gamepadAxis   = GamepadAxis::LeftX;

    /** Gamepad device index; -1 means any connected gamepad. */
    int             gamepadId     = -1;

    /** Minimum absolute axis value to consider the axis "active". */
    float           axisThreshold = 0.5f;
    float           axisDeadzone  = 0.1f;

    // ?? Trigger ???????????????????????????????????????????????????????????????

    TriggerType     triggerType   = TriggerType::Pressed;

    /** Seconds the source must be held before Completed fires (Held trigger). */
    float           holdDuration  = 0.5f;

    /** Seconds between activations that still count as a double-tap. */
    float           tapWindow     = 0.3f;

    // ?? Combo Modifiers ???????????????????????????????????????????????????????

    /** A modifier that must be active simultaneously with the primary source.
     *  Supported source types: MouseButton and GamepadButton. */
    struct Modifier
    {
        InputSourceType sourceType    = InputSourceType::MouseButton;
        MouseButton     mouseButton   = MouseButton::Left;
        GamepadButton   gamepadButton = GamepadButton::A;
        int             gamepadId     = -1; ///< -1 = same gamepad as the primary binding
    };

    /** All modifiers must be active when the primary source fires (Combo trigger only). */
    std::vector<Modifier> comboModifiers;

    // ?? Factory helpers ???????????????????????????????????????????????????????

    static InputBinding MouseButton_(std::string action, MouseButton btn,
                                     TriggerType trigger = TriggerType::Pressed);

    static InputBinding MouseAxis_(std::string action, MouseAxis axis,
                                   float threshold = 0.1f, float deadzone = 0.05f);

    /** @brief Continuous mouse axis binding — fires Ongoing every frame while active. */
    static InputBinding MouseAxisContinuous(std::string action, MouseAxis axis,
                                            float threshold = 0.05f, float deadzone = 0.02f);

    static InputBinding GamepadButton_(std::string action, GamepadButton btn,
                                       TriggerType trigger = TriggerType::Pressed,
                                       int gamepadId = -1);

    static InputBinding GamepadAxis_(std::string action, GamepadAxis axis,
                                     float threshold = 0.1f, float deadzone = 0.05f,
                                     int gamepadId = -1);

    /** @brief Continuous gamepad axis binding — fires Ongoing every frame while active. */
    static InputBinding GamepadAxisContinuous(std::string action, GamepadAxis axis,
                                              float threshold = 0.05f, float deadzone = 0.1f,
                                              int gamepadId = -1);

    static InputBinding MouseHeld(std::string action, MouseButton btn,
                                  float holdDuration = 0.5f);

    static InputBinding GamepadHeld(std::string action, GamepadButton btn,
                                    float holdDuration = 0.5f, int gamepadId = -1);

    static InputBinding MouseDoubleTap(std::string action, MouseButton btn,
                                       float tapWindow = 0.3f);

    static InputBinding GamepadDoubleTap(std::string action, GamepadButton btn,
                                         float tapWindow = 0.3f, int gamepadId = -1);

    /** @brief Combo: primary mouse button pressed while all modifiers are held.
     *  @param trigger Must be TriggerType::Combo (default). */
    static InputBinding MouseCombo(std::string action, MouseButton triggerBtn,
                                   std::vector<Modifier> modifiers);

    /** @brief Combo: primary gamepad button pressed while all modifiers are held. */
    static InputBinding GamepadCombo(std::string action, GamepadButton triggerBtn,
                                     std::vector<Modifier> modifiers, int gamepadId = -1);
};

} // namespace Input
