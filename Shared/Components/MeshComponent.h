#pragma once
#include "ComponentSystem/ComponentSystem.h"
#include "ResourceManager/ResourceManager.h"
#include "StaticMeshResource.h"
#include "Math/Matrix4f.h"

// Forward-declare to avoid pulling in the full physics header here.
class CPhysicsBodyComponent;
class CTransformComponent;
class CLightManagerComponent;

class CRenderComponent : public ComponentSystem::Component
{
public:
	REFL_DECLARE_OBJECT(CRenderComponent, ComponentSystem::Component);
	DECLARE_COMPONENT();
	CRenderComponent()
	{
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
	CTransformComponent* m_parentTransform = nullptr;
	bool m_physicsTransformInitialized = false;
protected:
	Matrix4f m_scale;
	/** @brief Cached weak reference to the sibling physics body — resolved once on first use. */
	ComponentSystem::CachedComponentRef<CPhysicsBodyComponent> m_physicsBodyRef;
};

class CDebugRenderComponent : public CRenderComponent
{
public:
	/** @brief The primitive shape drawn by this component. */
	enum class EDebugShape
	{
		WireBox,
		WireSphere,
		WireCone,
	};

	REFL_DECLARE_OBJECT(CDebugRenderComponent, CRenderComponent);
	DECLARE_COMPONENT();

	void Render(bgfx::ViewId viewId) override;
	bool OnInitialize() override;
	void OnUpdate(double deltaTime) override;
	void OnShutdown() override;

	EDebugShape GetShape() const { return m_shape; }
	void SetShape(EDebugShape shape) { m_shape = shape; }

	const Vector4f& GetColor() const { return m_color; }
	void SetColor(Vector4f color) { m_color = color; }

private:
	EDebugShape m_shape = EDebugShape::WireBox;
	Vector4f    m_color = { 1.0f, 1.0f, 1.0f, 1.0f };
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

	void Render(bgfx::ViewId viewId) override;
	virtual bool IsLoaded() const;
	std::shared_ptr<CMeshResource> GetMeshResource() const { return m_meshResource.GetResourceAs<CMeshResource>(); }
	void SetMeshResource(const CMeshResourceReference& meshRef) { m_meshResource = meshRef; m_meshStateInitialized = false; }

	std::shared_ptr<CMaterialResource> GetMaterialResource() const;
	void SetMaterialResource(const CMaterialResourceReference& matRef);

	virtual std::shared_ptr<Vector4f> GetBoundingSphere() const override;

protected:
	// ── Protected API for derived classes ───────────────────────────────

	/** @brief Initialize mesh rendering state from material and mesh resources.
	 *  Can be overridden by derived classes for custom initialization logic. */
	virtual void InitializeMesh(std::shared_ptr<CMeshResource> meshRes, bgfx::ViewId viewId);

	/** @brief Called when mesh or material resource changes. Can be overridden by subclasses. */
	virtual void OnMeshResourceChanged() { m_meshStateInitialized = false; }

	/** @brief Pushes the current light uniform values to bgfx. Called every frame from Render(). */
	void ApplyLightUniforms();

	// ── Protected mesh state members ────────────────────────────────────────

	CMaterialResourceReference m_materialResource;
	CMeshResourceReference m_meshResource;

	MeshState m_meshState;
	MeshState::Texture m_texture[4];
	bgfx::UniformHandle m_samplers[4] = { BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE };
	bgfx::UniformHandle m_lightDir = BGFX_INVALID_HANDLE;
	bgfx::UniformHandle m_lightColor = BGFX_INVALID_HANDLE;
	bgfx::UniformHandle m_ambient = BGFX_INVALID_HANDLE;
	bgfx::UniformHandle m_materialColor = BGFX_INVALID_HANDLE;
	bool m_meshStateInitialized = false;

	/** @brief Cached pointer to the scene light manager — resolved once on initialize. */
	CLightManagerComponent* m_lightManager = nullptr;
private:
	CMeshResourceReference m_staticMeshResource;
};