#pragma once
#include "Reflection/ReflectionBase.h"
#include "ResourceManager/ResourceManager.h"
#include "ShaderResource.h"

#include "bgfx\bgfx.h"
#include "bgfx_utils.h"
class CTextureResource;

class CMaterialDefinition : public CReflectedBase
{
public:
	REFL_DECLARE_OBJECT(CMaterialDefinition, CReflectedBase);
	
	std::shared_ptr<CShaderResource> GeVertexShaderResource() const { return m_vertexShader; }
	std::shared_ptr<CShaderResource> GeFragmentShaderResource() const { return m_fragmentShader; }

	bool Initialize();
	void Reset() 
	{ 
		m_fragmentShader.reset(); m_vertexShader.reset();m_textures.clear(); 
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
	bool IsLoaded() const { return m_vertexShader && m_vertexShader->IsFinalized() && m_fragmentShader && m_fragmentShader->IsFinalized(); }
private:
	CResourceReference m_vertexShaderResource;
	CResourceReference m_fragmentShaderResource;

	bgfx::ProgramHandle m_shader = BGFX_INVALID_HANDLE;
	std::vector<CResourceReference*> m_textureResources;
	std::shared_ptr<CShaderResource> m_vertexShader;
	std::shared_ptr<CShaderResource> m_fragmentShader;
	std::vector<std::shared_ptr<CTextureResource>>  m_textures;
};
