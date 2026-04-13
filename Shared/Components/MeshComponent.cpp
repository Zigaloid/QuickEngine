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


	// declare static handle once (outside per-frame code)
	u_sampler = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
	u_lightDir = bgfx::createUniform("u_lightDir", bgfx::UniformType::Vec4);
	u_lightColor = bgfx::createUniform("u_lightColor", bgfx::UniformType::Vec4);
	u_ambient = bgfx::createUniform("u_ambient", bgfx::UniformType::Vec4);
	u_materialColor = bgfx::createUniform("u_materialColor", bgfx::UniformType::Vec4);
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

	bgfx::destroy(u_sampler);
	bgfx::destroy(u_lightDir);
	bgfx::destroy(u_lightColor);
	bgfx::destroy(u_ambient);
	bgfx::destroy(u_materialColor);
	u_sampler = BGFX_INVALID_HANDLE;
	u_lightDir = BGFX_INVALID_HANDLE;
	u_lightColor = BGFX_INVALID_HANDLE;
	u_ambient = BGFX_INVALID_HANDLE;
	u_materialColor = BGFX_INVALID_HANDLE;
}
void CMeshComponent::Render(bgfx::ViewId viewId, const float* mtx)
{
	if (IsReady())
	{
		// set uniform values (adjust values as needed)
		const float lightDir[4] = { 0.57735f, -0.57735f, 0.57735f, 0.0f }; // normalized direction
		const float lightColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		const float ambientColor[4] = { 0.2f, 0.2f, 0.2f, 1.0f };
		const float materialColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

		bgfx::setUniform(u_lightDir, lightDir);
		bgfx::setUniform(u_lightColor, lightColor);
		bgfx::setUniform(u_ambient, ambientColor);
		bgfx::setUniform(u_materialColor, materialColor);

		// assign the sampler handle (don't recreate each frame)
		m_texture.m_texture = m_materialDefinition.GetTexture(0);
		m_texture.m_flags = 0;
		m_texture.m_sampler = u_sampler;
		m_meshState.m_textures[0] = m_texture;
		m_meshState.m_numTextures = 1;
		m_meshState.m_state = BGFX_STATE_WRITE_RGB	| BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_CULL_CCW | BGFX_STATE_MSAA;
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