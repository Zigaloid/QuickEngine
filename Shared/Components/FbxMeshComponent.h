#pragma once
#include "ComponentSystem/ComponentSystem.h"
#include "ResourceManager/ResourceManager.h"
#include "MaterialResource.h"

class CFbxMeshResource;

class CFbxMeshComponent : public ComponentSystem::Component
{
public:
    REFL_DECLARE_OBJECT(CFbxMeshComponent, Component);
    DECLARE_COMPONENT();

    // Component lifecycle
    bool OnInitialize() override;
    void OnUpdate(double deltaTime) override;
    void OnShutdown() override;

    /// Submit the mesh for rendering on the given bgfx view with a 4x4 transform.
    void Render(bgfx::ViewId viewId, const float* mtx);

    /// Returns true when both the mesh and material have been fully loaded
    /// and finalized by the ResourceManager.
    bool IsLoaded() const;
    bool IsReady() const { return m_ready; }

    // Resource accessors
    std::shared_ptr<CFbxMeshResource> GetMeshResource() const { return m_meshRes; }

    /// Override the scale factor applied during FBX → binary conversion.
    void SetScaleFactor(float s);

private:
    // Serialized resource paths (populated via reflection / data files)
    CResourceReference m_fbxResource;      // FBX path, e.g. "models/hero.fbx"
    CResourceReference m_materialFile;     // Material definition path

    // Runtime resource handles (managed by ResourceManager, not serialized)
    std::shared_ptr<CFbxMeshResource> m_meshRes;

    MeshState            m_meshState;
    MeshState::Texture   m_texture[4];
    bgfx::UniformHandle  u_samplers[4]     = { BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE,
                                                BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE };
    CMaterialDefinition  m_materialDefinition;
    bool m_ready                = false;
    bool m_meshStateInitialized = false;

    bgfx::UniformHandle u_lightDir       = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_lightColor     = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_ambient        = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_materialColor  = BGFX_INVALID_HANDLE;
};