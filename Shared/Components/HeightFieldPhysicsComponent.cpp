#include "CoreSystem/CoreSystem.h"
#include "ComponentSystem/ComponentSystem.h"
#include "HeightFieldPhysicsComponent.h"
#include "HeightFieldMeshComponent.h"
#include "TransformComponent.h"
#include "Physics/PhysicsManager.h"

#include <Jolt/Jolt.h>
JPH_SUPPRESS_WARNINGS
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>

#include <algorithm>
#include <cmath>

REGISTER_COMPONENT(CHeightFieldPhysicsComponent, "HeightFieldPhysics", "Physics");

REFL_DEFINE_OBJECT(CHeightFieldPhysicsComponent)
REFL_DEFINE_INT_MEMBER(CHeightFieldPhysicsComponent, m_motionType),
REFL_DEFINE_FLOAT_MEMBER(CHeightFieldPhysicsComponent, m_friction),
REFL_DEFINE_FLOAT_MEMBER(CHeightFieldPhysicsComponent, m_restitution),
REFL_DEFINE_INT_MEMBER(CHeightFieldPhysicsComponent, m_terrainSizeX),
REFL_DEFINE_INT_MEMBER(CHeightFieldPhysicsComponent, m_terrainSizeZ)
REFL_DEFINE_END


bool CHeightFieldPhysicsComponent::OnInitialize()
{
    DECLARE_FUNC_VLOW();
    CacheParentTransform();
    return true;
}


void CHeightFieldPhysicsComponent::OnUpdate(double deltaTime)
{
    DECLARE_FUNC_VLOW();
    PhysicsManager* physics = PhysicsManager::Get();

    if (!m_bodyInitialized)
    {
        if (physics && physics->IsInitialized() && IsMeshLoaded() && m_parentTransform)
        {
            m_bodyInitialized = CreateBodyAtTransform(physics);
        }
    }

    if (m_bodyInitialized && m_parentTransform && physics && physics->IsInitialized() && !m_bodyId.IsInvalid())
    {
        Matrix4f world = ComputeWorldTransform();
        ApplyTransformToParent(world);
    }
}


std::shared_ptr<CMeshResource> CHeightFieldPhysicsComponent::GetMeshResource() const
{
    auto* meshComponent = FindSibling<CHeightFieldMeshComponent>();
    if (meshComponent)
    {
        return meshComponent->GetMeshResource();
    }
    return nullptr;
}


bool CHeightFieldPhysicsComponent::IsMeshLoaded() const
{
    auto meshRes = GetMeshResource();
    if (!meshRes)
        return false;

    if (!meshRes->IsLoaded())
        return false;

    if (!meshRes->IsFinalized())
        return false;

    Mesh* mesh = meshRes->GetMesh();
    if (!mesh || mesh->m_groups.empty())
        return false;

    return true;
}


bool CHeightFieldPhysicsComponent::ExtractHeightSamplesFromMesh(
    std::vector<float>& outHeights, int& outSizeX, int& outSizeZ)
{
    auto meshRes = GetMeshResource();
    if (!meshRes)
    {
        std::cerr << "CHeightFieldPhysicsComponent: Mesh resource not available" << std::endl;
        return false;
    }

    Mesh* mesh = meshRes->GetMesh();
    if (!mesh || mesh->m_groups.empty())
    {
        std::cerr << "CHeightFieldPhysicsComponent: Mesh has no geometry" << std::endl;
        return false;
    }

    const Group& group = mesh->m_groups[0];
    if (!group.m_vertices || group.m_numVertices == 0)
    {
        std::cerr << "CHeightFieldPhysicsComponent: Mesh group has no vertex data" << std::endl;
        return false;
    }

    if (m_terrainSizeX == 0 || m_terrainSizeZ == 0)
    {
        uint32_t gridSize = static_cast<uint32_t>(std::sqrt(group.m_numVertices));
        if (gridSize * gridSize != group.m_numVertices)
        {
            gridSize = static_cast<uint32_t>(std::ceil(std::sqrt(group.m_numVertices)));
        }
        outSizeX = gridSize;
        outSizeZ = gridSize;
    }
    else
    {
        outSizeX = m_terrainSizeX;
        outSizeZ = m_terrainSizeZ;
    }

    uint32_t expectedCount = static_cast<uint32_t>(outSizeX * outSizeZ);
    if (expectedCount != group.m_numVertices)
    {
        std::cerr << "CHeightFieldPhysicsComponent: Vertex count (" << group.m_numVertices
            << ") does not match expected grid size (" << outSizeX << "x" << outSizeZ << ")" << std::endl;
        return false;
    }

    const uint16_t stride = mesh->m_layout.getStride();
    const float* vertexData = reinterpret_cast<const float*>(group.m_vertices);

    outHeights.resize(group.m_numVertices);
    for (uint32_t i = 0; i < group.m_numVertices; ++i)
    {
        const float* vertex = vertexData + (i * stride / sizeof(float));
        outHeights[i] = vertex[1];
    }

    return true;
}

void CHeightFieldPhysicsComponent::ComputeBoundingSphere(
    const std::vector<float>& heights, int sizeX, int sizeZ)
{
    if (heights.empty())
    {
        m_boundingSphere = Vector4f(0.0f, 0.0f, 0.0f, 1.0f);
        return;
    }

    float minHeight = *std::min_element(heights.begin(), heights.end());
    float maxHeight = *std::max_element(heights.begin(), heights.end());

    float centerX = (sizeX - 1) * 0.5f;
    float centerZ = (sizeZ - 1) * 0.5f;
    float centerY = (minHeight + maxHeight) * 0.5f;

    float radiusXZ = std::sqrt(centerX * centerX + centerZ * centerZ);
    float radiusY = (maxHeight - minHeight) * 0.5f;
    float radius = std::sqrt(radiusXZ * radiusXZ + radiusY * radiusY);

    m_boundingSphere = Vector4f(centerX, centerY, centerZ, radius);
}

void CHeightFieldPhysicsComponent::InitializeShape(const Matrix4f& scaleMtx)
{
    DECLARE_FUNC_VLOW();

    if (m_shapeInitialized)
        return;

    if (!IsMeshLoaded())
    {
        return;
    }

    std::vector<float> heights;
    int sizeX = 0, sizeZ = 0;

    if (!ExtractHeightSamplesFromMesh(heights, sizeX, sizeZ))
    {
        std::cerr << "CHeightFieldPhysicsComponent::InitializeShape: Failed to extract height samples" << std::endl;
        return;
    }

    // Jolt only supports square height fields
    if (sizeX != sizeZ)
    {
        std::cerr << "CHeightFieldPhysicsComponent::InitializeShape: Height field must be square (X size: "
            << sizeX << " != Z size: " << sizeZ << ")" << std::endl;
        return;
    }

    ComputeBoundingSphere(heights, sizeX, sizeZ);

    // Get the step size from the sibling mesh component to properly scale the physics shape
    float stepSize = 1.0f;
    auto* meshComponent = FindSibling<CHeightFieldMeshComponent>();
    if (meshComponent)
    {
        stepSize = meshComponent->GetStepSize();
    }

    // Align the physics shape origin with the rendered mesh origin (at the first vertex)
    JPH::HeightFieldShapeSettings settings(
        heights.data(),
        JPH::Vec3(0.0f, 0.0f, 0.0f),
        JPH::Vec3(stepSize, 1.0f, stepSize),
        static_cast<JPH::uint32>(sizeX)
    );

    JPH::ShapeSettings::ShapeResult result = settings.Create();
    if (result.HasError())
    {
        std::cerr << "CHeightFieldPhysicsComponent::InitializeShape: Failed to create heightfield shape: "
            << result.GetError().c_str() << std::endl;
        return;
    }

    m_shape = result.Get();
    m_shapeInitialized = true;
}

JPH::BodyCreationSettings CHeightFieldPhysicsComponent::MakeBodyCreationSettings(
    JPH::RVec3Arg    position,
    JPH::QuatArg     rotation,
    JPH::ObjectLayer objectLayer) const
{
    JPH::BodyCreationSettings settings(
        m_shape,
        position,
        rotation,
        GetMotionType(),
        objectLayer
    );

    settings.mFriction = m_friction;
    settings.mRestitution = m_restitution;

    return settings;
}


void CHeightFieldPhysicsComponent::ApplyTransformToParent(const Matrix4f& worldTransform)
{
    CTransformComponent* parentTransform = FindSibling<CTransformComponent>();
    if (parentTransform)
    {
        parentTransform->GetTransform() = worldTransform;
    }
}

void CHeightFieldPhysicsComponent::DebugRender(bgfx::ViewId viewId, Matrix4f& transform) const
{
}