#pragma once
#include "ComponentSystem/ComponentSystem.h"
#include "ResourceManager/ResourceManager.h"
#include "StaticMeshResource.h"
#include "Math/Matrix4f.h"

class CRenderComponent : public ComponentSystem::Component
{
public:
    REFL_DECLARE_OBJECT(CRenderComponent, ComponentSystem::Component);
    DECLARE_COMPONENT();
	CRenderComponent()
	{
        m_modelMatrix.Identity();
	};

	virtual void Render(bgfx::ViewId viewId) {};
    Matrix4f GetModelMatrix() const { return m_modelMatrix; }
private:
    Matrix4f m_modelMatrix;
};

class CMeshComponent : public CRenderComponent
{
public:
	REFL_DECLARE_OBJECT(CMeshComponent, CRenderComponent);
	DECLARE_COMPONENT();

	// ── IComponent lifecycle ────────────────────────────────────────────

	bool OnInitialize() override;
	void OnUpdate(double deltaTime) override;
	void OnShutdown() override;

	// ── Public API ──────────────────────────────────────────────────────

	void Render(bgfx::ViewId viewId);
	bool IsLoaded() const;	
	std::shared_ptr<CMeshResource> GetMeshResource() const { return m_meshResource.GetResourceAs<CMeshResource>(); }
	void SetMeshResource(const CStaticMeshResourceReference& meshRef) { m_meshResource = meshRef; }
private:
	CStaticMeshResourceReference m_meshResource;

	MeshState m_meshState;
	MeshState::Texture m_texture[4];
	bgfx::UniformHandle m_samplers[4] = { BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE };
	bool m_meshStateInitialized = false;
	bgfx::UniformHandle m_lightDir       = BGFX_INVALID_HANDLE;
	bgfx::UniformHandle m_lightColor     = BGFX_INVALID_HANDLE;
	bgfx::UniformHandle m_ambient        = BGFX_INVALID_HANDLE;
	bgfx::UniformHandle m_materialColor  = BGFX_INVALID_HANDLE;
};