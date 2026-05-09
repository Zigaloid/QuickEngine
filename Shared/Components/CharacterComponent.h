#pragma once

#include "ComponentSystem/ComponentSystem.h"
#include "Physics/PhysicsManager.h"

#include <Jolt/Jolt.h>
JPH_SUPPRESS_WARNINGS
#include <Jolt/Physics/Body/BodyID.h>

// Simple character component that owns a capsule body and exposes a BodyID
class CCharacterComponent : public ComponentSystem::Component
{
public:
    REFL_DECLARE_OBJECT(CCharacterComponent, Component);
    DECLARE_COMPONENT();

    CCharacterComponent() = default;
    ~CCharacterComponent() override = default;

    bool OnInitialize() override;
    void OnUpdate(double deltaTime) override;
    void OnShutdown() override;

    JPH::BodyID GetBodyID() const { return m_bodyId; }

    // Basic tuning parameters
    void SetCapsuleGeometry(float halfHeight, float radius) { m_halfHeight = halfHeight; m_radius = radius; }
    void SetMoveSpeed(float s) { m_moveSpeed = s; }
    void SetJumpImpulse(float j) { m_jumpImpulse = j; }

private:
    JPH::BodyID m_bodyId = JPH::BodyID();

    // Geometry defaults
    float m_halfHeight = 0.9f; // half-height along Y
    float m_radius = 0.3f;

    // Movement parameters
    float m_moveSpeed = 5.0f;
    float m_jumpImpulse = 6.0f;
};
