#pragma once
#include "Reflection/ReflectionBase.h"
#include "ResourceManager/ResourceManager.h"
#include "ShaderResource.h"
#include "TextureResource.h"

#include "bgfx\bgfx.h"
#include "bgfx_utils.h"

class CMaterialDefinition : public CReflectedBase
{
public:
	REFL_DECLARE_OBJECT(CMaterialDefinition, CReflectedBase);

	~CMaterialDefinition() override
	{
		if (bgfx::isValid(m_shader))
		{
			bgfx::destroy(m_shader);
			m_shader = BGFX_INVALID_HANDLE;
		}
	}

	std::shared_ptr<CShaderResource> GeVertexShaderResource() const { return m_vertexShader; }
	std::shared_ptr<CShaderResource> GeFragmentShaderResource() const { return m_fragmentShader; }

	bool Initialize();

	void Reset() 
	{ 
		m_fragmentShader.reset(); m_vertexShader.reset(); m_textureResources.clear();
		bgfx::ProgramHandle m_shader = BGFX_INVALID_HANDLE;
		
	}

	const bgfx::ProgramHandle GetShaderProgram() const
	{ 
		return m_shader; 
	}

	void Update()
	{
		if (!IsReady())
		{
			if (IsLoaded())
			{
				m_shader = bgfx::createProgram(m_vertexShader->GetShaderHandle(), m_fragmentShader->GetShaderHandle(), false);
			}
		}
	}


	bool IsReady() const { return isValid(m_shader); }
	bool IsLoaded() const;
	
	bgfx::TextureHandle GetTexture(int index)
	{
		if (index < m_textures.size() )
		{
			if (m_textures[index]->IsFinalized())
				return m_textures[index]->GetTextureHandle();
		}
		return BGFX_INVALID_HANDLE;
	}
private:
	CResourceReference m_vertexShaderResource;
	CResourceReference m_fragmentShaderResource;	
	std::vector<std::unique_ptr<CResourceReference>> m_textureResources;

	bgfx::ProgramHandle m_shader = BGFX_INVALID_HANDLE;	
	std::shared_ptr<CShaderResource> m_vertexShader;
	std::shared_ptr<CShaderResource> m_fragmentShader;	
	std::vector<std::shared_ptr<CTextureResource>> m_textures;
};
