#include "CoreSystem/CoreSystem.h"
#include "ComponentSystem/ComponentSystem.h"
#include "TransformComponent.h"
#include "ShaderResource.h"
#include "MeshResource.h"

// ── CMeshComponent ─────────────────────────────────────────────────
REGISTER_COMPONENT(CTransformComponent, "Transform", "Rendering");

REFL_DEFINE_OBJECT(CTransformComponent)
	REFL_DEFINE_VECTOR3_MEMBER(CTransformComponent, m_rotation),
	REFL_DEFINE_VECTOR3_MEMBER(CTransformComponent, m_translation),
	REFL_DEFINE_VECTOR3_MEMBER(CTransformComponent, m_scale),
REFL_DEFINE_END

bool CTransformComponent::OnInitialize()
{
    // Build the transformation matrix from the rotation, translation, and scale
    m_matrix.fromTRS(m_translation, m_rotation, m_scale);
	return true;
}

void CTransformComponent::OnUpdate(double /*deltaTime*/)
{

}

void CTransformComponent::OnShutdown()
{

}

bool CTransformComponent::IsLoaded() const
{
	return true;
}