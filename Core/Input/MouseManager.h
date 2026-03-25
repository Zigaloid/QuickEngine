#pragma once

#include "MouseInterface.h"
#include "MouseInputHandler.h"
#include "ComponentSystem/ComponentSystem.h"
#include <memory>
#include <vector>
#include <functional>
#include <algorithm>

namespace Input {

    // Mouse manager for handling mouse input - derives from Component
    class MouseManager : public ComponentSystem::Component {
    public:
        REFL_DECLARE_OBJECT(MouseManager, ComponentSystem::Component);
    private:
        std::unique_ptr<IMouse> m_mouse;
        MouseConfig m_defaultConfig;

        // Input handler stack (ordered by priority, highest first)
        std::vector<std::shared_ptr<IMouseInputHandler>> m_inputHandlers;
        bool m_handlerStackDirty; // Flag to indicate handlers need re-sorting

        // Global callbacks
        MouseConnectedCallback m_connectedCallback;
        MouseDisconnectedCallback m_disconnectedCallback;
        MouseButtonCallback m_globalButtonCallback;
        MouseMoveCallback m_globalMoveCallback;
        MouseScrollCallback m_globalScrollCallback;

        // Manager state
        bool m_enabled;

        // Helper methods
        void addMouse(std::unique_ptr<IMouse> mouse);
        void removeMouse();

        // Input handler management
        void sortHandlersByPriority();
        void processMouseInput(double deltaTime);

    protected:
        // Component lifecycle overrides
        bool OnInitialize() override;
        void OnUpdate(double deltaTime) override;
        void OnShutdown() override;

    public:
        MouseManager(const MouseConfig& defaultConfig = MouseConfig{});
        virtual ~MouseManager();

        // Configuration
        void SetDefaultConfig(const MouseConfig& config) { m_defaultConfig = config; }
        const MouseConfig& GetDefaultConfig() const { return m_defaultConfig; }

        void SetEnabled(bool enabled) { m_enabled = enabled; }
        bool IsEnabled() const { return m_enabled; }

        // Input handler management
        void AddInputHandler(std::shared_ptr<IMouseInputHandler> handler);
        void RemoveInputHandler(std::shared_ptr<IMouseInputHandler> handler);
        void ClearInputHandlers();
        size_t GetInputHandlerCount() const { return m_inputHandlers.size(); }

        // Mouse access
        IMouse* GetMouse() const;
        bool HasMouse() const;

        // Mouse state queries
        bool IsButtonPressed(MouseButton button) const;
        bool WasButtonJustPressed(MouseButton button) const;
        bool WasButtonJustReleased(MouseButton button) const;
        double GetPositionX() const;
        double GetPositionY() const;
        void GetPosition(double& x, double& y) const;
        float GetMovementX() const;
        float GetMovementY() const;
        void GetMovement(float& deltaX, float& deltaY) const;
        float GetScrollX() const;
        float GetScrollY() const;
        void GetScroll(float& deltaX, float& deltaY) const;

        // Cursor control
        void SetCursorVisible(bool visible);
        bool IsCursorVisible() const;
        void SetCursorLocked(bool locked);
        bool IsCursorLocked() const;
        void SetCursorPosition(double x, double y);

        // Configuration
        void SetSensitivity(float sensitivity);
        float GetSensitivity() const;
        void SetScrollSensitivity(float sensitivity);
        float GetScrollSensitivity() const;

        // Global event callbacks
        void SetConnectedCallback(MouseConnectedCallback callback) { m_connectedCallback = callback; }
        void SetDisconnectedCallback(MouseDisconnectedCallback callback) { m_disconnectedCallback = callback; }
        void SetGlobalButtonCallback(MouseButtonCallback callback) { m_globalButtonCallback = callback; }
        void SetGlobalMoveCallback(MouseMoveCallback callback) { m_globalMoveCallback = callback; }
        void SetGlobalScrollCallback(MouseScrollCallback callback) { m_globalScrollCallback = callback; }

        // Manual mouse management
        bool SetMouse(std::unique_ptr<IMouse> mouse);
        void RemoveMouse();

        // Debugging and information
        void PrintMouseInfo() const;
        void PrintInputHandlers() const;
        std::string GetMouseInfo() const;

        // Convenience methods
        bool IsLeftPressed() const { return IsButtonPressed(MouseButton::Left); }
        bool IsRightPressed() const { return IsButtonPressed(MouseButton::Right); }
        bool IsMiddlePressed() const { return IsButtonPressed(MouseButton::Middle); }
        bool WasLeftJustPressed() const { return WasButtonJustPressed(MouseButton::Left); }
        bool WasRightJustPressed() const { return WasButtonJustPressed(MouseButton::Right); }
        bool WasMiddleJustPressed() const { return WasButtonJustPressed(MouseButton::Middle); }
    };

} // namespace Input