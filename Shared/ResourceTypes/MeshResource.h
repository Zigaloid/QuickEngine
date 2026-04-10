#pragma once
#include "Reflection/ReflectionBase.h"
#include "ResourceManager/ResourceManager.h"
#include "bgfx\bgfx.h"
#include "bgfx_utils.h"

class CMeshResource : public ResourceSystem::Resource
{
public:
	Mesh* m_mesh = nullptr;

	CMeshResource(const std::string& path)
		: Resource(path)
		, m_mesh(nullptr)
	{
	}

	~CMeshResource() override
	{
		if (m_mesh)
		{
			meshUnload(m_mesh);
			m_mesh = nullptr;
		}
	}

	// Skip base-class file reading; bgfx meshLoad handles its own I/O.
	bool Update(FileSystem::FileSystemManager& /*fileSystem*/) override
	{
		isLoaded_ = true;
		return true;
	}

	// Finalize runs on the main thread – safe for bgfx resource creation.
	// path_ should be the mesh binary path, e.g. "meshes/bunny.bin".
	void Finalize() override
	{
		m_mesh = meshLoad(path_.c_str());
		isFinalized_ = (m_mesh != nullptr);
	}
};
