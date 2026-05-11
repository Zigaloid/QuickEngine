#pragma once

#include "Components/PhysicsComponent.h"
#include "Math/Matrix4f.h"
#include <memory>

// Simple character component that owns a capsule body and exposes a BodyID
class CCharacterComponent : public CPhysicsComponent
{
public:
    REFL_DECLARE_OBJECT(CCharacterComponent, CPhysicsComponent);
    DECLARE_COMPONENT();

    CCharacterComponent()
    {
        // Default shape: diameter = radius*2, totalHeight = halfHeight*2 + diameter.
        const float diameter    = m_radius * 2.0f;
        const float totalHeight = m_halfHeight * 2.0f + diameter;
        m_objectMatrix = Matrix4f::Scale(Vector3f(diameter, totalHeight, diameter));
        m_localTransformPtr = std::shared_ptr<Matrix4f>(&m_objectMatrix, [](Matrix4f*) {});
        m_boundingSpherePtr = std::shared_ptr<Vector4f>(&m_boundingSphere,  [](Vector4f*) {});
    }
    ~CCharacterComponent() override = default;

    // Sets capsule geometry and keeps m_localTransform in sync.
    void SetCapsuleGeometry(float halfHeight, float radius)
    {
        m_halfHeight = halfHeight;
        m_radius     = radius;
        const float diameter    = radius * 2.0f;
        const float totalHeight = halfHeight * 2.0f + diameter;
        m_objectMatrix = Matrix4f::Scale(Vector3f(diameter, totalHeight, diameter));
    }
    void SetMoveSpeed(float s) { m_moveSpeed = s; }
    void SetJumpImpulse(float j) { m_jumpImpulse = j; }

    void DebugRender(bgfx::ViewId viewId, Matrix4f& transform) const override;

    // CPhysicsComponent editor interface — allows the gizmo to edit the capsule
    // position offset and dimensions (scale X = diameter, Y = total height).
    std::shared_ptr<Matrix4f> GetEditableTransform()      const override { return m_localTransformPtr; }
    std::shared_ptr<Vector4f> GetEditableBoundingSphere() const override { return m_boundingSpherePtr; }

    /// Rebuilds m_radius / m_halfHeight from the current m_localTransform scale.
    /// Call after the gizmo has modified the local transform.
    void SyncGeometryFromLocalTransform();

protected:
    void InitializeShape(const Matrix4f& scaleMtx) override;
    JPH::BodyCreationSettings MakeBodyCreationSettings(
        JPH::RVec3Arg    position,
        JPH::QuatArg     rotation,
        JPH::ObjectLayer objectLayer) const override;

private:
    // Local-space transform: translation = offset from entity origin,
    // scale X/Z = diameter (radius*2), scale Y = total height (halfHeight*2 + diameter).
    // Owned here so the gizmo can edit it in-place via the shared_ptr.    
    Vector4f  m_boundingSphere{ 0.0f, 0.0f, 0.0f, 1.0f };
    std::shared_ptr<Matrix4f> m_localTransformPtr;
    std::shared_ptr<Vector4f> m_boundingSpherePtr;

    // Derived from m_localTransform — kept in sync by SyncGeometryFromLocalTransform().
    float m_halfHeight = 0.5f;
    float m_radius     = 0.3f;

    // Movement parameters
    float m_moveSpeed   = 5.0f;
    float m_jumpImpulse = 6.0f;
};
