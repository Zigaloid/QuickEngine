#pragma once

#include "ComponentSystem/ComponentSystem.h"
#include "Input/MouseInputHandler.h"
#include "Input/MouseManager.h"
#include "ManipulatorComponent.h"
#include "RenderComponent.h"
#include "math/Vector3f.h"
#include "math/Matrix4f.h"
#include <memory>
#include <vector>
#include <functional>

// Forward declarations
class Viewport;

namespace Rendering {

    class RenderComponentSelector : public ComponentSystem::Component, public Input::MouseInputHandler {
    public:
        // Selection change callback - called when selection changes
        using SelectionChangeCallback = std::function<void(RenderComponent* selected)>;

    private:
        // Selection state
        RenderComponent* selectedComponent_ = nullptr;
        std::vector<RenderComponent*> hoveredComponents_;
        
        // Manipulator reference
        ManipulatorComponent* manipulatorComponent_ = nullptr;
        
        // Mouse manager registration
        Input::MouseManager* mouseManager_ = nullptr;
        std::shared_ptr<RenderComponentSelector> selfReference_;
        
        // Selection settings
        bool selectionEnabled_ = true;
        float maxSelectionDistance_ = 1000.0f;
        
        // Callbacks
        SelectionChangeCallback selectionChangeCallback_;
        
        // Helper methods
        std::vector<RenderComponent*> GetAllRenderComponents() const;
        RenderComponent* TestRayIntersectionWithComponents(const Vector3f& rayOrigin, const Vector3f& rayDirection, float& closestDistance) const;
        bool TestRayIntersectionWithComponent(const RenderComponent* component, const Vector3f& rayOrigin, const Vector3f& rayDirection, float& distance) const;
        void UpdateManipulatorFromSelection();
        void OnManipulatorTransformChanged(const Vector3f& position, const Vector3f& rotation, const Vector3f& scale);
        
        void RegisterWithMouseManager();
        void UnregisterFromMouseManager();

    protected:
        bool OnInitialize() override;
        void OnUpdate(double deltaTime) override;
        void OnShutdown() override;

    public:
        RenderComponentSelector();
        ~RenderComponentSelector() = default;

        // MouseInputHandler interface
        bool HandleButtonInput(Input::MouseButton button, bool pressed, bool justPressed, bool justReleased, double x, double y) override;
        bool HandleMovementInput(double x, double y, float deltaX, float deltaY) override;
        bool HandleScrollInput(float deltaX, float deltaY) override;

        // Selection management
        void SetSelectedComponent(RenderComponent* component);
        RenderComponent* GetSelectedComponent() const { return selectedComponent_; }
        void ClearSelection();
        
        // Settings
        void SetSelectionEnabled(bool enabled) { selectionEnabled_ = enabled; }
        bool IsSelectionEnabled() const { return selectionEnabled_; }
        
        void SetMaxSelectionDistance(float distance) { maxSelectionDistance_ = distance; }
        float GetMaxSelectionDistance() const { return maxSelectionDistance_; }
        
        // Callbacks
        void SetSelectionChangeCallback(const SelectionChangeCallback& callback) { selectionChangeCallback_ = callback; }
        
        // Manipulator integration
        void SetManipulatorComponent(ManipulatorComponent* manipulator);
        ManipulatorComponent* GetManipulatorComponent() const { return manipulatorComponent_; }
        
        // Utility methods
        bool IsComponentSelected(const RenderComponent* component) const { return selectedComponent_ == component; }
        
        // Input priority - should be lower than manipulator to allow manipulator to handle its interactions first
        int GetPriority() const override { return -1; }
    };

} // namespace Rendering