
#include "MaterialResource.h"
#include "ResourceManager/ResourceManager.h"
#include "CoreSystem/CoreSystem.h"

// ── CResourceReference ────────────────────────────────────────────
REFL_DEFINE_OBJECT(CMaterialDefinition)
	REFL_DEFINE_OBJECT_MEMBER(CMaterialDefinition, m_vertexShaderResource),
	REFL_DEFINE_OBJECT_MEMBER(CMaterialDefinition, m_fragmentShaderResource),	
REFL_DEFINE_END


bool CMaterialDefinition::Initialize()
{
	m_shader = BGFX_INVALID_HANDLE;
	auto* resourceManager = Core::CoreSystem::GetResourceManager();
	if (!resourceManager)
	{
		std::cerr << "CMaterialDefinition: ResourceManager is null" << std::endl;
		return false;
	}		

	m_vertexShader = resourceManager->RequestResource<CShaderResource>(m_vertexShaderResource.GetResourceFileName());
	if (!m_vertexShader)
	{
		std::cerr << "CMaterialDefinition: Failed to request shader resource: " << m_vertexShaderResource.GetResourceFileName();
		return false;
	}
	m_fragmentShader = resourceManager->RequestResource<CShaderResource>(m_fragmentShaderResource.GetResourceFileName());
	if (!m_fragmentShader)
	{
		std::cerr << "CMaterialDefinition: Failed to request shader resource: " << m_fragmentShaderResource.GetResourceFileName();
		return false;
	}	

/*
	for (const auto& texResRef : m_textureResources)
	{
		if (texResRef && !texResRef->m_resourceFileName.empty())
		{
			auto texRes = resourceManager->RequestResource<CTextureResource>(texResRef->m_resourceFileName);
			if (texRes)
			{
				m_textures.push_back(texRes);
			}
			else
			{
				std::cerr << "CMaterialDefinition: Failed to request texture resource: "
					<< texResRef->m_resourceFileName << std::endl;
				return false;
			}
		}
	}
*/

	return true;
}
