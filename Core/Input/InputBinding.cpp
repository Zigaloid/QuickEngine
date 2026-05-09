#include "InputBinding.h"

namespace Input {

// ?? Mouse factory helpers ?????????????????????????????????????????????????????

InputBinding InputBinding::MouseButton_(std::string action, MouseButton btn, TriggerType trigger)
{
    InputBinding b;
    b.actionName  = std::move(action);
    b.sourceType  = InputSourceType::MouseButton;
    b.mouseButton = btn;
    b.triggerType = trigger;
    return b;
}

InputBinding InputBinding::MouseAxis_(std::string action, MouseAxis axis, float threshold, float deadzone)
{
    InputBinding b;
    b.actionName    = std::move(action);
    b.sourceType    = InputSourceType::MouseAxis;
    b.mouseAxis     = axis;
    b.triggerType   = TriggerType::Pressed; // axes use threshold crossing as "activation"
    b.axisThreshold = threshold;
    b.axisDeadzone  = deadzone;
    return b;
}

InputBinding InputBinding::MouseAxisContinuous(std::string action, MouseAxis axis, float threshold, float deadzone)
{
    InputBinding b;
    b.actionName    = std::move(action);
    b.sourceType    = InputSourceType::MouseAxis;
    b.mouseAxis     = axis;
    b.triggerType   = TriggerType::Continuous;
    b.axisThreshold = threshold;
    b.axisDeadzone  = deadzone;
    return b;
}

InputBinding InputBinding::GamepadButton_(std::string action, GamepadButton btn, TriggerType trigger, int gamepadId)
{
    InputBinding b;
    b.actionName    = std::move(action);
    b.sourceType    = InputSourceType::GamepadButton;
    b.gamepadButton = btn;
    b.triggerType   = trigger;
    b.gamepadId     = gamepadId;
    return b;
}

InputBinding InputBinding::GamepadAxis_(std::string action, GamepadAxis axis, float threshold, float deadzone, int gamepadId)
{
    InputBinding b;
    b.actionName    = std::move(action);
    b.sourceType    = InputSourceType::GamepadAxis;
    b.gamepadAxis   = axis;
    b.triggerType   = TriggerType::Pressed;
    b.axisThreshold = threshold;
    b.axisDeadzone  = deadzone;
    b.gamepadId     = gamepadId;
    return b;
}

InputBinding InputBinding::GamepadAxisContinuous(std::string action, GamepadAxis axis, float threshold, float deadzone, int gamepadId)
{
    InputBinding b;
    b.actionName    = std::move(action);
    b.sourceType    = InputSourceType::GamepadAxis;
    b.gamepadAxis   = axis;
    b.triggerType   = TriggerType::Continuous;
    b.axisThreshold = threshold;
    b.axisDeadzone  = deadzone;
    b.gamepadId     = gamepadId;
    return b;
}

// ?? Hold helpers ??????????????????????????????????????????????????????????????

InputBinding InputBinding::MouseHeld(std::string action, MouseButton btn, float holdDuration)
{
    InputBinding b;
    b.actionName    = std::move(action);
    b.sourceType    = InputSourceType::MouseButton;
    b.mouseButton   = btn;
    b.triggerType   = TriggerType::Held;
    b.holdDuration  = holdDuration;
    return b;
}

InputBinding InputBinding::GamepadHeld(std::string action, GamepadButton btn, float holdDuration, int gamepadId)
{
    InputBinding b;
    b.actionName    = std::move(action);
    b.sourceType    = InputSourceType::GamepadButton;
    b.gamepadButton = btn;
    b.triggerType   = TriggerType::Held;
    b.holdDuration  = holdDuration;
    b.gamepadId     = gamepadId;
    return b;
}

// ?? DoubleTap helpers ?????????????????????????????????????????????????????????

InputBinding InputBinding::MouseDoubleTap(std::string action, MouseButton btn, float tapWindow)
{
    InputBinding b;
    b.actionName  = std::move(action);
    b.sourceType  = InputSourceType::MouseButton;
    b.mouseButton = btn;
    b.triggerType = TriggerType::DoubleTap;
    b.tapWindow   = tapWindow;
    return b;
}

InputBinding InputBinding::GamepadDoubleTap(std::string action, GamepadButton btn, float tapWindow, int gamepadId)
{
    InputBinding b;
    b.actionName    = std::move(action);
    b.sourceType    = InputSourceType::GamepadButton;
    b.gamepadButton = btn;
    b.triggerType   = TriggerType::DoubleTap;
    b.tapWindow     = tapWindow;
    b.gamepadId     = gamepadId;
    return b;
}

// ?? Combo helpers ?????????????????????????????????????????????????????????????

InputBinding InputBinding::MouseCombo(std::string action, MouseButton triggerBtn, std::vector<Modifier> modifiers)
{
    InputBinding b;
    b.actionName      = std::move(action);
    b.sourceType      = InputSourceType::MouseButton;
    b.mouseButton     = triggerBtn;
    b.triggerType     = TriggerType::Combo;
    b.comboModifiers  = std::move(modifiers);
    return b;
}

InputBinding InputBinding::GamepadCombo(std::string action, GamepadButton triggerBtn, std::vector<Modifier> modifiers, int gamepadId)
{
    InputBinding b;
    b.actionName      = std::move(action);
    b.sourceType      = InputSourceType::GamepadButton;
    b.gamepadButton   = triggerBtn;
    b.triggerType     = TriggerType::Combo;
    b.comboModifiers  = std::move(modifiers);
    b.gamepadId       = gamepadId;
    return b;
}

} // namespace Input
