#pragma once

#include "Reflection/ReflectionBase.h"
#include "PhysicsComponent.h"
#include "FileSystem/FileSystemManager.h"

#include <Jolt/Jolt.h>
JPH_SUPPRESS_WARNINGS

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/MotionType.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>

#include "Math/Matrix4f.h"
#include "Math/Vector3f.h"
#include "Math/Vector4f.h"

#include <bgfx/bgfx.h> // for bgfx::ViewId

/// Forward-declare the minimal primitive renderer to avoid heavy includes in all translation units.
namespace Rendering { class BgfxRenderPrimitives; }

/// Selects which primitive collision shape to build.
/// Stored as int so it can be serialised through the reflection system.
///
///  0 – Box      : scale defines full X/Y/Z extents  (halfExtent = scale * 0.5)
///  1 – Sphere   : scale.x defines diameter          (radius = scale.x * 0.5, uniform)
///  2 – Capsule  : scale.x = diameter, scale.y = total height
///  3 – Cylinder : scale.x = diameter, scale.y = total height
enum class EPhysicsShapeType : int
{
    Box      = 0,
    Sphere   = 1,
    Capsule  = 2,
    Cylinder = 3,
};

class CPhysicsBodyComponent : public CPhysicsComponent
{
public:
    REFL_DECLARE_OBJECT(CPhysicsBodyComponent, CPhysicsComponent);
    DECLARE_COMPONENT();

    CPhysicsBodyComponent()
    {
        m_modelMatrixPtr = std::shared_ptr<Matrix4f>(&m_objectMatrix, [](Matrix4f*) {});
        m_boundingSpherePtr = std::shared_ptr<Vector4f>(&m_boundingSphere, [](Vector4f*) {});
    }
    ~CPhysicsBodyComponent() override = default;

    // CPhysicsComponent overrides
    void ApplyTransformToParent(const Matrix4f& worldTransform) override;

    /// Shape type as EPhysicsShapeType (cast from the serialised int).
    EPhysicsShapeType GetShapeType() const
    {
        return static_cast<EPhysicsShapeType>(m_shapeType);
    }

    /// The scale of the unit shape. Derived shape parameters are:
    ///   Box:               halfExtent  = scale * 0.5
    ///   Sphere:            radius      = scale.x * 0.5
    ///   Capsule/Cylinder:  radius      = scale.x * 0.5, halfHeight = scale.y * 0.5
    Vector3f GetScale() const
    {
        return GetConstObjectMatrix().ExtractScale();
    }

    // ── Body parameter accessors ───────────────────────────────────────────

    /// Motion type as JPH::EMotionType (cast from the serialised int).
    /// 0 = Static, 1 = Kinematic, 2 = Dynamic.
    JPH::EMotionType GetMotionType()     const override { return static_cast<JPH::EMotionType>(m_motionType); }
    float            GetFriction()       const { return m_friction; }
    float            GetRestitution()    const { return m_restitution; }
    float            GetLinearDamping()  const { return m_linearDamping; }
    float            GetAngularDamping() const { return m_angularDamping; }

    // ── Built shape ────────────────────────────────────────────────────────

    /// Returns the compiled Jolt shape, or nullptr before InitializeShape() is called.
    const JPH::Shape* GetShape() const { return m_shape.GetPtr(); }

    /// Debug render the collision shape using the supplied primitive renderer.
    /// Called by the editor visualizer to draw physics shapes into the 3D view.
    void DebugRender(bgfx::ViewId viewId, Matrix4f& transform) const override;

    // ── Expose shared pointers to transform / bounding-sphere via the resource
    //     so that selectables and gizmos can hold and mutate the same live data.
    std::shared_ptr<Matrix4f> GetModelMatrix() const { return m_modelMatrixPtr; }
    std::shared_ptr<Vector4f> GetBoundingSphere() const { return m_boundingSpherePtr; }

    // CPhysicsComponent editor interface
    std::shared_ptr<Matrix4f> GetEditableTransform()      const override { return GetModelMatrix(); }
    std::shared_ptr<Vector4f> GetEditableBoundingSphere() const override { return GetBoundingSphere(); }

protected:
    // ── CPhysicsComponent template-method overrides ────────────────────────
    void InitializeShape(const Matrix4f& scaleMtx) override;
    JPH::BodyCreationSettings MakeBodyCreationSettings(
        JPH::RVec3Arg    position,
        JPH::QuatArg     rotation,
        JPH::ObjectLayer objectLayer) const override;

private:

    // --- Serialized (reflected) members
    int      m_shapeType      = 0;
    int      m_motionType     = 0;
    float    m_friction       = 0.2f;
    float    m_restitution    = 0.0f;
    float    m_linearDamping  = 0.05f;
    float    m_angularDamping = 0.05f;

    // Shared bounding data and pointer wrappers for external references (selectables, gizmos).
    mutable Vector4f m_boundingSphere;
    mutable std::shared_ptr<Matrix4f> m_modelMatrixPtr;
    mutable std::shared_ptr<Vector4f> m_boundingSpherePtr;
};

