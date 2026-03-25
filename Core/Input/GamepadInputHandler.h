#pragma once

#include "GamepadInterface.h"

namespace Input {

    // Input handler interface for gamepad input processing
    class IGamepadInputHandler {
    public:
        virtual ~IGamepadInputHandler() = default;

        // Button input handling
        // Return true if the input was handled and should not be passed to the next handler
        virtual bool HandleButtonInput(int gamepadId, GamepadButton button, bool pressed, bool justPressed, bool justReleased) = 0;

        // Axis input handling
        // Return true if the input was handled and should not be passed to the next handler
        virtual bool HandleAxisInput(int gamepadId, GamepadAxis axis, float value, float delta) = 0;

        // Called at the beginning of each update frame
        virtual void OnUpdate(double deltaTime) {}

        // Called when a gamepad is connected
        virtual void OnGamepadConnected(int gamepadId) {}

        // Called when a gamepad is disconnected
        virtual void OnGamepadDisconnected(int gamepadId) {}

        // Handler priority (higher values = higher priority, called first)
        virtual int GetPriority() const { return 0; }

        // Whether this handler is currently active
        virtual bool IsActive() const { return true; }

        // Optional handler name for debugging
        virtual const char* GetName() const { return "Unknown Handler"; }
    };

    // Base implementation with common functionality
    class GamepadInputHandler : public IGamepadInputHandler {
    private:
        bool m_active;
        int m_priority;
        std::string m_name;

    public:
        GamepadInputHandler(const std::string& name = "BaseHandler", int priority = 0, bool active = true)
            : m_active(active), m_priority(priority), m_name(name) {}

        // IGamepadInputHandler interface
        bool HandleButtonInput(int gamepadId, GamepadButton button, bool pressed, bool justPressed, bool justReleased) override {
            return false; // Base implementation doesn't handle anything
        }

        bool HandleAxisInput(int gamepadId, GamepadAxis axis, float value, float delta) override {
            return false; // Base implementation doesn't handle anything
        }

        // Accessor methods
        bool IsActive() const override { return m_active; }
        void SetActive(bool active) { m_active = active; }

        int GetPriority() const override { return m_priority; }
        void SetPriority(int priority) { m_priority = priority; }

        const char* GetName() const override { return m_name.c_str(); }
        void SetName(const std::string& name) { m_name = name; }
    };

} // namespace Input