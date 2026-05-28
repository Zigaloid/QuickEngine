#pragma once

#include "ComponentSystem/ComponentSystem.h"
#include "Physics/PhysicsManager.h"
#include "TransformComponent.h"
#include "Math/Matrix4f.h"
#include "Math/Vector3f.h"
#include "Math/Quaternion.h"

#include <bgfx/bgfx.h>

#include <Jolt/Jolt.h>
JPH_SUPPRESS_WARNINGS
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>

// Base class for components that own a physics body. Provides common logic
// for caching the parent transform, initializing the body when physics is
// available, and syncing the simulation transform back to the entity.
class CPhysicsComponent : public ComponentSystem::Component
{
public:
    REFL_DECLARE_OBJECT(CPhysicsComponent, ComponentSystem::Component);
    CPhysicsComponent() = default;
    ~CPhysicsComponent() override = default;

    bool OnInitialize() override
    {
        CacheParentTransform();

        PhysicsManager* physics = PhysicsManager::Get();
        if (physics && physics->IsInitialized())
        {
            if (!m_bodyInitialized)
                m_bodyInitialized = CreateBodyAtTransform(physics);
        }
        return true;
    }

    void OnUpdate(double /*deltaTime*/) override
    {
        PhysicsManager* physics = PhysicsManager::Get();

        // Attempt deferred initialization
        if (!m_bodyInitialized)
        {
            if (physics && physics->IsInitialized() && m_parentTransform)
                m_bodyInitialized = CreateBodyAtTransform(physics);
        }

        // Sync simulation pose back to the owning transform
        if (m_bodyInitialized && m_parentTransform && physics && physics->IsInitialized() && !m_bodyId.IsInvalid())
        {
            Matrix4f world = ComputeWorldTransform();
            ApplyTransformToParent(world);
        }
    }

    void OnShutdown() override
    {
        m_shape = nullptr;

        if (m_bodyId.IsInvalid())
            return;

        PhysicsManager* physics = PhysicsManager::Get();
        if (physics && physics->IsInitialized())
            physics->RemoveBody(m_bodyId);

        m_bodyId = JPH::BodyID();
        m_bodyInitialized = false;
        m_parentTransform = nullptr;
        Component::OnShutdown();
    }

	JPH::BodyID GetBodyID() const { return m_bodyId; }

	// Returns the body's current world-space position and rotation as a Matrix4f.
	// Returns identity if the body is not yet valid.
	Matrix4f GetWorldTransform() const { return ComputeWorldTransform(); }

	/// Debug render the collision shape into the 3D view. Override in derived classes.
	virtual void DebugRender(bgfx::ViewId /*viewId*/, Matrix4f& /*transform*/) const {}

	// ── Editor interface ───────────────────────────────────────────────────
	// Derived classes expose a shared Matrix4f so the gizmo system can edit
	// the shape's local transform (position / rotation / scale) in-place.
	// The matrix convention matches DebugRender: scale encodes shape dimensions.
	// Returns nullptr if editing is not supported.
	virtual std::shared_ptr<Matrix4f> GetEditableTransform()      const { return nullptr; }
	virtual std::shared_ptr<Vector4f> GetEditableBoundingSphere() const { return nullptr; }
    
    Matrix4f& GetObjectMatrix() { return m_objectMatrix; }
    const Matrix4f& GetConstObjectMatrix() const { return m_objectMatrix; }
    // Pushes position + rotation extracted from @p transform into the simulation.
    void SetWorldTransform(const Matrix4f& transform,
        JPH::EActivation activation = JPH::EActivation::Activate);
protected:
	// ── Template-method hooks ──────────────────────────────────────────────
	// Builds the collision shape from the cached scale matrix. Called once
	// before the body is created. Override to set m_shape.
	virtual void InitializeShape(const Matrix4f& /*scaleMtx*/) {}

	// Returns the motion type for this body (used to select the object layer).
	virtual JPH::EMotionType GetMotionType() const { return JPH::EMotionType::Dynamic; }

	// Returns fully configured BodyCreationSettings using m_shape.
	// Override to apply per-type properties (friction, damping, etc.).
	virtual JPH::BodyCreationSettings MakeBodyCreationSettings(
		JPH::RVec3Arg    position,
		JPH::QuatArg     rotation,
		JPH::ObjectLayer objectLayer) const;

	// Common body creation and placement — calls InitializeShape,
	// MakeBodyCreationSettings, and SetWorldTransform.
	virtual bool CreateBodyAtTransform(PhysicsManager* physics);
	bool CreateBody(const Matrix4f& worldTransform, PhysicsManager* physics);

	// Called by base when the body's world transform has been computed.
	// Default applies worldTransform * m_cachedScale to the parent transform.
	// Override to add extra behaviour (e.g. skip static bodies).
	virtual void ApplyTransformToParent(const Matrix4f& worldTransform)
	{
		if (m_parentTransform)
			*m_parentTransform = worldTransform * m_cachedScale;
	}

    // Helper: compute world transform from the physics body.
    Matrix4f ComputeWorldTransform() const
    {
        PhysicsManager* physics = PhysicsManager::Get();
        if (!physics || !physics->IsInitialized() || m_bodyId.IsInvalid())
            return Matrix4f::GetIdentity();

        JPH::BodyInterface& bi = physics->GetBodyInterfaceLocking();
        const JPH::RVec3 pos = bi.GetPosition(m_bodyId);
        const JPH::Quat rot = bi.GetRotation(m_bodyId);

        const Quaternion q(rot.GetW(), rot.GetX(), rot.GetY(), rot.GetZ());
        Matrix4f result = q.ToMatrix();
        result.SetTranslation(Vector3f(
            static_cast<float>(pos.GetX()),
            static_cast<float>(pos.GetY()),
            static_cast<float>(pos.GetZ())));
        return result;
    }

    void CacheParentTransform()
    {
        auto* parent = GetParent();
        if (parent)
        {
            CTransformComponent* parentTransform = parent->FindActiveChild<CTransformComponent>();
            if (parentTransform)
                m_parentTransform = &parentTransform->GetTransform();
        }
    }

protected:
	Matrix4f m_objectMatrix;
	// Runtime state shared by physics-based components
	JPH::BodyID    m_bodyId          = JPH::BodyID();
	Matrix4f*      m_parentTransform = nullptr;
	bool           m_bodyInitialized = false;
	JPH::ShapeRefC m_shape;
	Matrix4f       m_cachedScale;
};
