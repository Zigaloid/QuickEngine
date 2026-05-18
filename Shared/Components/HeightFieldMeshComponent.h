#pragma once

#include "ComponentSystem/ComponentSystem.h"
#include "ResourceManager/ResourceManager.h"
#include "MaterialResource.h"
#include "MeshComponent.h"
#include "Math/Matrix4f.h"
#include "Math/Vector3f.h"
#include "bgfx/bgfx.h"
#include "bgfx_common/bgfx_utils.h"

// Forward-declare to avoid pulling in the full physics header here.
class CPhysicsBodyComponent;
class CTransformComponent;

class CHeightFieldMeshComponent : public CRenderComponent
{
public:
    REFL_DECLARE_OBJECT(CHeightFieldMeshComponent, CRenderComponent);
    DECLARE_COMPONENT();

    // -- Constructor with parameters -------------------------------------
    CHeightFieldMeshComponent()
        : m_xSteps(10)
        , m_zSteps(10)
        , m_stepSize(1.0f)
        , m_isDirty(false)
    {
    }

    explicit CHeightFieldMeshComponent(uint32_t xSteps, uint32_t zSteps, float stepSize)
        : m_xSteps(xSteps)
        , m_zSteps(zSteps)
        , m_stepSize(stepSize)
        , m_isDirty(true)
    {
    }

    // -- IComponent lifecycle --------------------------------------------

    bool OnInitialize() override;
    void OnUpdate(double deltaTime) override;
    void OnShutdown() override;

    // -- Public API ------------------------------------------------------

    void Render(bgfx::ViewId viewId);
    bool IsLoaded() const;

    // Getters and setters for height field parameters
    uint32_t GetXSteps() const { return m_xSteps; }
    void SetXSteps(uint32_t xSteps) 
    { 
        if (m_xSteps != xSteps) 
        { 
            m_xSteps = xSteps; 
            m_isDirty = true; 
        }
    }

    uint32_t GetZSteps() const { return m_zSteps; }
    void SetZSteps(uint32_t zSteps) 
    { 
        if (m_zSteps != zSteps) 
        { 
            m_zSteps = zSteps; 
            m_isDirty = true; 
        }
    }

    float GetStepSize() const { return m_stepSize; }
    void SetStepSize(float stepSize) 
    { 
        if (m_stepSize != stepSize) 
        { 
            m_stepSize = stepSize; 
            m_isDirty = true; 
        }
    }

    // Material property
    std::shared_ptr<CMaterialResource> GetMaterialResource() const { return m_materialResource.GetResourceAs<CMaterialResource>(); }
    void SetMaterialResource(const CMaterialResourceReference& matRef) { m_materialResource = matRef; m_meshStateInitialized = false; }

    virtual std::shared_ptr<Vector4f> GetBoundingSphere() const;

    // Height field data management
    float GetHeightAt(uint32_t xIndex, uint32_t zIndex) const;
    void SetHeightAt(uint32_t xIndex, uint32_t zIndex, float height);
    void RebuildMesh();
    void ForceRenderStateReset() { m_meshStateInitialized = false; }

    // Mesh persistence
    bool SaveMesh(const std::string& filePath);

private:
    // Height field parameters
    uint32_t m_xSteps;
    uint32_t m_zSteps;
    float m_stepSize;
    bool m_isDirty;

    // Height data: stored as a flattened array [z * width + x]
    std::vector<float> m_heightData;

    // Material
    CMaterialResourceReference m_materialResource;



    // Mesh for storing geometry
    Mesh m_mesh;

    // Material and rendering uniforms
    MeshState m_meshState;
    MeshState::Texture m_texture[4];
    bgfx::UniformHandle m_samplers[4] = { BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE };
    bgfx::UniformHandle m_lightDir = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_lightColor = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_ambient = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_materialColor = BGFX_INVALID_HANDLE;
    bool m_meshStateInitialized = false;

    // Rendering state
    uint64_t m_renderState = 0;
    bgfx::ProgramHandle m_program = BGFX_INVALID_HANDLE;

    void GenerateHeightFieldGeometry();
    void ClearGeometry();
};
