#pragma once
#include "ComponentSystem/ComponentSystem.h"
#include "ResourceManager/ResourceManager.h"
#include "MaterialResource.h"

class CShaderResource;
class CMeshResource;

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
	void Render(bgfx::ViewId viewId, const float* mtx);

	/// Returns true when both the mesh and shader resources have been
	/// fully loaded and finalized by the ResourceManager.
	bool IsLoaded() const;
	bool IsReady() const { return m_ready; }

	// Resource accessors
	std::shared_ptr<CMeshResource> GetMeshResource() const { return m_meshRes; }		

private:
	// Serialized resource paths (populated via reflection / data files)	
	CResourceReference m_meshResource;    // Mesh binary path,         e.g. "meshes/bunny.bin"
	
	// Runtime resource handles (managed by ResourceManager, not serialized)
	std::shared_ptr<CMeshResource>  m_meshRes;	
	CResourceReference m_materialFile;

	MeshState m_meshState;
	MeshState::Texture m_texture;

	CMaterialDefinition m_materialDefinition;
	bool m_ready = false;

	// declare static handle once (outside per-frame code)
	bgfx::UniformHandle u_sampler = BGFX_INVALID_HANDLE;
	bgfx::UniformHandle u_lightDir = BGFX_INVALID_HANDLE;
	bgfx::UniformHandle u_lightColor = BGFX_INVALID_HANDLE;
	bgfx::UniformHandle u_ambient = BGFX_INVALID_HANDLE;
	bgfx::UniformHandle u_materialColor = BGFX_INVALID_HANDLE;


};