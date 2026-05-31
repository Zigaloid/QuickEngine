#include "CoreSystem/CoreSystem.h"
#include "ComponentSystem/ComponentSystem.h"
#include "LevelComponent.h"
#include "ShaderResource.h"
#include "MeshResource.h"
#include "NavigationResource.h"

// ── CMeshComponent ─────────────────────────────────────────────────
REGISTER_COMPONENT(CLevelComponent, "Level", "LD");

REFL_DEFINE_OBJECT(CLevelComponent)
	REFL_DEFINE_STRING_MEMBER(CLevelComponent, m_name),
	REFL_DEFINE_BOOL_MEMBER(CLevelComponent, m_visibleInEditor),
	REFL_DEFINE_OBJECT_MEMBER(CLevelComponent, m_navMeshResource),
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
	Component::OnShutdown();
}

bool CLevelComponent::IsReady() const
{	
	return true;
}

const dtNavMesh* CLevelComponent::GetNavMesh() const
{
	auto res = m_navMeshResource.GetResourceAs<CNavigationResource>();
	if (res && res->IsFinalized())
		return res->GetNavMesh();
	return nullptr;
}
