#include "StaticMeshResource.h"
#include "ResourceManager/ResourceManager.h"
#include "CoreSystem/CoreSystem.h"

// ── CMaterialDefinition ───────────────────────────────────────────
REFL_DEFINE_OBJECT(CStaticMeshResourceReference)
REFL_DEFINE_END

REFL_DEFINE_OBJECT(CStaticMeshResource)
	REFL_DEFINE_OBJECT_MEMBER(CStaticMeshResource, m_meshResource),
	REFL_DEFINE_OBJECT_MEMBER(CStaticMeshResource, m_materialResource),
REFL_DEFINE_END

bool CStaticMeshResource::IsLoaded() const
{
	if (!m_meshResource.GetResourceAs<CMeshResource>()
		|| !m_meshResource.GetResourceAs<CMeshResource>()->IsLoaded()
		|| !m_meshResource.GetResourceAs<CMeshResource>()->IsFinalized())
	{
		return false;
	}
	if (!m_materialResource.GetResourceAs<CMaterialResource>()
		|| !m_materialResource.GetResourceAs<CMaterialResource>()->IsLoaded()
		|| !m_materialResource.GetResourceAs<CMaterialResource>()->IsFinalized())
	{
		return false;
	}

	return true;
}
bool CStaticMeshResource::Initialize()
{	
	return true;
}

bool CStaticMeshResource::Update(FileSystem::FileSystemManager& fileSystem)
{	
	m_isLoaded = true;
	SafeRead(GetPath());
	return true;
}

void CStaticMeshResource::Finalize()
{
	// Ensure the shader sub-resources are available and finalized
	auto mesh = m_meshResource.GetResourceAs<CMeshResource>();
	auto material = m_materialResource.GetResourceAs<CMaterialResource>();

	if (!mesh || !material)
	{
		m_isFinalized = false;
		return;
	}
	if (!mesh->IsFinalized() || !material->IsFinalized())
	{
		m_isFinalized = false;
		return;
	}
	// mark finalized
	m_isFinalized = true;
}
