#include "FbxMeshComponent.h"

#include "CoreSystem/CoreSystem.h"
#include "ComponentSystem/ComponentSystem.h"
#include "FbxMeshResource.h"

// ── CFbxMeshComponent ──────────────────────────────────────────────
REGISTER_COMPONENT(CFbxMeshComponent, "FBX Mesh", "Graphics");

REFL_DEFINE_OBJECT(CFbxMeshComponent)
    REFL_DEFINE_OBJECT_MEMBER(CFbxMeshComponent, m_fbxResource),
    REFL_DEFINE_OBJECT_MEMBER(CFbxMeshComponent, m_materialFile),
REFL_DEFINE_END

bool CFbxMeshComponent::OnInitialize()
{
    m_ready = false;
    m_meshStateInitialized = false;
    m_materialDefinition.Reset();

    if (!Core::CoreSystem::IsInitialized())
    {
        std::cerr << "CFbxMeshComponent: CoreSystem is not initialized" << std::endl;
        return false;
    }

    auto* resourceManager = Core::CoreSystem::GetResourceManager();
    if (!resourceManager)
    {
        std::cerr << "CFbxMeshComponent: ResourceManager is null" << std::endl;
        return false;
    }

    // Request the FBX mesh resource (async: worker converts FBX → binary,
    // main thread finalises into GPU buffers via Mesh::load).
    if (!m_fbxResource.GetResourceFileName().empty())
    {
        m_meshRes = resourceManager->RequestResource<CFbxMeshResource>(
            m_fbxResource.GetResourceFileName());

        if (!m_meshRes)
        {
            std::cerr << "CFbxMeshComponent: Failed to request FBX resource: "
                << m_fbxResource.GetResourceFileName() << std::endl;
            return false;
        }
    }

    // Load material definition (shader + textures).
    m_materialDefinition.SafeRead(m_materialFile.GetResourceFileName());
    m_materialDefinition.Initialize();

    // Create uniform handles once.
    for (int i = 0; i < 4; i++)
    {
        m_samplers[i] = bgfx::createUniform(
            ("s_texColor" + std::to_string(i)).c_str(),
            bgfx::UniformType::Sampler);
    }

    m_lightDir       = bgfx::createUniform("u_lightDir",       bgfx::UniformType::Vec4);
    m_lightColor     = bgfx::createUniform("u_lightColor",     bgfx::UniformType::Vec4);
    m_ambient        = bgfx::createUniform("u_ambient",        bgfx::UniformType::Vec4);
    m_materialColor  = bgfx::createUniform("u_materialColor",  bgfx::UniformType::Vec4);

    return true;
}

void CFbxMeshComponent::OnUpdate(double /*deltaTime*/)
{
    if (!m_ready)
    {
        if (IsLoaded())
        {
            m_ready = true;
        }
        m_materialDefinition.Update();
    }
}

void CFbxMeshComponent::OnShutdown()
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

    m_lightDir      = BGFX_INVALID_HANDLE;
    m_lightColor    = BGFX_INVALID_HANDLE;
    m_ambient       = BGFX_INVALID_HANDLE;
    m_materialColor = BGFX_INVALID_HANDLE;
    m_meshStateInitialized = false;
}

void CFbxMeshComponent::Render(bgfx::ViewId viewId, const float* mtx)
{
    if (!IsReady())
        return;

    Mesh* mesh = m_meshRes->GetMesh();
    if (!mesh)
        return;

    if (!m_meshStateInitialized)
    {
        // Set lighting uniforms.
        const float lightDir[4]   = { 0.57735f, -0.57735f, 0.57735f, 0.0f };
        const float lightColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        bgfx::setUniform(m_lightDir,   lightDir);
        bgfx::setUniform(m_lightColor, lightColor);
        bgfx::setUniform(m_ambient,        m_materialDefinition.GetAmbientColor().data());
        bgfx::setUniform(m_materialColor,  m_materialDefinition.GetMaterialColor().data());

        // Bind texture samplers.
        int numTextures = m_materialDefinition.GetNumberOfTextures();
        if (numTextures > 3)
            numTextures = 3;

        for (int i = 0; i <= numTextures; i++)
        {
            m_texture[i].m_texture = m_materialDefinition.GetTexture(i);
            m_texture[i].m_flags   = 0;
            m_texture[i].m_sampler = m_samplers[i];
            m_meshState.m_textures[i] = m_texture[i];
        }

        m_meshState.m_numTextures = 1;
        m_meshState.m_state   = BGFX_STATE_WRITE_RGB
                              | BGFX_STATE_WRITE_A
                              | BGFX_STATE_WRITE_Z
                              | BGFX_STATE_DEPTH_TEST_LESS
                              | BGFX_STATE_CULL_CCW
                              | BGFX_STATE_MSAA;
        m_meshState.m_program = m_materialDefinition.GetShaderProgram();
        m_meshState.m_viewId  = viewId;
        m_meshStateInitialized = true;
    }

    if (m_meshStateInitialized)
    {
        const MeshState* statePtr = &m_meshState;
        mesh->submit(&statePtr, 1, mtx, 1);
    }
}

bool CFbxMeshComponent::IsLoaded() const
{
    return m_meshRes
        && m_meshRes->IsFinalized()
        && m_meshRes->GetMesh() != nullptr
        && m_materialDefinition.IsReady();
}

void CFbxMeshComponent::SetScaleFactor(float s)
{
    if (m_meshRes)
        m_meshRes->SetScaleFactor(s);
}