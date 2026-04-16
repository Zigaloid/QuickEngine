#pragma once
#include "Reflection/ReflectionBase.h"
#include "ResourceManager/ResourceManager.h"
#include "bgfx\bgfx.h"
#include "bgfx_utils.h"

class CTextureResource : public ResourceSystem::Resource
{
public:
	explicit CTextureResource(const std::string& path)
		: Resource(path)
		, m_texture(BGFX_INVALID_HANDLE)
	{
	}

	~CTextureResource() override
	{
		if (bgfx::isValid(m_texture))
		{
			bgfx::destroy(m_texture);
			m_texture = BGFX_INVALID_HANDLE;
		}
	}

    bgfx::TextureHandle GetTextureHandle() const { return m_texture; }

	// Use base-class Update to load the file data on the worker thread.
	bool Update(FileSystem::FileSystemManager& fileSystem) override
	{
		return Resource::Update(fileSystem);
	}

	// Finalize runs on the main thread – safe for bgfx resource creation.
	// Uses bgfx_utils to create a texture from the in-memory data loaded by the Resource system.
	void Finalize() override
	{
		// Guard: only attempt to create texture if data was actually loaded.
		if (GetLoadedSize() == 0 || GetData().empty())
		{
			m_isFinalized = false;
			return;
		}

		// bgfx_utils should provide a helper similar to loadShaderFromMemory for textures.
		// This mirrors how shaders are created in CShaderResource.
		m_texture = loadTextureFromMemory(GetData().data(), static_cast<uint32_t>(GetLoadedSize()), m_path.c_str());
		m_isFinalized = bgfx::isValid(m_texture);
	}
private:
	bgfx::TextureHandle m_texture = BGFX_INVALID_HANDLE;
};