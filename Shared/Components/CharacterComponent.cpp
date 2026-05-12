#include "CharacterComponent.h"
#include "CharacterComponent.h"
#include "BgfxRenderPrimitives.h"
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>

REFL_DEFINE_OBJECT(CCharacterComponent)    
REFL_DEFINE_END

REGISTER_COMPONENT(CCharacterComponent, "Character", "Physics");

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

    // Use very high angular damping to prevent the character from tipping over.
    // This keeps the character upright without locking rotation entirely.
    settings.mAngularDamping = 1000.0f;

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

