#include "GamepadInterface.h"

namespace Input {

float ApplyDeadzone(float value, float deadzone)
{
    if (std::abs(value) < deadzone)
        return 0.0f;
    // Remap the value to use the full range after the deadzone
    float sign = (value >= 0.0f) ? 1.0f : -1.0f;
    return sign * (std::abs(value) - deadzone) / (1.0f - deadzone);
}

}