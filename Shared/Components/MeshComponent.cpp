#include "CoreSystem/CoreSystem.h"
#include "ComponentSystem/ComponentSystem.h"
#include "MeshComponent.h"
#include "ShaderResource.h"
#include "MeshResource.h"

// ── CMeshComponent ─────────────────────────────────────────────────
REGISTER_COMPONENT(CMeshComponent, "Mesh", "Graphics");

REFL_DEFINE_OBJECT(CMeshComponent)
	REFL_DEFINE_OBJECT_MEMBER(CMeshComponent, m_meshResource),	
	REFL_DEFINE_OBJECT_MEMBER(CMeshComponent, m_materialFile),
REFL_DEFINE_END

bool CMeshComponent::OnInitialize()
{
	m_ready = false;
	m_materialDefinition.Reset();
	if (!Core::CoreSystem::IsInitialized())
	{
		std::cerr << "CMeshComponent: CoreSystem is not initialized" << std::endl;
		return false;
	}

	auto* resourceManager = Core::CoreSystem::GetResourceManager();
	if (!resourceManager)
	{
		std::cerr << "CMeshComponent: ResourceManager is null" << std::endl;
		return false;
	}

	// Request the mesh resource (async load → finalize on main thread)
	if (!m_meshResource.GetResourceFileName().empty())
	{
		m_meshRes = resourceManager->RequestResource<CMeshResource>(m_meshResource.GetResourceFileName());
		if (!m_meshRes)
		{
			std::cerr << "CMeshComponent: Failed to request mesh resource: "
				<< m_meshResource.GetResourceFileName() << std::endl;
			return false;
		}
	}

	// Request the mesh resource (async load → finalize on main thread)
	m_materialDefinition.SafeRead(m_materialFile.GetResourceFileName());
	m_materialDefinition.Initialize();
	return true;
}

void CMeshComponent::OnUpdate(double /*deltaTime*/)
{
	// Wait for loaded before setting ready.
	if (!m_ready)
	{			
		if (IsLoaded())
		{
			m_ready = true;
		}
		m_materialDefinition.Update();
	}
}

void CMeshComponent::OnShutdown()
{
	m_meshRes.reset();
	m_materialDefinition.Reset();	
}

void CMeshComponent::Render(bgfx::ViewId viewId, const float* mtx) 
{
	if (IsReady())
	{		
		m_texture.m_texture = m_materialDefinition.GetTexture(0);
		m_texture.m_flags = 0;
		m_texture.m_sampler = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
		m_meshState.m_textures[0] = m_texture;
		m_meshState.m_numTextures = 1;
		m_meshState.m_state = BGFX_STATE_DEFAULT;
		m_meshState.m_program = m_materialDefinition.GetShaderProgram();
		m_meshState.m_viewId = viewId;

		const MeshState* statePtr = &m_meshState;
		m_meshRes->m_mesh->submit(&statePtr, 1, mtx, 1);
	}
}

bool CMeshComponent::IsLoaded() const
{	
	return (m_meshRes
		&& m_meshRes->IsFinalized()
		&& m_meshRes->m_mesh != nullptr
		&& m_materialDefinition.IsReady());
}