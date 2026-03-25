#include "RenderComponentSelector.h"
#include "CoreSystem/CoreSystem.h"
#include "RenderingSystem.h"
#include "GLTFRenderComponent.h"
#include "Viewport.h"
#include <iostream>
#include <algorithm>
#include <limits>

namespace Rendering {

    RenderComponentSelector::RenderComponentSelector()
        : Component()
    {
    }

    bool RenderComponentSelector::OnInitialize() {
        RegisterWithMouseManager();
        
        // Find the manipulator component
        auto* manipulator = Core::CoreSystem::GetFirstComponentOfType<ManipulatorComponent>();
        if (manipulator) {
            SetManipulatorComponent(manipulator);
            std::cout << "RenderComponentSelector: Found and connected to ManipulatorComponent" << std::endl;
        } else {
            std::cout << "Warning: No ManipulatorComponent found for RenderComponentSelector" << std::endl;
        }
        
        return true;
    }

    void RenderComponentSelector::OnUpdate(double deltaTime) {
        // Update hover state for visual feedback (if needed in the future)
        // For now, we mainly handle selection through mouse input
    }

    void RenderComponentSelector::OnShutdown() {
        SetSelectionChangeCallback(nullptr);
        manipulatorComponent_ = nullptr;
        UnregisterFromMouseManager();
        ClearSelection();
        
    }

    void RenderComponentSelector::RegisterWithMouseManager() {
        mouseManager_ = Core::CoreSystem::GetFirstComponentOfType<Input::MouseManager>();
        
        if (mouseManager_) {
            selfReference_ = std::shared_ptr<RenderComponentSelector>(this, [](RenderComponentSelector*) {
                // Custom deleter that does nothing - component lifetime is managed by ComponentManager
            });
            mouseManager_->AddInputHandler(selfReference_);
            std::cout << "RenderComponentSelector registered with MouseManager" << std::endl;
        } else {
            std::cout << "Warning: No MouseManager found for RenderComponentSelector registration" << std::endl;
        }
    }

    void RenderComponentSelector::UnregisterFromMouseManager() {
        if (mouseManager_ && selfReference_) {
            mouseManager_->RemoveInputHandler(selfReference_);
            selfReference_.reset();
            mouseManager_ = nullptr;
            std::cout << "RenderComponentSelector unregistered from MouseManager" << std::endl;
        }
    }

    bool RenderComponentSelector::HandleButtonInput(Input::MouseButton button, bool pressed, bool justPressed, bool justReleased, double x, double y) {
        if (!selectionEnabled_ || button != Input::MouseButton::Left || !justPressed) {
            return false;
        }

        // Don't handle selection if manipulator is currently being used
        if (manipulatorComponent_ && manipulatorComponent_->IsDragging()) {
            return false;
        }

        Viewport* viewport = Rendering::RenderingSystem::GetActiveViewport();
        if (!viewport) {
            return false;
        }

        // Cast ray into the scene
        Vector3f rayOrigin = viewport->screenToWorld(x, y, 0.0f);
        Vector3f rayDirection;
        
        // Calculate ray direction
        Vector3f nearPoint = viewport->screenToWorld(x, y, 0.0f);
        Vector3f farPoint = viewport->screenToWorld(x, y, 1.0f);
        rayDirection = (farPoint - nearPoint).normalized();

        // Test intersection with all render components
        float closestDistance;
        RenderComponent* hitComponent = TestRayIntersectionWithComponents(rayOrigin, rayDirection, closestDistance);
        
        if (hitComponent && closestDistance <= maxSelectionDistance_) {
            SetSelectedComponent(hitComponent);
            std::cout << "Selected RenderComponent at distance: " << closestDistance << std::endl;
            return true; // We handled the selection
        } else {
            // Click on empty space - clear selection
            ClearSelection();
            std::cout << "Cleared selection - clicked on empty space" << std::endl;
            return false; // Allow other handlers to process
        }
    }

    bool RenderComponentSelector::HandleMovementInput(double x, double y, float deltaX, float deltaY) {
        // We don't handle mouse movement for selection, only clicking
        return false;
    }

    bool RenderComponentSelector::HandleScrollInput(float deltaX, float deltaY) {
        // We don't handle scroll input for selection
        return false;
    }

    void RenderComponentSelector::SetSelectedComponent(RenderComponent* component) {
        if (selectedComponent_ == component) {
            return; // Already selected
        }

        selectedComponent_ = component;
        UpdateManipulatorFromSelection();
        
        // Call selection change callback
        if (selectionChangeCallback_) {
            selectionChangeCallback_(selectedComponent_);
        }
    }

    void RenderComponentSelector::ClearSelection() {
        if (selectedComponent_) {
            selectedComponent_ = nullptr;
            
            // Hide manipulator when nothing is selected
            if (manipulatorComponent_) {
                manipulatorComponent_->SetVisible(false);
            }
            
            // Call selection change callback
            if (selectionChangeCallback_) {
                selectionChangeCallback_(nullptr);
            }
        }
    }

    void RenderComponentSelector::SetManipulatorComponent(ManipulatorComponent* manipulator) {
        manipulatorComponent_ = manipulator;
        
        if (manipulatorComponent_) {
            // Set up transform change callback
            manipulatorComponent_->SetTransformChangeCallback(
                [this](const Vector3f& position, const Vector3f& rotation, const Vector3f& scale) {
                    OnManipulatorTransformChanged(position, rotation, scale);
                }
            );
            
            // Update manipulator from current selection
            UpdateManipulatorFromSelection();
        }
    }

    std::vector<RenderComponent*> RenderComponentSelector::GetAllRenderComponents() const {
        std::vector<RenderComponent*> components;
        
        // Get all RenderComponents from the core system
        auto renderComponents = Core::CoreSystem::GetActiveComponentsOfType<RenderComponent>();
        
        for (auto* component : renderComponents) {
            if (component && component->isVisible()) {
                components.push_back(component);
            }
        }
        
        // Get all GLTFRenderComponents from the core system
        auto gltfRenderComponents = Core::CoreSystem::GetActiveComponentsOfType<GLTFRenderComponent>();
        
        for (auto* component : gltfRenderComponents) {
            if (component && component->isVisible() && component->isEnabledForRender()) {
                components.push_back(component);
            }
        }
        
        return components;
    }

    RenderComponent* RenderComponentSelector::TestRayIntersectionWithComponents(const Vector3f& rayOrigin, const Vector3f& rayDirection, float& closestDistance) const {
        auto components = GetAllRenderComponents();
        RenderComponent* closestComponent = nullptr;
        closestDistance = std::numeric_limits<float>::max();

        for (auto* component : components) {
            float distance;
            if (TestRayIntersectionWithComponent(component, rayOrigin, rayDirection, distance)) {
                if (distance < closestDistance) {
                    closestDistance = distance;
                    closestComponent = component;
                }
            }
        }

        return closestComponent;
    }

    bool RenderComponentSelector::TestRayIntersectionWithComponent(const RenderComponent* component, const Vector3f& rayOrigin, const Vector3f& rayDirection, float& distance) const {
        if (!component || !component->isVisible()) {
            return false;
        }

        // Additional check for GLTFRenderComponent to ensure it's ready for rendering
        if (auto* gltfComponent = dynamic_cast<const GLTFRenderComponent*>(component)) {
            if (!gltfComponent->isEnabledForRender() || !gltfComponent->isReadyToRender()) {
                return false;
            }
        }

        // Get component's world bounding sphere
        BoundingSphere worldBounds = component->getWorldBounds();
        
        // Test ray-sphere intersection
        Vector3f oc = rayOrigin - worldBounds.center;
        float a = rayDirection.dot(rayDirection);
        float b = 2.0f * oc.dot(rayDirection);
        float c = oc.dot(oc) - worldBounds.radius * worldBounds.radius;
        float discriminant = b * b - 4 * a * c;

        if (discriminant < 0) {
            return false; // No intersection
        }

        float t1 = (-b - std::sqrt(discriminant)) / (2.0f * a);
        float t2 = (-b + std::sqrt(discriminant)) / (2.0f * a);

        // Use the closest positive intersection
        if (t1 > 0) {
            distance = t1;
            return true;
        } else if (t2 > 0) {
            distance = t2;
            return true;
        }

        return false; // Both intersections behind ray origin
    }

    void RenderComponentSelector::UpdateManipulatorFromSelection() {
        if (!manipulatorComponent_) {
            return;
        }

        if (selectedComponent_) {
            // Set manipulator to selected component's transform
            manipulatorComponent_->SetTargetTransform(
                selectedComponent_->getPosition(),
                selectedComponent_->getRotation(),
                selectedComponent_->getScale()
            );
            manipulatorComponent_->SetVisible(true);
        } else {
            // No selection - hide manipulator
            manipulatorComponent_->SetVisible(false);
        }
    }

    void RenderComponentSelector::OnManipulatorTransformChanged(const Vector3f& position, const Vector3f& rotation, const Vector3f& scale) {
        if (selectedComponent_) {
            // Apply manipulator changes to selected component
            selectedComponent_->setTransform(position, rotation, scale);
        }
    }

} // namespace Rendering