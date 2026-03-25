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

	// Gamepad manager for handling multiple gamepads - now derives from Component
	class GamepadManager : public ComponentSystem::Component {
	public:
		REFL_DECLARE_OBJECT(GamepadManager, ComponentSystem::Component);
	private:
		std::vector<std::unique_ptr<IGamepad>> m_gamepads;
		std::unordered_map<int, size_t> m_gamepadIdToIndex;
		GamepadConfig m_defaultConfig;

		// Input handler stack (ordered by priority, highest first)
		std::vector<std::shared_ptr<IGamepadInputHandler>> m_inputHandlers;
		bool m_handlerStackDirty; // Flag to indicate handlers need re-sorting

		// Global callbacks
		GamepadConnectedCallback m_connectedCallback;
		GamepadDisconnectedCallback m_disconnectedCallback;
		GamepadButtonCallback m_globalButtonCallback;
		GamepadAxisCallback m_globalAxisCallback;

		// Manager state
		bool m_autoDetectEnabled;
		int m_maxGamepads;

		// Helper methods
		void detectNewGamepads(); // Deprecated - no longer used
		void checkDisconnectedGamepads();
		size_t findAvailableSlot() const;
		void addGamepad(std::unique_ptr<IGamepad> gamepad); // Modified to accept IGamepad
		void removeGamepad(int gamepadId);

		// Input handler management
		void sortHandlersByPriority();
		void processGamepadInput(double deltaTime);

	protected:
		// Component lifecycle overrides
		bool OnInitialize() override;
		void OnUpdate(double deltaTime) override;
		void OnShutdown() override;

	public:
		GamepadManager(const GamepadConfig& defaultConfig = GamepadConfig{});
		virtual ~GamepadManager();

		// Configuration
		void SetDefaultConfig(const GamepadConfig& config) { m_defaultConfig = config; }
		const GamepadConfig& GetDefaultConfig() const { return m_defaultConfig; }

		void SetAutoDetect(bool enabled) { m_autoDetectEnabled = enabled; }
		bool IsAutoDetectEnabled() const { return m_autoDetectEnabled; }

		void SetMaxGamepads(int maxGamepads);
		int GetMaxGamepads() const { return m_maxGamepads; }

		// Input handler management - now allows multiple handlers with same name
		void AddInputHandler(std::shared_ptr<IGamepadInputHandler> handler);
		void RemoveInputHandler(std::shared_ptr<IGamepadInputHandler> handler);
		void ClearInputHandlers();
		size_t GetInputHandlerCount() const { return m_inputHandlers.size(); }

		// Gamepad access
		IGamepad* GetGamepad(int gamepadId) const;
		IGamepad* GetGamepadByIndex(size_t index) const;
		size_t GetConnectedGamepadCount() const;
		std::vector<int> GetConnectedGamepadIds() const;

		// First available gamepad (useful for single-player games)
		IGamepad* GetFirstConnectedGamepad() const;

		// State queries across all gamepads
		bool IsAnyButtonPressed(GamepadButton button) const;
		bool IsAnyButtonJustPressed(GamepadButton button) const;
		IGamepad* GetGamepadWithButtonPressed(GamepadButton button) const;

		// Global event callbacks
		void SetConnectedCallback(GamepadConnectedCallback callback) { m_connectedCallback = callback; }
		void SetDisconnectedCallback(GamepadDisconnectedCallback callback) { m_disconnectedCallback = callback; }
		void SetGlobalButtonCallback(GamepadButtonCallback callback) { m_globalButtonCallback = callback; }
		void SetGlobalAxisCallback(GamepadAxisCallback callback) { m_globalAxisCallback = callback; }

		// Manual gamepad management
		bool AddGamepad(std::unique_ptr<IGamepad> gamepad); // New method to accept IGamepad instances
		bool RemoveGamepad(int gamepadId);
		void RemoveAllGamepads();

		// Debugging and information
		void PrintConnectedGamepads() const;
		void PrintInputHandlers() const;
		std::string GetGamepadInfo(int gamepadId) const;

		// Static utility methods - Deprecated since they were GLFWGamepad specific
		[[deprecated("External code should manage gamepad detection")]]
		static std::vector<int> GetAvailableGamepadIds();
		[[deprecated("External code should manage gamepad detection")]]
		static bool IsGamepadConnected(int gamepadId);
		[[deprecated("External code should manage gamepad detection")]]
		static std::string GetGamepadName(int gamepadId);
	};

} // namespace Input