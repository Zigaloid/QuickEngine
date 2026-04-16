#include "MeshComponent.h"

#include "CoreSystem/CoreSystem.h"
#include "ComponentSystem/ComponentSystem.h"
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


	m_materialDefinition.SafeRead(m_materialFile.GetResourceFileName());
	m_materialDefinition.Initialize();

    for (int i = 0; i < 4; i++)
	{
		m_samplers[i] = bgfx::createUniform(("s_texColor" + std::to_string(i)).c_str(), bgfx::UniformType::Sampler);
	}    

	m_lightDir = bgfx::createUniform("u_lightDir", bgfx::UniformType::Vec4);
	m_lightColor = bgfx::createUniform("u_lightColor", bgfx::UniformType::Vec4);
	m_ambient = bgfx::createUniform("u_ambient", bgfx::UniformType::Vec4);
	m_materialColor = bgfx::createUniform("u_materialColor", bgfx::UniformType::Vec4);

    
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
	
    bgfx::destroy(m_lightDir);
	bgfx::destroy(m_lightColor);
	bgfx::destroy(m_ambient);
	bgfx::destroy(m_materialColor);
    for (int i = 0; i < 4; i++)
	{
        bgfx::destroy(m_samplers[i]);
		m_samplers[i] = BGFX_INVALID_HANDLE;
	}	
    m_lightDir = BGFX_INVALID_HANDLE;
	m_lightColor = BGFX_INVALID_HANDLE;
	m_ambient = BGFX_INVALID_HANDLE;
	m_materialColor = BGFX_INVALID_HANDLE;
	m_meshStateInitialized = false;
}
void CMeshComponent::Render(bgfx::ViewId viewId, const float* mtx)
{
	if (IsReady())
	{
		if (m_meshStateInitialized == false)
		{
			// set uniform values (adjust values as needed)
			const float lightDir[4] = { 0.57735f, -0.57735f, 0.57735f, 0.0f }; // normalized direction
			const float lightColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
            bgfx::setUniform(m_lightDir, lightDir);
			bgfx::setUniform(m_lightColor, lightColor);
			bgfx::setUniform(m_ambient, m_materialDefinition.GetAmbientColor().data());
			bgfx::setUniform(m_materialColor, m_materialDefinition.GetMaterialColor().data());


			// assign the sampler handle (don't recreate each frame)
			int numTextures = m_materialDefinition.GetNumberOfTextures();
			if (numTextures > 3) numTextures = 3;
			for (int i = 0; i <= numTextures; i++)
			{
				m_texture[i].m_texture = m_materialDefinition.GetTexture(i);
				m_texture[i].m_flags = 0;
                m_texture[i].m_sampler = m_samplers[i];
				m_meshState.m_textures[i] = m_texture[i];
			}

			m_meshState.m_numTextures = 1;
			m_meshState.m_state = BGFX_STATE_WRITE_RGB
			                    | BGFX_STATE_WRITE_A
			                    | BGFX_STATE_WRITE_Z
			                    | BGFX_STATE_DEPTH_TEST_LESS
			                    | BGFX_STATE_CULL_CCW
			                    | BGFX_STATE_MSAA;
			m_meshState.m_program = m_materialDefinition.GetShaderProgram();
			m_meshState.m_viewId = viewId;
			m_meshStateInitialized = true;
		}
		
		if (m_meshStateInitialized)
		{
			const MeshState* statePtr = &m_meshState;
            if (m_meshRes->GetMesh())
			{
				m_meshRes->GetMesh()->submit(&statePtr, 1, mtx, 1);
			}
		}
	}
}

bool CMeshComponent::IsLoaded() const
{	
	return (m_meshRes
		&& m_meshRes->IsFinalized()
    && m_meshRes->GetMesh() != nullptr
		&& m_materialDefinition.IsReady());
}