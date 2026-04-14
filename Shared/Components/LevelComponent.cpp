#include "CoreSystem/CoreSystem.h"
#include "ComponentSystem/ComponentSystem.h"
#include "LevelComponent.h"
#include "ShaderResource.h"
#include "MeshResource.h"

// ── CMeshComponent ─────────────────────────────────────────────────
REGISTER_COMPONENT(CLevelComponent, "Level", "LD");

REFL_DEFINE_OBJECT(CLevelComponent)
	REFL_DEFINE_STRING_MEMBER(CLevelComponent, m_name),
REFL_DEFINE_END

bool CLevelComponent::OnInitialize()
{   
	return true;
}

void CLevelComponent::OnUpdate(double /*deltaTime*/)
{

}

void CLevelComponent::OnShutdown()
{

}

bool CLevelComponent::IsReady() const
{
	return true;
}