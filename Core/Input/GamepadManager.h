#pragma once

#include "GamepadInterface.h"
#include "GamepadInputHandler.h"
#include "ComponentSystem/ComponentSystem.h"

#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>
#include <algorithm>

namespace Input {

/** @brief Gamepad manager for handling multiple gamepads; derives from Component. */
class GamepadManager : public ComponentSystem::Component
{
public:
    REFL_DECLARE_OBJECT(GamepadManager, ComponentSystem::Component);

private:
    // ── State ────────────────────────────────────────────────────────────────

    std::vector<std::unique_ptr<IGamepad>>  m_gamepads;
    std::unordered_map<int, size_t>         m_gamepadIdToIndex;
    GamepadConfig                           m_defaultConfig;

    std::vector<std::shared_ptr<IGamepadInputHandler>> m_inputHandlers;
    bool m_handlerStackDirty = false;

    GamepadConnectedCallback    m_connectedCallback;
    GamepadDisconnectedCallback m_disconnectedCallback;
    GamepadButtonCallback       m_globalButtonCallback;
    GamepadAxisCallback         m_globalAxisCallback;

    bool m_autoDetectEnabled = true;
    int  m_maxGamepads       = 4;

    // ── Private Helpers ──────────────────────────────────────────────────────

    void   DetectNewGamepads();
    void   CheckDisconnectedGamepads();
    size_t FindAvailableSlot() const;
    void   AddGamepadInternal(std::unique_ptr<IGamepad> gamepad);
    void   RemoveGamepadInternal(int gamepadId);

    void SortHandlersByPriority();
    void ProcessGamepadInput(double deltaTime);

protected:
    // ── Component Lifecycle ──────────────────────────────────────────────────

    bool OnInitialize()           override;
    void OnUpdate(double deltaTime) override;
    void OnShutdown()             override;

public:
    explicit GamepadManager(const GamepadConfig& defaultConfig = GamepadConfig{});
    virtual ~GamepadManager();

    // ── Configuration ────────────────────────────────────────────────────────

    void SetDefaultConfig(const GamepadConfig& config) { m_defaultConfig = config; }
    const GamepadConfig& GetDefaultConfig() const { return m_defaultConfig; }

    void SetAutoDetect(bool enabled) { m_autoDetectEnabled = enabled; }
    bool IsAutoDetectEnabled() const { return m_autoDetectEnabled; }

    void SetMaxGamepads(int maxGamepads);
    int  GetMaxGamepads() const { return m_maxGamepads; }

    // ── Input Handler Management ─────────────────────────────────────────────

    void   AddInputHandler(std::shared_ptr<IGamepadInputHandler> handler);
    void   RemoveInputHandler(std::shared_ptr<IGamepadInputHandler> handler);
    void   ClearInputHandlers();
    size_t GetInputHandlerCount() const { return m_inputHandlers.size(); }

    // ── Gamepad Access ───────────────────────────────────────────────────────

    IGamepad*          GetGamepad(int gamepadId)       const;
    IGamepad*          GetGamepadByIndex(size_t index) const;
    size_t             GetConnectedGamepadCount()       const;
    std::vector<int>   GetConnectedGamepadIds()         const;
    IGamepad*          GetFirstConnectedGamepad()       const;

    // ── Cross-Gamepad Queries ────────────────────────────────────────────────

    bool      IsAnyButtonPressed(GamepadButton button)          const;
    bool      IsAnyButtonJustPressed(GamepadButton button)      const;
    IGamepad* GetGamepadWithButtonPressed(GamepadButton button) const;

    // ── Global Callbacks ─────────────────────────────────────────────────────

    void SetConnectedCallback(GamepadConnectedCallback callback)       { m_connectedCallback = callback; }
    void SetDisconnectedCallback(GamepadDisconnectedCallback callback) { m_disconnectedCallback = callback; }
    void SetGlobalButtonCallback(GamepadButtonCallback callback)       { m_globalButtonCallback = callback; }
    void SetGlobalAxisCallback(GamepadAxisCallback callback)           { m_globalAxisCallback = callback; }

    // ── Manual Gamepad Management ────────────────────────────────────────────

    bool AddGamepad(std::unique_ptr<IGamepad> gamepad);
    bool RemoveGamepad(int gamepadId);
    void RemoveAllGamepads();

    // ── Debug / Info ─────────────────────────────────────────────────────────

    void        PrintConnectedGamepads() const;
    void        PrintInputHandlers()     const;
    std::string GetGamepadInfo(int gamepadId) const;

    [[deprecated("External code should manage gamepad detection")]]
    static std::vector<int> GetAvailableGamepadIds();
    [[deprecated("External code should manage gamepad detection")]]
    static bool IsGamepadConnected(int gamepadId);
    [[deprecated("External code should manage gamepad detection")]]
    static std::string GetGamepadName(int gamepadId);
};

} // namespace Input
