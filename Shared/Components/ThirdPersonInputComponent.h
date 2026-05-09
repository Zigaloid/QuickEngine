#pragma once

#include "ComponentSystem/ComponentSystem.h"
#include "Input/InputAction.h"
#include "Math/Vector3f.h"

#include <string>
#include <memory>

namespace Input { class InputActionManager; class ActionContext; }
class CPhysicsBodyComponent;

/** @brief Drives a parent entity's physics body from input actions.
 *
 *  Attach as a child of a CEntityComponent that also has a CPhysicsBodyComponent sibling.
 *  Register an InputActionManager and push an ActionContext with the expected action names
 *  before use.
 *
 *  ### Expected action names (all configurable)
 *  | Default name | Recommended binding                       | Effect                             |
 *  |---|---|---|
 *  | "MoveX"      | GamepadAxisContinuous(LeftX)              | Horizontal movement (world X)      |
 *  | "MoveY"      | GamepadAxisContinuous(LeftY)              | Depth movement (world -Z)          |
 *  | "Jump"       | GamepadButton_(A, Pressed)                | Vertical impulse when grounded     |
 *  | "Sprint"     | GamepadButton_(LeftBumper, Continuous)    | Speed multiplier while held        |
 *
 *  ### Movement model
 *  Each frame the horizontal velocity (X/Z) is set directly so that Jolt gravity
 *  continues to act on the Y axis undisturbed.  A jump queues an upward velocity
 *  addition that is applied once on the next OnUpdate.
 *
 *  @code
 *  auto* inputComp = entity->CreateChild<CThirdPersonInputComponent>();
 *  inputComp->SetActionManager(&actionManager);
 *  inputComp->SetMoveSpeed(6.0f);
 *  inputComp->SetJumpImpulse(7.0f);
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

    // ?? Ground check ??????????????????????????????????????????????????????????

    /** Allow jumping even when not grounded (e.g. during air time). */
    void SetAllowAirJump(bool allow)             { m_allowAirJump     = allow; }
    bool GetAllowAirJump()                 const { return m_allowAirJump; }

    /** Vertical velocity threshold below which the entity is considered grounded. */
    void  SetGroundedVelocityThreshold(float t)  { m_groundedVelThreshold = t; }
    float GetGroundedVelocityThreshold()   const { return m_groundedVelThreshold; }

protected:
    bool OnInitialize()             override;
    void OnUpdate(double deltaTime) override;
    void OnShutdown()               override;

private:
    void Subscribe();
    void Unsubscribe();

    ComponentSystem::Component* FindPhysicsBody() const;
    bool                        IsGrounded()      const;

    // ?? Wired dependencies ????????????????????????????????????????????????????

    Input::InputActionManager* m_actionManager = nullptr;
    ComponentSystem::Component* m_physicsBody   = nullptr;
    std::shared_ptr<Input::ActionContext> m_context;

    // ?? Action names ??????????????????????????????????????????????????????????

    std::string m_moveXAction  = "MoveX";
    std::string m_moveYAction  = "MoveY";
    std::string m_jumpAction   = "Jump";
    std::string m_sprintAction = "Sprint";

    // ?? Parameters ????????????????????????????????????????????????????????????

    float m_moveSpeed            = 5.0f;
    float m_sprintMultiplier     = 2.0f;
    float m_jumpImpulse          = 6.0f;
    float m_maxMoveSpeed         = 0.0f;
    float m_groundedVelThreshold = 0.1f;
    bool  m_allowAirJump         = false;

    // ?? Runtime state ?????????????????????????????????????????????????????????

    float m_inputX      = 0.f;
    float m_inputZ      = 0.f;
    bool  m_isSprinting = false;
    bool  m_jumpQueued  = false;

    // Listener handles for safe unsubscription
    Input::ActionListenerHandle m_hMoveX;
    Input::ActionListenerHandle m_hMoveY;
    Input::ActionListenerHandle m_hJump;
    Input::ActionListenerHandle m_hSprint;
};
