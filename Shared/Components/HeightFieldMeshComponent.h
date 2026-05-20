#pragma once

#include "ComponentSystem/ComponentSystem.h"
#include "ResourceManager/ResourceManager.h"
#include "MaterialResource.h"
#include "MeshResource.h"
#include "MeshComponent.h"
#include "Math/Matrix4f.h"
#include "Math/Vector3f.h"
#include "bgfx/bgfx.h"
#include "bgfx_common/bgfx_utils.h"

class CHeightFieldMeshComponent : public CMeshComponent
{
public:
    REFL_DECLARE_OBJECT(CHeightFieldMeshComponent, CMeshComponent);
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
    bool IsLoaded() const override;

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

	virtual std::shared_ptr<Vector4f> GetBoundingSphere() const override;

	// Mesh persistence
	bool SaveMesh(const std::string& filePath);
	bool isMeshInitialized() const { return m_meshStateInitialized; }

	// Height-based texture blending parameters
	void SetBlendHeights(const Vector4f& heights) { m_blendHeights = heights; }
	Vector4f GetBlendHeights() const { return m_blendHeights; }

	void SetBlendTransition(float transition) { m_blendTransition = transition; }
	float GetBlendTransition() const { return m_blendTransition; }

protected:
	// ── Override parent's mesh initialization ───────────────────────────
	void InitializeMesh(std::shared_ptr<CMeshResource> meshRes, bgfx::ViewId viewId) override;

private:
	// Height field parameters
	uint32_t m_xSteps;
	uint32_t m_zSteps;
	float m_stepSize;
	bool m_isDirty;

	// Height-based texture blending
	Vector4f m_blendHeights = Vector4f(-2.0f, 0.0f, 2.0f, 4.0f);
	float m_blendTransition = 0.5f;
	bgfx::UniformHandle m_blendHeightsUniform = BGFX_INVALID_HANDLE;
	bgfx::UniformHandle m_blendTransitionUniform = BGFX_INVALID_HANDLE;
};
