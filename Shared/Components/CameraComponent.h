#pragma once

#include "ComponentSystem/ComponentSystem.h"
#include "Rendering/BgfxGameCamera.h"
#include "Math/Matrix4f.h"
#include "Math/Vector3f.h"

/**
 * @brief Third-person camera component that follows a character.
 *
 *  Attach as a child of a CEntityComponent (same as your character).
 *  The camera will automatically follow the entity's position and maintain
 *  a configurable orbit distance and angle.
 *
 *  ### Features
 *  - Automatic follow: tracks character position
 *  - Orbit control: configurable yaw/pitch angles
 *  - Distance control: configurable distance from character
 *  - Smooth transitions: updates camera matrices each frame
 *
 *  @code
 *  auto* cameraComp = entity->CreateChild<CCameraComponent>();
 *  cameraComp->SetDistance(5.0f);      // 5 meters behind character
 *  cameraComp->SetYaw(0.0f);           // Look directly ahead
 *  cameraComp->SetPitch(0.3f);         // Look down slightly
 *  cameraComp->SetFOV(60.0f);          // 60 degree field of view
 *  @endcode
 */
class CCameraComponent : public ComponentSystem::Component
{
public:
    REFL_DECLARE_OBJECT(CCameraComponent, Component);
    DECLARE_COMPONENT();

    CCameraComponent() = default;
    ~CCameraComponent() override = default;

    // ?? Camera positioning ????????????????????????????????????????????

    /** Set how far the camera is from the character (in meters). */
    void SetDistance(float distance)
    {
        m_distance = distance;
        if (m_distance < 0.1f) m_distance = 0.1f;
    }

    float GetDistance() const { return m_distance; }

    /** Orbit the camera by adjusting yaw/pitch. */
    void Orbit(float deltaYaw, float deltaPitch)
    {
        m_camera.Orbit(deltaYaw, deltaPitch);
    }

    /** Pan the camera (move up/down and left/right). */
    void Pan(float deltaRight, float deltaUp)
    {
        m_camera.Pan(deltaRight, deltaUp);
    }

    /** Zoom the camera closer or farther. */
    void Zoom(float delta)
    {
        m_camera.Zoom(delta);
    }

    // ?? Camera projection ?????????????????????????????????????????????

    /** Set field of view in degrees. */
    void SetFOV(float fovDeg) { m_camera.SetFov(fovDeg); }
    float GetFOV() const { return m_camera.GetFov(); }

    /** Set near plane distance. */
    void SetNearPlane(float nearPlane)
    {
        // Store for recreation; BgfxGameCamera doesn't expose setter
        m_nearPlane = nearPlane;
    }

    float GetNearPlane() const { return m_nearPlane; }

    /** Set far plane distance. */
    void SetFarPlane(float farPlane)
    {
        // Store for recreation; BgfxGameCamera doesn't expose setter
        m_farPlane = farPlane;
    }

    float GetFarPlane() const { return m_farPlane; }

    // ?? Matrix generation ?????????????????????????????????????????????

    /** Get the camera's view matrix for rendering. */
    void GetViewMatrix(float* outMatrix) const
    {
        m_camera.GetViewMatrix(outMatrix);
    }

    /** Get the camera's projection matrix for rendering. */
    void GetProjectionMatrix(float* outMatrix, float aspectRatio) const
    {
        m_camera.GetProjectionMatrix(outMatrix, aspectRatio);
    }

    // ?? Direct camera access ??????????????????????????????????????????

    BgfxGameCamera& GetCamera() { return m_camera; }
    const BgfxGameCamera& GetCamera() const { return m_camera; }

protected:
    bool OnInitialize() override;
    void OnUpdate(double deltaTime) override;

private:
    BgfxGameCamera m_camera;

    // Camera positioning relative to character
    float m_distance = 5.0f;        // Distance behind character
    Vector3f m_offset{0.0f, 1.5f, 0.0f}; // Height offset above character

    // Camera projection
    float m_nearPlane = 0.1f;
    float m_farPlane = 1000.0f;
};
