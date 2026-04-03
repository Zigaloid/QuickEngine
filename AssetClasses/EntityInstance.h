#pragma once

#include <string>
#include <memory>

#include "Reflection/ReflectionBase.h"
#include "ComponentSystem/ComponentSystem.h"
#include "ComponentSystem/ComponentRegistry.h"
#include "ResourceManager/ResourceManager.h"
#include "Reflection/ReflectionBase.h"

#include "math\Vector4f.h"
#include "bgfx\bgfx.h"
#include "bgfx_utils.h"

class CEntityInstance : public ComponentSystem::Component
{
public:
	REFL_DECLARE_OBJECT(CEntityInstance, Component);
	DECLARE_COMPONENT();

private:
	std::string m_entityDefinition = "undfined";
	std::string m_name = "undfined";
	Vector4f m_color = Vector4f(1, 1, 1, 1);
};

class CResourceReference : public CReflectedBase
{
public:
	REFL_DECLARE_OBJECT(CResourceReference, CReflectedBase);

private:
	std::string m_resourceFileName = "undifined";
};



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

class CShaderResource : public ResourceSystem::Resource
{
public:
	bgfx::ProgramHandle m_shader = BGFX_INVALID_HANDLE;

	// path is used as the base program name.
	// Convention: "mesh" will load shaders "vs_mesh" and "fs_mesh".
	CShaderResource(const std::string& path)
		: Resource(path)
		, m_shader(BGFX_INVALID_HANDLE)
	{
	}

	~CShaderResource() override
	{
		if (bgfx::isValid(m_shader))
		{
			bgfx::destroy(m_shader);
			m_shader = BGFX_INVALID_HANDLE;
		}
	}

	// Skip base-class file reading; bgfx loadProgram handles its own I/O.
	bool Update(FileSystem::FileSystemManager& /*fileSystem*/) override
	{
		isLoaded_ = true;
		return true;
	}

	// Finalize runs on the main thread – safe for bgfx resource creation.
	// Constructs vertex/fragment shader names from the base name:
	//   "mesh" -> loadProgram("vs_mesh", "fs_mesh")
	void Finalize() override
	{
		std::string vsName = "vs_" + path_;
		std::string fsName = "fs_" + path_;
		m_shader = loadProgram(vsName.c_str(), fsName.c_str());
		isFinalized_ = bgfx::isValid(m_shader);
	}
};

class CMeshComponent : public ComponentSystem::Component
{
public:
	REFL_DECLARE_OBJECT(CMeshComponent, Component);
	DECLARE_COMPONENT();

	// Component lifecycle
	bool OnInitialize() override;
	void OnUpdate(double deltaTime) override;
	void OnShutdown() override;

	/// Submit the mesh for rendering on the given bgfx view with a 4x4 transform.
	/// Follows the bgfx 04-mesh example pattern: meshSubmit(mesh, view, program, mtx).
	void Render(bgfx::ViewId viewId, const float* mtx) const;

	/// Returns true when both the mesh and shader resources have been
	/// fully loaded and finalized by the ResourceManager.
	bool IsReadyToRender() const;

	// Path accessors
	void SetMeshPath(const std::string& path) { m_meshResource = path; }
	const std::string& GetMeshPath() const { return m_meshResource; }

	void SetShaderPath(const std::string& path) { m_shaderResource = path; }
	const std::string& GetShaderPath() const { return m_shaderResource; }

	// Resource accessors
	std::shared_ptr<CMeshResource> GetMeshResource() const { return m_meshRes; }
	std::shared_ptr<CShaderResource> GetShaderResource() const { return m_shaderRes; }

private:
	// Serialized resource paths (populated via reflection / data files)
	std::string m_meshResource;    // Mesh binary path,         e.g. "meshes/bunny.bin"
	std::string m_shaderResource;  // Shader program base name, e.g. "mesh" -> vs_mesh + fs_mesh

	// Runtime resource handles (managed by ResourceManager, not serialized)
	std::shared_ptr<CMeshResource>  m_meshRes;
	std::shared_ptr<CShaderResource> m_shaderRes;
};

