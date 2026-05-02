#include "PhysicsBodyComponent.h"
#include "ResourceManager/ResourceManager.h"
#include "CoreSystem/CoreSystem.h"
#include "Physics/PhysicsManager.h"
#include "Math/Quaternion.h"

// ── Reflection registration ────────────────────────────────────────────────

REFL_DEFINE_OBJECT(CPhysicsBodyComponent)
    REFL_DEFINE_INT_MEMBER  (CPhysicsBodyComponent, m_shapeType),
    REFL_DEFINE_FLOAT_MEMBER(CPhysicsBodyComponent, m_halfExtentX),
    REFL_DEFINE_FLOAT_MEMBER(CPhysicsBodyComponent, m_halfExtentY),
    REFL_DEFINE_FLOAT_MEMBER(CPhysicsBodyComponent, m_halfExtentZ),
    REFL_DEFINE_FLOAT_MEMBER(CPhysicsBodyComponent, m_radius),
    REFL_DEFINE_FLOAT_MEMBER(CPhysicsBodyComponent, m_halfHeight),
    REFL_DEFINE_INT_MEMBER  (CPhysicsBodyComponent, m_motionType),
    REFL_DEFINE_FLOAT_MEMBER(CPhysicsBodyComponent, m_friction),
    REFL_DEFINE_FLOAT_MEMBER(CPhysicsBodyComponent, m_restitution),
    REFL_DEFINE_FLOAT_MEMBER(CPhysicsBodyComponent, m_linearDamping),
    REFL_DEFINE_FLOAT_MEMBER(CPhysicsBodyComponent, m_angularDamping),
REFL_DEFINE_END

REGISTER_COMPONENT(CPhysicsBodyComponent, "PhysBody", "Physics");

bool CPhysicsBodyComponent::OnInitialize() 
{
    JPH::ShapeSettings::ShapeResult result;

    switch (static_cast<EPhysicsShapeType>(m_shapeType))
    {
    case EPhysicsShapeType::Box:
    {
        const JPH::Vec3 halfExtent(m_halfExtentX, m_halfExtentY, m_halfExtentZ);
        result = JPH::BoxShapeSettings(halfExtent).Create();
        break;
    }
    case EPhysicsShapeType::Sphere:
    {
        result = JPH::SphereShapeSettings(m_radius).Create();
        break;
    }
    case EPhysicsShapeType::Capsule:
    {
        result = JPH::CapsuleShapeSettings(m_halfHeight, m_radius).Create();
        break;
    }
    case EPhysicsShapeType::Cylinder:
    {
        result = JPH::CylinderShapeSettings(m_halfHeight, m_radius).Create();
        break;
    }
    default:
        return false;
    }

    if (result.HasError())
    {
        return false;
    }
    m_shape = result.Get();

    PhysicsManager* physics = PhysicsManager::Get();
    if (physics && physics->IsInitialized())
    {
        JPH::ObjectLayer layer = (GetMotionType() == JPH::EMotionType::Static)
            ? PhysicsLayers::NON_MOVING
            : PhysicsLayers::MOVING;

        JPH::BodyCreationSettings bcs = MakeBodyCreationSettings(
            JPH::RVec3::sZero(), JPH::Quat::sIdentity(), layer);

        m_bodyId = physics->GetBodyInterface().CreateAndAddBody(
            bcs, JPH::EActivation::Activate);
    }

    return true;
}

// ── BodyCreationSettings factory ───────────────────────────────────────────

JPH::BodyCreationSettings CPhysicsBodyComponent::MakeBodyCreationSettings(
    JPH::RVec3Arg    position,
    JPH::QuatArg     rotation,
    JPH::ObjectLayer objectLayer) const
{
    JPH::BodyCreationSettings settings(
        m_shape,
        position,
        rotation,
        static_cast<JPH::EMotionType>(m_motionType),
        objectLayer);

    settings.mFriction       = m_friction;
    settings.mRestitution    = m_restitution;
    settings.mLinearDamping  = m_linearDamping;
    settings.mAngularDamping = m_angularDamping;

    return settings;
}

void CPhysicsBodyComponent::OnShutdown()
{
    if (m_bodyId.IsInvalid())
        return;

    PhysicsManager* physics = PhysicsManager::Get();
    if (physics && physics->IsInitialized())
        physics->RemoveBody(m_bodyId);

    m_bodyId = JPH::BodyID();
}

// ── Transform helpers ──────────────────────────────────────────────────────

Matrix4f CPhysicsBodyComponent::GetWorldTransform() const
{
    if (m_bodyId.IsInvalid())
        return Matrix4f::GetIdentity();

    PhysicsManager* physics = PhysicsManager::Get();
    if (!physics || !physics->IsInitialized())
        return Matrix4f::GetIdentity();

    JPH::BodyInterface& bi  = physics->GetBodyInterface();
    const JPH::RVec3    pos = bi.GetPosition(m_bodyId);
    const JPH::Quat     rot = bi.GetRotation(m_bodyId);

    const Quaternion q(rot.GetW(), rot.GetX(), rot.GetY(), rot.GetZ());
    Matrix4f result = q.ToMatrix();
    result.SetTranslation(Vector3f(
        static_cast<float>(pos.GetX()),
        static_cast<float>(pos.GetY()),
        static_cast<float>(pos.GetZ())));
    return result;
}

void CPhysicsBodyComponent::SetWorldTransform(const Matrix4f& transform, JPH::EActivation activation)
{
    if (m_bodyId.IsInvalid())
        return;

    PhysicsManager* physics = PhysicsManager::Get();
    if (!physics || !physics->IsInitialized())
        return;

    const Vector3f   t = transform.ExtractTranslation();
    const Quaternion q = Quaternion::FromMatrix(transform);

    physics->GetBodyInterface().SetPositionAndRotation(
        m_bodyId,
        JPH::RVec3(t.GetX(), t.GetY(), t.GetZ()),
        JPH::Quat(q.GetX(), q.GetY(), q.GetZ(), q.GetW()),
        activation);
}
