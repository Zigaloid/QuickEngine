#include "CharacterComponent.h"
#include "Physics/PhysicsManager.h"

#include <Jolt/Jolt.h>
JPH_SUPPRESS_WARNINGS
#include <Jolt/Physics/Body/BodyInterface.h>

REFL_DEFINE_OBJECT(CCharacterComponent)
REFL_DEFINE_END

REGISTER_COMPONENT(CCharacterComponent, "Character", "Physics");

bool CCharacterComponent::OnInitialize()
{
    PhysicsManager* physics = PhysicsManager::Get();
    if (!physics || !physics->IsInitialized())
        return true; // postpone until physics available

    // Create a capsule body at origin. Higher-level code may reposition with SetWorldTransform on a sibling physics body if needed.
    m_bodyId = physics->AddCapsule(m_halfHeight, m_radius, JPH::RVec3::sZero(), JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic);
    return true;
}

void CCharacterComponent::OnUpdate(double /*deltaTime*/)
{
    // No per-frame logic here - movement is typically driven by input components which will set linear velocity on this body's BodyID.
}

void CCharacterComponent::OnShutdown()
{
    if (m_bodyId.IsInvalid())
        return;

    PhysicsManager* physics = PhysicsManager::Get();
    if (physics && physics->IsInitialized())
        physics->RemoveBody(m_bodyId);

    m_bodyId = JPH::BodyID();
}
