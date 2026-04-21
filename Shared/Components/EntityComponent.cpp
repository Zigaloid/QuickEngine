#include "CoreSystem/CoreSystem.h"
#include "ComponentSystem/ComponentSystem.h"
#include "EntityComponent.h"
#include "ShaderResource.h"
#include "MeshResource.h"

// ── CMeshComponent ─────────────────────────────────────────────────
REGISTER_COMPONENT(CEntityComponent, "Level", "LD");

REFL_DEFINE_OBJECT(CEntityComponent)
	REFL_DEFINE_STRING_MEMBER(CEntityComponent, m_name),
REFL_DEFINE_END

bool CEntityComponent::OnInitialize()
{
	return true;
}

void CEntityComponent::OnUpdate(double /*deltaTime*/)
{

}

void CEntityComponent::OnShutdown()
{

}
