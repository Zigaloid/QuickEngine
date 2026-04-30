#include "WindowsMouse.h"

#include <iostream>
#include <algorithm>
#include <cmath>

#pragma comment(lib, "comctl32.lib")

namespace Input {

// ── Constructor / Destructor ──────────────────────────────────────────────────

WindowsMouse::WindowsMouse(HWND windowHandle, const MouseConfig& config)
    : m_windowHandle(windowHandle)
    , m_config(config)
    , m_sensitivity(config.sensitivity)
    , m_scrollSensitivity(config.scrollSensitivity)
    , m_cursorVisible(true)
    , m_cursorLocked(false)
    , m_rawInputEnabled(false)
    , m_subclassed(false)
    , m_initialized(false)
    , m_lastPressedButton(MouseButton::Left)
    , m_currentX(0.0)
    , m_currentY(0.0)
    , m_previousX(0.0)
    , m_previousY(0.0)
    , m_deltaX(0.0f)
    , m_deltaY(0.0f)
    , m_scrollDeltaX(0.0f)
    , m_scrollDeltaY(0.0f)
    , m_buttonCallback(nullptr)
    , m_moveCallback(nullptr)
    , m_scrollCallback(nullptr)
{
    m_currentState   = {};
    m_previousState  = {};
    m_lockArea       = {};
    m_rawInputDevice = {};
}

WindowsMouse::~WindowsMouse()
{
    Shutdown();
}

// ── IMouse Lifecycle ──────────────────────────────────────────────────────────

bool WindowsMouse::Initialize()
{
    if (m_initialized)
        return true;

    UpdateDeviceName();

    if (m_config.enableRawInput)
        SetupRawInput();

    POINT cursorPos;
    if (GetCursorPos(&cursorPos))
    {
        if (m_windowHandle)
            ScreenToClient(m_windowHandle, &cursorPos);
        m_currentX = m_previousX = static_cast<double>(cursorPos.x);
        m_currentY = m_previousY = static_cast<double>(cursorPos.y);
    }

    m_currentState.connected  = true;
    m_currentState.positionX  = m_currentX;
    m_currentState.positionY  = m_currentY;

    m_initialized = true;

    InstallScrollHook(); // intercept WM_MOUSEWHEEL without modifying entry code

    std::cout << "WindowsMouse initialized successfully" << std::endl;
    return true;
}

void WindowsMouse::Update()
{
    if (!m_initialized)
        return;

    m_previousState = m_currentState;
    m_previousX     = m_currentX;
    m_previousY     = m_currentY;

    // Always poll button state via GetAsyncKeyState — works regardless of HWND.
    UpdateButtonStates();

    if (!m_rawInputEnabled)
        UpdatePosition();

    m_currentState.positionX = m_currentX;
    m_currentState.positionY = m_currentY;
    m_currentState.axes[static_cast<size_t>(MouseAxis::X)]       = m_deltaX * m_sensitivity;
    m_currentState.axes[static_cast<size_t>(MouseAxis::Y)]       = m_deltaY * m_sensitivity;
    m_currentState.axes[static_cast<size_t>(MouseAxis::ScrollX)] = m_scrollDeltaX * m_scrollSensitivity;
    m_currentState.axes[static_cast<size_t>(MouseAxis::ScrollY)] = m_scrollDeltaY * m_scrollSensitivity;

    if (m_config.invertY)
        m_currentState.axes[static_cast<size_t>(MouseAxis::Y)] *= -1.0f;

    // Always process button callbacks via polling path.
    ProcessButtonCallbacks();

    if (m_moveCallback && (std::abs(m_deltaX) > 0.001f || std::abs(m_deltaY) > 0.001f))
        m_moveCallback(m_currentX, m_currentY, m_deltaX, m_deltaY);

    if (m_scrollCallback && (std::abs(m_scrollDeltaX) > 0.001f || std::abs(m_scrollDeltaY) > 0.001f))
        m_scrollCallback(m_scrollDeltaX, m_scrollDeltaY);
}

void WindowsMouse::Shutdown()
{
    if (!m_initialized)
        return;

    RemoveScrollHook();
    CleanupRawInput();

    if (!m_cursorVisible)
        SetCursorVisible(true);
    if (m_cursorLocked)
        SetCursorLocked(false);

    m_buttonCallback = nullptr;
    m_moveCallback   = nullptr;
    m_scrollCallback = nullptr;
    m_initialized    = false;

    std::cout << "WindowsMouse shutdown" << std::endl;
}

// ── IMouse State Queries ──────────────────────────────────────────────────────

bool WindowsMouse::IsConnected()                     const { return m_currentState.connected; }
const MouseState& WindowsMouse::GetState()           const { return m_currentState; }
const MouseState& WindowsMouse::GetPreviousState()   const { return m_previousState; }

std::string WindowsMouse::GetName() const
{
    return m_deviceName.empty() ? "Windows Mouse" : m_deviceName;
}

std::string WindowsMouse::GetGUID() const { return "windows-mouse-device"; }

double WindowsMouse::GetPositionX() const { return m_currentX; }
double WindowsMouse::GetPositionY() const { return m_currentY; }

void WindowsMouse::GetPosition(double& x, double& y) const
{
    x = m_currentX;
    y = m_currentY;
}

float WindowsMouse::GetMovementX() const { return m_deltaX; }
float WindowsMouse::GetMovementY() const { return m_deltaY; }

void WindowsMouse::GetMovement(float& deltaX, float& deltaY) const
{
    deltaX = m_deltaX;
    deltaY = m_deltaY;
}

bool WindowsMouse::IsButtonPressed(MouseButton button) const
{
    return m_currentState.buttons[static_cast<size_t>(button)];
}

bool WindowsMouse::IsButtonReleased(MouseButton button) const { return !m_currentState.buttons[static_cast<size_t>(button)]; }
bool WindowsMouse::IsButtonDown(MouseButton button)     const { return IsButtonPressed(button); }
bool WindowsMouse::IsButtonUp(MouseButton button)       const { return IsButtonReleased(button); }

bool WindowsMouse::WasButtonJustPressed(MouseButton button) const
{
    size_t index = static_cast<size_t>(button);
    return m_currentState.buttons[index] && !m_previousState.buttons[index];
}

bool WindowsMouse::WasButtonJustReleased(MouseButton button) const
{
    size_t index = static_cast<size_t>(button);
    return !m_currentState.buttons[index] && m_previousState.buttons[index];
}

float WindowsMouse::GetScrollX() const { return m_scrollDeltaX; }
float WindowsMouse::GetScrollY() const { return m_scrollDeltaY; }

void WindowsMouse::ResetScrollDeltas()
{
    m_scrollDeltaX = 0.0f;
    m_scrollDeltaY = 0.0f;
}

void WindowsMouse::GetScroll(float& deltaX, float& deltaY) const
{
    deltaX = m_scrollDeltaX;
    deltaY = m_scrollDeltaY;
}

bool WindowsMouse::IsAnyButtonPressed() const
{
    for (const auto& buttonPressed : m_currentState.buttons)
    {
        if (buttonPressed)
            return true;
    }
    return false;
}

MouseButton WindowsMouse::GetLastPressedButton() const { return m_lastPressedButton; }

// ── Cursor Control ────────────────────────────────────────────────────────────

void WindowsMouse::SetPosition(double x, double y)
{
    POINT newPos = { static_cast<LONG>(x), static_cast<LONG>(y) };

    if (m_windowHandle)
        ClientToScreen(m_windowHandle, &newPos);

    SetCursorPos(newPos.x, newPos.y);
    m_currentX = x;
    m_currentY = y;
}

void WindowsMouse::SetCursorVisible(bool visible)
{
    if (visible != m_cursorVisible)
    {
        m_cursorVisible = visible;
        ShowCursor(visible ? TRUE : FALSE);
    }
}

bool WindowsMouse::IsCursorVisible() const { return m_cursorVisible; }

void WindowsMouse::SetCursorLocked(bool locked)
{
    if (locked != m_cursorLocked)
    {
        m_cursorLocked = locked;
        if (locked)
            LockCursorToWindow();
        else
            UnlockCursor();
    }
}

bool WindowsMouse::IsCursorLocked() const { return m_cursorLocked; }

// ── Configuration ─────────────────────────────────────────────────────────────

void WindowsMouse::SetSensitivity(float sensitivity)
{
    m_sensitivity = std::max(0.01f, sensitivity);
}

float WindowsMouse::GetSensitivity() const { return m_sensitivity; }

void WindowsMouse::SetScrollSensitivity(float sensitivity)
{
    m_scrollSensitivity = std::max(0.01f, sensitivity);
}

float WindowsMouse::GetScrollSensitivity() const { return m_scrollSensitivity; }

// ── Callbacks ─────────────────────────────────────────────────────────────────

void WindowsMouse::SetButtonCallback(MouseButtonCallback callback) { m_buttonCallback = callback; }
void WindowsMouse::SetMoveCallback(MouseMoveCallback callback)     { m_moveCallback   = callback; }
void WindowsMouse::SetScrollCallback(MouseScrollCallback callback) { m_scrollCallback = callback; }

const MouseButtonCallback& WindowsMouse::GetButtonCallback() const { return m_buttonCallback; }
const MouseMoveCallback&   WindowsMouse::GetMoveCallback()   const { return m_moveCallback; }
const MouseScrollCallback& WindowsMouse::GetScrollCallback() const { return m_scrollCallback; }

// ── Windows-Specific ──────────────────────────────────────────────────────────

void WindowsMouse::SetWindowHandle(HWND windowHandle)
{
    m_windowHandle = windowHandle;
}

bool WindowsMouse::ProcessRawInput(LPARAM lParam)
{
    if (!m_rawInputEnabled)
        return false;

    UINT dwSize = 0;
    GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, nullptr, &dwSize, sizeof(RAWINPUTHEADER));

    if (dwSize == 0)
        return false;

    std::unique_ptr<BYTE[]> lpb = std::make_unique<BYTE[]>(dwSize);
    if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, lpb.get(), &dwSize, sizeof(RAWINPUTHEADER)) != dwSize)
        return false;

    RAWINPUT* raw = reinterpret_cast<RAWINPUT*>(lpb.get());
    if (raw->header.dwType == RIM_TYPEMOUSE)
    {
        m_deltaX = static_cast<float>(raw->data.mouse.lLastX);
        m_deltaY = static_cast<float>(raw->data.mouse.lLastY);

        if (!(raw->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE))
        {
            m_currentX += m_deltaX;
            m_currentY += m_deltaY;
        }
        else
        {
            m_currentX = static_cast<double>(raw->data.mouse.lLastX);
            m_currentY = static_cast<double>(raw->data.mouse.lLastY);
        }
        return true;
    }
    return false;
}

bool WindowsMouse::ProcessWindowMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP:
    {
        MouseButton button  = GetMouseButtonFromMessage(message, wParam);
        bool        pressed = (message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN ||
                               message == WM_MBUTTONDOWN || message == WM_XBUTTONDOWN);

        m_currentState.buttons[static_cast<size_t>(button)] = pressed;

        if (pressed)
            m_lastPressedButton = button;

        POINT cursorPos;
        GetCursorPos(&cursorPos);
        if (m_windowHandle)
            ScreenToClient(m_windowHandle, &cursorPos);

        if (m_buttonCallback)
            m_buttonCallback(button, pressed, static_cast<double>(cursorPos.x), static_cast<double>(cursorPos.y));

        return true;
    }

    case WM_MOUSEMOVE:
    {
        if (!m_rawInputEnabled)
        {
            POINT cursorPos;
            GetCursorPos(&cursorPos);
            if (m_windowHandle)
                ScreenToClient(m_windowHandle, &cursorPos);

            double newX = static_cast<double>(cursorPos.x);
            double newY = static_cast<double>(cursorPos.y);

            m_deltaX   = static_cast<float>(newX - m_currentX);
            m_deltaY   = static_cast<float>(newY - m_currentY);
            m_currentX = newX;
            m_currentY = newY;
        }
        return true;
    }

    case WM_MOUSEWHEEL:
    {
        m_scrollDeltaY += GET_WHEEL_DELTA_WPARAM(wParam) / static_cast<float>(WHEEL_DELTA);
        return true;
    }

    case WM_MOUSEHWHEEL:
    {
        m_scrollDeltaX += GET_WHEEL_DELTA_WPARAM(wParam) / static_cast<float>(WHEEL_DELTA);
        return true;
    }

    case WM_INPUT:
        return ProcessRawInput(lParam);
    }

    return false;
}

void WindowsMouse::SetRawInputEnabled(bool enabled)
{
    if (enabled != m_rawInputEnabled)
    {
        m_rawInputEnabled = enabled;
        if (m_initialized)
        {
            if (enabled)
                SetupRawInput();
            else
                CleanupRawInput();
        }
    }
}

// ── Scroll WndProc Hook ───────────────────────────────────────────────────────

bool WindowsMouse::InstallScrollHook()
{
    if (!m_windowHandle || m_originalWndProc)
        return false;

    // Store our instance pointer on the HWND so the static WndProc can find it.
    SetProp(m_windowHandle, L"WindowsMouse", reinterpret_cast<HANDLE>(this));

    m_originalWndProc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtr(m_windowHandle, GWLP_WNDPROC,
                         reinterpret_cast<LONG_PTR>(ScrollWndProc)));

    return m_originalWndProc != nullptr;
}

void WindowsMouse::RemoveScrollHook()
{
    if (m_windowHandle && m_originalWndProc)
    {
        SetWindowLongPtr(m_windowHandle, GWLP_WNDPROC,
                         reinterpret_cast<LONG_PTR>(m_originalWndProc));
        RemoveProp(m_windowHandle, L"WindowsMouse");
        m_originalWndProc = nullptr;
    }
}

LRESULT CALLBACK WindowsMouse::ScrollWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* self = reinterpret_cast<WindowsMouse*>(GetProp(hwnd, L"WindowsMouse"));

    if (self)
    {
        if (msg == WM_MOUSEWHEEL)
        {
            self->m_scrollDeltaY += GET_WHEEL_DELTA_WPARAM(wParam) / static_cast<float>(WHEEL_DELTA);
            // Still call the original so entry.cpp also sees the message.
            return CallWindowProc(self->m_originalWndProc, hwnd, msg, wParam, lParam);
        }
        if (msg == WM_MOUSEHWHEEL)
        {
            self->m_scrollDeltaX += GET_WHEEL_DELTA_WPARAM(wParam) / static_cast<float>(WHEEL_DELTA);
            return CallWindowProc(self->m_originalWndProc, hwnd, msg, wParam, lParam);
        }
    }

    // For all other messages, find the original proc safely.
    if (self && self->m_originalWndProc)
        return CallWindowProc(self->m_originalWndProc, hwnd, msg, wParam, lParam);

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ── Private Helpers ───────────────────────────────────────────────────────────

void WindowsMouse::UpdateButtonStates()
{
    m_currentState.buttons[static_cast<size_t>(MouseButton::Left)]   = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    m_currentState.buttons[static_cast<size_t>(MouseButton::Right)]  = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
    m_currentState.buttons[static_cast<size_t>(MouseButton::Middle)] = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;
    m_currentState.buttons[static_cast<size_t>(MouseButton::X1)]     = (GetAsyncKeyState(VK_XBUTTON1) & 0x8000) != 0;
    m_currentState.buttons[static_cast<size_t>(MouseButton::X2)]     = (GetAsyncKeyState(VK_XBUTTON2) & 0x8000) != 0;
}

void WindowsMouse::UpdatePosition()
{
    POINT cursorPos;
    if (GetCursorPos(&cursorPos))
    {
        if (m_windowHandle)
            ScreenToClient(m_windowHandle, &cursorPos);

        double newX = static_cast<double>(cursorPos.x);
        double newY = static_cast<double>(cursorPos.y);

        m_deltaX   = static_cast<float>(newX - m_currentX);
        m_deltaY   = static_cast<float>(newY - m_currentY);
        m_currentX = newX;
        m_currentY = newY;
    }
}

void WindowsMouse::UpdateScrollDelta()
{
    // Scroll updates are handled directly in ProcessWindowMessage.
}

void WindowsMouse::ResetScrollDelta()
{
    m_scrollDeltaX = 0.0f;
    m_scrollDeltaY = 0.0f;
}

void WindowsMouse::ProcessButtonCallbacks()
{
    if (!m_buttonCallback)
        return;

    for (size_t i = 0; i < static_cast<size_t>(MouseButton::COUNT); ++i)
    {
        bool currentPressed  = m_currentState.buttons[i];
        bool previousPressed = m_previousState.buttons[i];

        if (currentPressed != previousPressed)
        {
            MouseButton button = static_cast<MouseButton>(i);
            m_buttonCallback(button, currentPressed, m_currentX, m_currentY);
        }
    }
}

void WindowsMouse::LockCursorToWindow()
{
    if (m_windowHandle)
    {
        GetClientRect(m_windowHandle, &m_lockArea);
        ClientToScreen(m_windowHandle, reinterpret_cast<POINT*>(&m_lockArea.left));
        ClientToScreen(m_windowHandle, reinterpret_cast<POINT*>(&m_lockArea.right));
        ClipCursor(&m_lockArea);
    }
}

void WindowsMouse::UnlockCursor()
{
    ClipCursor(nullptr);
}

bool WindowsMouse::SetupRawInput()
{
    m_rawInputDevice.usUsagePage = 0x01;
    m_rawInputDevice.usUsage     = 0x02;
    m_rawInputDevice.dwFlags     = 0;
    m_rawInputDevice.hwndTarget  = m_windowHandle;

    if (RegisterRawInputDevices(&m_rawInputDevice, 1, sizeof(RAWINPUTDEVICE)) == FALSE)
    {
        std::cerr << "Failed to register raw input device for mouse" << std::endl;
        return false;
    }

    m_rawInputEnabled = true;
    return true;
}

void WindowsMouse::CleanupRawInput()
{
    if (m_rawInputEnabled)
    {
        m_rawInputDevice.dwFlags    = RIDEV_REMOVE;
        m_rawInputDevice.hwndTarget = nullptr;
        RegisterRawInputDevices(&m_rawInputDevice, 1, sizeof(RAWINPUTDEVICE));
        m_rawInputEnabled = false;
    }
}

void WindowsMouse::UpdateDeviceName()
{
    m_deviceName = "Windows System Mouse";

    UINT deviceCount = 0;
    if (GetRawInputDeviceList(nullptr, &deviceCount, sizeof(RAWINPUTDEVICELIST)) != static_cast<UINT>(-1) && deviceCount > 0)
    {
        std::vector<RAWINPUTDEVICELIST> deviceList(deviceCount);
        if (GetRawInputDeviceList(deviceList.data(), &deviceCount, sizeof(RAWINPUTDEVICELIST)) != static_cast<UINT>(-1))
        {
            for (const auto& device : deviceList)
            {
                if (device.dwType == RIM_TYPEMOUSE)
                {
                    UINT nameSize = 0;
                    if (GetRawInputDeviceInfo(device.hDevice, RIDI_DEVICENAME, nullptr, &nameSize) != static_cast<UINT>(-1) && nameSize > 0)
                    {
                        std::wstring deviceName(nameSize, L'\0');
                        if (GetRawInputDeviceInfo(device.hDevice, RIDI_DEVICENAME, deviceName.data(), &nameSize) != static_cast<UINT>(-1))
                        {
                            int requiredSize = WideCharToMultiByte(CP_UTF8, 0, deviceName.c_str(), -1, nullptr, 0, nullptr, nullptr);
                            if (requiredSize > 0)
                            {
                                std::string name(requiredSize - 1, '\0');
                                if (WideCharToMultiByte(CP_UTF8, 0, deviceName.c_str(), -1, name.data(), requiredSize, nullptr, nullptr) > 0)
                                {
                                    if (!name.empty())
                                    {
                                        m_deviceName = name;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

bool WindowsMouse::CheckStateChanged() const
{
    return m_currentState.buttons != m_previousState.buttons;
}

MouseButton WindowsMouse::FindPressedButton() const
{
    for (size_t i = 0; i < static_cast<size_t>(MouseButton::COUNT); ++i)
    {
        if (m_currentState.buttons[i])
            return static_cast<MouseButton>(i);
    }
    return MouseButton::Left;
}

MouseButton WindowsMouse::GetMouseButtonFromMessage(UINT message, WPARAM wParam)
{
    switch (message)
    {
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:   return MouseButton::Left;
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:   return MouseButton::Right;
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:   return MouseButton::Middle;
    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP:   return (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) ? MouseButton::X1 : MouseButton::X2;
    default:             return MouseButton::Left;
    }
}

bool WindowsMouse::CheckButtonPressed(WPARAM wParam, MouseButton button)
{
    DWORD buttonFlag = BUTTON_MAPPINGS[static_cast<size_t>(button)];
    return (wParam & buttonFlag) != 0;
}

// ── Free Helper Functions ─────────────────────────────────────────────────────

const char* GetButtonName(MouseButton button)
{
    switch (button)
    {
    case MouseButton::Left:   return "Left";
    case MouseButton::Right:  return "Right";
    case MouseButton::Middle: return "Middle";
    case MouseButton::X1:     return "X1";
    case MouseButton::X2:     return "X2";
    default:                  return "Unknown";
    }
}

const char* GetAxisName(MouseAxis axis)
{
    switch (axis)
    {
    case MouseAxis::X:       return "Mouse X";
    case MouseAxis::Y:       return "Mouse Y";
    case MouseAxis::ScrollX: return "Scroll X";
    case MouseAxis::ScrollY: return "Scroll Y";
    default:                 return "Unknown";
    }
}

} // namespace Input
