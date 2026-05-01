#pragma once
#include "Reflection/ReflectionBase.h"
#include "ResourceManager/ResourceManager.h"
#include "MaterialResource.h"
#include "MeshResource.h"
#include "math/Vector4f.h"

#include "bgfx\bgfx.h"
#include "bgfx_utils.h"

class CStaticMeshResource : public ResourceSystem::Resource
{
public:
	REFL_DECLARE_OBJECT(CStaticMeshResource, ResourceSystem::Resource);

	static std::vector<std::string_view> GetSupportedExtensions()
	{
		return { ".smesh.obj.json" };
	}

	CStaticMeshResource()
	{
	}

	explicit CStaticMeshResource(const std::string& path)
		: Resource(path)
	{
	}

	~CStaticMeshResource() override
	{
	}

	// Use base-class Update to load the file data on the worker thread. 
	bool Update(FileSystem::FileSystemManager& fileSystem) override;

	// Finalize runs on the main thread – safe for bgfx resource creation.
	// Uses shader handles from shader resources to create a program.
	void Finalize() override;

	std::shared_ptr<CMeshResource> GetMeshResource() const { return m_meshResource.GetResourceAs<CMeshResource>(); }
	std::shared_ptr<CMaterialResource> GetMaterialResource() const { return m_materialResource.GetResourceAs<CMaterialResource>(); }

	bool Initialize();

	void Reset()
	{
		m_meshResource.GetResourceAs<CMeshResource>().reset();
		m_materialResource.GetResourceAs<CMaterialResource>().reset();
	}

	bool IsReady() const 
	{ 		
		return IsLoaded();
	}
	bool IsLoaded() const;

private:
	// Serialized resource paths (populated via reflection / data files)	
	CMeshResourceReference m_meshResource;    // Mesh binary path,         e.g. "meshes/bunny.bin"	
	CMaterialResourceReference m_materialResource;
};

class CStaticMeshResourceReference : public CTypedResourceReference<CStaticMeshResource>
{
public:
	REFL_DECLARE_OBJECT(CStaticMeshResourceReference, CTypedResourceReference<CStaticMeshResource>);
};
