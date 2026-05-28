#include "ThirdPersonInputComponent.h"
#include "PhysicsBodyComponent.h"
#include "CharacterComponent.h"
#include "CameraComponent.h"
#include "EntityComponent.h"
#include "Input/InputActionManager.h"
#include "Input/InputBinding.h"
#include "Input/ActionContext.h"
#include "Physics/PhysicsManager.h"

#include <Jolt/Jolt.h>
JPH_SUPPRESS_WARNINGS
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>

#include <cmath>
#include <bx/math.h>

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
    m_character = FindSibling<CCharacterComponent>();
    m_camera = FindSibling<CCameraComponent>();

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

void CThirdPersonInputComponent::OnUpdate(double deltaTime)
{
    if (!m_character)
    {
        // Try again each frame until the sibling character is ready.
        m_character = FindSibling<CCharacterComponent>();
        if (!m_character)
            return;
    }

    // Try to find camera if not already found
    if (!m_camera && m_useCameraRelativeMovement)
    {
        m_camera = FindSibling<CCameraComponent>();
    }

    JPH::BodyID bodyId = m_character->GetBodyID();

    if (bodyId.IsInvalid())
        return;

    PhysicsManager* physics = PhysicsManager::Get();
    if (!physics || !physics->IsInitialized())
        return;

    JPH::BodyInterface& bi = physics->GetBodyInterface();

    // Evaluate ground state from contact manifold
    UpdateGroundState();

    // Determine if we can apply movement
    bool canMove = m_controlMovementDuringJump || m_isGrounded;

    // ?? Desired horizontal velocity ?????????????????????????????????????

    JPH::Vec3 desiredVelocity = GetDesiredVelocity();

    // Apply camera-relative movement if enabled and camera is available
    if (m_useCameraRelativeMovement && m_camera)
    {
        desiredVelocity = GetCameraRelativeVelocity(desiredVelocity);
    }

    // ?? Apply velocity blending ??????????????????????????????????????????????????????

    JPH::Vec3 currentVel = bi.GetLinearVelocity(bodyId);
    JPH::Vec3 newVelocity;

    if (canMove)
    {
        // Blend current velocity with desired: (1-blend) * current + blend * desired
        const float blendFactor = m_velocityBlendFactor;
        newVelocity = JPH::Vec3(
            (1.0f - blendFactor) * currentVel.GetX() + blendFactor * desiredVelocity.GetX(),
            currentVel.GetY(),  // Preserve Y for gravity
            (1.0f - blendFactor) * currentVel.GetZ() + blendFactor * desiredVelocity.GetZ()
        );
    }
    else
    {
        // In air and movement disabled: preserve current velocity but don't accelerate
        newVelocity = JPH::Vec3(
            currentVel.GetX(),
            currentVel.GetY(),  // Preserve Y for gravity
            currentVel.GetZ()
        );
    }

    bi.SetLinearVelocity(bodyId, newVelocity);

    // ?? Maintain upright orientation ????????????????????????????????????????

    // Prevent the character from tipping over by zeroing angular velocity
    // This keeps the character's orientation stable during movement
    bi.SetAngularVelocity(bodyId, JPH::Vec3::sZero());

    // ?? Camera orbit control ???????????????????????????????????????????????????

    if (m_camera && (m_cameraInputX != 0.0f || m_cameraInputY != 0.0f))
    {
        // Apply time scaling for smooth rotation
        float deltaTime_s = static_cast<float>(deltaTime);

        // Calculate rotation deltas
        float yawDelta = m_cameraInputX * m_cameraYawSpeed * deltaTime_s;
        float pitchDelta = m_cameraInputY * m_cameraPitchSpeed * deltaTime_s;

        // Optionally invert pitch
        if (m_invertCameraPitch)
            pitchDelta = -pitchDelta;

        // Apply orbit to camera
        m_camera->Orbit(yawDelta, pitchDelta);
    }

    // ?? Jump ??????????????????????????????????????????????????????????????????

    if (m_jumpQueued && (m_allowAirJump || m_isGrounded))
    {
        m_jumpQueued = false;  // Consume the jump input
        bi.AddLinearVelocity(bodyId, JPH::Vec3(0.0f, m_jumpImpulse, 0.0f));
    }
    else if (m_jumpQueued && !m_isGrounded && !m_allowAirJump)
    {
        // Player tried to jump but couldn't - clear the queue so it doesn't persist
        // This allows re-queuing on the next press
        m_jumpQueued = false;
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
    m_character = nullptr;
    Component::OnShutdown();
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
        m_context->AddBinding(Input::InputBinding::GamepadAxisContinuous(m_cameraXAction, Input::GamepadAxis::RightX));
        m_context->AddBinding(Input::InputBinding::GamepadAxisContinuous(m_cameraYAction, Input::GamepadAxis::RightY));

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
            m_inputZ = e.value.x;  // Single-axis binding uses e.value.x
        else if (e.phase == Input::ActionPhase::Completed || e.phase == Input::ActionPhase::Canceled)
            m_inputZ = 0.f;
    });

	// Jump — queue once on the initial press
	m_hJump = m_actionManager->Subscribe(m_jumpAction, [this](const Input::ActionEvent& e)
	{
		if (e.phase == Input::ActionPhase::Started)
			m_jumpQueued = true;
		// Note: Don't clear the flag here - let OnUpdate() consume it
	});

    // Sprint — active while the action is ongoing
    m_hSprint = m_actionManager->Subscribe(m_sprintAction, [this](const Input::ActionEvent& e)
    {
        m_isSprinting = (e.phase == Input::ActionPhase::Started ||
                         e.phase == Input::ActionPhase::Ongoing);
    });

    // CameraX — continuous axis for yaw (left/right)
    m_hCameraX = m_actionManager->Subscribe(m_cameraXAction, [this](const Input::ActionEvent& e)
    {
        if (e.phase == Input::ActionPhase::Ongoing || e.phase == Input::ActionPhase::Started)
            m_cameraInputX = e.value.x;
        else if (e.phase == Input::ActionPhase::Completed || e.phase == Input::ActionPhase::Canceled)
            m_cameraInputX = 0.f;
    });

    // CameraY — continuous axis for pitch (up/down)
    m_hCameraY = m_actionManager->Subscribe(m_cameraYAction, [this](const Input::ActionEvent& e)
    {
        if (e.phase == Input::ActionPhase::Ongoing || e.phase == Input::ActionPhase::Started)
            m_cameraInputY = e.value.x;  // Single-axis binding uses e.value.x
        else if (e.phase == Input::ActionPhase::Completed || e.phase == Input::ActionPhase::Canceled)
            m_cameraInputY = 0.f;
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
    m_actionManager->Unsubscribe(m_hCameraX);
    m_actionManager->Unsubscribe(m_hCameraY);

    m_hMoveX    = {};
    m_hMoveY    = {};
    m_hJump     = {};
    m_hSprint   = {};
    m_hCameraX  = {};
    m_hCameraY  = {};

    // Clear runtime state so stale inputs don't carry over if re-attached.
    m_inputX       = 0.f;
    m_inputZ       = 0.f;
    m_cameraInputX = 0.f;
    m_cameraInputY = 0.f;
    m_isSprinting  = false;
    m_jumpQueued   = false;
}

// ?? Private helpers ???????????????????????????????????????????????????????????

void CThirdPersonInputComponent::UpdateGroundState()
{
    if (!m_character)
    {
        m_isGrounded = false;
        m_groundContactCount = 0;
        return;
    }

    JPH::BodyID bodyId = m_character->GetBodyID();
    if (bodyId.IsInvalid())
    {
        m_isGrounded = false;
        m_groundContactCount = 0;
        return;
    }

    PhysicsManager* physics = PhysicsManager::Get();
    if (!physics || !physics->IsInitialized())
    {
        m_isGrounded = false;
        m_groundContactCount = 0;
        return;
    }

    JPH::BodyInterface& bi = physics->GetBodyInterface();
    JPH::Vec3 vel = bi.GetLinearVelocity(bodyId);

    // Simple velocity-based ground check: if falling slowly or moving upward slightly, not grounded
    // If velocity Y is very low (near zero or slightly negative), consider grounded
    const float groundedThreshold = 0.5f;  // m/s - allow slight upward velocity
    m_isGrounded = vel.GetY() <= groundedThreshold;
}


JPH::Vec3 CThirdPersonInputComponent::GetDesiredVelocity() const
{
    const float speed = m_moveSpeed * (m_isSprinting ? m_sprintMultiplier : 1.0f);
    float moveX = m_inputX * speed;
    float moveZ = -m_inputZ * speed;  // Negate Y input to map to Z axis correctly

    // Clamp to max speed if specified
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

    // Check slope: reject movement that would climb steep surfaces
    if (m_isGrounded && (moveX != 0.0f || moveZ != 0.0f))
    {
        // Convert ground normal to work with it
        JPH::Vec3 normalHorizontal(m_groundNormal.GetX(), 0.0f, m_groundNormal.GetZ());
        const float normalLenSq = normalHorizontal.LengthSq();

        if (normalLenSq > 0.0001f)
        {
            JPH::Vec3 moveDir(moveX, 0.0f, moveZ);
            const float dot = moveDir.Dot(normalHorizontal);

            // If moving into the surface normal (dot < 0), check slope angle
            if (dot < 0.0f)
            {
                const float cosMaxSlope = std::cos(m_maxSlopeAngleDeg * 3.14159265f / 180.0f);
                const float cosActualSlope = std::abs(m_groundNormal.GetY());

                // If slope is steeper than max, cancel movement in that direction
                if (cosActualSlope < cosMaxSlope)
                {
                    const float adjustment = dot / normalLenSq;
                    moveX -= adjustment * normalHorizontal.GetX();
                    moveZ -= adjustment * normalHorizontal.GetZ();
                }
            }
        }
    }

    return JPH::Vec3(moveX, 0.0f, moveZ);
}

JPH::Vec3 CThirdPersonInputComponent::GetCameraRelativeVelocity(const JPH::Vec3& inputVelocity) const
{
    if (!m_camera)
        return inputVelocity;

    // Get camera's view matrix to extract forward and right vectors
    float viewMtx[16];
    m_camera->GetViewMatrix(viewMtx);

    // Extract forward vector from view matrix (negative Z column)
    bx::Vec3 forward = bx::normalize(bx::Vec3(-viewMtx[2], -viewMtx[6], -viewMtx[10]));

    // Extract right vector from view matrix (X column)
    bx::Vec3 right = bx::normalize(bx::Vec3(viewMtx[0], viewMtx[4], viewMtx[8]));

    // Project camera directions onto horizontal plane (ignore Y)
    forward.y = 0.0f;
    right.y = 0.0f;

    forward = bx::normalize(forward);
    right = bx::normalize(right);

    // Transform input velocity from world-aligned to camera-relative
    // inputVelocity.GetX() is movement along world X (right)
    // inputVelocity.GetZ() is movement along world -Z (already negated in GetDesiredVelocity)

    // Create the movement based on camera-relative directions
    bx::Vec3 cameraRelativeMove = 
        bx::add(
            bx::mul(right, inputVelocity.GetX()),
            bx::mul(forward, inputVelocity.GetZ())  // Forward direction
        );

    return JPH::Vec3(cameraRelativeMove.x, inputVelocity.GetY(), cameraRelativeMove.z);
}
