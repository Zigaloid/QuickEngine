#include "GamepadInterface.h"

#include <cmath>

namespace Input {

float ApplyDeadzone(float value, float deadzone)
{
    if (std::abs(value) < deadzone)
        return 0.0f;
    float sign = (value >= 0.0f) ? 1.0f : -1.0f;
    return sign * (std::abs(value) - deadzone) / (1.0f - deadzone);
}

float ApplySensitivity(float value, float sensitivity)
{
    return value * sensitivity;
}

} // namespace Input
