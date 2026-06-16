#pragma once
#include "Reflection/ReflectionBase.h"
#include "ResourceManager/ResourceManager.h"
#include "bgfx\bgfx.h"
#include "bgfx_utils.h"

class CMeshResource : public ResourceSystem::Resource
{
public:
	static std::vector<std::string_view> GetSupportedExtensions()
	{
		return { ".mesh.bin" };
	}

	explicit CMeshResource(const std::string& path)
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
		m_isLoaded = true;
		return true;
	}

	// Finalize runs on the main thread – safe for bgfx resource creation.
	// path_ should be the mesh binary path, e.g. "meshes/bunny.mesh.bin".
	void Finalize() override
	{
		m_mesh = meshLoad(m_path.c_str(), true);  // true = keep vertex data in RAM for CPU access
		m_isFinalized = (m_mesh != nullptr);
	}

	// Accessor
	Mesh* GetMesh() { return m_mesh; }
	const Mesh* GetMesh() const { return m_mesh; }

	// Set an in-memory Mesh instance for this resource.
	// Transfers ownership to the resource; any previous mesh is unloaded.
	// Marks the resource as loaded/finalized so callers treat it as ready.
	void SetMesh(Mesh* mesh)
	{
		if (m_mesh)
		{
			meshUnload(m_mesh);
			m_mesh = nullptr;
		}
		m_mesh = mesh;
		// Resource base class members are protected; set them to indicate ready state.
		m_isLoaded = true;
		m_isFinalized = true;
	}

private:
	Mesh* m_mesh = nullptr;
};


class CMeshResourceReference : public CTypedResourceReference<CMeshResource>
{
public:
	REFL_DECLARE_OBJECT(CMeshResourceReference, CTypedResourceReference<CMeshResource>);
};