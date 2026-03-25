#pragma once

#include "MouseInterface.h"
#include <string>

namespace Input {

    // Input handler interface for mouse input processing
    class IMouseInputHandler {
    public:
        virtual ~IMouseInputHandler() = default;

        // Button input handling
        // Return true if the input was handled and should not be passed to the next handler
        virtual bool HandleButtonInput(MouseButton button, bool pressed, bool justPressed, bool justReleased, double x, double y) = 0;

        // Movement input handling
        // Return true if the input was handled and should not be passed to the next handler
        virtual bool HandleMovementInput(double x, double y, float deltaX, float deltaY) = 0;

        // Scroll input handling
        // Return true if the input was handled and should not be passed to the next handler
        virtual bool HandleScrollInput(float deltaX, float deltaY) = 0;

        // Called at the beginning of each update frame
        virtual void OnUpdate(double deltaTime) {}

        // Called when mouse enters the window area
        virtual void OnMouseEnter() {}

        // Called when mouse leaves the window area
        virtual void OnMouseLeave() {}

        // Handler priority (higher values = higher priority, called first)
        virtual int GetPriority() const { return 0; }

        // Whether this handler is currently active
        virtual bool IsEnabled() const { return true; }

        // Optional handler name for debugging
        virtual const char* GetName() const { return "Unknown Handler"; }
    };

    // Base implementation with common functionality
    class MouseInputHandler : public IMouseInputHandler {
    private:
        bool m_active;
        int m_priority;
        std::string m_name;

    public:
        MouseInputHandler(const std::string& name = "BaseHandler", int priority = 0, bool active = true)
            : m_active(active), m_priority(priority), m_name(name) {}

        // IMouseInputHandler interface
        bool HandleButtonInput(MouseButton button, bool pressed, bool justPressed, bool justReleased, double x, double y) override {
            return false; // Base implementation doesn't handle anything
        }

        bool HandleMovementInput(double x, double y, float deltaX, float deltaY) override {
            return false; // Base implementation doesn't handle anything
        }

        bool HandleScrollInput(float deltaX, float deltaY) override {
            return false; // Base implementation doesn't handle anything
        }

        // Accessor methods
        bool IsEnabled() const override { return m_active; }
        void SetEnabled(bool active) { m_active = active; }

        int GetPriority() const override { return m_priority; }
        void SetPriority(int priority) { m_priority = priority; }

        const char* GetName() const override { return m_name.c_str(); }
        void SetName(const std::string& name) { m_name = name; }
    };

} // namespace Input