#include "MouseManager.h"

#include "Profiler/Profiler.h"

#include <iostream>
#include <algorithm>

using namespace Input;
REFL_DEFINE_OBJECT(MouseManager)
REFL_DEFINE_END

namespace Input {

// ── Constructor / Destructor ──────────────────────────────────────────────────

MouseManager::MouseManager(const MouseConfig& defaultConfig)
    : ComponentSystem::Component()
    , m_defaultConfig(defaultConfig)
    , m_handlerStackDirty(false)
    , m_enabled(true)
{
}

MouseManager::~MouseManager()
{
    Shutdown();
}

// ── Component Lifecycle ───────────────────────────────────────────────────────

bool MouseManager::OnInitialize()
{
    std::cout << "Initializing Mouse Manager..." << std::endl;
    std::cout << "Mouse Manager initialized with "
              << (HasMouse() ? "1" : "0") << " mouse" << std::endl;
    return true;
}

void MouseManager::OnUpdate(double deltaTime)
{
    DECLARE_FUNC_LOW();

    if (!m_enabled)
        return;

    if (m_handlerStackDirty)
    {
        SortHandlersByPriority();
        m_handlerStackDirty = false;
    }

    if (m_mouse)
    {
        m_mouse->Update();

        if (!m_mouse->IsConnected())
        {
            std::cout << "Mouse disconnected" << std::endl;
            if (m_disconnectedCallback)
                m_disconnectedCallback();
            RemoveMouseInternal();
            return;
        }
    }

    for (auto& handler : m_inputHandlers)
    {
        if (handler && handler->IsEnabled())
            handler->OnUpdate(deltaTime);
    }

    ProcessMouseInput(deltaTime);

    if (m_mouse && (m_globalButtonCallback || m_globalMoveCallback || m_globalScrollCallback))
    {
        if (!m_mouse->GetButtonCallback() && m_globalButtonCallback)
            m_mouse->SetButtonCallback(m_globalButtonCallback);
        if (!m_mouse->GetMoveCallback() && m_globalMoveCallback)
            m_mouse->SetMoveCallback(m_globalMoveCallback);
        if (!m_mouse->GetScrollCallback() && m_globalScrollCallback)
            m_mouse->SetScrollCallback(m_globalScrollCallback);
    }
}

void MouseManager::OnShutdown()
{
    std::cout << "Shutting down Mouse Manager..." << std::endl;

    if (m_mouse)
    {
        m_mouse->Shutdown();
        m_mouse.reset();
    }

    m_inputHandlers.clear();
    std::cout << "Mouse Manager shutdown complete" << std::endl;
}

// ── Private Helpers ───────────────────────────────────────────────────────────

void MouseManager::SortHandlersByPriority()
{
    std::sort(m_inputHandlers.begin(), m_inputHandlers.end(),
        [](const std::shared_ptr<IMouseInputHandler>& a, const std::shared_ptr<IMouseInputHandler>& b)
        {
            return a->GetPriority() > b->GetPriority();
        });
}

void MouseManager::ProcessMouseInput(double deltaTime)
{
    if (!m_mouse || !m_mouse->IsConnected())
        return;

    // Button input
    for (int i = 0; i < static_cast<int>(MouseButton::COUNT); ++i)
    {
        MouseButton button      = static_cast<MouseButton>(i);
        bool        pressed     = m_mouse->IsButtonPressed(button);
        bool        justPressed  = m_mouse->WasButtonJustPressed(button);
        bool        justReleased = m_mouse->WasButtonJustReleased(button);

        if (justPressed || justReleased || pressed)
        {
            double x = 0.0;
            double y = 0.0;
            m_mouse->GetPosition(x, y);

            for (auto& handler : m_inputHandlers)
            {
                if (handler && handler->IsEnabled())
                {
                    if (handler->HandleButtonInput(button, pressed, justPressed, justReleased, x, y))
                        break;
                }
            }
        }
    }

    // Movement input
    float  deltaX = m_mouse->GetMovementX();
    float  deltaY = m_mouse->GetMovementY();
    double posX   = m_mouse->GetPositionX();
    double posY   = m_mouse->GetPositionY();

    if (std::abs(deltaX) > 0.001f || std::abs(deltaY) > 0.001f)
    {
        for (auto& handler : m_inputHandlers)
        {
            if (handler && handler->IsEnabled())
            {
                if (handler->HandleMovementInput(posX, posY, deltaX, deltaY))
                    break;
            }
        }
    }

    // Scroll input
    float scrollX = m_mouse->GetScrollX();
    float scrollY = m_mouse->GetScrollY();

    if (std::abs(scrollX) > 0.001f || std::abs(scrollY) > 0.001f)
    {
        for (auto& handler : m_inputHandlers)
        {
            if (handler && handler->IsEnabled())
            {
                if (handler->HandleScrollInput(scrollX, scrollY))
                    break;
            }
        }
    }

    m_mouse->ResetScrollDeltas();
}

// ── Input Handler Management ──────────────────────────────────────────────────

void MouseManager::AddInputHandler(std::shared_ptr<IMouseInputHandler> handler)
{
    if (!handler) return;

    m_inputHandlers.push_back(handler);
    m_handlerStackDirty = true;

    std::cout << "Added mouse input handler: " << handler->GetName()
              << " (Priority: " << handler->GetPriority() << ")" << std::endl;
}

void MouseManager::RemoveInputHandler(std::shared_ptr<IMouseInputHandler> handler)
{
    if (!handler) return;

    auto it = std::find(m_inputHandlers.begin(), m_inputHandlers.end(), handler);
    if (it != m_inputHandlers.end())
    {
        std::cout << "Removed mouse input handler: " << (*it)->GetName() << std::endl;
        m_inputHandlers.erase(it);
    }
}

void MouseManager::ClearInputHandlers()
{
    std::cout << "Clearing all mouse input handlers (" << m_inputHandlers.size() << " handlers)" << std::endl;
    m_inputHandlers.clear();
}

void MouseManager::AddMouseInternal(std::unique_ptr<IMouse> mouse)
{
    if (!mouse)
    {
        std::cout << "Cannot add null mouse" << std::endl;
        return;
    }

    if (m_mouse)
        RemoveMouseInternal();

    if (!mouse->IsConnected() && !mouse->Initialize())
    {
        std::cout << "Failed to initialize mouse" << std::endl;
        return;
    }

    m_mouse = std::move(mouse);
    std::cout << "Added mouse (" << m_mouse->GetName() << ")" << std::endl;

    for (auto& handler : m_inputHandlers)
    {
        if (handler && handler->IsEnabled())
            handler->OnMouseEnter();
    }

    if (m_connectedCallback)
        m_connectedCallback();
}

void MouseManager::RemoveMouseInternal()
{
    if (m_mouse)
    {
        std::cout << "Removing mouse (" << m_mouse->GetName() << ")" << std::endl;

        for (auto& handler : m_inputHandlers)
        {
            if (handler && handler->IsEnabled())
                handler->OnMouseLeave();
        }

        m_mouse->Shutdown();
        m_mouse.reset();

        if (m_disconnectedCallback)
            m_disconnectedCallback();
    }
}

// ── Mouse Access ──────────────────────────────────────────────────────────────

IMouse* MouseManager::GetMouse() const
{
    return m_mouse.get();
}

bool MouseManager::HasMouse() const
{
    return m_mouse && m_mouse->IsConnected();
}

// ── State Queries ─────────────────────────────────────────────────────────────

bool MouseManager::IsButtonPressed(MouseButton button) const
{
    return m_mouse ? m_mouse->IsButtonPressed(button) : false;
}

bool MouseManager::WasButtonJustPressed(MouseButton button) const
{
    return m_mouse ? m_mouse->WasButtonJustPressed(button) : false;
}

bool MouseManager::WasButtonJustReleased(MouseButton button) const
{
    return m_mouse ? m_mouse->WasButtonJustReleased(button) : false;
}

double MouseManager::GetPositionX() const
{
    return m_mouse ? m_mouse->GetPositionX() : 0.0;
}

double MouseManager::GetPositionY() const
{
    return m_mouse ? m_mouse->GetPositionY() : 0.0;
}

void MouseManager::GetPosition(double& x, double& y) const
{
    if (m_mouse)
    {
        m_mouse->GetPosition(x, y);
    }
    else
    {
        x = y = 0.0;
    }
}

float MouseManager::GetMovementX() const
{
    return m_mouse ? m_mouse->GetMovementX() : 0.0f;
}

float MouseManager::GetMovementY() const
{
    return m_mouse ? m_mouse->GetMovementY() : 0.0f;
}

void MouseManager::GetMovement(float& deltaX, float& deltaY) const
{
    if (m_mouse)
    {
        m_mouse->GetMovement(deltaX, deltaY);
    }
    else
    {
        deltaX = deltaY = 0.0f;
    }
}

float MouseManager::GetScrollX() const
{
    return m_mouse ? m_mouse->GetScrollX() : 0.0f;
}

float MouseManager::GetScrollY() const
{
    return m_mouse ? m_mouse->GetScrollY() : 0.0f;
}

void MouseManager::GetScroll(float& deltaX, float& deltaY) const
{
    if (m_mouse)
    {
        m_mouse->GetScroll(deltaX, deltaY);
    }
    else
    {
        deltaX = deltaY = 0.0f;
    }
}

// ── Cursor Control ────────────────────────────────────────────────────────────

void MouseManager::SetCursorVisible(bool visible)
{
    if (m_mouse)
        m_mouse->SetCursorVisible(visible);
}

bool MouseManager::IsCursorVisible() const
{
    return m_mouse ? m_mouse->IsCursorVisible() : true;
}

void MouseManager::SetCursorLocked(bool locked)
{
    if (m_mouse)
        m_mouse->SetCursorLocked(locked);
}

bool MouseManager::IsCursorLocked() const
{
    return m_mouse ? m_mouse->IsCursorLocked() : false;
}

void MouseManager::SetCursorPosition(double x, double y)
{
    if (m_mouse)
        m_mouse->SetPosition(x, y);
}

// ── Sensitivity ───────────────────────────────────────────────────────────────

void MouseManager::SetSensitivity(float sensitivity)
{
    if (m_mouse)
        m_mouse->SetSensitivity(sensitivity);
}

float MouseManager::GetSensitivity() const
{
    return m_mouse ? m_mouse->GetSensitivity() : 1.0f;
}

void MouseManager::SetScrollSensitivity(float sensitivity)
{
    if (m_mouse)
        m_mouse->SetScrollSensitivity(sensitivity);
}

float MouseManager::GetScrollSensitivity() const
{
    return m_mouse ? m_mouse->GetScrollSensitivity() : 1.0f;
}

// ── Manual Mouse Management ───────────────────────────────────────────────────

bool MouseManager::SetMouse(std::unique_ptr<IMouse> mouse)
{
    if (!mouse)
    {
        std::cout << "Cannot set null mouse" << std::endl;
        return false;
    }
    mouse->Initialize();
    AddMouseInternal(std::move(mouse));
    return true;
}

void MouseManager::RemoveMouse()
{
    RemoveMouseInternal();
}

// ── Debug / Info ──────────────────────────────────────────────────────────────

void MouseManager::PrintMouseInfo() const
{
    std::cout << "\n=== Mouse Information ===" << std::endl;
    if (m_mouse)
    {
        std::cout << "Mouse: " << m_mouse->GetName() << std::endl;
        std::cout << "Connected: " << (m_mouse->IsConnected() ? "Yes" : "No") << std::endl;

        double x = 0.0;
        double y = 0.0;
        m_mouse->GetPosition(x, y);
        std::cout << "Position: (" << x << ", " << y << ")" << std::endl;

        float deltaX = 0.0f;
        float deltaY = 0.0f;
        m_mouse->GetMovement(deltaX, deltaY);
        std::cout << "Movement: (" << deltaX << ", " << deltaY << ")" << std::endl;

        std::cout << "Sensitivity: " << m_mouse->GetSensitivity() << std::endl;
        std::cout << "Scroll Sensitivity: " << m_mouse->GetScrollSensitivity() << std::endl;
        std::cout << "Cursor Visible: " << (m_mouse->IsCursorVisible() ? "Yes" : "No") << std::endl;
        std::cout << "Cursor Locked: " << (m_mouse->IsCursorLocked() ? "Yes" : "No") << std::endl;
    }
    else
    {
        std::cout << "No mouse connected" << std::endl;
    }
    std::cout << "========================\n" << std::endl;
}

void MouseManager::PrintInputHandlers() const
{
    std::cout << "\n=== Mouse Input Handlers ===" << std::endl;
    std::cout << "Total handlers: " << m_inputHandlers.size() << std::endl;

    for (size_t i = 0; i < m_inputHandlers.size(); ++i)
    {
        const auto& handler = m_inputHandlers[i];
        if (handler)
        {
            std::cout << "  " << (i + 1) << ". " << handler->GetName()
                      << " (Priority: " << handler->GetPriority()
                      << ", Active: " << (handler->IsEnabled() ? "Yes" : "No") << ")" << std::endl;
        }
    }
    std::cout << "===========================\n" << std::endl;
}

std::string MouseManager::GetMouseInfo() const
{
    if (!m_mouse)
        return "Mouse not found";

    return "Name: " + m_mouse->GetName() +
           ", Connected: " + (m_mouse->IsConnected() ? "Yes" : "No");
}

} // namespace Input
