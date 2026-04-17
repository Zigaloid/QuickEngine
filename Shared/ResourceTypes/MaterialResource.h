#pragma once
#include "Reflection/ReflectionBase.h"
#include "ResourceManager/ResourceManager.h"
#include "ShaderResource.h"
#include "TextureResource.h"
#include "math/Vector4f.h"

#include "bgfx\bgfx.h"
#include "bgfx_utils.h"

class CMaterialResource: public ResourceSystem::Resource
{
public:
	REFL_DECLARE_OBJECT(CMaterialResource, ResourceSystem::Resource);
	~CMaterialResource() override
	{
		if (bgfx::isValid(m_shader))
		{
			bgfx::destroy(m_shader);
			m_shader = BGFX_INVALID_HANDLE;
		}
	}

	std::shared_ptr<CShaderResource> GetVertexShaderResource() const { return m_vertexShaderResource.GetResourceAs<CShaderResource>(); }
	std::shared_ptr<CShaderResource> GetFragmentShaderResource() const { return m_fragmentShaderResource.GetResourceAs<CShaderResource>(); }

	bool Initialize();

	void Reset()
	{
		m_vertexShaderResource.GetResourceAs<CShaderResource>().reset(); m_fragmentShaderResource.GetResourceAs<CShaderResource>().reset(); m_textureResources.clear();
		
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
				m_shader = bgfx::createProgram(m_vertexShaderResource.GetResourceAs<CShaderResource>()->GetShaderHandle(), m_fragmentShaderResource.GetResourceAs<CShaderResource>()->GetShaderHandle(), false);
			}
		}
	}


	bool IsReady() const { return bgfx::isValid(m_shader); }
	bool IsLoaded() const;

	int GetNumberOfTextures() const { return static_cast<int>(m_textureResources.size()); }

	bgfx::TextureHandle GetTexture(int index) const
	{
		if (index >= 0 && static_cast<size_t>(index) < m_textureResources.size())
		{
			if (m_textureResources[index] && m_textureResources[index]->GetResource()->IsFinalized())
				return m_textureResources[index]->GetResourceAs<CTextureResource>()->GetTextureHandle();
		}
		return BGFX_INVALID_HANDLE;
	}
	const Vector4f& GetMaterialColor() const { return m_materialColor; }
	const Vector4f& GetAmbientColor() const { return m_ambientColor; }

private:
	CShaderResourceReference m_vertexShaderResource;
	CShaderResourceReference m_fragmentShaderResource;
	std::vector<std::unique_ptr<CTextureResourceReference>> m_textureResources;	
	bgfx::ProgramHandle m_shader = BGFX_INVALID_HANDLE;
	std::vector<int> m_textureFlags;
	std::vector<int> m_textureStages;
	// set uniform values (adjust values as needed)
	Vector4f m_materialColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	Vector4f m_ambientColor = { 0.3f, 0.3f, 0.3f, 1.0f };
	// assign the sampler handle (don't recreate each frame)
	int m_flags = 0;
};
