#pragma once

#include "MouseInterface.h"

#include <string>

namespace Input {

/** @brief Input handler interface for mouse input processing. */
class IMouseInputHandler
{
public:
    virtual ~IMouseInputHandler() = default;

    // ── Input Handling ───────────────────────────────────────────────────────

    /** Return true if the input was handled and should not be passed to the next handler. */
    virtual bool HandleButtonInput(MouseButton button, bool pressed, bool justPressed, bool justReleased, double x, double y) = 0;

    /** Return true if the input was handled and should not be passed to the next handler. */
    virtual bool HandleMovementInput(double x, double y, float deltaX, float deltaY) = 0;

    /** Return true if the input was handled and should not be passed to the next handler. */
    virtual bool HandleScrollInput(float deltaX, float deltaY) = 0;

    // ── Lifecycle ────────────────────────────────────────────────────────────

    virtual void OnUpdate(double deltaTime) {}
    virtual void OnMouseEnter() {}
    virtual void OnMouseLeave() {}

    // ── Accessors ────────────────────────────────────────────────────────────

    virtual int         GetPriority() const { return 0; }
    virtual bool        IsEnabled()   const { return true; }
    virtual const char* GetName()     const { return "Unknown Handler"; }
};

/** @brief Base implementation of IMouseInputHandler with common functionality. */
class MouseInputHandler : public IMouseInputHandler
{
private:
    bool        m_active   = true;
    int         m_priority = 0;
    std::string m_name;

public:
    explicit MouseInputHandler(const std::string& name = "BaseHandler", int priority = 0, bool active = true)
        : m_active(active), m_priority(priority), m_name(name) {}

    // ── IMouseInputHandler ───────────────────────────────────────────────────

    bool HandleButtonInput(MouseButton button, bool pressed, bool justPressed, bool justReleased, double x, double y) override
    {
        return false;
    }

    bool HandleMovementInput(double x, double y, float deltaX, float deltaY) override
    {
        return false;
    }

    bool HandleScrollInput(float deltaX, float deltaY) override
    {
        return false;
    }

    // ── Accessors ────────────────────────────────────────────────────────────

    bool IsEnabled() const override { return m_active; }
    void SetEnabled(bool active) { m_active = active; }

    int  GetPriority() const override { return m_priority; }
    void SetPriority(int priority) { m_priority = priority; }

    const char* GetName() const override { return m_name.c_str(); }
    void        SetName(const std::string& name) { m_name = name; }
};

} // namespace Input
