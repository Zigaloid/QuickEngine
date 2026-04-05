#include "EntityInstance.h"
#include "Reflection/Reflection.h"
#include "CoreSystem/CoreSystem.h"

#include <bx/math.h>
#include <iostream>

// ── CEntityInstance ────────────────────────────────────────────────
REGISTER_COMPONENT(CEntityInstance, "Entity", "Game Entity");

REFL_DEFINE_OBJECT(CEntityInstance)
	REFL_DEFINE_STRING_MEMBER(CEntityInstance, m_entityDefinition),
	REFL_DEFINE_STRING_MEMBER(CEntityInstance, m_name),
	REFL_DEFINE_VECTOR4_MEMBER(CEntityInstance, m_color)
REFL_DEFINE_END

// ── CResourceReference ────────────────────────────────────────────
REFL_DEFINE_OBJECT(CResourceReference)
	REFL_DEFINE_OBJECT_MEMBER(CResourceReference, m_resourceFileName),
REFL_DEFINE_END

// ── CMeshComponent ─────────────────────────────────────────────────
REGISTER_COMPONENT(CMeshComponent, "Mesh", "Graphics");

REFL_DEFINE_OBJECT(CMeshComponent)
	REFL_DEFINE_STRING_MEMBER(CMeshComponent, m_meshResource),
	REFL_DEFINE_STRING_MEMBER(CMeshComponent, m_shaderResource),
REFL_DEFINE_END

bool CMeshComponent::OnInitialize()
{
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
	if (!m_meshResource.empty())
	{
		m_meshRes = resourceManager->RequestResource<CMeshResource>(m_meshResource);
		if (!m_meshRes)
		{
			std::cerr << "CMeshComponent: Failed to request mesh resource: "
			          << m_meshResource << std::endl;
			return false;
		}
	}

	// Request the shader program resource
	if (!m_shaderResource.empty())
	{
		m_shaderRes = resourceManager->RequestResource<CShaderResource>(m_shaderResource);
		if (!m_shaderRes)
		{
			std::cerr << "CMeshComponent: Failed to request shader resource: "
			          << m_shaderResource << std::endl;
			return false;
		}
	}

	return true;
}

void CMeshComponent::OnUpdate(double /*deltaTime*/)
{
	// Resources are loaded asynchronously by the ResourceManager.
	// Nothing to do here – rendering is driven externally via Render().
}

void CMeshComponent::OnShutdown()
{
	m_meshRes.reset();
	m_shaderRes.reset();
}

void CMeshComponent::Render(bgfx::ViewId viewId, const float* mtx) const
{
	if (!IsReadyToRender())
	{
		return;
	}

	meshSubmit(m_meshRes->m_mesh, viewId, m_shaderRes->m_shader, mtx);
}

bool CMeshComponent::IsReadyToRender() const
{
	return m_meshRes
		&& m_meshRes->IsFinalized()
		&& m_meshRes->m_mesh != nullptr
		&& m_shaderRes
		&& m_shaderRes->IsFinalized()
		&& bgfx::isValid(m_shaderRes->m_shader);
}