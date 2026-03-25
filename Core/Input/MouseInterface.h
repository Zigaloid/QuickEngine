#pragma once

#include <string>
#include <functional>
#include <array>

namespace Input {

    // Mouse button enumeration
    enum class MouseButton : int {
        Left = 0,
        Right = 1,
        Middle = 2,
        X1 = 3,         // Forward/Back button 1
        X2 = 4,         // Forward/Back button 2
        COUNT = 5
    };

    // Mouse axis enumeration for movement and scroll
    enum class MouseAxis : int {
        X = 0,              // Horizontal movement
        Y = 1,              // Vertical movement
        ScrollX = 2,        // Horizontal scroll wheel
        ScrollY = 3,        // Vertical scroll wheel
        COUNT = 4
    };

    // Mouse state structure
    struct MouseState {
        std::array<bool, static_cast<size_t>(MouseButton::COUNT)> buttons{};
        std::array<float, static_cast<size_t>(MouseAxis::COUNT)> axes{};
        double positionX = 0.0;
        double positionY = 0.0;
        bool connected = true;  // Mouse is typically always connected
        
        // Helper methods
        bool IsButtonPressed(MouseButton button) const {
            return buttons[static_cast<size_t>(button)];
        }
        
        float GetAxis(MouseAxis axis) const {
            return axes[static_cast<size_t>(axis)];
        }
        
        // Convenient access methods
        bool IsLeftPressed() const { return IsButtonPressed(MouseButton::Left); }
        bool IsRightPressed() const { return IsButtonPressed(MouseButton::Right); }
        bool IsMiddlePressed() const { return IsButtonPressed(MouseButton::Middle); }
        
        float GetMovementX() const { return GetAxis(MouseAxis::X); }
        float GetMovementY() const { return GetAxis(MouseAxis::Y); }
        float GetScrollX() const { return GetAxis(MouseAxis::ScrollX); }
        float GetScrollY() const { return GetAxis(MouseAxis::ScrollY); }
    };

    // Event callbacks
    using MouseConnectedCallback = std::function<void()>;
    using MouseDisconnectedCallback = std::function<void()>;
    using MouseButtonCallback = std::function<void(MouseButton button, bool pressed, double x, double y)>;
    using MouseMoveCallback = std::function<void(double x, double y, float deltaX, float deltaY)>;
    using MouseScrollCallback = std::function<void(float deltaX, float deltaY)>;

    // Abstract mouse interface
    class IMouse {
    public:
        virtual ~IMouse() = default;

        // Core functionality
        virtual bool Initialize() = 0;
        virtual void Update() = 0;
        virtual void Shutdown() = 0;

        // State queries
        virtual bool IsConnected() const = 0;
        virtual const MouseState& GetState() const = 0;
        virtual const MouseState& GetPreviousState() const = 0;
        
        // Mouse information
        virtual std::string GetName() const = 0;
        virtual std::string GetGUID() const = 0;

        // Position queries
        virtual double GetPositionX() const = 0;
        virtual double GetPositionY() const = 0;
        virtual void GetPosition(double& x, double& y) const = 0;

        // Movement queries (delta from previous frame)
        virtual float GetMovementX() const = 0;
        virtual float GetMovementY() const = 0;
        virtual void GetMovement(float& deltaX, float& deltaY) const = 0;

        // Button state queries
        virtual bool IsButtonPressed(MouseButton button) const = 0;
        virtual bool IsButtonReleased(MouseButton button) const = 0;
        virtual bool IsButtonDown(MouseButton button) const = 0;
        virtual bool IsButtonUp(MouseButton button) const = 0;
        
        // Was button just pressed this frame?
        virtual bool WasButtonJustPressed(MouseButton button) const = 0;
        // Was button just released this frame?
        virtual bool WasButtonJustReleased(MouseButton button) const = 0;

        // Scroll queries
        virtual float GetScrollX() const = 0;
        virtual float GetScrollY() const = 0;
        virtual void GetScroll(float& deltaX, float& deltaY) const = 0;

        // Convenience methods for common operations
        virtual bool IsAnyButtonPressed() const = 0;
        virtual MouseButton GetLastPressedButton() const = 0;

        // Cursor control
        virtual void SetPosition(double x, double y) = 0;
        virtual void SetCursorVisible(bool visible) = 0;
        virtual bool IsCursorVisible() const = 0;
        virtual void SetCursorLocked(bool locked) = 0;
        virtual bool IsCursorLocked() const = 0;

        // Configuration
        virtual void SetSensitivity(float sensitivity) = 0;
        virtual float GetSensitivity() const = 0;
        virtual void SetScrollSensitivity(float sensitivity) = 0;
        virtual float GetScrollSensitivity() const = 0;

        // Event callbacks
        virtual void SetButtonCallback(MouseButtonCallback callback) = 0;
        virtual void SetMoveCallback(MouseMoveCallback callback) = 0;
        virtual void SetScrollCallback(MouseScrollCallback callback) = 0;
        
        // Add getter methods for callbacks
        virtual const MouseButtonCallback& GetButtonCallback() const = 0;
        virtual const MouseMoveCallback& GetMoveCallback() const = 0;
        virtual const MouseScrollCallback& GetScrollCallback() const = 0;
        virtual void ResetScrollDeltas() = 0;

        // Convenient button access methods
        virtual bool IsLeftPressed() const { return IsButtonPressed(MouseButton::Left); }
        virtual bool IsRightPressed() const { return IsButtonPressed(MouseButton::Right); }
        virtual bool IsMiddlePressed() const { return IsButtonPressed(MouseButton::Middle); }
        virtual bool WasLeftJustPressed() const { return WasButtonJustPressed(MouseButton::Left); }
        virtual bool WasRightJustPressed() const { return WasButtonJustPressed(MouseButton::Right); }
        virtual bool WasMiddleJustPressed() const { return WasButtonJustPressed(MouseButton::Middle); }
                
    };

    // Mouse configuration structure
    struct MouseConfig {
        float sensitivity = 1.0f;                   // Default sensitivity multiplier
        float scrollSensitivity = 1.0f;             // Default scroll sensitivity
        bool enableHighPrecision = true;            // Enable high precision mouse input
        bool enableRawInput = true;                 // Use raw input when available
        bool autoCapture = false;                   // Automatically capture mouse in fullscreen
        bool invertY = false;                       // Invert Y-axis movement
        
        MouseConfig() = default;
    };

    // Helper functions for mouse button names
    const char* GetButtonName(MouseButton button);
    const char* GetAxisName(MouseAxis axis);

} // namespace Input