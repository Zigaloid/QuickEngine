#include "MeshComponent.h"

#include "CoreSystem/CoreSystem.h"
#include "ComponentSystem/ComponentSystem.h"
#include "ShaderResource.h"
#include "MeshResource.h"
#include <bx/bounds.h>

// ── CMeshComponent ─────────────────────────────────────────────────
REGISTER_COMPONENT(CRenderComponent, "RenComp", "Graphics");
REGISTER_COMPONENT(CMeshComponent, "Mesh", "Graphics");

REFL_DEFINE_OBJECT(CMeshComponent)
	REFL_DEFINE_OBJECT_MEMBER(CMeshComponent, m_meshResource),
REFL_DEFINE_END

REFL_DEFINE_OBJECT(CRenderComponent)	
	REFL_DEFINE_MATRIX4_MEMBER(CRenderComponent, m_modelMatrix),
REFL_DEFINE_END

bool CMeshComponent::OnInitialize()
{	
	if (!Core::CoreSystem::IsInitialized())
	{
		std::cerr << "CMeshComponent: CoreSystem is not initialized" << std::endl;
		return false;
	}

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
}

void CMeshComponent::OnShutdown()
{
	// Replace resource references with default-constructed objects to clear stored resource/shared_ptr
	m_meshResource = CStaticMeshResourceReference();	
	
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

void CMeshComponent::Render(bgfx::ViewId viewId)
{
	if (IsLoaded())
	{	
		if (m_meshStateInitialized == false)
		{
			// set uniform values (adjust values as needed)
			const float lightDir[4] = { 0.57735f, -0.57735f, 0.57735f, 0.0f }; // normalized direction
			const float lightColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
            bgfx::setUniform(m_lightDir, lightDir);
			bgfx::setUniform(m_lightColor, lightColor);

			auto matRes = m_meshResource.GetResourceAs<CStaticMeshResource>()->GetMaterialResource();
			if (!matRes)
			{
				// Material not available — bail out of state init.
				return;
			}

			bgfx::setUniform(m_ambient, matRes->GetAmbientColor().data());
			bgfx::setUniform(m_materialColor, matRes->GetMaterialColor().data());

			// assign the sampler handle (don't recreate each frame)
			int numTextures = matRes->GetNumberOfTextures();
			if (numTextures > 4) numTextures = 4; // cap to available slots

			// populate textures [0, numTextures)
			for (int i = 0; i < numTextures; i++)
			{
				m_texture[i].m_texture = matRes->GetTexture(i);
				m_texture[i].m_flags = 0;
				m_texture[i].m_stage = i;
                m_texture[i].m_sampler = m_samplers[i];
				m_meshState.m_textures[i] = m_texture[i];
			}

			m_meshState.m_numTextures = numTextures > 0 ? numTextures : 0;
			m_meshState.m_state = BGFX_STATE_WRITE_RGB
			                    | BGFX_STATE_WRITE_A
			                    | BGFX_STATE_WRITE_Z
			                    | BGFX_STATE_DEPTH_TEST_LESS
			                    | BGFX_STATE_CULL_CCW
			                    | BGFX_STATE_MSAA;
			m_meshState.m_program = matRes->GetShaderProgram();
			m_meshState.m_viewId = viewId;
			m_meshStateInitialized = true;

            auto staticMeshRes = m_meshResource.GetResourceAs<CStaticMeshResource>();
            auto meshRes = staticMeshRes->GetMeshResource();
            const Mesh* mesh = meshRes->GetMesh();

			// Seed with the first group's sphere, then expand to enclose each subsequent one.
            bx::Sphere result = mesh->m_groups[0].m_sphere;

            for (uint32_t i = 1; i < mesh->m_groups.size(); ++i)
            {
                const bx::Sphere& s = mesh->m_groups[i].m_sphere;

                const bx::Vec3 diff = bx::sub(s.center, result.center);
                const float dist = bx::length(diff);

                // Check if 's' is already contained within 'result'.
                if (dist + s.radius <= result.radius)
                    continue;

                // Check if 'result' is fully inside 's'.
                if (dist + result.radius <= s.radius)
                {
                    result = s;
                    continue;
                }

                // Merge: new radius spans both spheres along their axis.
                const float newRadius = (dist + result.radius + s.radius) * 0.5f;
                const float t = (newRadius - result.radius) / dist;
                result.center = bx::mad(diff, t, result.center);
                result.radius = newRadius;
            }

            // Set and return the sphere. This is shitty. We should cache this.
            *CRenderComponent::GetBoundingSphere() = Vector4f(result.center.x, result.center.y, result.center.z, result.radius);

		}
		
		if (m_meshStateInitialized)
		{
			auto matshRes = m_meshResource.GetResourceAs<CStaticMeshResource>()->GetMeshResource();
			const MeshState* statePtr = &m_meshState;
            if (matshRes->GetMesh())
			{
				matshRes->GetMesh()->submit(&statePtr, 1, GetModelMatrix()->GetData().data(), 1);
			}
		}
	}

}

bool CMeshComponent::IsLoaded() const
{	
	if (!m_meshResource.GetResource()) return false;
	if (!m_meshResource.GetResourceAs<CStaticMeshResource>()->IsFinalized()) return false;
	return true;
}

std::shared_ptr<Vector4f> CMeshComponent::GetBoundingSphere() const
{
	return CRenderComponent::GetBoundingSphere();
}