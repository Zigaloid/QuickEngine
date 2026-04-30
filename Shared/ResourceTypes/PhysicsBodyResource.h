#pragma once

#include "Reflection/ReflectionBase.h"
#include "ResourceManager/ResourceManager.h"

#include <Jolt/Jolt.h>
JPH_SUPPRESS_WARNINGS
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/MotionType.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>

/// Selects which primitive collision shape to build.
/// Stored as int so it can be serialised through the reflection system.
///
///  0 – Box      : half-extents (m_halfExtentX/Y/Z)
///  1 – Sphere   : m_radius
///  2 – Capsule  : m_radius + m_halfHeight  (cylindrical portion, Y-axis aligned)
///  3 – Cylinder : m_radius + m_halfHeight  (Y-axis aligned)
enum class EPhysicsShapeType : int
{
    Box      = 0,
    Sphere   = 1,
    Capsule  = 2,
    Cylinder = 3,
};

/**
 * @brief A resource that describes a Jolt physics body built from a single
 *        primitive collision shape (Box / Sphere / Capsule / Cylinder).
 *
 * All shape and body parameters are serialisable through the reflection
 * system, following the same pattern as CStaticMeshResource.
 *
 * File extension convention: ".physics.json"
 *
 * Typical usage:
 * @code
 *   auto res = resourceManager->RequestResource<CPhysicsBodyResource>("player.physics.json");
 *   // ...wait for IsReady()...
 *   JPH::BodyCreationSettings bcs = res->MakeBodyCreationSettings(position, rotation, layer);
 *   JPH::BodyID id = bodyInterface.CreateAndAddBody(bcs, JPH::EActivation::Activate);
 * @endcode
 */
class CPhysicsBodyResource : public ResourceSystem::Resource
{
public:
    REFL_DECLARE_OBJECT(CPhysicsBodyResource, ResourceSystem::Resource);

    static std::vector<std::string_view> GetSupportedExtensions()
    {
        return { ".physics.json" };
    }

    CPhysicsBodyResource() = default;

    explicit CPhysicsBodyResource(const std::string& path)
        : Resource(path)
    {}

    ~CPhysicsBodyResource() override = default;

    // ?? Resource pipeline ?????????????????????????????????????????????

    /// Deserialises the JSON file on the worker thread.
    bool Update(FileSystem::FileSystemManager& fileSystem) override;

    /// Builds the Jolt shape on the main thread.
    void Finalize() override;

    // ?? Shape parameter accessors ??????????????????????????????????????

    /// Shape type as EPhysicsShapeType (cast from the serialised int).
    EPhysicsShapeType GetShapeType() const { return static_cast<EPhysicsShapeType>(m_shapeType); }

    /// Box half-extents along each axis.
    float GetHalfExtentX() const { return m_halfExtentX; }
    float GetHalfExtentY() const { return m_halfExtentY; }
    float GetHalfExtentZ() const { return m_halfExtentZ; }

    /// Radius used by Sphere, Capsule, and Cylinder shapes.
    float GetRadius()     const { return m_radius; }

    /// Half-height of the cylindrical section for Capsule / Cylinder shapes.
    float GetHalfHeight() const { return m_halfHeight; }

    // ?? Body parameter accessors ???????????????????????????????????????

    /// Motion type as JPH::EMotionType (cast from the serialised int).
    /// 0 = Static, 1 = Kinematic, 2 = Dynamic.
    JPH::EMotionType GetMotionType()    const { return static_cast<JPH::EMotionType>(m_motionType); }
    float            GetFriction()      const { return m_friction; }
    float            GetRestitution()   const { return m_restitution; }
    float            GetLinearDamping() const { return m_linearDamping; }
    float            GetAngularDamping()const { return m_angularDamping; }

    // ?? Built shape ????????????????????????????????????????????????????

    /// Returns the compiled Jolt shape, or nullptr before Finalize() succeeds.
    const JPH::Shape* GetShape() const { return m_shape; }

    /// Constructs a BodyCreationSettings with this resource's shape and body
    /// properties applied. Fill in position / rotation / object layer before
    /// passing to JPH::BodyInterface::CreateAndAddBody().
    JPH::BodyCreationSettings MakeBodyCreationSettings(
        JPH::RVec3Arg    position    = JPH::RVec3::sZero(),
        JPH::QuatArg     rotation    = JPH::Quat::sIdentity(),
        JPH::ObjectLayer objectLayer = 0) const;

    /// True once the shape has been successfully built.
    bool IsReady() const { return m_isFinalized && m_shape != nullptr; }

private:
    // ?? Reflected (serialised) members ????????????????????????????????

    /// Shape selector: 0=Box, 1=Sphere, 2=Capsule, 3=Cylinder.
    int   m_shapeType      = 0;

    // Box
    float m_halfExtentX    = 0.5f;
    float m_halfExtentY    = 0.5f;
    float m_halfExtentZ    = 0.5f;

    // Sphere / Capsule / Cylinder
    float m_radius         = 0.5f;

    // Capsule / Cylinder
    float m_halfHeight     = 0.5f;

    // Body properties
    /// Motion type: 0=Static, 1=Kinematic, 2=Dynamic.
    int   m_motionType     = 0;
    float m_friction       = 0.2f;
    float m_restitution    = 0.0f;
    float m_linearDamping  = 0.05f;
    float m_angularDamping = 0.05f;

    // ?? Runtime (not reflected) ????????????????????????????????????????
    JPH::ShapeRefC m_shape;
};

class CPhysicsBodyResourceReference : public CTypedResourceReference<CPhysicsBodyResource>
{
public:
    REFL_DECLARE_OBJECT(CPhysicsBodyResourceReference, CTypedResourceReference<CPhysicsBodyResource>);
};
