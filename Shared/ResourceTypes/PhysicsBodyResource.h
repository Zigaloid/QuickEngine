#pragma once

#include "Reflection/ReflectionBase.h"
#include "ResourceManager/ResourceManager.h"
#include "Math/Matrix4f.h"
#include "Math/Vector3f.h"

#include <Jolt/Jolt.h>
JPH_SUPPRESS_WARNINGS
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/MotionType.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>

// Mirror of the serialisable parameters previously embedded in CPhysicsBodyComponent.
class CPhysicsBodyResource : public ResourceSystem::Resource
{
public:
    REFL_DECLARE_OBJECT(CPhysicsBodyResource, ResourceSystem::Resource);

    static std::vector<std::string_view> GetSupportedExtensions()
    {
        return { ".phys.obj.json" };
    }

    CPhysicsBodyResource() {}
    explicit CPhysicsBodyResource(const std::string& path) : Resource(path) {}

    // Use base-class Update to load the file data on the worker thread.
    bool Update(FileSystem::FileSystemManager& fileSystem) override;

    // Finalize constructs the Jolt shape on the main thread.
    void Finalize() override;

    // Accessors for serialized parameters
    int             GetShapeType()      const { return m_shapeType; }
    const Vector3f& GetScale()          const { return m_scale; }
    int             GetMotionType()     const { return m_motionType; }
    float           GetFriction()       const { return m_friction; }
    float           GetRestitution()    const { return m_restitution; }
    float           GetLinearDamping()  const { return m_linearDamping; }
    float           GetAngularDamping() const { return m_angularDamping; }

    // Runtime
    const JPH::Shape* GetShape() const { return m_shape.GetPtr(); }

    JPH::BodyCreationSettings MakeBodyCreationSettings(
        JPH::RVec3Arg    position    = JPH::RVec3::sZero(),
        JPH::QuatArg     rotation    = JPH::Quat::sIdentity(),
        JPH::ObjectLayer objectLayer = 0) const;

private:
    // Reflected (serialised) members
    // m_scale represents the size of a unit shape:
    //   Box:               halfExtent  = m_scale * 0.5
    //   Sphere:            radius      = m_scale.x * 0.5  (uniform)
    //   Capsule/Cylinder:  radius      = m_scale.x * 0.5
    //                      halfHeight  = m_scale.y * 0.5
    int     m_shapeType      = 0;
    Vector3f m_scale         = { 1.0f, 1.0f, 1.0f };
    int     m_motionType     = 0;
    float   m_friction       = 0.2f;
    float   m_restitution    = 0.0f;
    float   m_linearDamping  = 0.05f;
    float   m_angularDamping = 0.05f;

    // Runtime
    JPH::ShapeRefC m_shape;
};

class CPhysicsBodyResourceReference : public CTypedResourceReference<CPhysicsBodyResource>
{
public:
    REFL_DECLARE_OBJECT(CPhysicsBodyResourceReference, CTypedResourceReference<CPhysicsBodyResource>);
};
