#pragma once

#include "Reflection/ReflectionBase.h"
#include "ComponentSystem/ComponentSystem.h"
#include "FileSystem/FileSystemManager.h"
#include "Physics/PhysicsManager.h"
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

#include "PhysicsBodyResource.h"

#include <bgfx/bgfx.h> // for bgfx::ViewId

/// Forward-declare the minimal primitive renderer to avoid heavy includes in all translation units.
namespace ImGuiVisualizers { class BgfxRenderPrimitives; }

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

class CPhysicsBodyComponent : public ComponentSystem::Component
{
public:
    REFL_DECLARE_OBJECT(CPhysicsBodyComponent, Component);
    DECLARE_COMPONENT();

    CPhysicsBodyComponent() = default;
    ~CPhysicsBodyComponent() override = default;

    bool OnInitialize() override;
    void OnShutdown()   override;
    bool CreateBody();

    /// Returns the Jolt BodyID assigned when this component was added to the simulation.
    /// Will be JPH::BodyID::cInvalidBodyID if not yet initialized or if no PhysicsManager was available.
    JPH::BodyID GetBodyID() const { return m_bodyId; }

    /// Shape type as EPhysicsShapeType (cast from the serialised int).
    EPhysicsShapeType GetShapeType() const
    {
        auto res = m_bodyResource.GetResourceAs<CPhysicsBodyResource>();
        return res ? static_cast<EPhysicsShapeType>(res->GetShapeType()) : EPhysicsShapeType::Box;
    }

    /// The scale of the unit shape. Derived shape parameters are:
    ///   Box:               halfExtent  = scale * 0.5
    ///   Sphere:            radius      = scale.x * 0.5
    ///   Capsule/Cylinder:  radius      = scale.x * 0.5, halfHeight = scale.y * 0.5
    Vector3f GetScale() const
    {
        auto r = m_bodyResource.GetResourceAs<CPhysicsBodyResource>();
        return r ? r->GetScale() : Vector3f(1.0f, 1.0f, 1.0f);
    }

    // ── Body parameter accessors ───────────────────────────────────────────

    /// Motion type as JPH::EMotionType (cast from the serialised int).
    /// 0 = Static, 1 = Kinematic, 2 = Dynamic.
    JPH::EMotionType GetMotionType()     const { auto r = m_bodyResource.GetResourceAs<CPhysicsBodyResource>(); return r ? static_cast<JPH::EMotionType>(r->GetMotionType()) : JPH::EMotionType::Static; }
    float            GetFriction()       const { auto r = m_bodyResource.GetResourceAs<CPhysicsBodyResource>(); return r ? r->GetFriction()       : 0.2f;  }
    float            GetRestitution()    const { auto r = m_bodyResource.GetResourceAs<CPhysicsBodyResource>(); return r ? r->GetRestitution()    : 0.0f;  }
    float            GetLinearDamping()  const { auto r = m_bodyResource.GetResourceAs<CPhysicsBodyResource>(); return r ? r->GetLinearDamping()  : 0.05f; }
    float            GetAngularDamping() const { auto r = m_bodyResource.GetResourceAs<CPhysicsBodyResource>(); return r ? r->GetAngularDamping() : 0.05f; }

    // ── Built shape ────────────────────────────────────────────────────────

    /// Returns the compiled Jolt shape, or nullptr before Finalize() succeeds.
    const JPH::Shape* GetShape() const { auto r = m_bodyResource.GetResourceAs<CPhysicsBodyResource>(); return r ? r->GetShape() : nullptr; }

    /// Constructs a BodyCreationSettings with this resource's shape and body
    /// properties applied. Fill in position / rotation / object layer before
    /// passing to JPH::BodyInterface::CreateAndAddBody().
    JPH::BodyCreationSettings MakeBodyCreationSettings(
        JPH::RVec3Arg    position    = JPH::RVec3::sZero(),
        JPH::QuatArg     rotation    = JPH::Quat::sIdentity(),
        JPH::ObjectLayer objectLayer = 0) const;

    // ── Transform helpers ──────────────────────────────────────────────────

    /// Reads the body's current world-space position and rotation from the
    /// physics simulation and returns them as a Matrix4f.
    /// Returns identity if the body is not yet valid.
    Matrix4f GetWorldTransform() const;

    /// Decomposes @p transform into position and rotation and pushes them
    /// into the physics simulation.
    /// @param activation  Whether to wake the body on the next step.
    void SetWorldTransform(const Matrix4f& transform,
                           JPH::EActivation activation = JPH::EActivation::Activate);

    /// Debug render the collision shape using the supplied primitive renderer.
    /// Called by the editor visualizer to draw physics shapes into the 3D view.
    void DebugRender(bgfx::ViewId viewId, ImGuiVisualizers::BgfxRenderPrimitives& prims) const;

private:

    // Physics data is now provided by a CPhysicsBodyResource referenced by this component.
    CPhysicsBodyResourceReference m_bodyResource;

    // ── Runtime (not reflected) ───────────────────────────────────────────
    JPH::BodyID m_bodyId;
};

