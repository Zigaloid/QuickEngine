#pragma once

#include "Reflection/ReflectionBase.h"
#include "ResourceManager/ResourceManager.h"
#include "Math/Matrix4f.h"
#include "Math/Vector3f.h"
#include "Math/Vector4f.h"

#include <memory>

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

    CPhysicsBodyResource()
    {
        m_modelMatrixPtr = std::shared_ptr<Matrix4f>(&m_transform, [](Matrix4f*) {});
        m_boundingSpherePtr = std::shared_ptr<Vector4f>(&m_boundingSphere, [](Vector4f*) {});
    }
    explicit CPhysicsBodyResource(const std::string& path) : Resource(path)
    {
        m_modelMatrixPtr = std::shared_ptr<Matrix4f>(&m_transform, [](Matrix4f*) {});
        m_boundingSpherePtr = std::shared_ptr<Vector4f>(&m_boundingSphere, [](Vector4f*) {});
    }

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
    Matrix4f&       GetTransformMutable()      { return m_transform; }
    void            SetTransform(const Matrix4f& t) { m_transform = t; }

    // Shared model matrix and bounding sphere for selectables / gizmos.
    // The model matrix points directly at m_transform.
    std::shared_ptr<Matrix4f> GetModelMatrix()      const { return m_modelMatrixPtr; }
    std::shared_ptr<Vector4f> GetBoundingSphere()   const { return m_boundingSpherePtr; }
    Matrix4f&                 GetModelMatrixRef()         { return m_transform; }
    Vector4f&                 GetBoundingSphereRef()      { return m_boundingSphere; }

    int             GetMotionType()     const { return m_motionType; }
    float           GetFriction()       const { return m_friction; }
    float           GetRestitution()    const { return m_restitution; }
    float           GetLinearDamping()  const { return m_linearDamping; }
    float           GetAngularDamping() const { return m_angularDamping; }





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



    // Shared bounding data and pointer wrappers for external references (selectables, gizmos).
    // m_modelMatrixPtr points directly at m_transform (no-op deleter).
    mutable Vector4f m_boundingSphere;
    mutable std::shared_ptr<Matrix4f> m_modelMatrixPtr;
    mutable std::shared_ptr<Vector4f> m_boundingSpherePtr;
};

class CPhysicsBodyResourceReference : public CTypedResourceReference<CPhysicsBodyResource>
{
public:
    REFL_DECLARE_OBJECT(CPhysicsBodyResourceReference, CTypedResourceReference<CPhysicsBodyResource>);
};
