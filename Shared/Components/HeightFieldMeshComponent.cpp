#include "CoreSystem/CoreSystem.h"
#include "ComponentSystem/ComponentSystem.h"
#include "HeightFieldMeshComponent.h"
#include "TransformComponent.h"
#include "PhysicsBodyComponent.h"
#include "Physics/PhysicsManager.h"
#include "Math/Quaternion.h"
#include "Utils/BgfxBinaryMeshWriter.h"
#include <bx/bounds.h>
#include <bgfx/bgfx.h>
#include <bx/math.h>

#include <fstream>

REGISTER_COMPONENT(CHeightFieldMeshComponent, "HeightFieldMesh", "Graphics");

REFL_DEFINE_OBJECT(CHeightFieldMeshComponent)
REFL_DEFINE_INT_MEMBER(CHeightFieldMeshComponent, m_xSteps),
REFL_DEFINE_INT_MEMBER(CHeightFieldMeshComponent, m_zSteps),
REFL_DEFINE_FLOAT_MEMBER(CHeightFieldMeshComponent, m_stepSize),
REFL_DEFINE_VECTOR4_MEMBER(CHeightFieldMeshComponent, m_blendHeights),
REFL_DEFINE_FLOAT_MEMBER(CHeightFieldMeshComponent, m_blendTransition)
REFL_DEFINE_END


bool CHeightFieldMeshComponent::OnInitialize()
{
    DECLARE_FUNC_VLOW();
    CMeshComponent::OnInitialize();
    return true;
}

void CHeightFieldMeshComponent::OnUpdate(double deltaTime)
{
    DECLARE_FUNC_MEDIUM();
    CMeshComponent::OnUpdate(deltaTime);
}

void CHeightFieldMeshComponent::OnShutdown()
{
    DECLARE_FUNC_VLOW();
    CMeshComponent::OnShutdown();
}

void CHeightFieldMeshComponent::Render(bgfx::ViewId viewId)
{
    DECLARE_FUNC_MEDIUM();

    auto meshRes = GetMeshResource();
    if (!meshRes)
        return;

    if (m_meshStateInitialized == false && IsLoaded())
    {
        InitializeMesh(meshRes, viewId);
    }

    if (m_meshStateInitialized)
    {
        // Set height-based texture blending uniforms
        if (bgfx::isValid(m_blendHeightsUniform))
        {
            bgfx::setUniform(m_blendHeightsUniform, m_blendHeights.data());
        }

        if (bgfx::isValid(m_blendTransitionUniform))
        {
            float blendTransitionData[4] = { m_blendTransition, 0.0f, 0.0f, 0.0f };
            bgfx::setUniform(m_blendTransitionUniform, blendTransitionData);
        }

        Mesh* mesh = meshRes->GetMesh();
        if (mesh && !mesh->m_groups.empty())
        {
            const MeshState* meshStatePtr = &m_meshState;
            std::shared_ptr<Matrix4f> modelMatrix = GetModelMatrix();
            if (!modelMatrix)
            {
                Matrix4f identity = Matrix4f::GetIdentity();
                modelMatrix = std::make_shared<Matrix4f>(identity);
                
            }
            mesh->submit(&meshStatePtr, 1, modelMatrix->GetData().data(), 1);          
        }
    }
}

void CHeightFieldMeshComponent::InitializeMesh(std::shared_ptr<CMeshResource> meshRes, bgfx::ViewId viewId)
{
    Mesh* mesh = meshRes->GetMesh();
    if (!mesh)
        return;

    // Create uniform handles for height-based texture blending if not already created
    if (!bgfx::isValid(m_blendHeightsUniform))
    {
        m_blendHeightsUniform = bgfx::createUniform("u_blendHeights", bgfx::UniformType::Vec4);
    }
    if (!bgfx::isValid(m_blendTransitionUniform))
    {
        m_blendTransitionUniform = bgfx::createUniform("u_blendTransition", bgfx::UniformType::Vec4);
    }

    // set uniform values (adjust values as needed)
    const float lightDir[4] = { 0.57735f, -1.0f, 0.57735f, 0.0f };
    const float lightColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    bgfx::setUniform(m_lightDir, lightDir);
    bgfx::setUniform(m_lightColor, lightColor);

    auto matRes = GetMaterialResource();
    if (!matRes)
    {
        return;
    }

    bgfx::setUniform(m_ambient, matRes->GetAmbientColor().data());
    bgfx::setUniform(m_materialColor, matRes->GetMaterialColor().data());

    int numTextures = matRes->GetNumberOfTextures();
    if (numTextures > 4) numTextures = 4;

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

    if (mesh->m_groups.empty())
        return;

    // Seed with the first group's sphere, then expand to enclose each subsequent one.
    bx::Sphere result = mesh->m_groups[0].m_sphere;

    for (uint32_t i = 1; i < mesh->m_groups.size(); ++i)
    {
        const bx::Sphere& s = mesh->m_groups[i].m_sphere;

        const bx::Vec3 diff = bx::sub(s.center, result.center);
        const float dist = bx::length(diff);

        if (dist + s.radius <= result.radius)
            continue;

        if (dist + result.radius <= s.radius)
        {
            result = s;
            continue;
        }

        const float newRadius = (dist + result.radius + s.radius) * 0.5f;
        const float t = (newRadius - result.radius) / dist;
        result.center = bx::mad(diff, t, result.center);
        result.radius = newRadius;
    }

    *CRenderComponent::GetBoundingSphere() = Vector4f(result.center.x, result.center.y, result.center.z, result.radius);
}


bool CHeightFieldMeshComponent::IsLoaded() const
{
    if (!GetMaterialResource()) return false;
    if (!GetMaterialResource()->IsLoaded()) return false;
    if (!GetMaterialResource()->IsFinalized()) return false;

    if (!GetMeshResource()) return false;
    if (!GetMeshResource()->IsLoaded()) return false;
    if (!GetMeshResource()->IsFinalized()) return false;

    return true;
}

std::shared_ptr<Vector4f> CHeightFieldMeshComponent::GetBoundingSphere() const
{
    return CRenderComponent::GetBoundingSphere();
}

bool CHeightFieldMeshComponent::SaveMesh(const std::string& filePath)
{
    auto meshRes = GetMeshResource();
    if (!meshRes)
    {
        std::cerr << "CHeightFieldMeshComponent::SaveMesh: Mesh resource is not available" << std::endl;
        return false;
    }

    Mesh* mesh = meshRes->GetMesh();
    if (!mesh)
    {
        std::cerr << "CHeightFieldMeshComponent::SaveMesh: Mesh pointer is null" << std::endl;
        return false;
    }

    if (mesh->m_groups.empty())
    {
        std::cerr << "CHeightFieldMeshComponent::SaveMesh: No mesh geometry to save" << std::endl;
        return false;
    }

    BinaryMeshData meshData;
    meshData.layout = mesh->m_layout;

    for (const auto& srcGroup : mesh->m_groups)
    {
        BinaryMeshGroup dstGroup;
        dstGroup.sphere = srcGroup.m_sphere;
        dstGroup.aabb = srcGroup.m_aabb;
        dstGroup.obb = srcGroup.m_obb;
        dstGroup.materialName.clear();
        dstGroup.numVertices = srcGroup.m_numVertices;
        dstGroup.numIndices = srcGroup.m_numIndices;

        if (srcGroup.m_vertices)
        {
            uint16_t stride = mesh->m_layout.getStride();
            uint32_t vertexDataSize = srcGroup.m_numVertices * stride;
            dstGroup.vertexData.assign(srcGroup.m_vertices, srcGroup.m_vertices + vertexDataSize);
        }

        if (srcGroup.m_indices && srcGroup.m_numIndices > 0)
        {
            dstGroup.indexData.assign(srcGroup.m_indices, srcGroup.m_indices + srcGroup.m_numIndices);
        }

        for (const auto& srcPrim : srcGroup.m_prims)
        {
            BinaryMeshPrimitive dstPrim;
            dstPrim.name = "";
            dstPrim.startIndex = srcPrim.m_startIndex;
            dstPrim.numIndices = srcPrim.m_numIndices;
            dstPrim.startVertex = srcPrim.m_startVertex;
            dstPrim.numVertices = srcPrim.m_numVertices;
            dstPrim.sphere = srcPrim.m_sphere;
            dstPrim.aabb = srcPrim.m_aabb;
            dstPrim.obb = srcPrim.m_obb;
            dstGroup.primitives.push_back(dstPrim);
        }

        meshData.groups.push_back(dstGroup);
    }

    std::vector<uint8_t> binaryBuffer;
    if (!WriteBgfxBinaryMesh(meshData, binaryBuffer))
    {
        std::cerr << "CHeightFieldMeshComponent::SaveMesh: Failed to write binary mesh data" << std::endl;
        return false;
    }

    const Core::AppConfig& config = Core::AppConfig::Instance();
    std::string finalPath = config.ResolvePath(filePath);

    std::ofstream outFile(finalPath, std::ios::binary);

    if (!outFile.is_open())
    {
        std::cerr << "CHeightFieldMeshComponent::SaveMesh: Failed to open file for writing: " << filePath << std::endl;
        return false;
    }

    outFile.write(reinterpret_cast<const char*>(binaryBuffer.data()), static_cast<std::streamsize>(binaryBuffer.size()));
    if (!outFile.good())
    {
        std::cerr << "CHeightFieldMeshComponent::SaveMesh: Failed to write to file: " << filePath << std::endl;
        outFile.close();
        return false;
    }

    outFile.close();
    std::cout << "CHeightFieldMeshComponent::SaveMesh: Successfully saved mesh to: " << filePath << std::endl;

    m_meshResource.SetResourceFileName(filePath);

    return true;
}

void CHeightFieldMeshComponent::RecalculateMeshNormals()
{
    auto meshRes = GetMeshResource();
    if (!meshRes || !meshRes->IsLoaded())
        return;

    Mesh* mesh = meshRes->GetMesh();
    if (!mesh)
        return;

    // Iterate through all groups and recalculate normals
    for (auto& group : mesh->m_groups)
    {
        if (!group.m_vertices || !group.m_indices || group.m_numIndices == 0)
            continue;

        uint16_t stride = mesh->m_layout.getStride();

        // Temporary storage for accumulated normals
        std::vector<Vector3f> accumulatedNormals(group.m_numVertices, Vector3f(0.0f, 0.0f, 0.0f));

        // Accumulate face normals to vertices
        for (uint32_t i = 0; i < group.m_numIndices; i += 3)
        {
            uint16_t idx0 = group.m_indices[i];
            uint16_t idx1 = group.m_indices[i + 1];
            uint16_t idx2 = group.m_indices[i + 2];

            // Validate indices
            if (idx0 >= group.m_numVertices || idx1 >= group.m_numVertices || idx2 >= group.m_numVertices)
                continue;

            // Get vertex positions
            uint8_t* v0Data = group.m_vertices + (idx0 * stride);
            uint8_t* v1Data = group.m_vertices + (idx1 * stride);
            uint8_t* v2Data = group.m_vertices + (idx2 * stride);

            const float* p0 = reinterpret_cast<const float*>(v0Data);
            const float* p1 = reinterpret_cast<const float*>(v1Data);
            const float* p2 = reinterpret_cast<const float*>(v2Data);

            Vector3f v0(p0[0], p0[1], p0[2]);
            Vector3f v1(p1[0], p1[1], p1[2]);
            Vector3f v2(p2[0], p2[1], p2[2]);

            // Calculate face normal
            Vector3f edge1 = v1 - v0;
            Vector3f edge2 = v2 - v0;
            Vector3f faceNormal = edge1.Cross(edge2);

            // Accumulate to all three vertices
            accumulatedNormals[idx0] += faceNormal;
            accumulatedNormals[idx1] += faceNormal;
            accumulatedNormals[idx2] += faceNormal;
        }

        // Write normalized normals back to vertex buffer
        for (uint32_t i = 0; i < group.m_numVertices; ++i)
        {
            uint8_t* vertexData = group.m_vertices + (i * stride);

            // Normalize the accumulated normal
            Vector3f normal = accumulatedNormals[i].Normalize();

            // Encode normal as RGBA8
            uint8_t nx = static_cast<uint8_t>((normal.x * 0.5f + 0.5f) * 255.0f);
            uint8_t ny = static_cast<uint8_t>((normal.y * 0.5f + 0.5f) * 255.0f);
            uint8_t nz = static_cast<uint8_t>((normal.z * 0.5f + 0.5f) * 255.0f);
            uint8_t nw = 0xFF;

            // Pack as ABGR
            uint32_t packedNormal = (nw << 24) | (nz << 16) | (ny << 8) | nx;

            // Write to the normal attribute
            if (mesh->m_layout.has(bgfx::Attrib::Normal))
            {
                uint16_t offset = mesh->m_layout.getOffset(bgfx::Attrib::Normal);
                uint32_t* normalPtr = reinterpret_cast<uint32_t*>(vertexData + offset);
                *normalPtr = packedNormal;
            }
        }
    }
}
