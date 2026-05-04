#include "PhysicsBodyResource.h"
#include "CoreSystem/CoreSystem.h"

// Reflection for the reference wrapper
REFL_DEFINE_OBJECT(CPhysicsBodyResourceReference)
REFL_DEFINE_END

REFL_DEFINE_OBJECT(CPhysicsBodyResource)
    REFL_DEFINE_INT_MEMBER    (CPhysicsBodyResource, m_shapeType),
    REFL_DEFINE_MATRIX4_MEMBER(CPhysicsBodyResource, m_transform),
    REFL_DEFINE_INT_MEMBER    (CPhysicsBodyResource, m_motionType),
    REFL_DEFINE_FLOAT_MEMBER  (CPhysicsBodyResource, m_friction),
    REFL_DEFINE_FLOAT_MEMBER  (CPhysicsBodyResource, m_restitution),
    REFL_DEFINE_FLOAT_MEMBER  (CPhysicsBodyResource, m_linearDamping),
    REFL_DEFINE_FLOAT_MEMBER  (CPhysicsBodyResource, m_angularDamping)
REFL_DEFINE_END

bool CPhysicsBodyResource::Update(FileSystem::FileSystemManager& fileSystem)
{
    m_isLoaded = true;
    SafeRead(this->GetPath());
    return true;
}

// Construct the Jolt shape based on serialised parameters.
void CPhysicsBodyResource::Finalize()
{
    JPH::ShapeSettings::ShapeResult result;

    // Extract scale from the serialized transform
    const Vector3f scale = m_transform.ExtractScale();

    switch (static_cast<int>(m_shapeType))
    {
    case 0: // Box — unit 1x1x1 cube, half-extents = scale * 0.5
    {
        const JPH::Vec3 halfExtent(scale.GetX() * 0.5f, scale.GetY() * 0.5f, scale.GetZ() * 0.5f);
        result = JPH::BoxShapeSettings(halfExtent).Create();
        break;
    }
    case 1: // Sphere — unit sphere of radius 0.5, scaled uniformly by scale.x
    {
        result = JPH::SphereShapeSettings(scale.GetX() * 0.5f).Create();
        break;
    }
    case 2: // Capsule — unit capsule: radius = scale.x * 0.5, halfHeight = scale.y * 0.5
    {
        result = JPH::CapsuleShapeSettings(scale.GetY() * 0.5f, scale.GetX() * 0.5f).Create();
        break;
    }
    case 3: // Cylinder — unit cylinder: radius = scale.x * 0.5, halfHeight = scale.y * 0.5
    {
        result = JPH::CylinderShapeSettings(scale.GetY() * 0.5f, scale.GetX() * 0.5f).Create();
        break;
    }
    default:
        return;
    }

    if (result.HasError())
    {
        return;
    }

    m_shape = result.Get();
    ResourceSystem::Resource::Finalize();
}

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
