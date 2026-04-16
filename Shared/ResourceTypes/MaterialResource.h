#pragma once
#include "Reflection/ReflectionBase.h"
#include "ResourceManager/ResourceManager.h"
#include "ShaderResource.h"
#include "TextureResource.h"
#include "math/Vector4f.h"

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

	std::shared_ptr<CShaderResource> GetVertexShaderResource() const { return m_vertexShader; }
	std::shared_ptr<CShaderResource> GetFragmentShaderResource() const { return m_fragmentShader; }

	bool Initialize();

	void Reset() 
	{ 
		m_fragmentShader.reset(); m_vertexShader.reset(); m_textureResources.clear();
		m_textures.clear();		
		if (bgfx::isValid(m_shader))
		{
			bgfx::destroy(m_shader);
			m_shader = BGFX_INVALID_HANDLE;
		}		
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


    bool IsReady() const { return bgfx::isValid(m_shader); }
	bool IsLoaded() const;

	int GetNumberOfTextures() const { return static_cast<int>(m_textures.size()); }

	bgfx::TextureHandle GetTexture(int index) const
	{
		if (index >= 0 && static_cast<size_t>(index) < m_textures.size())
		{
			if (m_textures[index] && m_textures[index]->IsFinalized())
				return m_textures[index]->GetTextureHandle();
		}
		return BGFX_INVALID_HANDLE;
	}
    const Vector4f &GetMaterialColor() const { return m_materialColor; }
    const Vector4f &GetAmbientColor() const { return m_ambientColor; }

private:
	CResourceReference m_vertexShaderResource;
	CResourceReference m_fragmentShaderResource;	
	std::vector<std::unique_ptr<CResourceReference>> m_textureResources;

	bgfx::ProgramHandle m_shader = BGFX_INVALID_HANDLE;	
	std::shared_ptr<CShaderResource> m_vertexShader;
	std::shared_ptr<CShaderResource> m_fragmentShader;	
	std::vector<std::shared_ptr<CTextureResource>> m_textures;
    std::vector<int> m_textureFlags;
    std::vector<int> m_textureStages;
	// set uniform values (adjust values as needed)
	Vector4f m_materialColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	Vector4f m_ambientColor = { 0.3f, 0.3f, 0.3f, 1.0f };
	// assign the sampler handle (don't recreate each frame)
	int m_flags = 0;
};
