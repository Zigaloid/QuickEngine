#include "ResourceManager/ResourceManager.h"
#include "CoreSystem/CoreSystem.h"
#include "Physics/PhysicsManager.h"
#include "Math/Quaternion.h"
#include "PhysicsBodyComponent.h"
#include "PhysicsBodyResource.h"

// Include primitives renderer for DebugRender implementation
#include "BgfxRenderPrimitives.h"
#include <bx/math.h>

// ── Reflection registration ────────────────────────────────────────────────

REFL_DEFINE_OBJECT(CPhysicsBodyComponent)
REFL_DEFINE_OBJECT_MEMBER(CPhysicsBodyComponent, m_bodyResource),
REFL_DEFINE_END

REGISTER_COMPONENT(CPhysicsBodyComponent, "PhysBody", "Physics");

bool CPhysicsBodyComponent::OnInitialize()
{
    return true;
}

bool CPhysicsBodyComponent::CreateBody()
{
    // If it's already valid just return.
    if (m_bodyId.IsInvalid() == false)
        return true;

    // Resolve the referenced resource and ensure it is finalized.
    auto res = m_bodyResource.GetResourceAs<CPhysicsBodyResource>();
    if (!res || !res->IsFinalized())
    {
        // Resource not ready yet.
        return false;
    }

    PhysicsManager* physics = PhysicsManager::Get();
    if (physics && physics->IsInitialized())
    {
        JPH::ObjectLayer layer = (GetMotionType() == JPH::EMotionType::Static)
            ? PhysicsLayers::NON_MOVING
            : PhysicsLayers::MOVING;

        JPH::BodyCreationSettings bcs = MakeBodyCreationSettings(
            JPH::RVec3::sZero(), JPH::Quat::sIdentity(), layer);

        m_bodyId = physics->GetBodyInterface().CreateAndAddBody(
            bcs, JPH::EActivation::Activate);

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
    if (res && res->GetShape())
    {
        // If caller provided default position/rotation use the resource TRS as the body transform.
        // Otherwise use the supplied position/rotation as-is.
        bool useResourceTRS = true;

        // Check position == zero
        const double epsPos = 1e-9;
        if (std::abs(position.GetX()) > epsPos || std::abs(position.GetY()) > epsPos || std::abs(position.GetZ()) > epsPos)
            useResourceTRS = false;

        // Check rotation == identity
        const double epsRot = 1e-9;
        if (std::abs(rotation.GetX()) > epsRot || std::abs(rotation.GetY()) > epsRot || std::abs(rotation.GetZ()) > epsRot || std::abs(rotation.GetW() - 1.0) > epsRot)
            useResourceTRS = false;

        JPH::RVec3 finalPos = position;
        JPH::Quat  finalRot = rotation;

        if (useResourceTRS)
        {
            const Matrix4f& t = res->GetTransform();
            const Vector3f   trans = t.ExtractTranslation();
            const Vector3f   rotDeg = t.ExtractRotationEuler(); // degrees

            // Convert degrees -> radians for Quaternion constructor (expects radians)
            const float degToRad = static_cast<float>(M_PI) / 180.0f;
            const Quaternion q(rotDeg.GetX() * degToRad, rotDeg.GetY() * degToRad, rotDeg.GetZ() * degToRad);

            finalPos = JPH::RVec3(trans.GetX(), trans.GetY(), trans.GetZ());
            finalRot = JPH::Quat(q.GetX(), q.GetY(), q.GetZ(), q.GetW());
        }

        JPH::BodyCreationSettings settings(
            res->GetShape(),
            finalPos,
            finalRot,
            static_cast<JPH::EMotionType>(res->GetMotionType()),
            objectLayer);

        settings.mFriction = res->GetFriction();
        settings.mRestitution = res->GetRestitution();
        settings.mLinearDamping = res->GetLinearDamping();
        settings.mAngularDamping = res->GetAngularDamping();

        return settings;
    }

    // Fallback: return empty settings if resource not available.
    return JPH::BodyCreationSettings(JPH::ShapeRefC(), position, rotation, JPH::EMotionType::Static, objectLayer);
}

void CPhysicsBodyComponent::OnShutdown()
{
    if (m_bodyId.IsInvalid())
        return;

    PhysicsManager* physics = PhysicsManager::Get();
    if (physics && physics->IsInitialized())
        physics->RemoveBody(m_bodyId);

    m_bodyId = JPH::BodyID();
}

// ── Transform helpers ──────────────────────────────────────────────────────

Matrix4f CPhysicsBodyComponent::GetWorldTransform() const
{
    if (m_bodyId.IsInvalid())
        return m_modelMatrix;

    PhysicsManager* physics = PhysicsManager::Get();
    if (!physics || !physics->IsInitialized())
        return m_modelMatrix;

    JPH::BodyInterface& bi = physics->GetBodyInterface();
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
    // Always update internal matrix so DebugRender uses the latest transform.
    m_modelMatrix = transform;

    if (m_bodyId.IsInvalid())
        return;

    PhysicsManager* physics = PhysicsManager::Get();
    if (!physics || !physics->IsInitialized())
        return;

    const Vector3f   t = transform.ExtractTranslation();
    const Quaternion q = Quaternion::FromMatrix(transform);

    physics->GetBodyInterface().SetPositionAndRotation(
        m_bodyId,
        JPH::RVec3(t.GetX(), t.GetY(), t.GetZ()),
        JPH::Quat(q.GetX(), q.GetY(), q.GetZ(), q.GetW()),
        activation);
}

// ── Debug rendering ───────────────────────────────────────────────────────

void CPhysicsBodyComponent::DebugRender(bgfx::ViewId viewId, ImGuiVisualizers::BgfxRenderPrimitives& prims) const
{
    if (!IsActive())
        return;

    const Matrix4f world = GetWorldTransform();
    const Vector3f scale = GetScale();
    const uint32_t color = 0xff00ffff; // cyan

    // Compose with resource TRS so local offsets / rotation are visualized.
    Matrix4f drawMatrix = world;
    auto res = m_bodyResource.GetResourceAs<CPhysicsBodyResource>();
    if (res)
    {
        drawMatrix = world * res->GetTransform();
    }

    const float* mtx = drawMatrix.data();

    switch (GetShapeType())
    {
    case EPhysicsShapeType::Box:
        prims.RenderWireBox(viewId, mtx, scale.x * 0.5f, scale.y * 0.5f, scale.z * 0.5f, color);
        break;
    case EPhysicsShapeType::Sphere:
        prims.RenderWireSphere(viewId, mtx, scale.x * 0.5f, color);
        break;
    case EPhysicsShapeType::Capsule:
        prims.RenderWireCapsule(viewId, mtx, scale.x * 0.5f, scale.y * 0.5f, color);
        break;
    case EPhysicsShapeType::Cylinder:
        prims.RenderWireCylinder(viewId, mtx, scale.x * 0.5f, scale.y * 0.5f, color);
        break;
    default:
        break;
    }
}