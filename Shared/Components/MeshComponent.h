#pragma once
#include "ComponentSystem/ComponentSystem.h"
#include "ResourceManager/ResourceManager.h"
#include "StaticMeshResource.h"
#include "Math/Matrix4f.h"

// Forward-declare to avoid pulling in the full physics header here.
class CPhysicsBodyComponent;
class CTransformComponent;

class CRenderComponent : public ComponentSystem::Component
{
public:
    REFL_DECLARE_OBJECT(CRenderComponent, ComponentSystem::Component);
    DECLARE_COMPONENT();
	CRenderComponent()
	{
        m_modelMatrix.Identity();
        m_transformPtr = std::shared_ptr<Matrix4f>(&m_modelMatrix, [](Matrix4f*) {});
        m_boundingSpherePtr = std::shared_ptr<Vector4f>(&m_boundingSphere, [](Vector4f*) {});
	};

	virtual std::shared_ptr<Vector4f> GetBoundingSphere() const
	{
		return m_boundingSpherePtr;
	}

	virtual void Render(bgfx::ViewId viewId) {};
	bool OnInitialize() override;
	void OnUpdate(double deltaTime) override;
	void OnShutdown() override;
	std::shared_ptr<Matrix4f> GetModelMatrix() const { return m_transformPtr; }    
private:
	std::shared_ptr<Matrix4f> m_transformPtr;
	std::shared_ptr<Vector4f> m_boundingSpherePtr;
	Vector4f m_boundingSphere;
	Matrix4f m_modelMatrix;
	CTransformComponent* m_parentTransform = nullptr;
	bool m_physicsTransformInitialized = false;
protected:
	Matrix4f m_scale;
	/** @brief Cached weak reference to the sibling physics body — resolved once on first use. */
	ComponentSystem::CachedComponentRef<CPhysicsBodyComponent> m_physicsBodyRef;
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

	virtual std::shared_ptr<Vector4f> GetBoundingSphere() const;

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