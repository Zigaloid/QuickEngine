#include "MeshComponent.h"

#include "CoreSystem/CoreSystem.h"
#include "ComponentSystem/ComponentSystem.h"
#include "TransformComponent.h"
#include "ShaderResource.h"
#include "MeshResource.h"
#include "MaterialResource.h"
#include "PhysicsBodyComponent.h"
#include "LightManagerComponent.h"
#include "Physics/PhysicsManager.h"
#include "Math/Quaternion.h"
#include "Rendering/BgfxRenderPrimitives.h"
#include <bx/bounds.h>


// ── CMeshComponent ─────────────────────────────────────────────────────────
REGISTER_COMPONENT(CRenderComponent, "RenComp", "Graphics");
REGISTER_COMPONENT(CDebugRenderComponent, "DebugRender", "Graphics");
REGISTER_COMPONENT(CMeshComponent, "Mesh", "Graphics");

REFL_DEFINE_OBJECT(CMeshComponent)
	REFL_DEFINE_OBJECT_MEMBER(CMeshComponent, m_meshResource),
	REFL_DEFINE_OBJECT_MEMBER(CMeshComponent, m_materialResource)
REFL_DEFINE_END

REFL_DEFINE_OBJECT(CRenderComponent)
REFL_DEFINE_END

REFL_DEFINE_OBJECT(CDebugRenderComponent)
	REFL_DEFINE_INT_MEMBER(CDebugRenderComponent,m_shape),
	REFL_DEFINE_VECTOR4_MEMBER(CDebugRenderComponent,m_color)
REFL_DEFINE_END

bool CRenderComponent::OnInitialize()
{
	static Matrix4f identity = Matrix4f::GetIdentity();
	m_parentTransform = FindParentTransform(this);
	if (!m_parentTransform)
	{
		m_transformPtr = std::shared_ptr<Matrix4f>(&identity, [](Matrix4f*) {});
	}
	else
	{
		m_transformPtr = std::shared_ptr<Matrix4f>(&m_parentTransform->GetTransform(), [](Matrix4f*) {});	
	}
	m_boundingSphere = Vector4f(0.0f, 0.0f, 0.0f, 1.0f);
	

	return true;
}

void CRenderComponent::OnUpdate(double /*deltaTime*/)
{
}

void CRenderComponent::OnShutdown()
{
	m_physicsBodyRef.Reset();
	m_physicsTransformInitialized = false;
	Component::OnShutdown();
}

// ── CDebugRenderComponent ──────────────────────────────────────────────────

bool CDebugRenderComponent::OnInitialize()
{
	if (!CRenderComponent::OnInitialize())
		return false;

	return true;
}

void CDebugRenderComponent::OnUpdate(double /*deltaTime*/)
{
	auto* renderFunctionQueue = Core::CoreSystem::GetRenderFunctionQueue();
	if (renderFunctionQueue)
	{
		renderFunctionQueue->AddFunction([this]()
			{
				Render(0);
			}, "CDebugRenderComponent::Render");
	}
}

void CDebugRenderComponent::OnShutdown()
{
	CRenderComponent::OnShutdown();
}

void CDebugRenderComponent::Render(bgfx::ViewId viewId)
{
	auto modelMatrix = GetModelMatrix();
	if (!modelMatrix)
		return;

	const float* mtx = modelMatrix->GetData().data();
	Rendering::BgfxRenderPrimitives& prims = Rendering::BgfxRenderPrimitives::Instance();
	uint32_t color;
	bx::packRgba8(&color, m_color.data());

	switch (m_shape)
	{
	case EDebugShape::WireBox:
		prims.RenderWireBox(viewId, mtx, color);
		break;
	case EDebugShape::WireSphere:
		prims.RenderWireSphere(viewId, mtx, color);
		break;
	case EDebugShape::WireCone:
		prims.RenderWireCone(viewId, mtx, color);
		break;
	}
}

// ── CMeshComponent ─────────────────────────────────────────────────────────
int g_INITCOUNT = 0;
bool CMeshComponent::OnInitialize()
{
	g_INITCOUNT++;
	DECLARE_FUNC_VLOW();
	CRenderComponent::OnInitialize();

	if (!Core::CoreSystem::IsInitialized())
	{
		std::cerr << "CMeshComponent: CoreSystem is not initialized" << std::endl;
		return false;
	}

	for (int i = 0; i < 4; i++)
	{
		m_samplers[i] = bgfx::createUniform(("s_texColor" + std::to_string(i)).c_str(), bgfx::UniformType::Sampler);
	}

	m_lightDir      = bgfx::createUniform("u_lightDir",      bgfx::UniformType::Vec4);
	m_lightColor    = bgfx::createUniform("u_lightColor",    bgfx::UniformType::Vec4);
	m_ambient       = bgfx::createUniform("u_ambient",       bgfx::UniformType::Vec4);
	m_materialColor = bgfx::createUniform("u_materialColor", bgfx::UniformType::Vec4);

	return true;
}

void CMeshComponent::OnUpdate(double deltaTime)
{
	DECLARE_FUNC_MEDIUM();
	CRenderComponent::OnUpdate(deltaTime);

	auto* renderFunctionQueue = Core::CoreSystem::GetRenderFunctionQueue();
	if (renderFunctionQueue)
	{
		renderFunctionQueue->AddFunction([this]()
			{
				Render(0);
			}, "CMeshComponent::Render");
	}
}

void CMeshComponent::OnShutdown()
{
	g_INITCOUNT--;
	DECLARE_FUNC_VLOW();

	m_materialResource = CMaterialResourceReference();
	m_meshResource = CMeshResourceReference();

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
	m_lightManager = nullptr;

	CRenderComponent::OnShutdown();
}

std::shared_ptr<CMaterialResource> CMeshComponent::GetMaterialResource() const
{
	if (!m_materialResource.GetResource())
		return nullptr;
	return m_materialResource.GetResourceAs<CMaterialResource>();
}

void CMeshComponent::SetMaterialResource(const CMaterialResourceReference& matRef)
{
	m_materialResource = matRef;
	OnMeshResourceChanged();
}

void CMeshComponent::ApplyLightUniforms()
{
	// Lazy-resolve: if not yet found, walk up to the scene root and search the hierarchy.
	if (!m_lightManager)
	{
		ComponentSystem::Component* root = this;
		while (root->GetParent())
			root = root->GetParent();

		m_lightManager = root->FindDescendant<CLightManagerComponent>();
	}

	Vector4f lightDir, lightColor, ambientColor;
	if (m_lightManager)
	{
		lightDir     = m_lightManager->GetActiveLightDir();
		lightColor   = m_lightManager->GetActiveLightColor();
		ambientColor = m_lightManager->GetActiveAmbientColor();
	}
	else
	{
		lightDir     = Vector4f( 0.57735f, -0.57735f, 0.57735f, 0.0f);
		lightColor   = Vector4f(1.0f, 1.0f, 1.0f, 1.0f);
		ambientColor = Vector4f(0.15f, 0.15f, 0.15f, 1.0f);
	}

	bgfx::setUniform(m_lightDir,   lightDir.data());
	bgfx::setUniform(m_lightColor, lightColor.data());
	bgfx::setUniform(m_ambient,    ambientColor.data());
}

void CMeshComponent::Render(bgfx::ViewId viewId)
{
	DECLARE_FUNC_MEDIUM();
	if (IsLoaded())
	{
		if (m_meshStateInitialized == false)
		{
			InitializeMesh(GetMeshResource(), viewId);
		}

		if (m_meshStateInitialized)
		{
			ApplyLightUniforms();

			auto meshRes = GetMeshResource();
			const Mesh* mesh = meshRes->GetMesh();

			if (mesh)
			{
				const MeshState* statePtr = &m_meshState;
				auto modelMatrix = GetModelMatrix();
				if (modelMatrix)
					mesh->submit(&statePtr, 1, modelMatrix->GetData().data(), 1);
			}
		}

		// Render the physics body if available (for debugging)
		Component* parent = GetParent();
		if (!parent)
			return;
#ifdef PHYSICS_DEBUG_RENDER
		auto physBody = m_physicsBodyRef.Get();
		if (physBody)
		{
			auto res = physBody->GetBodyResource();
			if (res)
			{
				Matrix4f physMatrix = physBody->GetWorldTransform();
				physMatrix = physMatrix * m_scale;
				Matrix4f matrix = res->GetTransform();
				Vector3f resScale = matrix.ExtractScale();
				matrix = physMatrix * matrix;
				physBody->DebugRender(0, matrix);
			}
		}
#endif
	}
}

void CMeshComponent::InitializeMesh(std::shared_ptr<CMeshResource> meshRes, bgfx::ViewId viewId)
{
	if (!meshRes || !meshRes->GetMesh())
		return;

	Mesh* mesh = meshRes->GetMesh();
	if (mesh->m_groups.empty())
		return;

	auto matRes = GetMaterialResource();
	if (!matRes)
	{
		// Material not available — bail out of state init.
		return;
	}

	bgfx::setUniform(m_materialColor, matRes->GetMaterialColor().data());

	// assign the sampler handle (don't recreate each frame)
	int numTextures = matRes->GetNumberOfTextures();
	if (numTextures > 4) numTextures = 4;

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

	// Set the bounding sphere
	*CRenderComponent::GetBoundingSphere() = Vector4f(result.center.x, result.center.y, result.center.z, result.radius);
}

bool CMeshComponent::IsLoaded() const
{
	if (!m_meshResource.GetResource()) return false;
	auto meshRes = m_meshResource.GetResourceAs<CMeshResource>();
	if (!meshRes || !meshRes->IsLoaded() || !meshRes->IsFinalized()) return false;

	if (!m_materialResource.GetResource()) return false;
	auto matRes = m_materialResource.GetResourceAs<CMaterialResource>();
	if (!matRes || !matRes->IsLoaded() || !matRes->IsFinalized()) return false;

	return true;
}

std::shared_ptr<Vector4f> CMeshComponent::GetBoundingSphere() const
{
	return CRenderComponent::GetBoundingSphere();
}