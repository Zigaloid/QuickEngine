#include "PhysicsBodyComponent.h"
#include "ResourceManager/ResourceManager.h"
#include "CoreSystem/CoreSystem.h"
#include "Physics/PhysicsManager.h"
#include "Math/Quaternion.h"

#include "../ResourceTypes/PhysicsBodyResource.h"

// Include primitives renderer for DebugRender implementation
#include "../ImguiVizualizers/BgfxRenderPrimitives.h"
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
        return res->MakeBodyCreationSettings(position, rotation, objectLayer);
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
        return Matrix4f::GetIdentity();

    PhysicsManager* physics = PhysicsManager::Get();
    if (!physics || !physics->IsInitialized())
        return Matrix4f::GetIdentity();

    JPH::BodyInterface& bi  = physics->GetBodyInterface();
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

    const Matrix4f world    = GetWorldTransform();
    const Vector3f scale    = GetScale();
    const float*   mtx      = world.data();
    const uint32_t color    = 0xff00ffff; // cyan

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
