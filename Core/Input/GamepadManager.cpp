#include "GamepadManager.h"

#include "Profiler/Profiler.h"

#include <iostream>
#include <algorithm>

using namespace Input;
REFL_DEFINE_OBJECT(GamepadManager)
REFL_DEFINE_END

namespace Input {

// ── Constructor / Destructor ──────────────────────────────────────────────────

GamepadManager::GamepadManager(const GamepadConfig& defaultConfig)
    : ComponentSystem::Component()
    , m_defaultConfig(defaultConfig)
    , m_handlerStackDirty(false)
    , m_autoDetectEnabled(true)
    , m_maxGamepads(4)
{
    m_gamepads.reserve(m_maxGamepads);
}

GamepadManager::~GamepadManager()
{
    Shutdown();
}

// ── Component Lifecycle ───────────────────────────────────────────────────────

bool GamepadManager::OnInitialize()
{
    std::cout << "Initializing Gamepad Manager..." << std::endl;
    std::cout << "Gamepad Manager initialized with "
              << GetConnectedGamepadCount() << " connected gamepad(s)" << std::endl;
    return true;
}

void GamepadManager::OnUpdate(double deltaTime)
{
    DECLARE_FUNC_LOW();

    if (m_handlerStackDirty)
    {
        SortHandlersByPriority();
        m_handlerStackDirty = false;
    }

    for (auto& gamepad : m_gamepads)
    {
        if (gamepad)
            gamepad->Update();
    }

    CheckDisconnectedGamepads();

    for (auto& handler : m_inputHandlers)
    {
        if (handler && handler->IsActive())
            handler->OnUpdate(deltaTime);
    }

    ProcessGamepadInput(deltaTime);

    if (m_globalButtonCallback || m_globalAxisCallback)
    {
        for (auto& gamepad : m_gamepads)
        {
            if (gamepad && gamepad->IsConnected())
            {
                if (!gamepad->GetButtonCallback() && m_globalButtonCallback)
                    gamepad->SetButtonCallback(m_globalButtonCallback);
                if (!gamepad->GetAxisCallback() && m_globalAxisCallback)
                    gamepad->SetAxisCallback(m_globalAxisCallback);
            }
        }
    }
}

void GamepadManager::OnShutdown()
{
    std::cout << "Shutting down Gamepad Manager..." << std::endl;

    for (auto& gamepad : m_gamepads)
    {
        if (gamepad)
            gamepad->Shutdown();
    }

    m_gamepads.clear();
    m_gamepadIdToIndex.clear();
    m_inputHandlers.clear();

    std::cout << "Gamepad Manager shutdown complete" << std::endl;
}

// ── Private Helpers ───────────────────────────────────────────────────────────

void GamepadManager::SortHandlersByPriority()
{
    std::sort(m_inputHandlers.begin(), m_inputHandlers.end(),
        [](const std::shared_ptr<IGamepadInputHandler>& a, const std::shared_ptr<IGamepadInputHandler>& b)
        {
            return a->GetPriority() > b->GetPriority();
        });
}

void GamepadManager::ProcessGamepadInput(double deltaTime)
{
    for (auto& gamepad : m_gamepads)
    {
        if (!gamepad || !gamepad->IsConnected())
            continue;

        int gamepadId = gamepad->GetGamepadId();

        for (int i = 0; i < static_cast<int>(GamepadButton::COUNT); ++i)
        {
            GamepadButton button      = static_cast<GamepadButton>(i);
            bool          pressed     = gamepad->IsButtonPressed(button);
            bool          justPressed  = gamepad->WasButtonJustPressed(button);
            bool          justReleased = gamepad->WasButtonJustReleased(button);

            if (justPressed || justReleased || pressed)
            {
                for (auto& handler : m_inputHandlers)
                {
                    if (handler && handler->IsActive())
                    {
                        if (handler->HandleButtonInput(gamepadId, button, pressed, justPressed, justReleased))
                            break;
                    }
                }
            }
        }

        for (int i = 0; i < static_cast<int>(GamepadAxis::COUNT); ++i)
        {
            GamepadAxis axis         = static_cast<GamepadAxis>(i);
            float       currentValue = gamepad->GetAxisValue(axis);
            float       delta        = gamepad->GetAxisDelta(axis);

            if (std::abs(delta) > 0.001f || std::abs(currentValue) > 0.001f)
            {
                for (auto& handler : m_inputHandlers)
                {
                    if (handler && handler->IsActive())
                    {
                        if (handler->HandleAxisInput(gamepadId, axis, currentValue, delta))
                            break;
                    }
                }
            }
        }
    }
}

// ── Input Handler Management ──────────────────────────────────────────────────

void GamepadManager::AddInputHandler(std::shared_ptr<IGamepadInputHandler> handler)
{
    if (!handler) return;

    m_inputHandlers.push_back(handler);
    m_handlerStackDirty = true;

    std::cout << "Added input handler: " << handler->GetName()
              << " (Priority: " << handler->GetPriority() << ")" << std::endl;
}

void GamepadManager::RemoveInputHandler(std::shared_ptr<IGamepadInputHandler> handler)
{
    if (!handler) return;

    auto it = std::find(m_inputHandlers.begin(), m_inputHandlers.end(), handler);
    if (it != m_inputHandlers.end())
    {
        std::cout << "Removed input handler: " << (*it)->GetName() << std::endl;
        m_inputHandlers.erase(it);
    }
}

void GamepadManager::ClearInputHandlers()
{
    std::cout << "Clearing all input handlers (" << m_inputHandlers.size() << " handlers)" << std::endl;
    m_inputHandlers.clear();
}

void GamepadManager::DetectNewGamepads()
{
    // No longer used; external code provides IGamepad instances via AddGamepad().
}

void GamepadManager::CheckDisconnectedGamepads()
{
    std::vector<int> toRemove;

    for (const auto& [gamepadId, index] : m_gamepadIdToIndex)
    {
        if (index < m_gamepads.size() && m_gamepads[index])
        {
            if (!m_gamepads[index]->IsConnected())
                toRemove.push_back(gamepadId);
        }
    }

    for (int gamepadId : toRemove)
        RemoveGamepadInternal(gamepadId);
}

size_t GamepadManager::FindAvailableSlot() const
{
    for (size_t i = 0; i < m_gamepads.size(); ++i)
    {
        if (!m_gamepads[i])
            return i;
    }
    return m_gamepads.size();
}

void GamepadManager::AddGamepadInternal(std::unique_ptr<IGamepad> gamepad)
{
    if (!gamepad)
    {
        std::cout << "Cannot add null gamepad" << std::endl;
        return;
    }

    int gamepadId = gamepad->GetGamepadId();

    if (GetConnectedGamepadCount() >= static_cast<size_t>(m_maxGamepads))
    {
        std::cout << "Maximum number of gamepads (" << m_maxGamepads
                  << ") reached. Cannot add gamepad " << gamepadId << std::endl;
        return;
    }

    if (m_gamepadIdToIndex.find(gamepadId) != m_gamepadIdToIndex.end())
    {
        std::cout << "Gamepad " << gamepadId << " already exists" << std::endl;
        return;
    }

    size_t slot = FindAvailableSlot();

    if (slot >= m_gamepads.size())
        m_gamepads.resize(slot + 1);

    if (!gamepad->IsConnected() && !gamepad->Initialize())
    {
        std::cout << "Failed to initialize gamepad " << gamepadId << std::endl;
        return;
    }

    m_gamepads[slot] = std::move(gamepad);
    m_gamepadIdToIndex[gamepadId] = slot;

    std::cout << "Added gamepad " << gamepadId
              << " (" << m_gamepads[slot]->GetName() << ") at slot " << slot << std::endl;

    for (auto& handler : m_inputHandlers)
    {
        if (handler && handler->IsActive())
            handler->OnGamepadConnected(gamepadId);
    }

    if (m_connectedCallback)
        m_connectedCallback(gamepadId);
}

void GamepadManager::RemoveGamepadInternal(int gamepadId)
{
    auto it = m_gamepadIdToIndex.find(gamepadId);
    if (it != m_gamepadIdToIndex.end())
    {
        size_t index = it->second;

        if (index < m_gamepads.size() && m_gamepads[index])
        {
            std::cout << "Removing gamepad " << gamepadId
                      << " (" << m_gamepads[index]->GetName() << ")" << std::endl;

            for (auto& handler : m_inputHandlers)
            {
                if (handler && handler->IsActive())
                    handler->OnGamepadDisconnected(gamepadId);
            }

            m_gamepads[index]->Shutdown();
            m_gamepads[index].reset();
            m_gamepadIdToIndex.erase(it);

            if (m_disconnectedCallback)
                m_disconnectedCallback(gamepadId);
        }
    }
}

// ── Configuration ─────────────────────────────────────────────────────────────

void GamepadManager::SetMaxGamepads(int maxGamepads)
{
    m_maxGamepads = std::max(1, maxGamepads);

    while (GetConnectedGamepadCount() > static_cast<size_t>(m_maxGamepads))
    {
        for (const auto& [gamepadId, index] : m_gamepadIdToIndex)
        {
            RemoveGamepadInternal(gamepadId);
            break;
        }
    }
}

// ── Gamepad Access ────────────────────────────────────────────────────────────

IGamepad* GamepadManager::GetGamepad(int gamepadId) const
{
    auto it = m_gamepadIdToIndex.find(gamepadId);
    if (it != m_gamepadIdToIndex.end())
    {
        size_t index = it->second;
        if (index < m_gamepads.size())
            return m_gamepads[index].get();
    }
    return nullptr;
}

IGamepad* GamepadManager::GetGamepadByIndex(size_t index) const
{
    if (index < m_gamepads.size())
        return m_gamepads[index].get();
    return nullptr;
}

size_t GamepadManager::GetConnectedGamepadCount() const
{
    size_t count = 0;
    for (const auto& gamepad : m_gamepads)
    {
        if (gamepad && gamepad->IsConnected())
            ++count;
    }
    return count;
}

std::vector<int> GamepadManager::GetConnectedGamepadIds() const
{
    std::vector<int> ids;
    for (const auto& [gamepadId, index] : m_gamepadIdToIndex)
    {
        if (index < m_gamepads.size() && m_gamepads[index] && m_gamepads[index]->IsConnected())
            ids.push_back(gamepadId);
    }
    return ids;
}

IGamepad* GamepadManager::GetFirstConnectedGamepad() const
{
    for (const auto& gamepad : m_gamepads)
    {
        if (gamepad && gamepad->IsConnected())
            return gamepad.get();
    }
    return nullptr;
}

// ── Cross-Gamepad Queries ─────────────────────────────────────────────────────

bool GamepadManager::IsAnyButtonPressed(GamepadButton button) const
{
    for (const auto& gamepad : m_gamepads)
    {
        if (gamepad && gamepad->IsConnected() && gamepad->IsButtonPressed(button))
            return true;
    }
    return false;
}

bool GamepadManager::IsAnyButtonJustPressed(GamepadButton button) const
{
    for (const auto& gamepad : m_gamepads)
    {
        if (gamepad && gamepad->IsConnected() && gamepad->WasButtonJustPressed(button))
            return true;
    }
    return false;
}

IGamepad* GamepadManager::GetGamepadWithButtonPressed(GamepadButton button) const
{
    for (const auto& gamepad : m_gamepads)
    {
        if (gamepad && gamepad->IsConnected() && gamepad->IsButtonPressed(button))
            return gamepad.get();
    }
    return nullptr;
}

// ── Manual Gamepad Management ─────────────────────────────────────────────────

bool GamepadManager::AddGamepad(std::unique_ptr<IGamepad> gamepad)
{
    if (!gamepad)
    {
        std::cout << "Cannot add null gamepad" << std::endl;
        return false;
    }

    int gamepadId = gamepad->GetGamepadId();

    if (m_gamepadIdToIndex.find(gamepadId) != m_gamepadIdToIndex.end())
    {
        std::cout << "Gamepad " << gamepadId << " already exists" << std::endl;
        return false;
    }

    AddGamepadInternal(std::move(gamepad));
    return true;
}

bool GamepadManager::RemoveGamepad(int gamepadId)
{
    if (m_gamepadIdToIndex.find(gamepadId) == m_gamepadIdToIndex.end())
        return false;

    RemoveGamepadInternal(gamepadId);
    return true;
}

void GamepadManager::RemoveAllGamepads()
{
    std::vector<int> gamepadIds = GetConnectedGamepadIds();
    for (int gamepadId : gamepadIds)
        RemoveGamepadInternal(gamepadId);
}

// ── Debug / Info ──────────────────────────────────────────────────────────────

void GamepadManager::PrintConnectedGamepads() const
{
    std::cout << "\n=== Connected Gamepads ===" << std::endl;
    std::cout << "Total connected: " << GetConnectedGamepadCount() << std::endl;

    for (const auto& [gamepadId, index] : m_gamepadIdToIndex)
    {
        if (index < m_gamepads.size() && m_gamepads[index] && m_gamepads[index]->IsConnected())
        {
            auto* gamepad = m_gamepads[index].get();
            std::cout << "  Gamepad " << gamepadId
                      << " (Slot " << index << "): "
                      << gamepad->GetName() << std::endl;
        }
    }
    std::cout << "========================\n" << std::endl;
}

void GamepadManager::PrintInputHandlers() const
{
    std::cout << "\n=== Input Handlers ===" << std::endl;
    std::cout << "Total handlers: " << m_inputHandlers.size() << std::endl;

    for (size_t i = 0; i < m_inputHandlers.size(); ++i)
    {
        const auto& handler = m_inputHandlers[i];
        if (handler)
        {
            std::cout << "  " << (i + 1) << ". " << handler->GetName()
                      << " (Priority: " << handler->GetPriority()
                      << ", Active: " << (handler->IsActive() ? "Yes" : "No") << ")" << std::endl;
        }
    }
    std::cout << "=====================\n" << std::endl;
}

std::string GamepadManager::GetGamepadInfo(int gamepadId) const
{
    IGamepad* gamepad = GetGamepad(gamepadId);
    if (!gamepad)
        return "Gamepad not found";

    return "ID: " + std::to_string(gamepadId) +
           ", Name: " + gamepad->GetName() +
           ", Connected: " + (gamepad->IsConnected() ? "Yes" : "No");
}

// ── Deprecated Static Utilities ───────────────────────────────────────────────

std::vector<int> GamepadManager::GetAvailableGamepadIds()
{
    std::cout << "GetAvailableGamepadIds() is deprecated. External code should manage gamepad detection." << std::endl;
    return {};
}

bool GamepadManager::IsGamepadConnected(int gamepadId)
{
    std::cout << "IsGamepadConnected(int) is deprecated. External code should manage gamepad detection." << std::endl;
    return false;
}

std::string GamepadManager::GetGamepadName(int gamepadId)
{
    std::cout << "GetGamepadName(int) is deprecated. External code should manage gamepad detection." << std::endl;
    return "Not Available";
}

} // namespace Input
