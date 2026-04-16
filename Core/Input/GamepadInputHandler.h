#pragma once

#include "GamepadInterface.h"
#include <string>

namespace Input {

/** @brief Input handler interface for gamepad input processing. */
class IGamepadInputHandler
{
public:
    virtual ~IGamepadInputHandler() = default;

    // ── Button/Axis Handling ──────────────────────────────────────────────────

    /** @param gamepadId The gamepad index.
     *  @param button The button that changed state.
     *  @param pressed Whether the button is currently pressed.
     *  @param justPressed True on the first frame the button was pressed.
     *  @param justReleased True on the first frame the button was released.
     *  Return true if the input was handled and should not be passed to the next handler. */
    virtual bool HandleButtonInput(int gamepadId, GamepadButton button, bool pressed, bool justPressed, bool justReleased) = 0;

    /** @param gamepadId The gamepad index.
     *  @param axis The axis that changed.
     *  @param value Current axis value.
     *  @param delta Change from last frame.
     *  Return true if the input was handled and should not be passed to the next handler. */
    virtual bool HandleAxisInput(int gamepadId, GamepadAxis axis, float value, float delta) = 0;

    // ── Lifecycle ────────────────────────────────────────────────────────────

    /** @param deltaTime Time since last frame in seconds. */
    virtual void OnUpdate(double deltaTime) {}

    /** @param gamepadId The gamepad that connected. */
    virtual void OnGamepadConnected(int gamepadId) {}

    /** @param gamepadId The gamepad that disconnected. */
    virtual void OnGamepadDisconnected(int gamepadId) {}

    // ── Accessors ────────────────────────────────────────────────────────────

    virtual int GetPriority() const { return 0; }
    virtual bool IsActive() const { return true; }
    virtual const char* GetName() const { return "Unknown Handler"; }
};

/** @brief Base implementation of IGamepadInputHandler with common functionality. */
class GamepadInputHandler : public IGamepadInputHandler
{
private:
    bool        m_active   = true;
    int         m_priority = 0;
    std::string m_name;

public:
    explicit GamepadInputHandler(const std::string& name = "BaseHandler", int priority = 0, bool active = true)
        : m_active(active), m_priority(priority), m_name(name) {}

    // ── IGamepadInputHandler ─────────────────────────────────────────────────

    bool HandleButtonInput(int gamepadId, GamepadButton button, bool pressed, bool justPressed, bool justReleased) override
    {
        return false;
    }

    bool HandleAxisInput(int gamepadId, GamepadAxis axis, float value, float delta) override
    {
        return false;
    }

    // ── Accessors ────────────────────────────────────────────────────────────

    bool IsActive() const override { return m_active; }
    void SetActive(bool active) { m_active = active; }

    int GetPriority() const override { return m_priority; }
    void SetPriority(int priority) { m_priority = priority; }

    const char* GetName() const override { return m_name.c_str(); }
    void SetName(const std::string& name) { m_name = name; }
};

} // namespace Input
