#include "CoreSystem/CoreSystem.h"
#include "ComponentSystem/ComponentSystem.h"
#include "TransformComponent.h"
#include "ShaderResource.h"
#include "MeshResource.h"

// ── CMeshComponent ─────────────────────────────────────────────────
REGISTER_COMPONENT(CTransformComponent, "Transform", "Rendering");

REFL_DEFINE_OBJECT(CTransformComponent)
	REFL_DEFINE_MATRIX4_MEMBER(CTransformComponent, m_matrix),
REFL_DEFINE_END

bool CTransformComponent::IsLoaded() const
{
	return true;
}