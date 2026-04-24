#pragma once
#include "Reflection/ReflectionBase.h"
#include "ResourceManager/ResourceManager.h"
#include "ComponentSystem\ComponentSystem.h"
#include "EntityComponent.h"

#include "bgfx\bgfx.h"
#include "bgfx_utils.h"

class CEntityComponentResource : public ResourceSystem::Resource
{
public:
	static std::vector<std::string_view> GetSupportedExtensions()
	{
		return { ".ent.obj.json" };
	}

	explicit CEntityComponentResource(const std::string& path)
		: Resource(path)		
	{
	}

	~CEntityComponentResource() override
	{
	}
    
	// Use base-class Update to load the file data on the worker thread.
	bool Update(FileSystem::FileSystemManager& fileSystem) override
	{
        m_component = std::make_shared<CEntityComponent>();
		if( m_component->SafeRead(m_path).IsError())
		{
			m_component = nullptr;
			m_isLoaded = false;
			return false;
		}
        m_isLoaded = true;
		return true;
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
		m_isFinalized = true;
	}
private:
	std::shared_ptr<CEntityComponent> m_component = nullptr;
};

class CEntiyComponentResourceReference : public CTypedResourceReference<CEntityComponentResource>
{
public:
	REFL_DECLARE_OBJECT(CEntiyComponentResourceReference, CResourceReference);
};