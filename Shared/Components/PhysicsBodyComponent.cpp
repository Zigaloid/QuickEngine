#include "ResourceManager/ResourceManager.h"
#include "CoreSystem/CoreSystem.h"
#include "Physics/PhysicsManager.h"
#include "Math/Quaternion.h"
#include "PhysicsBodyComponent.h"
#include "PhysicsBodyResource.h"
#include "TransformComponent.h"

// Include primitives renderer for DebugRender implementation
#include "BgfxRenderPrimitives.h"
#include <bx/math.h>

#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>

// ── Reflection registration ────────────────────────────────────────────────

REFL_DEFINE_OBJECT(CPhysicsBodyComponent)
REFL_DEFINE_OBJECT_MEMBER(CPhysicsBodyComponent, m_bodyResource),
REFL_DEFINE_END

REGISTER_COMPONENT(CPhysicsBodyComponent, "PhysBody", "Physics");

bool CPhysicsBodyComponent::OnInitialize()
{
    return true;
}

bool CPhysicsBodyComponent::SetupPhysicsBody()
{    
    // Resolve the referenced resource and ensure it is finalized.
    auto res = m_bodyResource.GetResourceAs<CPhysicsBodyResource>();
    if (!res || !res->IsFinalized())
    {
        return false;
    }

    auto* parent = GetParent();
    if (parent)
    {
        CTransformComponent* parentTransform = parent->FindActiveChild<CTransformComponent>();
        if (parentTransform)
        {
            m_parentTransform = &parentTransform->GetTransform();
            m_cachedScale = Matrix4f::Scale(m_parentTransform->ExtractScale());
			// If the body resource is already finalized, create the body immediately; otherwise defer until the resource finalizes.
            if(m_bodyId.IsInvalid()) InitializeShape(m_cachedScale);
            // Strip scale before passing to CreateBody so that SetWorldTransform
            // receives a clean rotation+translation matrix for quaternion extraction.
            Vector3f scale = m_parentTransform->ExtractScale();
            Vector3f invScale(1.0f / scale.GetX(), 1.0f / scale.GetY(), 1.0f / scale.GetZ());
            Matrix4f worldNoScale = *m_parentTransform * Matrix4f::Scale(invScale);
            CreateBody(worldNoScale);
            return true;
        }
    }
    return false;
}

void CPhysicsBodyComponent::OnUpdate(double deltaTime)
{
    if (m_bodyInitialized == false)
    {
        m_bodyInitialized = SetupPhysicsBody();
    }

    if ( GetMotionType() != JPH::EMotionType::Static )
    {
        *m_parentTransform = GetWorldTransform() * m_cachedScale;
    }
}

bool CPhysicsBodyComponent::CreateBody( const Matrix4f &worldTransform )
{

    PhysicsManager* physics = PhysicsManager::Get();
    if (physics && physics->IsInitialized())
    {
        JPH::ObjectLayer layer = (GetMotionType() == JPH::EMotionType::Static)
            ? PhysicsLayers::NON_MOVING
            : PhysicsLayers::MOVING;

        JPH::BodyCreationSettings bcs = MakeBodyCreationSettings(
            JPH::RVec3::sZero(), JPH::Quat::sIdentity(), layer);

        m_bodyId = physics->GetBodyInterfaceLocking().CreateAndAddBody(
            bcs, JPH::EActivation::Activate);

        if (m_bodyId.IsInvalid())
            return false;

        // Place the body at the mesh's world position directly.
        // The resource offset is already baked into the shape via RotatedTranslatedShape.
        SetWorldTransform(worldTransform);

        // Signal that the broad phase needs rebuilding (done on main thread before next step).
        physics->SetBroadPhaseDirty();
        return true;
    }
    return false;
}

// ── BodyCreationSettings factory ───────────────────────────────────────────

JPH::BodyCreationSettings CPhysicsBodyComponent::MakeBodyCreationSettings(
    JPH::RVec3Arg    position,
    JPH::QuatArg     rotation,
    JPH::ObjectLayer objectLayer) const
{
    auto res = m_bodyResource.GetResourceAs<CPhysicsBodyResource>();
    if (res && m_shape)
    {
        JPH::BodyCreationSettings settings(
            m_shape,
            position,
            rotation,
            static_cast<JPH::EMotionType>(res->GetMotionType()),
            objectLayer);

        settings.mFriction = res->GetFriction();
        settings.mRestitution = res->GetRestitution();
        settings.mLinearDamping = res->GetLinearDamping();
        settings.mAngularDamping = res->GetAngularDamping();

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
    auto res = m_bodyResource.GetResourceAs<CPhysicsBodyResource>();
    if (!res)
        return;

    JPH::ShapeSettings::ShapeResult result;

    // Extract scale from the serialized transform
    Vector3f scale = (res->GetTransform() * objTran).ExtractScale();

    switch (res->GetShapeType())
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
    Vector3f resPos = res->GetTransform().ExtractTranslation();
    Quaternion resRot = Quaternion::FromMatrix(res->GetTransform()).Normalized();

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

void CPhysicsBodyComponent::OnShutdown()
{
    if (m_bodyId.IsInvalid())
        return;

    PhysicsManager* physics = PhysicsManager::Get();
    if (physics && physics->IsInitialized())
        physics->RemoveBody(m_bodyId);

    m_bodyId = JPH::BodyID();

    m_bodyInitialized = false;
}

// ── Transform helpers ──────────────────────────────────────────────────────

Matrix4f CPhysicsBodyComponent::GetWorldTransform() const
{
    PhysicsManager* physics = PhysicsManager::Get();
    if (!physics || !physics->IsInitialized())
        return Matrix4f::GetIdentity();

    JPH::BodyInterface& bi = physics->GetBodyInterfaceLocking();
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
    PhysicsManager* physics = PhysicsManager::Get();
    if (!physics || !physics->IsInitialized())
        return;

    const Vector3f   t = transform.ExtractTranslation();
    const Quaternion rq = Quaternion::FromMatrix(transform);
    const Quaternion q = rq.Normalized();
    physics->GetBodyInterfaceLocking().SetPositionAndRotation(
        m_bodyId,
        JPH::RVec3(t.GetX(), t.GetY(), t.GetZ()),
        JPH::Quat(q.GetX(), q.GetY(), q.GetZ(), q.GetW()),
        activation);
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