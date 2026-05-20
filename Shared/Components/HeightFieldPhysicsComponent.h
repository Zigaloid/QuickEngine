#pragma once

#include "PhysicsComponent.h"
#include "MeshComponent.h"
#include "ResourceManager/ResourceManager.h"

#include <Jolt/Jolt.h>
JPH_SUPPRESS_WARNINGS
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>
#include <Jolt/Physics/Body/MotionType.h>

#include "Math/Matrix4f.h"
#include "Math/Vector3f.h"
#include "Math/Vector4f.h"

/// A physics component that generates a HeightFieldShape from mesh height data.
/// The mesh is expected to be a regular grid (X-Z plane) where the Y coordinate
/// represents height values. The component extracts vertex heights and converts
/// them into a Jolt HeightFieldShape for efficient terrain collision.
///
/// This component must be used with CHeightFieldMeshComponent on the same entity.
/// It defers body creation until the mesh resource is fully loaded and finalized.
class CHeightFieldPhysicsComponent : public CPhysicsComponent
{
public:
    REFL_DECLARE_OBJECT(CHeightFieldPhysicsComponent, CPhysicsComponent);
    DECLARE_COMPONENT();

    CHeightFieldPhysicsComponent()
    {
        m_modelMatrixPtr = std::shared_ptr<Matrix4f>(&m_objectMatrix, [](Matrix4f*) {});
        m_boundingSpherePtr = std::shared_ptr<Vector4f>(&m_boundingSphere, [](Vector4f*) {});
    }
    ~CHeightFieldPhysicsComponent() override = default;

    // CPhysicsComponent lifecycle overrides
    bool OnInitialize() override;
    void OnUpdate(double deltaTime) override;
    void ApplyTransformToParent(const Matrix4f& worldTransform) override;

    // ── Body parameter accessors ───────────────────────────────────────────

    /// Motion type as JPH::EMotionType (cast from the serialised int).
    /// For heightfield terrain, this is typically Static (0).
    JPH::EMotionType GetMotionType() const override { return static_cast<JPH::EMotionType>(m_motionType); }
    float GetFriction() const { return m_friction; }
    float GetRestitution() const { return m_restitution; }

    // ── Mesh data configuration ────────────────────────────────────────────

    /// Gets the mesh resource from the sibling CHeightFieldMeshComponent
    std::shared_ptr<CMeshResource> GetMeshResource() const;

    /// Expected number of samples along the X axis. If 0, inferred from mesh.
    int GetTerrainSizeX() const { return m_terrainSizeX; }
    void SetTerrainSizeX(int size) { m_terrainSizeX = size; m_shapeInitialized = false; }

    /// Expected number of samples along the Z axis. If 0, inferred from mesh.
    int GetTerrainSizeZ() const { return m_terrainSizeZ; }
    void SetTerrainSizeZ(int size) { m_terrainSizeZ = size; m_shapeInitialized = false; }

    // ── Built shape ────────────────────────────────────────────────────────

    /// Returns the compiled Jolt heightfield shape, or nullptr before initialization.
    const JPH::Shape* GetShape() const { return m_shape.GetPtr(); }

    /// Debug render the collision shape using the supplied primitive renderer.
    void DebugRender(bgfx::ViewId viewId, Matrix4f& transform) const override;

    // ── Expose shared pointers to transform / bounding-sphere for editor ─────

    std::shared_ptr<Matrix4f> GetModelMatrix() const { return m_modelMatrixPtr; }
    std::shared_ptr<Vector4f> GetBoundingSphere() const { return m_boundingSpherePtr; }

    // CPhysicsComponent editor interface
    std::shared_ptr<Matrix4f> GetEditableTransform() const override { return GetModelMatrix(); }
    std::shared_ptr<Vector4f> GetEditableBoundingSphere() const override { return GetBoundingSphere(); }

    /// Check if mesh is fully loaded and shape can be initialized
    bool IsMeshLoaded() const;

    /// Check if the physics shape has been successfully initialized
    bool IsShapeInitialized() const { return m_shapeInitialized; }

protected:
    // ── CPhysicsComponent template-method overrides ────────────────────────
    void InitializeShape(const Matrix4f& scaleMtx) override;
    JPH::BodyCreationSettings MakeBodyCreationSettings(
        JPH::RVec3Arg    position,
        JPH::QuatArg     rotation,
        JPH::ObjectLayer objectLayer) const override;

private:
    /// Extract height samples from mesh vertex data
    bool ExtractHeightSamplesFromMesh(std::vector<float>& outHeights, int& outSizeX, int& outSizeZ);

    /// Compute bounding sphere from height field data
    void ComputeBoundingSphere(const std::vector<float>& heights, int sizeX, int sizeZ);

    // --- Serialized (reflected) members
    int m_motionType = 0; // 0 = Static
    float m_friction = 0.2f;
    float m_restitution = 0.0f;
    int m_terrainSizeX = 0; // 0 = infer from mesh
    int m_terrainSizeZ = 0; // 0 = infer from mesh

    // State tracking
    bool m_shapeInitialized = false;

    // Cached shared pointers for editor
    std::shared_ptr<Matrix4f> m_modelMatrixPtr;
    std::shared_ptr<Vector4f> m_boundingSpherePtr;

    // Bounding sphere data
    Vector4f m_boundingSphere;
    Matrix4f m_objectMatrix;

    // Jolt shape reference
    JPH::Ref<JPH::Shape> m_shape;
};