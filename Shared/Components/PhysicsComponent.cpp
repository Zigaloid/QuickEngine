#include "PhysicsComponent.h"
#include "Reflection/Reflection.h"
#include "PhysicsComponent.h"
#include "Math/Quaternion.h"

REFL_DEFINE_OBJECT(CPhysicsComponent)
	REFL_DEFINE_MATRIX4_MEMBER(CPhysicsComponent, m_objectMatrix),
REFL_DEFINE_END

// ?? Common body creation ???????????????????????????????????????????????????

bool CPhysicsComponent::CreateBodyAtTransform(PhysicsManager* physics)
{
	if (!m_parentTransform)
		return false;

	m_cachedScale = Matrix4f::Scale(m_parentTransform->ExtractScale());

	if (m_bodyId.IsInvalid())
		InitializeShape(m_cachedScale);

	// Strip scale so SetWorldTransform receives a clean rotation+translation matrix.
	Vector3f scale = m_parentTransform->ExtractScale();
	Vector3f invScale(1.0f / scale.GetX(), 1.0f / scale.GetY(), 1.0f / scale.GetZ());
	Matrix4f worldNoScale = *m_parentTransform * Matrix4f::Scale(invScale);

	return CreateBody(worldNoScale, physics);
}

bool CPhysicsComponent::CreateBody(const Matrix4f& worldTransform, PhysicsManager* physics)
{
	if (!physics || !physics->IsInitialized())
		return false;

	JPH::ObjectLayer layer = (GetMotionType() == JPH::EMotionType::Static)
		? PhysicsLayers::NON_MOVING
		: PhysicsLayers::MOVING;

	JPH::BodyCreationSettings bcs = MakeBodyCreationSettings(
		JPH::RVec3::sZero(), JPH::Quat::sIdentity(), layer);

	m_bodyId = physics->GetBodyInterfaceLocking().CreateAndAddBody(
		bcs, JPH::EActivation::Activate);

	if (m_bodyId.IsInvalid())
		return false;

	SetWorldTransform(worldTransform);
	physics->SetBroadPhaseDirty();
	return true;
}

void CPhysicsComponent::SetWorldTransform(const Matrix4f& transform, JPH::EActivation activation)
{
	PhysicsManager* physics = PhysicsManager::Get();
	if (!physics || !physics->IsInitialized())
		return;

	const Vector3f   t  = transform.ExtractTranslation();
	const Quaternion rq = Quaternion::FromMatrix(transform);
	const Quaternion q  = rq.Normalized();
	physics->GetBodyInterfaceLocking().SetPositionAndRotation(
		m_bodyId,
		JPH::RVec3(t.GetX(), t.GetY(), t.GetZ()),
		JPH::Quat(q.GetX(), q.GetY(), q.GetZ(), q.GetW()),
		activation);
}

JPH::BodyCreationSettings CPhysicsComponent::MakeBodyCreationSettings(
	JPH::RVec3Arg position, JPH::QuatArg rotation, JPH::ObjectLayer objectLayer) const
{
	if (m_shape)
	{
		JPH::BodyCreationSettings settings(
			m_shape, position, rotation, GetMotionType(), objectLayer);
		if (settings.mMotionType == JPH::EMotionType::Dynamic)
			settings.mMotionQuality = JPH::EMotionQuality::LinearCast;
		return settings;
	}
	return JPH::BodyCreationSettings(JPH::ShapeRefC(), position, rotation, JPH::EMotionType::Static, objectLayer);
}