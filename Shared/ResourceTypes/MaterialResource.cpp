
#include "MaterialResource.h"
#include "ResourceManager/ResourceManager.h"
#include "CoreSystem/CoreSystem.h"

// ── CMaterialDefinition ───────────────────────────────────────────
REFL_DEFINE_OBJECT(CMaterialResource)
	REFL_DEFINE_VECTOR4_MEMBER(CMaterialResource, m_materialColor),
	REFL_DEFINE_VECTOR4_MEMBER(CMaterialResource, m_ambientColor),
	REFL_DEFINE_OBJECT_MEMBER(CMaterialResource, m_vertexShaderResource),
	REFL_DEFINE_OBJECT_MEMBER(CMaterialResource, m_fragmentShaderResource),
	REFL_DEFINE_OBJECT_PTR_VECTOR_MEMBER(CMaterialResource, m_textureResources),
	REFL_DEFINE_INT_VECTOR_MEMBER(CMaterialResource, m_textureFlags),
	REFL_DEFINE_INT_VECTOR_MEMBER(CMaterialResource, m_textureStages)
REFL_DEFINE_END

bool CMaterialResource::IsLoaded() const
{
	if (!m_vertexShaderResource.GetResourceAs<CShaderResource>()
		|| !m_vertexShaderResource.GetResourceAs<CShaderResource>()->IsLoaded()
		|| !m_vertexShaderResource.GetResourceAs<CShaderResource>()->IsFinalized())
	{
		return false;
	}
	if (!m_fragmentShaderResource.GetResourceAs<CShaderResource>()
		|| !m_fragmentShaderResource.GetResourceAs<CShaderResource>()->IsLoaded()
		|| !m_fragmentShaderResource.GetResourceAs<CShaderResource>()->IsFinalized())
	{
		return false;
	}

	for (const auto& texture : m_textureResources)
	{
		if (!texture || !texture->GetResource() || !texture->GetResource()->IsFinalized())
			return false;
	}

	return true;
}
bool CMaterialResource::Initialize()
{
	m_shader = BGFX_INVALID_HANDLE;
	return true;
}
