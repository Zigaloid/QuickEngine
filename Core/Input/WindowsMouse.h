#pragma once

#include "MouseInterface.h"
#define NOMINMAX
#include <Windows.h>
#include <memory>

namespace Input {

    class WindowsMouse : public IMouse {
    private:
        // Mouse state
        MouseState m_currentState;
        MouseState m_previousState;
        
        // Configuration
        MouseConfig m_config;
        float m_sensitivity;
        float m_scrollSensitivity;
        
        // Cursor state
        bool m_cursorVisible;
        bool m_cursorLocked;
        HWND m_windowHandle;
        RECT m_lockArea;
        
        // Raw input
        bool m_rawInputEnabled;
        RAWINPUTDEVICE m_rawInputDevice;
        
        // Event callbacks
        MouseButtonCallback m_buttonCallback;
        MouseMoveCallback m_moveCallback;
        MouseScrollCallback m_scrollCallback;
        
        // Internal state
        bool m_initialized;
        std::string m_deviceName;
        MouseButton m_lastPressedButton;
        
        // Position tracking
        double m_currentX, m_currentY;
        double m_previousX, m_previousY;
        float m_deltaX, m_deltaY;
        float m_scrollDeltaX, m_scrollDeltaY;
        
        // Button state mapping
        static constexpr DWORD BUTTON_MAPPINGS[static_cast<size_t>(MouseButton::COUNT)] = {
            MK_LBUTTON,     // MouseButton::Left
            MK_RBUTTON,     // MouseButton::Right
            MK_MBUTTON,     // MouseButton::Middle
            MK_XBUTTON1,    // MouseButton::X1
            MK_XBUTTON2     // MouseButton::X2
        };

    public:
        explicit WindowsMouse(HWND windowHandle = nullptr);
        virtual ~WindowsMouse();

        // IMouse interface implementation
        bool Initialize() override;
        void Update() override;
        void Shutdown() override;

        // State queries
        bool IsConnected() const override;
        const MouseState& GetState() const override;
        const MouseState& GetPreviousState() const override;
        
        // Mouse information
        std::string GetName() const override;
        std::string GetGUID() const override;

        // Position queries
        double GetPositionX() const override;
        double GetPositionY() const override;
        void GetPosition(double& x, double& y) const override;

        // Movement queries
        float GetMovementX() const override;
        float GetMovementY() const override;
        void GetMovement(float& deltaX, float& deltaY) const override;

        // Button state queries
        bool IsButtonPressed(MouseButton button) const override;
        bool IsButtonReleased(MouseButton button) const override;
        bool IsButtonDown(MouseButton button) const override;
        bool IsButtonUp(MouseButton button) const override;
        bool WasButtonJustPressed(MouseButton button) const override;
        bool WasButtonJustReleased(MouseButton button) const override;
        void ResetScrollDeltas() override;
        // Scroll queries
        float GetScrollX() const override;
        float GetScrollY() const override;
        void GetScroll(float& deltaX, float& deltaY) const override;

        // Convenience methods
        bool IsAnyButtonPressed() const override;
        MouseButton GetLastPressedButton() const override;

        // Cursor control
        void SetPosition(double x, double y) override;
        void SetCursorVisible(bool visible) override;
        bool IsCursorVisible() const override;
        void SetCursorLocked(bool locked) override;
        bool IsCursorLocked() const override;

        // Configuration
        void SetSensitivity(float sensitivity) override;
        float GetSensitivity() const override;
        void SetScrollSensitivity(float sensitivity) override;
        float GetScrollSensitivity() const override;

        // Event callbacks
        void SetButtonCallback(MouseButtonCallback callback) override;
        void SetMoveCallback(MouseMoveCallback callback) override;
        void SetScrollCallback(MouseScrollCallback callback) override;
        const MouseButtonCallback& GetButtonCallback() const override;
        const MouseMoveCallback& GetMoveCallback() const override;
        const MouseScrollCallback& GetScrollCallback() const override;

        // Windows-specific methods
        void SetWindowHandle(HWND windowHandle);
        HWND GetWindowHandle() const { return m_windowHandle; }
        bool ProcessRawInput(LPARAM lParam);
        bool ProcessWindowMessage(UINT message, WPARAM wParam, LPARAM lParam);
        
        // Raw input configuration
        void SetRawInputEnabled(bool enabled);
        bool IsRawInputEnabled() const { return m_rawInputEnabled; }

    private:
        // Internal helper methods
        void updateButtonStates();
        void updatePosition();
        void updateScrollDelta();
        void resetScrollDelta();
        void processButtonCallbacks();
        void lockCursorToWindow();
        void unlockCursor();
        bool setupRawInput();
        void cleanupRawInput();
        void updateDeviceName();
        
        // State comparison helpers
        bool hasStateChanged() const;
        MouseButton findPressedButton() const;
        
        // Windows message helpers
        static MouseButton getMouseButtonFromMessage(UINT message, WPARAM wParam);
        static bool isButtonPressed(WPARAM wParam, MouseButton button);
    };

} // namespace Input