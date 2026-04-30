#include "PhysicsBodyResource.h"
#include "ResourceManager/ResourceManager.h"
#include "CoreSystem/CoreSystem.h"

// ?? Reflection registration ???????????????????????????????????????????????

REFL_DEFINE_OBJECT(CPhysicsBodyResourceReference)
REFL_DEFINE_END

REFL_DEFINE_OBJECT(CPhysicsBodyResource)
    REFL_DEFINE_INT_MEMBER  (CPhysicsBodyResource, m_shapeType),
    REFL_DEFINE_FLOAT_MEMBER(CPhysicsBodyResource, m_halfExtentX),
    REFL_DEFINE_FLOAT_MEMBER(CPhysicsBodyResource, m_halfExtentY),
    REFL_DEFINE_FLOAT_MEMBER(CPhysicsBodyResource, m_halfExtentZ),
    REFL_DEFINE_FLOAT_MEMBER(CPhysicsBodyResource, m_radius),
    REFL_DEFINE_FLOAT_MEMBER(CPhysicsBodyResource, m_halfHeight),
    REFL_DEFINE_INT_MEMBER  (CPhysicsBodyResource, m_motionType),
    REFL_DEFINE_FLOAT_MEMBER(CPhysicsBodyResource, m_friction),
    REFL_DEFINE_FLOAT_MEMBER(CPhysicsBodyResource, m_restitution),
    REFL_DEFINE_FLOAT_MEMBER(CPhysicsBodyResource, m_linearDamping),
    REFL_DEFINE_FLOAT_MEMBER(CPhysicsBodyResource, m_angularDamping),
REFL_DEFINE_END

// ?? Resource pipeline ?????????????????????????????????????????????????????

bool CPhysicsBodyResource::Update(FileSystem::FileSystemManager& fileSystem)
{
    m_isLoaded = true;
    SafeRead(GetPath());
    return true;
}

void CPhysicsBodyResource::Finalize()
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
        m_isFinalized = false;
        return;
    }

    if (result.HasError())
    {
        m_isFinalized = false;
        return;
    }

    m_shape       = result.Get();
    m_isFinalized = true;
}

// ?? BodyCreationSettings factory ??????????????????????????????????????????

JPH::BodyCreationSettings CPhysicsBodyResource::MakeBodyCreationSettings(
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
