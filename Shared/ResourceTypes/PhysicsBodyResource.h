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
    // Returns the decomposed scale from the serialized transform (by value).
    Vector3f        GetScale()          const { return m_transform.ExtractScale(); }
    // Returns the decomposed translation from the serialized transform.
    Vector3f        GetPosition()       const { return m_transform.ExtractTranslation(); }
    // Returns the decomposed rotation (Euler angles in degrees, X/Y/Z) from the serialized transform.
    Vector3f        GetRotationEuler()  const { return m_transform.ExtractRotationEuler(); }
    // If callers need raw TRS matrix:
    const Matrix4f& GetTransform()      const { return m_transform; }

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
    // m_transform represents the TRS applied to a unit shape. The unit shape semantics:
    //   Box:               halfExtent  = (m_transform scale) * 0.5
    //   Sphere:            radius      = (m_transform scale).x * 0.5  (uniform expected)
    //   Capsule/Cylinder:  radius      = (m_transform scale).x * 0.5
    //                      halfHeight  = (m_transform scale).y * 0.5
    // The transform stores translation, rotation (Euler degrees), and scale.
    int      m_shapeType      = 0;
    Matrix4f m_transform      = Matrix4f::GetIdentity();
    int      m_motionType     = 0;
    float    m_friction       = 0.2f;
    float    m_restitution    = 0.0f;
    float    m_linearDamping  = 0.05f;
    float    m_angularDamping = 0.05f;

    // Runtime
    JPH::ShapeRefC m_shape;
};

class CPhysicsBodyResourceReference : public CTypedResourceReference<CPhysicsBodyResource>
{
public:
    REFL_DECLARE_OBJECT(CPhysicsBodyResourceReference, CTypedResourceReference<CPhysicsBodyResource>);
};
