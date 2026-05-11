#include "ResourceManager/ResourceManager.h"
#include "CoreSystem/CoreSystem.h"
#include "Math/Quaternion.h"
#include "PhysicsBodyComponent.h"
#include "Math/Matrix4f.h"

// Include primitives renderer for DebugRender implementation
#include "BgfxRenderPrimitives.h"
#include <bx/math.h>

#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>

// ── Reflection registration ────────────────────────────────────────────────

REFL_DEFINE_OBJECT(CPhysicsBodyComponent)
    REFL_DEFINE_INT_MEMBER(CPhysicsBodyComponent, m_shapeType),
    REFL_DEFINE_INT_MEMBER(CPhysicsBodyComponent, m_motionType),
    REFL_DEFINE_FLOAT_MEMBER(CPhysicsBodyComponent, m_friction),
    REFL_DEFINE_FLOAT_MEMBER(CPhysicsBodyComponent, m_restitution),
    REFL_DEFINE_FLOAT_MEMBER(CPhysicsBodyComponent, m_linearDamping),
    REFL_DEFINE_FLOAT_MEMBER(CPhysicsBodyComponent, m_angularDamping),
REFL_DEFINE_END

REGISTER_COMPONENT(CPhysicsBodyComponent, "PhysBody", "Physics");

void CPhysicsBodyComponent::ApplyTransformToParent(const Matrix4f& worldTransform)
{
	// Static bodies don't move; skip writing back to avoid overwriting the transform.
	if (GetMotionType() == JPH::EMotionType::Static)
		return;

	CPhysicsComponent::ApplyTransformToParent(worldTransform);
}

// ── BodyCreationSettings factory ───────────────────────────────────────────

JPH::BodyCreationSettings CPhysicsBodyComponent::MakeBodyCreationSettings(
    JPH::RVec3Arg    position,
    JPH::QuatArg     rotation,
    JPH::ObjectLayer objectLayer) const
{
    if (m_shape)
    {
        JPH::BodyCreationSettings settings(
            m_shape,
            position,
            rotation,
            static_cast<JPH::EMotionType>(m_motionType),
            objectLayer);

        settings.mFriction = m_friction;
        settings.mRestitution = m_restitution;
        settings.mLinearDamping = m_linearDamping;
        settings.mAngularDamping = m_angularDamping;

        // Use continuous collision detection for dynamic bodies to prevent tunneling.
        if (settings.mMotionType == JPH::EMotionType::Dynamic)
            settings.mMotionQuality = JPH::EMotionQuality::LinearCast;

        return settings;
    }

    // Fallback: return empty settings if resource not available.
    return JPH::BodyCreationSettings(JPH::ShapeRefC(), position, rotation, JPH::EMotionType::Static, objectLayer);
}

void CPhysicsBodyComponent::InitializeShape(const Matrix4f& objTran)
{
    JPH::ShapeSettings::ShapeResult result;

    // Extract scale from the serialized transform 
    Vector3f scale = (GetObjectMatrix() * objTran).ExtractScale();

    switch (m_shapeType)
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

    // Wrap the shape with the resource's local offset so the collision geometry
    // is offset from the body origin, without needing to shift the body position.
    JPH::ShapeRefC baseShape = result.Get();
    Vector3f resPos = m_objectMatrix.ExtractTranslation();
    Quaternion resRot = Quaternion::FromMatrix(m_objectMatrix).Normalized();

    bool hasOffset = (resPos.GetX() != 0.0f || resPos.GetY() != 0.0f || resPos.GetZ() != 0.0f);
    bool hasRotation = (std::abs(resRot.GetX()) > 0.0001f || std::abs(resRot.GetY()) > 0.0001f || std::abs(resRot.GetZ()) > 0.0001f);

    if (hasOffset || hasRotation)
    {
        JPH::RotatedTranslatedShapeSettings offsetSettings(
            JPH::Vec3(resPos.GetX(), resPos.GetY(), resPos.GetZ()),
            JPH::Quat(resRot.GetX(), resRot.GetY(), resRot.GetZ(), resRot.GetW()),
            baseShape);
        auto offsetResult = offsetSettings.Create();
        if (!offsetResult.HasError())
            baseShape = offsetResult.Get();
    }

    m_shape = baseShape;
}

// ── Debug rendering ───────────────────────────────────────────────────────

void CPhysicsBodyComponent::DebugRender(bgfx::ViewId viewId, Matrix4f &transform) const
{
    
    Rendering::BgfxRenderPrimitives& prims = Rendering::BgfxRenderPrimitives::Instance();

    const float* mtx = transform.data();
    const uint32_t color = 0xff00ffff;
    switch (GetShapeType())
    {
    case EPhysicsShapeType::Box:
        prims.RenderWireBox(viewId, mtx, color);
        break;
    case EPhysicsShapeType::Sphere:
        prims.RenderWireSphere(viewId, mtx, color);
        break;
    case EPhysicsShapeType::Capsule:
        prims.RenderWireCapsule(viewId, mtx, color);
        break;
    case EPhysicsShapeType::Cylinder:
        prims.RenderWireCylinder(viewId, mtx, color);
        break;
    default:
        break;
    }
}