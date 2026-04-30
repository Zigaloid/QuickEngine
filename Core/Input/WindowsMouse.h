#pragma once

#include "MouseInterface.h"

#define NOMINMAX
#include <Windows.h>
#include <commctrl.h>

#include <memory>
#include <string>
#include <vector>

namespace Input {

/** @brief Windows-specific mouse implementation using Win32 and raw input APIs. */
class WindowsMouse : public IMouse
{
private:
    // ── State ────────────────────────────────────────────────────────────────

    MouseState m_currentState;
    MouseState m_previousState;

    MouseConfig m_config;
    float       m_sensitivity       = 1.0f;
    float       m_scrollSensitivity = 1.0f;

    bool m_cursorVisible  = true;
    bool m_cursorLocked   = false;
    HWND m_windowHandle   = nullptr;
    RECT m_lockArea       = {};

    bool           m_rawInputEnabled    = false;
    bool           m_subclassed         = false;
    RAWINPUTDEVICE m_rawInputDevice     = {};

    MouseButtonCallback m_buttonCallback;
    MouseMoveCallback   m_moveCallback;
    MouseScrollCallback m_scrollCallback;

    bool        m_initialized       = false;
    std::string m_deviceName;
    MouseButton m_lastPressedButton = MouseButton::Left;

    double m_currentX     = 0.0;
    double m_currentY     = 0.0;
    double m_previousX    = 0.0;
    double m_previousY    = 0.0;
    float  m_deltaX       = 0.0f;
    float  m_deltaY       = 0.0f;
    float  m_scrollDeltaX = 0.0f;
    float  m_scrollDeltaY = 0.0f;

    static constexpr DWORD BUTTON_MAPPINGS[static_cast<size_t>(MouseButton::COUNT)] =
    {
        MK_LBUTTON,
        MK_RBUTTON,
        MK_MBUTTON,
        MK_XBUTTON1,
        MK_XBUTTON2
    };

    // ── Private Helpers ──────────────────────────────────────────────────────

    void UpdateButtonStates();
    void UpdatePosition();
    void UpdateScrollDelta();
    void ResetScrollDelta();
    void ProcessButtonCallbacks();
    void LockCursorToWindow();
    void UnlockCursor();
    bool SetupRawInput();
    void CleanupRawInput();
    void UpdateDeviceName();
    bool CheckStateChanged() const;
    MouseButton FindPressedButton() const;

    bool InstallScrollHook();
    void RemoveScrollHook();

    // Old-style WndProc subclassing — no commctrl dependency
    static LRESULT CALLBACK ScrollWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    WNDPROC m_originalWndProc = nullptr;

    static MouseButton GetMouseButtonFromMessage(UINT message, WPARAM wParam);
    static bool        CheckButtonPressed(WPARAM wParam, MouseButton button);

public:
    explicit WindowsMouse(HWND windowHandle = nullptr, const MouseConfig& config = MouseConfig{});
    virtual ~WindowsMouse();

    // ── IMouse ───────────────────────────────────────────────────────────────

    bool Initialize() override;
    void Update()     override;
    void Shutdown()   override;

    bool IsConnected()                     const override;
    const MouseState& GetState()           const override;
    const MouseState& GetPreviousState()   const override;

    std::string GetName() const override;
    std::string GetGUID() const override;

    double GetPositionX()                          const override;
    double GetPositionY()                          const override;
    void   GetPosition(double& x, double& y)       const override;

    float GetMovementX()                           const override;
    float GetMovementY()                           const override;
    void  GetMovement(float& deltaX, float& deltaY) const override;

    bool IsButtonPressed(MouseButton button)     const override;
    bool IsButtonReleased(MouseButton button)    const override;
    bool IsButtonDown(MouseButton button)        const override;
    bool IsButtonUp(MouseButton button)          const override;
    bool WasButtonJustPressed(MouseButton button)  const override;
    bool WasButtonJustReleased(MouseButton button) const override;
    void ResetScrollDeltas()                             override;

    float GetScrollX()                            const override;
    float GetScrollY()                            const override;
    void  GetScroll(float& deltaX, float& deltaY) const override;

    bool        IsAnyButtonPressed()     const override;
    MouseButton GetLastPressedButton()   const override;

    void SetPosition(double x, double y)   override;
    void SetCursorVisible(bool visible)    override;
    bool IsCursorVisible()           const override;
    void SetCursorLocked(bool locked)      override;
    bool IsCursorLocked()            const override;

    void  SetSensitivity(float sensitivity)       override;
    float GetSensitivity()                  const override;
    void  SetScrollSensitivity(float sensitivity) override;
    float GetScrollSensitivity()            const override;

    void SetButtonCallback(MouseButtonCallback callback) override;
    void SetMoveCallback(MouseMoveCallback callback)     override;
    void SetScrollCallback(MouseScrollCallback callback) override;
    const MouseButtonCallback& GetButtonCallback() const override;
    const MouseMoveCallback&   GetMoveCallback()   const override;
    const MouseScrollCallback& GetScrollCallback() const override;

    // ── Windows-Specific ─────────────────────────────────────────────────────

    void SetWindowHandle(HWND windowHandle);
    HWND GetWindowHandle() const { return m_windowHandle; }
    bool ProcessRawInput(LPARAM lParam);
    bool ProcessWindowMessage(UINT message, WPARAM wParam, LPARAM lParam);

    void SetRawInputEnabled(bool enabled);
    bool IsRawInputEnabled() const { return m_rawInputEnabled; }
};

} // namespace Input
