#include "ManipulatorComponent.h"
#include "CameraComponent.h"
#include "RenderingSystem.h"
#include "Viewport.h"  // Include for Viewport class
#include "glfw/glfw3.h"
#include <iostream>
#include <algorithm>
#include <cmath>

namespace Rendering {

	ManipulatorComponent::ManipulatorComponent()
		: Component()
		, manipulator_(std::make_unique<Rendering::Manipulators>())
		, targetPosition_(Vector3f::zero())
		, targetRotation_(Vector3f::zero())
		, targetScale_(Vector3f::one())
		, cameraPosition_(Vector3f::zero())
		, viewportWidth_(800)
		, viewportHeight_(600)
	{
		// Initialize with default settings
		settings_.mouseSensitivity = 0.01f;
		settings_.snapThreshold = 0.1f;
		settings_.enableSnapping = false;
		settings_.enableKeyboardShortcuts = true;
		settings_.manipulatorScale = 1.0f;
		settings_.autoScale = true;
		settings_.autoScaleFactor = 0.1f;

		// Initialize manipulator
		if (manipulator_) {
			manipulator_->SetPosition(targetPosition_);
			manipulator_->SetRotation(targetRotation_);
			manipulator_->SetScale(targetScale_);
			manipulator_->SetManipulatorType(Rendering::Manipulators::ManipulatorType::Translation);
		}
	}

	bool ManipulatorComponent::OnInitialize() {
		if (!manipulator_) {
			return false;
		}

		// Check if rendering system is available
		if (!manipulator_->IsInitialized()) {
			std::cerr << "ManipulatorComponent: Rendering system not initialized" << std::endl;
			return false;
		}
		RegisterWithMouseManager();
		return true;
	}

	void ManipulatorComponent::OnUpdate(double deltaTime) {
		if (!manipulator_) return;

		// Update manipulator scale if auto-scaling is enabled
		
		UpdateManipulatorScale();

		// Update manipulator position to match target
		manipulator_->SetPosition(targetPosition_);
		manipulator_->SetRotation(targetRotation_);
		manipulator_->SetScale(targetScale_);
		
		RenderingSystem::GetRenderQueue()->AddFunction([this]()
			{
				UpdateCameraInfoFromViewport(Rendering::RenderingSystem::GetCurrentlyRenderingViewport());
				Render();
			}, "RenderManipulator");

	}

	void ManipulatorComponent::OnShutdown() {
		// Reset interaction state
		UnregisterFromMouseManager();
		interactionState_ = InteractionState();
		transformChangeCallback_ = nullptr;
	}

	void ManipulatorComponent::SetTargetTransform(const Vector3f& position, const Vector3f& rotation, const Vector3f& scale) {
		targetPosition_ = position;
		targetRotation_ = rotation;
		targetScale_ = scale;

		if (manipulator_) {
			manipulator_->SetPosition(targetPosition_);
			manipulator_->SetRotation(targetRotation_);
			manipulator_->SetScale(targetScale_);
		}
	}

	void ManipulatorComponent::SetTargetPosition(const Vector3f& position) {
		targetPosition_ = position;
		if (manipulator_) {
			manipulator_->SetPosition(targetPosition_);
		}
	}

	void ManipulatorComponent::SetTargetRotation(const Vector3f& rotation) {
		targetRotation_ = rotation;
		if (manipulator_) {
			manipulator_->SetRotation(targetRotation_);
		}
	}

	void ManipulatorComponent::SetTargetScale(const Vector3f& scale) {
		targetScale_ = scale;
		if (manipulator_) {
			manipulator_->SetScale(targetScale_);
		}
	}

	void ManipulatorComponent::SetManipulatorType(Rendering::Manipulators::ManipulatorType type) {
		if (manipulator_) {
			manipulator_->SetManipulatorType(type);
			// Reset interaction state when changing types
			interactionState_.isDragging = false;
			interactionState_.dragAxis = Rendering::Manipulators::ManipulatorAxis::None;
		}
	}

	Rendering::Manipulators::ManipulatorType ManipulatorComponent::GetManipulatorType() const {
		return manipulator_ ? manipulator_->GetManipulatorType() : Rendering::Manipulators::ManipulatorType::Translation;
	}

	void ManipulatorComponent::CycleManipulatorType() {
		if (manipulator_) {
			manipulator_->CycleManipulatorType();
			// Reset interaction state when cycling types
			interactionState_.isDragging = false;
			interactionState_.dragAxis = Rendering::Manipulators::ManipulatorAxis::None;
		}
	}

	void ManipulatorComponent::SetVisible(bool visible) {
		if (manipulator_) {
			manipulator_->SetVisible(visible);
		}
	}

	bool ManipulatorComponent::IsVisible() const {
		return manipulator_ ? manipulator_->IsVisible() : false;
	}

	void ManipulatorComponent::SetCameraInfo(const Vector3f& cameraPosition, const Matrix4f& viewMatrix, const Matrix4f& projMatrix) {
		cameraPosition_ = cameraPosition;
		viewMatrix_ = viewMatrix;
		projectionMatrix_ = projMatrix;
	}

	void ManipulatorComponent::SetViewportSize(int width, int height) {
		viewportWidth_ = std::max(1, width);
		viewportHeight_ = std::max(1, height);
	}

	void ManipulatorComponent::RegisterWithMouseManager() {
		auto m_mouseManager = Core::CoreSystem::GetFirstComponentOfType<Input::MouseManager>();

		if (m_mouseManager) {
			m_selfReference = std::shared_ptr<ManipulatorComponent>(this, [](ManipulatorComponent*) {
				// Custom deleter that does nothing - component lifetime is managed by ComponentManager
				});
			m_mouseManager->AddInputHandler(m_selfReference);
			std::cout << "ManipulatorComponent registered with MouseManager" << std::endl;
		}
		else {
			std::cout << "Warning: No MouseManager found for ManipulatorComponent registration" << std::endl;
		}
	}

	void ManipulatorComponent::UnregisterFromMouseManager() 
	{
		if (m_mouseManager && m_selfReference) {
			m_mouseManager->RemoveInputHandler(m_selfReference);
			m_selfReference.reset();
			m_mouseManager = nullptr;
			std::cout << "CameraComponent unregistered from MouseManager" << std::endl;
		}
	}

	bool ManipulatorComponent::HandleButtonInput(Input::MouseButton button, bool pressed, bool justPressed, bool justReleased, double x, double y)
	{	

		if (!manipulator_ || !IsVisible()) return false;
		
		Viewport* viewport = Rendering::RenderingSystem::GetActiveViewport();
		if (!viewport) 	return false;
		double viewportX = x;
		double viewportY = y;
		
		if (button == Input::MouseButton::Left) {
			if (justPressed ) {
				Vector3f rayOrigin;
				Vector3f rayDirection;
				Viewport* viewport = Rendering::RenderingSystem::GetActiveViewport();
				if (viewport) {
					// Use viewport's proper screen-to-world conversion for ray origin
					rayOrigin = viewport->screenToWorld(viewportX, viewportY, 0.0f); // Near plane
					rayDirection = ScreenToWorldRay(viewportX, viewportY, viewport);
				} else {
					// Fallback: use camera position as ray origin
					rayOrigin = cameraPosition_;
					rayDirection = ScreenToWorldRay(viewportX, viewportY, viewport);
				}

				float distance;
				Rendering::Manipulators::ManipulatorAxis hitAxis = manipulator_->TestRayIntersection(rayOrigin, rayDirection, distance);
				
				if (hitAxis != Rendering::Manipulators::ManipulatorAxis::None) {
					// Start dragging
					interactionState_.isDragging = true;
					interactionState_.dragAxis = hitAxis;
					interactionState_.dragStartPosition = targetPosition_;
					interactionState_.dragStartMousePos = Vector3f(static_cast<float>(x), static_cast<float>(y), 0.0f);
					interactionState_.lastMouseX = x;
					interactionState_.lastMouseY = y;

					// Store initial transform based on manipulator type
					switch (GetManipulatorType()) {
					case Rendering::Manipulators::ManipulatorType::Translation:
						interactionState_.initialTransform = targetPosition_;
						break;
					case Rendering::Manipulators::ManipulatorType::Rotation:
						interactionState_.initialTransform = targetRotation_;
						break;
					case Rendering::Manipulators::ManipulatorType::Scale:
						interactionState_.initialTransform = targetScale_;
						break;
					}

					// Set selected axis
					manipulator_->SetSelectedAxis(hitAxis);
					return true; // We handled the input
				}
			}
			else if (justReleased) {
				// Stop dragging
				if (interactionState_.isDragging) {
					interactionState_.isDragging = false;
					manipulator_->SetSelectedAxis(Rendering::Manipulators::ManipulatorAxis::None);
					return true; // We handled the input
				}
			}
		}
		return false; // We didn't handle the input
	}
	bool ManipulatorComponent::HandleMovementInput(double x, double y, float deltaX, float deltaY)
	{
		if (!manipulator_ || !IsVisible()) return false;

		Viewport* viewport = Rendering::RenderingSystem::GetActiveViewport();
		if (!viewport) 	return false;

		double viewportX = x;
		double viewportY = y;

		// Update last mouse position
		interactionState_.lastMouseX = viewportX;
		interactionState_.lastMouseY = viewportY;

		if (interactionState_.isDragging) {
			// Calculate delta based on manipulator type
			Vector3f delta = Vector3f::zero();
			
			switch (GetManipulatorType()) {
			case Rendering::Manipulators::ManipulatorType::Translation:
				delta = CalculateTranslationDelta(viewportX, viewportY, interactionState_.dragAxis);
				SetTargetPosition(interactionState_.initialTransform + delta);
				break;

			case Rendering::Manipulators::ManipulatorType::Rotation:
				delta = CalculateRotationDelta(viewportX, viewportY, interactionState_.dragAxis);
				SetTargetRotation(interactionState_.initialTransform + delta);
				break;

			case Rendering::Manipulators::ManipulatorType::Scale:
			{
				Vector3f scaleFactors = CalculateScaleDelta(viewportX, viewportY, interactionState_.dragAxis);

				// Apply multiplicative scaling from the initial transform
				Vector3f newScale = Vector3f(
					interactionState_.initialTransform.getX() * scaleFactors.getX(),
					interactionState_.initialTransform.getY() * scaleFactors.getY(),
					interactionState_.initialTransform.getZ() * scaleFactors.getZ()
				);

				SetTargetScale(newScale);
			}
			break;
				SetTargetScale(interactionState_.initialTransform + delta);
				break;
			}

			// Call transform change callback
			if (transformChangeCallback_) {
				transformChangeCallback_(targetPosition_, targetRotation_, targetScale_);
			}
			
			return true; // We handled the input while dragging
		}
		else {
			// Update hover state for visual feedback using viewport-aware ray casting
			Vector3f rayOrigin;
			Vector3f rayDirection;
			Viewport* viewport = Rendering::RenderingSystem::GetActiveViewport();
			if (viewport) {
				// Use viewport's proper screen-to-world conversion for ray origin
				rayOrigin = viewport->screenToWorld(viewportX, viewportY, 0.0f); // Near plane
				rayDirection = ScreenToWorldRay(viewportX, viewportY, viewport);
			} else {
				// Fallback: use camera position as ray origin
				rayOrigin = cameraPosition_;
				rayDirection = ScreenToWorldRay(viewportX, viewportY, viewport);
			}

			float distance;
			Rendering::Manipulators::ManipulatorAxis hoveredAxis = manipulator_->TestRayIntersection(rayOrigin, rayDirection, distance);
			manipulator_->SetHoveredAxis(hoveredAxis);
			interactionState_.isHovering = (hoveredAxis != Rendering::Manipulators::ManipulatorAxis::None);
			
			// Return true only if we're hovering over the manipulator to prevent other handlers from interfering
			return interactionState_.isHovering;
		}
	}
	bool ManipulatorComponent::HandleScrollInput(float deltaX, float deltaY)
	{
		// Scroll input is not handled by the manipulator
		return false;
	}

	Vector3f ManipulatorComponent::ScreenToWorldRay(double mouseX, double mouseY, const Viewport* viewport) const {
		// If we have a viewport, use its screenToWorld method for proper ray casting
		if (viewport) {
			// Get the world position at depth 0 (near plane) and depth 1 (far plane)
			Vector3f nearPoint = viewport->screenToWorld(mouseX, mouseY, 0.0f);
			Vector3f farPoint = viewport->screenToWorld(mouseX, mouseY, 1.0f);
			
			// Calculate ray direction by subtracting near from far point
			Vector3f rayDirection = farPoint - nearPoint;
			
			// Normalize the direction
			float length = std::sqrt(rayDirection.getX() * rayDirection.getX() + 
									rayDirection.getY() * rayDirection.getY() + 
									rayDirection.getZ() * rayDirection.getZ());
			if (length > 0.0f) {
				rayDirection.set(rayDirection.getX() / length, rayDirection.getY() / length, rayDirection.getZ() / length);
			}
			
			return rayDirection;
		}
		else {
			// Fallback to basic implementation when no viewport is provided
			// Convert screen coordinates to normalized device coordinates
			float x = (2.0f * static_cast<float>(mouseX)) / viewportWidth_ - 1.0f;
			float y = 1.0f - (2.0f * static_cast<float>(mouseY)) / viewportHeight_;

			// Create ray direction using camera matrices
			Matrix4f viewMatrix = viewMatrix_;
			Matrix4f projMatrix = projectionMatrix_;
			Matrix4f invViewProj = (projMatrix * viewMatrix).inverse();
			
			// Create points in NDC space
			Vector3f nearNDC(x, y, -1.0f);
			Vector3f farNDC(x, y, 1.0f);
			
			// Transform to world space
			Vector3f nearWorld = invViewProj.transformPoint(nearNDC);
			Vector3f farWorld = invViewProj.transformPoint(farNDC);
			
			// Calculate ray direction
			Vector3f rayDirection = farWorld - nearWorld;
			
			// Normalize the direction
			float length = std::sqrt(rayDirection.getX() * rayDirection.getX() + 
									rayDirection.getY() * rayDirection.getY() + 
									rayDirection.getZ() * rayDirection.getZ());
			if (length > 0.0f) {
				rayDirection.set(rayDirection.getX() / length, rayDirection.getY() / length, rayDirection.getZ() / length);
			}

			return rayDirection;
		}
	}

	Vector3f ManipulatorComponent::CalculateTranslationDelta(double mouseX, double mouseY, Rendering::Manipulators::ManipulatorAxis axis) const {
    // Calculate mouse movement in screen space
    float deltaX = static_cast<float>(mouseX - interactionState_.dragStartMousePos.getX()) * settings_.mouseSensitivity;
    float deltaY = static_cast<float>(mouseY - interactionState_.dragStartMousePos.getY()) * settings_.mouseSensitivity;


	float scale = this->CalculateAutoScale();
	deltaX *= scale;
	deltaY *= scale;

    // Start with delta in local manipulator space
    Vector3f localDelta = Vector3f::zero();

    // Map mouse movement to local manipulator axes
    switch (axis) {
    case Rendering::Manipulators::ManipulatorAxis::X:
        localDelta.setX(deltaX);
        break;
    case Rendering::Manipulators::ManipulatorAxis::Y:
        localDelta.setY(-deltaY); // Invert Y for screen space
        break;
    case Rendering::Manipulators::ManipulatorAxis::Z:
        localDelta.setZ(-deltaX);
        break;
    case Rendering::Manipulators::ManipulatorAxis::XY:
        localDelta.setX(deltaX);
        localDelta.setY(-deltaY);
        break;
    case Rendering::Manipulators::ManipulatorAxis::XZ:
        localDelta.setX(deltaX);
        localDelta.setZ(-deltaY);
        break;
    case Rendering::Manipulators::ManipulatorAxis::YZ:
        localDelta.setY(-deltaY);
        localDelta.setZ(deltaX);
        break;
    }

    // If the manipulator has rotation, transform the local delta to world space
    if (targetRotation_.getX() != 0.0f || targetRotation_.getY() != 0.0f || targetRotation_.getZ() != 0.0f) {
        // Create rotation matrix from the manipulator's current rotation
        const float DEG_TO_RAD = static_cast<float>(M_PI) / 180.0f;
        float rotationXRad = targetRotation_.getX() * DEG_TO_RAD;
        float rotationYRad = targetRotation_.getY() * DEG_TO_RAD;
        float rotationZRad = targetRotation_.getZ() * DEG_TO_RAD;
        
        // Create rotation matrices (same order as used in rendering)
        Matrix4f rotationX = Matrix4f::rotationX(rotationXRad);
        Matrix4f rotationY = Matrix4f::rotationY(rotationYRad);
        Matrix4f rotationZ = Matrix4f::rotationZ(rotationZRad);
        
        // Combine rotations: apply in order Z, Y, X (same as manipulator rendering)
        Matrix4f rotationMatrix = rotationX * rotationY * rotationZ;
        
        // Transform the local delta to world space using the rotation matrix
        Vector3f worldDelta = rotationMatrix.transformVector(localDelta);
        
        return ApplySnapping(worldDelta);
    }
    else {
        // No rotation, use local delta as-is
        return ApplySnapping(localDelta);
    }
}

	Vector3f ManipulatorComponent::CalculateRotationDelta(double mouseX, double mouseY, Rendering::Manipulators::ManipulatorAxis axis) const {
		float deltaX = static_cast<float>(mouseX - interactionState_.dragStartMousePos.getX()) * settings_.mouseSensitivity * 57.2958f; // Convert to degrees
		float deltaY = static_cast<float>(mouseY - interactionState_.dragStartMousePos.getY()) * settings_.mouseSensitivity * 57.2958f;

		Vector3f delta = Vector3f::zero();

		switch (axis) {
		case Rendering::Manipulators::ManipulatorAxis::X:
			delta.setX(deltaY);
			break;
		case Rendering::Manipulators::ManipulatorAxis::Y:
			delta.setY(deltaX);
			break;
		case Rendering::Manipulators::ManipulatorAxis::Z:
			delta.setZ(deltaX);
			break;
		}

		return ApplySnapping(delta);
	}
	Vector3f ManipulatorComponent::CalculateScaleDelta(double mouseX, double mouseY, Rendering::Manipulators::ManipulatorAxis axis) const {
		float deltaX = static_cast<float>(mouseX - interactionState_.dragStartMousePos.getX()) * settings_.mouseSensitivity;
		float deltaY = static_cast<float>(mouseY - interactionState_.dragStartMousePos.getY()) * settings_.mouseSensitivity;

		// Use combined movement for scale - positive delta = scale up, negative = scale down
		float scaleDelta = (deltaX - deltaY) * 0.5f;

		// Convert linear delta to multiplicative scale factor
		// Use exponential scaling for more intuitive behavior
		float scaleFactor = std::exp(scaleDelta);

		// Ensure minimum scale to prevent negative or zero values
		scaleFactor = std::max(scaleFactor, 0.01f);

		Vector3f scaleFactors = Vector3f::one();

		switch (axis) {
		case Rendering::Manipulators::ManipulatorAxis::X:
			scaleFactors.setX(scaleFactor);
			break;
		case Rendering::Manipulators::ManipulatorAxis::Y:
			scaleFactors.setY(scaleFactor);
			break;
		case Rendering::Manipulators::ManipulatorAxis::Z:
			scaleFactors.setZ(scaleFactor);
			break;
		case Rendering::Manipulators::ManipulatorAxis::XYZ:
			// Uniform scaling
			scaleFactors.set(scaleFactor, scaleFactor, scaleFactor);
			break;
		}

		return scaleFactors;
	}

	Vector3f ManipulatorComponent::ApplySnapping(const Vector3f& value) const {
		if (!settings_.enableSnapping) {
			return value;
		}

		Vector3f snapped = value;
		float threshold = settings_.snapThreshold;

		// Snap each component individually
		if (std::abs(snapped.getX()) > threshold) {
			snapped.setX(std::round(snapped.getX() / threshold) * threshold);
		}
		if (std::abs(snapped.getY()) > threshold) {
			snapped.setY(std::round(snapped.getY() / threshold) * threshold);
		}
		if (std::abs(snapped.getZ()) > threshold) {
			snapped.setZ(std::round(snapped.getZ() / threshold) * threshold);
		}

		return snapped;
	}

	float ManipulatorComponent::CalculateAutoScale() const {
		// Calculate distance from camera to manipulator
		Vector3f toManipulator = targetPosition_ - cameraPosition_;
		float distance = std::sqrt(toManipulator.getX() * toManipulator.getX() + 
								  toManipulator.getY() * toManipulator.getY() + 
								  toManipulator.getZ() * toManipulator.getZ());

		// Scale based on distance and settings
		return distance * settings_.autoScaleFactor;
	}

	void ManipulatorComponent::UpdateManipulatorScale() {
		if (manipulator_ && settings_.autoScale) {
			float autoScale = CalculateAutoScale();
			if (autoScale <= 0.0f)autoScale = 1.0f;

			float finalScale = settings_.manipulatorScale * autoScale;
			manipulator_->SetSize(finalScale);
		}
	}

	void ManipulatorComponent::Render() const 
	{
		if (manipulator_ && IsVisible()) 
		{			
			manipulator_->Render();
		}
	}

	void ManipulatorComponent::Reset() {
		interactionState_ = InteractionState();
		if (manipulator_) {
			manipulator_->Reset();
		}
	}

	bool ManipulatorComponent::TestRayIntersection(const Vector3f& rayOrigin, const Vector3f& rayDirection, float& distance) const {
		if (!manipulator_) {
			return false;
		}

		Rendering::Manipulators::ManipulatorAxis axis = manipulator_->TestRayIntersection(rayOrigin, rayDirection, distance);
		return axis != Rendering::Manipulators::ManipulatorAxis::None;
	}

	void ManipulatorComponent::UpdateCameraInfoFromViewport(Viewport* viewport) {
		if (!viewport) return;
		
		// Update manipulator viewport size based on active viewport dimensions
		SetViewportSize(viewport->getWidth(), viewport->getHeight());

		// Get camera component from viewport
		auto* cameraComponent = viewport->getCameraComponent();
		if (cameraComponent) {
			// Update manipulator with camera information
			Vector3f cameraPos = cameraComponent->GetCamera()->getPosition();
			Matrix4f viewMatrix = cameraComponent->GetCamera()->getViewMatrix();
			Matrix4f projMatrix = cameraComponent->GetCamera()->getProjectionMatrix();
			
			SetCameraInfo(cameraPos, viewMatrix, projMatrix);
		}
		
		// Update manipulator scale based on new camera information
		UpdateManipulatorScale();
	}

} // namespace ComponentSystem