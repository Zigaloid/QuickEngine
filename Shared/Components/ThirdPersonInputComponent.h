#pragma once

#include "ComponentSystem/ComponentSystem.h"
#include "Input/InputAction.h"
#include "Math/Vector3f.h"

#include <Jolt/Jolt.h>
JPH_SUPPRESS_WARNINGS
#include <Jolt/Physics/Body/BodyID.h>

#include <string>
#include <memory>

namespace Input { class InputActionManager; class ActionContext; }
class CCharacterComponent;
class CCameraComponent;

/** @brief Drives a parent entity's physics body from input actions using a character controller model.
 *
 *  Attach as a child of a CEntityComponent that also has a CCharacterComponent sibling.
 *  Register an InputActionManager and push an ActionContext with the expected action names
 *  before use.
 *
 *  ### Expected action names (all configurable)
 *  | Default name | Recommended binding                       | Effect                             |
 *  |---|---|---|
 *  | "MoveX"      | GamepadAxisContinuous(LeftX)              | Horizontal movement (world X)      |
 *  | "MoveY"      | GamepadAxisContinuous(LeftY)              | Depth movement (world -Z)          |
 *  | "CameraX"    | GamepadAxisContinuous(RightX)             | Camera yaw (left/right rotation)   |
 *  | "CameraY"    | GamepadAxisContinuous(RightY)             | Camera pitch (up/down rotation)    |
 *  | "Jump"       | GamepadButton_(A, Pressed)                | Vertical impulse when grounded     |
 *  | "Sprint"     | GamepadButton_(LeftBumper, Continuous)    | Speed multiplier while held        |
 *
 *  ### Movement model (improved from Jolt's CharacterTest)
 *  - Velocity blending: smooth acceleration/deceleration via interpolation
 *  - Ground contact detection: tracks contacts to determine supported state
 *  - Slope validation: prevents climbing unclimbable surfaces
 *  - Jump control: restricted to ground or configurable air jumps
 *  - Movement during jump: optional to prevent unintended mid-air steering
 *
 *  ### Camera control
 *  - Right thumb stick orbits the camera around the character
 *  - Configurable rotation speeds for yaw (horizontal) and pitch (vertical)
 *  - Optional invert for pitch axis
 *
 *  @code
 *  auto* inputComp = entity->CreateChild<CThirdPersonInputComponent>();
 *  inputComp->SetActionManager(&actionManager);
 *  inputComp->SetMoveSpeed(6.0f);
 *  inputComp->SetJumpImpulse(7.0f);
 *  inputComp->SetVelocityBlendFactor(0.25f);  // 25% desired, 75% current
 *  inputComp->SetMaxSlopeAngle(45.0f);        // Don't climb steeper than 45 degrees
 *  inputComp->SetControlMovementDuringJump(false);  // Disable mid-air steering
 *  inputComp->SetCameraYawSpeed(2.0f);        // Camera yaw speed
 *  inputComp->SetCameraPitchSpeed(1.5f);      // Camera pitch speed
 *  @endcode
 */
class CThirdPersonInputComponent : public ComponentSystem::Component
{
public:
    REFL_DECLARE_OBJECT(CThirdPersonInputComponent, Component);
    DECLARE_COMPONENT();

    CThirdPersonInputComponent() = default;
    ~CThirdPersonInputComponent() override;

    // ?? Action manager ????????????????????????????????????????????????????????

    /** Set the InputActionManager to subscribe to.
     *  Must be called before OnInitialize, or will re-subscribe immediately if
     *  the component is already initialized. */
    void SetActionManager(Input::InputActionManager* manager);
    Input::InputActionManager* GetActionManager() const { return m_actionManager; }

    // ?? Action name bindings ??????????????????????????????????????????????????

    void SetMoveXAction(std::string name)  { m_moveXAction  = std::move(name); }
    void SetMoveYAction(std::string name)  { m_moveYAction  = std::move(name); }
    void SetJumpAction(std::string name)   { m_jumpAction   = std::move(name); }
    void SetSprintAction(std::string name) { m_sprintAction = std::move(name); }

    const std::string& GetMoveXAction()  const { return m_moveXAction; }
    const std::string& GetMoveYAction()  const { return m_moveYAction; }
    const std::string& GetJumpAction()   const { return m_jumpAction; }
    const std::string& GetSprintAction() const { return m_sprintAction; }

    // ?? Camera action bindings ????????????????????????????????????????????????????

    void SetCameraXAction(std::string name) { m_cameraXAction = std::move(name); }
    void SetCameraYAction(std::string name) { m_cameraYAction = std::move(name); }

    const std::string& GetCameraXAction() const { return m_cameraXAction; }
    const std::string& GetCameraYAction() const { return m_cameraYAction; }

    // ?? Movement parameters ???????????????????????????????????????????????????

    void  SetMoveSpeed(float speed)              { m_moveSpeed        = speed; }
    float GetMoveSpeed()                   const { return m_moveSpeed; }

    void  SetSprintMultiplier(float mult)        { m_sprintMultiplier = mult; }
    float GetSprintMultiplier()            const { return m_sprintMultiplier; }

    void  SetJumpImpulse(float impulse)          { m_jumpImpulse      = impulse; }
    float GetJumpImpulse()                 const { return m_jumpImpulse; }

    /** Maximum horizontal speed magnitude (m/s). 0 = no cap. */
    void  SetMaxMoveSpeed(float maxSpeed)        { m_maxMoveSpeed     = maxSpeed; }
    float GetMaxMoveSpeed()                const { return m_maxMoveSpeed; }

    /** Velocity blend factor (0-1): determines how much desired velocity influences current velocity.
     *  0.0 = no change (100% current), 1.0 = instant (100% desired).
     *  Typical value: 0.25 for smooth acceleration/deceleration. */
    void  SetVelocityBlendFactor(float factor)   { m_velocityBlendFactor = factor; }
    float GetVelocityBlendFactor()         const { return m_velocityBlendFactor; }

    /** Maximum climbable slope angle in degrees. Prevents sliding up steep surfaces. */
    void  SetMaxSlopeAngle(float degrees)        { m_maxSlopeAngleDeg = degrees; }
    float GetMaxSlopeAngle()               const { return m_maxSlopeAngleDeg; }

    /** Allow movement input while in the air (during jump). */
    void  SetControlMovementDuringJump(bool allow) { m_controlMovementDuringJump = allow; }
    bool  GetControlMovementDuringJump()   const { return m_controlMovementDuringJump; }

    // ?? Camera control parameters ?????????????????????????????????????????????

    /** Set the rotation speed for camera yaw (left/right rotation) in radians per second. */
    void  SetCameraYawSpeed(float speed)        { m_cameraYawSpeed = speed; }
    float GetCameraYawSpeed()           const { return m_cameraYawSpeed; }

    /** Set the rotation speed for camera pitch (up/down rotation) in radians per second. */
    void  SetCameraPitchSpeed(float speed)      { m_cameraPitchSpeed = speed; }
    float GetCameraPitchSpeed()         const { return m_cameraPitchSpeed; }

    /** Invert the pitch axis (Y input). */
    void  SetInvertCameraPitch(bool invert)     { m_invertCameraPitch = invert; }
    bool  GetInvertCameraPitch()        const { return m_invertCameraPitch; }

    // ?? Ground check ??????????????????????????????????????????????????????????

    /** Allow jumping even when not grounded (e.g. during air time). */
    void SetAllowAirJump(bool allow)             { m_allowAirJump     = allow; }
    bool GetAllowAirJump()                 const { return m_allowAirJump; }

    /** Query ground support state. */
    bool IsGrounded()                      const { return m_isGrounded; }

    /** Get the current ground normal (Y-axis removed for slope detection). */
    Vector3f GetGroundNormal()             const { return m_groundNormal; }

    // ?? Camera control ?????????????????????????????????????????????????????????

    /** Enable/disable camera-relative movement. When enabled, movement is relative to camera direction. */
    void  SetUseCameraRelativeMovement(bool use) { m_useCameraRelativeMovement = use; }
    bool  GetUseCameraRelativeMovement() const { return m_useCameraRelativeMovement; }

    /** Get the camera component (if attached to parent). */
    CCameraComponent* GetCamera() const { return m_camera; }

    /** Manually set the camera component (usually auto-found). */
    void SetCamera(CCameraComponent* camera) { m_camera = camera; }

protected:
    bool OnInitialize()             override;
    void OnUpdate(double deltaTime) override;
    void OnShutdown()               override;

private:
    void Subscribe();
    void Unsubscribe();

    /** Evaluate ground state based on contact manifold. */
    void UpdateGroundState();

    /** Cancel movement opposing ground normal when on steep ground. */
    JPH::Vec3 GetDesiredVelocity() const;

    /** Transform input velocity to camera-relative coordinates. */
    JPH::Vec3 GetCameraRelativeVelocity(const JPH::Vec3& inputVelocity) const;

    // ?? Wired dependencies ????????????????????????????????????????????????????

    Input::InputActionManager* m_actionManager = nullptr;
    CCharacterComponent* m_character = nullptr;
    CCameraComponent* m_camera = nullptr;
    std::shared_ptr<Input::ActionContext> m_context;

    // ?? Action names ??????????????????????????????????????????????????????????

    std::string m_moveXAction  = "MoveX";
    std::string m_moveYAction  = "MoveY";
    std::string m_jumpAction   = "Jump";
    std::string m_sprintAction = "Sprint";
    std::string m_cameraXAction = "CameraX";
    std::string m_cameraYAction = "CameraY";

    // ?? Parameters ????????????????????????????????????????????????????????????

    float m_moveSpeed              = 5.0f;
    float m_sprintMultiplier       = 2.0f;
    float m_jumpImpulse            = 6.0f;
    float m_maxMoveSpeed           = 0.0f;
    float m_velocityBlendFactor    = 0.25f;    // New: smooth acceleration
    float m_maxSlopeAngleDeg       = 45.0f;    // New: slope restriction
    bool  m_controlMovementDuringJump = true;  // New: allow mid-air control
    bool  m_allowAirJump           = false;
    bool  m_useCameraRelativeMovement = true;  // New: use camera-relative movement

    // Camera control parameters (NEW)
    float m_cameraYawSpeed         = 2.0f;     // Radians per second
    float m_cameraPitchSpeed       = 1.5f;     // Radians per second
    bool  m_invertCameraPitch      = false;    // Invert Y axis

    // ?? Runtime state ?????????????????????????????????????????????????????????

    float m_inputX      = 0.f;
    float m_inputZ      = 0.f;
    float m_cameraInputX = 0.f;  // NEW: Right stick X (yaw)
    float m_cameraInputY = 0.f;  // NEW: Right stick Y (pitch)
    bool  m_isSprinting = false;
    bool  m_jumpQueued  = false;

    // New: ground state tracking
    bool     m_isGrounded     = false;
    Vector3f m_groundNormal   = Vector3f(0.0f, 1.0f, 0.0f);
    int      m_groundContactCount = 0;

    // Listener handles for safe unsubscription
    Input::ActionListenerHandle m_hMoveX;
    Input::ActionListenerHandle m_hMoveY;
    Input::ActionListenerHandle m_hJump;
    Input::ActionListenerHandle m_hSprint;
    Input::ActionListenerHandle m_hCameraX;  // NEW
    Input::ActionListenerHandle m_hCameraY;  // NEW
};
