#include "CharacterComponent.h"
#include "BgfxRenderPrimitives.h"
#include "Physics/PhysicsManager.h"
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Body/BodyInterface.h>

#include <cmath>

REFL_DEFINE_OBJECT(CCharacterComponent)    
REFL_DEFINE_END

REGISTER_COMPONENT(CCharacterComponent, "Character", "Physics");

void CCharacterComponent::OnUpdate(double deltaTime)
{
    DECLARE_FUNC_MEDIUM();
    // Let the base class sync the physics body position back to the entity transform.
    CPhysicsComponent::OnUpdate(deltaTime);

    if (!m_faceDirectionOfTravel || m_bodyId.IsInvalid())
        return;

    PhysicsManager* physics = PhysicsManager::Get();
    if (!physics || !physics->IsInitialized())
        return;

    JPH::BodyInterface& bi = physics->GetBodyInterface();

    // Read horizontal velocity from the physics body.
    const JPH::Vec3 vel    = bi.GetLinearVelocity(m_bodyId);
    const float     horizX = vel.GetX();
    const float     horizZ = vel.GetZ();

    constexpr float kMinSpeedSq = 0.01f * 0.01f;   // ignore jitter / tiny drift
    if ((horizX * horizX + horizZ * horizZ) <= kMinSpeedSq)
        return;

    constexpr float kPi = 3.14159265358979323846f;

    // Target yaw from velocity direction (atan2(X,Z) matches +Z forward convention).
    const float targetYaw = std::atan2(horizX, horizZ);

    // Extract current yaw from the Y-axis quaternion:  yaw = 2 * atan2(qy, qw)
    const JPH::Quat currentRot = bi.GetRotation(m_bodyId);
    const float     currentYaw = 2.0f * std::atan2(currentRot.GetY(), currentRot.GetW());

    // Shortest-path delta, wrapped to [-π, π].
    float delta = targetYaw - currentYaw;
    while (delta >  kPi) delta -= 2.0f * kPi;
    while (delta < -kPi) delta += 2.0f * kPi;

    // Clamp step so we never overshoot in a single frame.
    const float maxStep = m_rotationSpeed * static_cast<float>(deltaTime);
    delta = std::clamp(delta, -maxStep, maxStep);

    const JPH::Quat newRot = JPH::Quat::sRotation(JPH::Vec3::sAxisY(), currentYaw + delta);
    bi.SetRotation(m_bodyId, newRot, JPH::EActivation::DontActivate);
}

void CCharacterComponent::InitializeShape(const Matrix4f& /*scaleMtx*/)
{
    // Derive capsule dimensions from the local transform scale in case the
    // gizmo has modified it since the last body creation.
    SyncGeometryFromLocalTransform();

    JPH::ShapeSettings::ShapeResult result =
        JPH::CapsuleShapeSettings(m_halfHeight, m_radius).Create();

    if (result.HasError())
        return;

    JPH::ShapeRefC baseShape = result.Get();

    // Apply any local offset baked into m_objectMatrix.
    Vector3f resPos = m_objectMatrix.ExtractTranslation();
    Quaternion resRot = Quaternion::FromMatrix(m_objectMatrix).Normalized();

    bool hasOffset   = (resPos.GetX() != 0.0f || resPos.GetY() != 0.0f || resPos.GetZ() != 0.0f);
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

JPH::BodyCreationSettings CCharacterComponent::MakeBodyCreationSettings(
    JPH::RVec3Arg position, JPH::QuatArg rotation, JPH::ObjectLayer objectLayer) const
{
    JPH::BodyCreationSettings settings(
        m_shape, position, rotation, JPH::EMotionType::Dynamic, objectLayer);
    settings.mMotionQuality = JPH::EMotionQuality::LinearCast;

    // Character bodies must never sleep — sleeping bodies stop receiving
    // contact callbacks, which breaks ground state detection.
    settings.mAllowSleeping = false;

    // Lock pitch and roll so the character can never tip over.
    // Only rotation around the Y (up) axis is allowed.
    settings.mAllowedDOFs = JPH::EAllowedDOFs::TranslationX | JPH::EAllowedDOFs::TranslationY |
                            JPH::EAllowedDOFs::TranslationZ | JPH::EAllowedDOFs::RotationY;

    return settings;
}

void CCharacterComponent::SyncGeometryFromLocalTransform()
{
    const Vector3f scale = m_objectMatrix.ExtractScale();
    m_radius     = scale.GetX() * 0.5f;                              // diameter -> radius
    m_halfHeight = (scale.GetY() - scale.GetX()) * 0.5f;            // totalHeight - diameter -> halfHeight*2 -> /2
    if (m_halfHeight < 0.0f) m_halfHeight = 0.0f;

    // Keep bounding sphere in sync.
    const float maxExtent = scale.GetY() * 0.5f;
    m_boundingSphere = Vector4f(0.0f, 0.0f, 0.0f, maxExtent);
}

void CCharacterComponent::DebugRender(bgfx::ViewId viewId, Matrix4f& transform) const
{
    Rendering::BgfxRenderPrimitives& prims = Rendering::BgfxRenderPrimitives::Instance();

    const float* mtx = transform.data();
    const uint32_t color = 0xff00ffff;
    prims.RenderWireCapsule(viewId, mtx, color);
}

