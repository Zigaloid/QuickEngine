#include "ThirdPersonInputComponent.h"
#include "PhysicsBodyComponent.h"
#include "CharacterComponent.h"
#include "Input/InputActionManager.h"
#include "Input/InputBinding.h"
#include "Input/ActionContext.h"
#include "Physics/PhysicsManager.h"

#include <Jolt/Jolt.h>
JPH_SUPPRESS_WARNINGS
#include <Jolt/Physics/Body/BodyInterface.h>

// ?? Reflection & registry ??????????????????????????????????????????????????????

REFL_DEFINE_OBJECT(CThirdPersonInputComponent)
REFL_DEFINE_END

REGISTER_COMPONENT(CThirdPersonInputComponent, "ThirdPersonInput", "Input");

// ?? Destructor ????????????????????????????????????????????????????????????????

CThirdPersonInputComponent::~CThirdPersonInputComponent()
{
    Unsubscribe();
}

// ?? Lifecycle ?????????????????????????????????????????????????????????????????

bool CThirdPersonInputComponent::OnInitialize()
{
    m_physicsBody = FindPhysicsBody();
    // Attempt to locate an InputActionManager created by the application.
    if (!m_actionManager)
    {
        // Prefer the CoreSystem singleton if present, otherwise look for a component instance.
        m_actionManager = Core::CoreSystem::GetActionManager();
    }

    if (m_actionManager)
        SetActionManager(m_actionManager);
    return true;
}

void CThirdPersonInputComponent::OnUpdate(double /*deltaTime*/)
{
    if (!m_physicsBody)
    {
        // Try again each frame until the sibling body is ready.
        m_physicsBody = FindPhysicsBody();
        if (!m_physicsBody)
            return;
    }

    JPH::BodyID bodyId = JPH::BodyID();
    if (auto pbody = dynamic_cast<CPhysicsBodyComponent*>(m_physicsBody))
        bodyId = pbody->GetBodyID();
    else if (auto cbody = dynamic_cast<CCharacterComponent*>(m_physicsBody))
        bodyId = cbody->GetBodyID();

    if (bodyId.IsInvalid())
        return;

    PhysicsManager* physics = PhysicsManager::Get();
    if (!physics || !physics->IsInitialized())
        return;

    JPH::BodyInterface& bi = physics->GetBodyInterface();

    // ?? Horizontal velocity ???????????????????????????????????????????????????

    const float speed = m_moveSpeed * (m_isSprinting ? m_sprintMultiplier : 1.0f);
    float moveX       = m_inputX * speed;
    float moveZ       = -m_inputZ * speed; // input Y maps to world -Z (forward)

    if (m_maxMoveSpeed > 0.0f)
    {
        const float hLen = std::sqrt(moveX * moveX + moveZ * moveZ);
        if (hLen > m_maxMoveSpeed)
        {
            const float inv = m_maxMoveSpeed / hLen;
            moveX *= inv;
            moveZ *= inv;
        }
    }

    // Preserve vertical (Y) velocity so Jolt gravity continues to act normally.
    JPH::Vec3 vel = bi.GetLinearVelocity(bodyId);
    vel            = JPH::Vec3(moveX, vel.GetY(), moveZ);
    bi.SetLinearVelocity(bodyId, vel);

    // ?? Jump ??????????????????????????????????????????????????????????????????

    if (m_jumpQueued)
    {
        m_jumpQueued = false;
        if (m_allowAirJump || IsGrounded())
        {
            bi.AddLinearVelocity(bodyId, JPH::Vec3(0.0f, m_jumpImpulse, 0.0f));
        }
    }
}

void CThirdPersonInputComponent::OnShutdown()
{
    Unsubscribe();
    // Pop our context from the action manager if it's still on top
    if (m_actionManager && m_context)
    {
        auto top = m_actionManager->GetTopContext();
        if (top && top == m_context)
        {
            m_actionManager->PopContext();
        }
    }

    m_context.reset();
    m_physicsBody = nullptr;
}

// ?? Action manager wiring ?????????????????????????????????????????????????????

void CThirdPersonInputComponent::SetActionManager(Input::InputActionManager* manager)
{
    if (m_actionManager != manager)
    {
        Unsubscribe();
        m_actionManager = manager;
    }

    // If we're initialized, create/push our context and subscribe immediately.
    if (m_actionManager && !m_context)
    {
        m_context = std::make_shared<Input::ActionContext>(std::string("ThirdPerson_") + std::to_string(GetId()));
        m_context->AddBinding(Input::InputBinding::GamepadAxisContinuous("MoveX", Input::GamepadAxis::LeftX));
        m_context->AddBinding(Input::InputBinding::GamepadAxisContinuous("MoveY", Input::GamepadAxis::LeftY));
        m_context->AddBinding(Input::InputBinding::GamepadButton_("Jump", Input::GamepadButton::A));
        m_context->AddBinding(Input::InputBinding::GamepadButton_("Sprint", Input::GamepadButton::LeftBumper, Input::TriggerType::Continuous));

        m_actionManager->PushContext(m_context, false);
    }

    Subscribe();
}

// ?? Subscription helpers ??????????????????????????????????????????????????????

void CThirdPersonInputComponent::Subscribe()
{
    if (!m_actionManager)
        return;

    // MoveX — continuous axis: cache the current value each frame
    m_hMoveX = m_actionManager->Subscribe(m_moveXAction, [this](const Input::ActionEvent& e)
    {
        if (e.phase == Input::ActionPhase::Ongoing || e.phase == Input::ActionPhase::Started)
            m_inputX = e.value.x;
        else if (e.phase == Input::ActionPhase::Completed || e.phase == Input::ActionPhase::Canceled)
            m_inputX = 0.f;
    });

    // MoveY — continuous axis: cache the current value each frame
    m_hMoveY = m_actionManager->Subscribe(m_moveYAction, [this](const Input::ActionEvent& e)
    {
        if (e.phase == Input::ActionPhase::Ongoing || e.phase == Input::ActionPhase::Started)
            m_inputZ = e.value.x;
        else if (e.phase == Input::ActionPhase::Completed || e.phase == Input::ActionPhase::Canceled)
            m_inputZ = 0.f;
    });

    // Jump — queue once on the initial press
    m_hJump = m_actionManager->Subscribe(m_jumpAction, [this](const Input::ActionEvent& e)
    {
        if (e.phase == Input::ActionPhase::Started)
            m_jumpQueued = true;
    });

    // Sprint — active while the action is ongoing
    m_hSprint = m_actionManager->Subscribe(m_sprintAction, [this](const Input::ActionEvent& e)
    {
        m_isSprinting = (e.phase == Input::ActionPhase::Started ||
                         e.phase == Input::ActionPhase::Ongoing);
    });
}

void CThirdPersonInputComponent::Unsubscribe()
{
    if (!m_actionManager)
        return;

    m_actionManager->Unsubscribe(m_hMoveX);
    m_actionManager->Unsubscribe(m_hMoveY);
    m_actionManager->Unsubscribe(m_hJump);
    m_actionManager->Unsubscribe(m_hSprint);

    m_hMoveX   = {};
    m_hMoveY   = {};
    m_hJump    = {};
    m_hSprint  = {};

    // Clear runtime state so stale inputs don't carry over if re-attached.
    m_inputX      = 0.f;
    m_inputZ      = 0.f;
    m_isSprinting = false;
    m_jumpQueued  = false;
}

// ?? Private helpers ???????????????????????????????????????????????????????????

ComponentSystem::Component* CThirdPersonInputComponent::FindPhysicsBody() const
{
    ComponentSystem::Component* parent = GetParent();
    if (!parent)
        return nullptr;

    // Prefer the legacy physics body component, but fall back to the character component if present.
    if (auto p = parent->FindChild<CPhysicsBodyComponent>())
        return p;
    return parent->FindChild<CCharacterComponent>();
}

bool CThirdPersonInputComponent::IsGrounded() const
{
    if (!m_physicsBody)
        return false;

    JPH::BodyID bodyId = JPH::BodyID();
    if (auto pbody = dynamic_cast<CPhysicsBodyComponent*>(m_physicsBody))
        bodyId = pbody->GetBodyID();
    else if (auto cbody = dynamic_cast<CCharacterComponent*>(m_physicsBody))
        bodyId = cbody->GetBodyID();
    if (bodyId.IsInvalid())
        return false;

    PhysicsManager* physics = PhysicsManager::Get();
    if (!physics || !physics->IsInitialized())
        return false;

    const JPH::Vec3 vel = physics->GetBodyInterface().GetLinearVelocity(bodyId);
    return std::abs(vel.GetY()) <= m_groundedVelThreshold;
}
