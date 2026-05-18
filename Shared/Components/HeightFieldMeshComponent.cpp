#include "CoreSystem/CoreSystem.h"
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
    REFL_DEFINE_OBJECT_MEMBER(CHeightFieldMeshComponent, m_materialResource)
REFL_DEFINE_END


bool CHeightFieldMeshComponent::OnInitialize()
{	
    DECLARE_FUNC_VLOW();
    CRenderComponent::OnInitialize();

    if (!Core::CoreSystem::IsInitialized())
    {
        std::cerr << "CHeightFieldMeshComponent: CoreSystem is not initialized" << std::endl;
        return false;
    }

    for (int i = 0; i < 4; i++)
    {
        m_samplers[i] = bgfx::createUniform(("s_texColor" + std::to_string(i)).c_str(), bgfx::UniformType::Sampler);
    }    

    m_lightDir = bgfx::createUniform("u_lightDir", bgfx::UniformType::Vec4);
    m_lightColor = bgfx::createUniform("u_lightColor", bgfx::UniformType::Vec4);
    m_ambient = bgfx::createUniform("u_ambient", bgfx::UniformType::Vec4);
    m_materialColor = bgfx::createUniform("u_materialColor", bgfx::UniformType::Vec4);

    m_heightData.resize(m_xSteps * m_zSteps, 0.0f);
    m_isDirty = true;

    return true;
}

void CHeightFieldMeshComponent::OnUpdate(double deltaTime)
{	
    DECLARE_FUNC_MEDIUM();
    CRenderComponent::OnUpdate(deltaTime);

    if (m_isDirty)
    {
        GenerateHeightFieldGeometry();
        m_isDirty = false;
    }

    auto* renderFunctionQueue = Core::CoreSystem::GetRenderFunctionQueue();
    if (renderFunctionQueue)
    {
        renderFunctionQueue->AddFunction([this]()
        {
            Render(0);
        }, "CHeightFieldMeshComponent::Render");	
    }
}

void CHeightFieldMeshComponent::OnShutdown()
{
    DECLARE_FUNC_VLOW();
    ClearGeometry();

    m_materialResource = CMaterialResourceReference();	

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
}

void CHeightFieldMeshComponent::Render(bgfx::ViewId viewId)
{
	DECLARE_FUNC_MEDIUM();

	if (m_meshStateInitialized == false && IsLoaded())
	{
		GenerateHeightFieldGeometry();

		// set uniform values (adjust values as needed)
		const float lightDir[4] = { 0.57735f, -1.0f, 0.57735f, 0.0f }; // normalized direction
		const float lightColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		bgfx::setUniform(m_lightDir, lightDir);
		bgfx::setUniform(m_lightColor, lightColor);

		auto matRes = m_materialResource.GetResourceAs<CMaterialResource>();
		if (!matRes)
		{
			// Material not available — bail out of state init.
			return;
		}

		bgfx::setUniform(m_ambient, matRes->GetAmbientColor().data());
		bgfx::setUniform(m_materialColor, matRes->GetMaterialColor().data());

		// assign the sampler handle (don't recreate each frame)
		int numTextures = matRes->GetNumberOfTextures();
		if (numTextures > 4) numTextures = 4; // cap to available slots

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

		const Mesh* mesh = &m_mesh;

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

		// Set and return the sphere. This is shitty. We should cache this.
		*CRenderComponent::GetBoundingSphere() = Vector4f(result.center.x, result.center.y, result.center.z, result.radius);

	}

	if (m_meshStateInitialized && !m_mesh.m_groups.empty())
	{
		Matrix4f modelMatrix = Matrix4f::GetIdentity();
		const MeshState* meshStatePtr = &m_meshState;
		////m_mesh.submit(m_meshState.m_viewId, m_meshState.m_program, modelMatrix.data(), m_meshState.m_state);
        m_mesh.submit(&meshStatePtr, 1, modelMatrix.GetData().data(), 1);
	}
}

bool CHeightFieldMeshComponent::IsLoaded() const
{	
    if (!GetMaterialResource()) return false;
    if (!GetMaterialResource()->IsLoaded()) return false;
    if (!GetMaterialResource()->IsFinalized()) return false;
    return true;
}

std::shared_ptr<Vector4f> CHeightFieldMeshComponent::GetBoundingSphere() const
{
    return CRenderComponent::GetBoundingSphere();
}

float CHeightFieldMeshComponent::GetHeightAt(uint32_t xIndex, uint32_t zIndex) const
{
    if (xIndex >= m_xSteps || zIndex >= m_zSteps)
        return 0.0f;
    return m_heightData[zIndex * m_xSteps + xIndex];
}

void CHeightFieldMeshComponent::SetHeightAt(uint32_t xIndex, uint32_t zIndex, float height)
{
    if (xIndex >= m_xSteps || zIndex >= m_zSteps)
        return;
    m_heightData[zIndex * m_xSteps + xIndex] = height;
    m_isDirty = true;
}

void CHeightFieldMeshComponent::RebuildMesh()
{
    m_isDirty = true;
}

void CHeightFieldMeshComponent::GenerateHeightFieldGeometry()
{
    DECLARE_FUNC_LOW();

    ClearGeometry();

    // Structure to hold vertex data
    struct Vertex
    {
        float position[3];
        uint32_t normal;
        float uv[2];
    };

    std::vector<Vertex> vertices;
    vertices.reserve(m_xSteps * m_zSteps);

    // Generate vertices with positions, UVs, and normals
    for (uint32_t z = 0; z < m_zSteps; ++z)
    {
        for (uint32_t x = 0; x < m_xSteps; ++x)
        {
            float posX = x * m_stepSize;
            float posZ = z * m_stepSize;
            float posY = GetHeightAt(x, z);

            // Calculate normal from height field gradient
            float hLeft = (x > 0) ? GetHeightAt(x - 1, z) : posY;
            float hRight = (x < m_xSteps - 1) ? GetHeightAt(x + 1, z) : posY;
            float hUp = (z > 0) ? GetHeightAt(x, z - 1) : posY;
            float hDown = (z < m_zSteps - 1) ? GetHeightAt(x, z + 1) : posY;

            float dX = (hRight - hLeft) / (2.0f * m_stepSize);
            float dZ = (hDown - hUp) / (2.0f * m_stepSize);

            Vector3f normal(-dX, 1.0f, -dZ);
            normal.Normalize();

            // Encode normal as packed uint32
            uint32_t nx = (uint32_t)((normal.x * 0.5f + 0.5f) * 255.0f) & 0xFF;
            uint32_t ny = (uint32_t)((normal.y * 0.5f + 0.5f) * 255.0f) & 0xFF;
            uint32_t nz = (uint32_t)((normal.z * 0.5f + 0.5f) * 255.0f) & 0xFF;
            uint32_t packedNormal = (nx << 0) | (ny << 8) | (nz << 16) | (255 << 24);

            Vertex vert;
            vert.position[0] = posX;
            vert.position[1] = posY;
            vert.position[2] = posZ;
            vert.normal = packedNormal;
            vert.uv[0] = posX;  // UV wraps every 1 meter
            vert.uv[1] = posZ;  // UV wraps every 1 meter

            vertices.push_back(vert);
        }
    }

    std::vector<uint16_t> indices;
    uint32_t numTriangles = (m_xSteps - 1) * (m_zSteps - 1) * 2;
    indices.reserve(numTriangles * 3);

    for (uint32_t z = 0; z < m_zSteps - 1; ++z)
    {
        for (uint32_t x = 0; x < m_xSteps - 1; ++x)
        {
            uint16_t i0 = z * m_xSteps + x;
            uint16_t i1 = z * m_xSteps + (x + 1);
            uint16_t i2 = (z + 1) * m_xSteps + x;
            uint16_t i3 = (z + 1) * m_xSteps + (x + 1);

            indices.push_back(i0);
            indices.push_back(i2);
            indices.push_back(i1);

            indices.push_back(i1);
            indices.push_back(i2);
            indices.push_back(i3);
        }
    }

    // Setup vertex layout
    bgfx::VertexLayout vertexLayout;
    vertexLayout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Normal, 4, bgfx::AttribType::Uint8, true)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();

    m_mesh.m_layout = vertexLayout;

    // Create a group for the mesh
    Group group;
    group.m_numVertices = (uint16_t)vertices.size();
    group.m_numIndices = (uint32_t)indices.size();

    // Allocate and copy vertex data
    const bgfx::Memory* vertexMem = bgfx::alloc((uint32_t)vertices.size() * (uint32_t)sizeof(Vertex));
    memcpy(vertexMem->data, vertices.data(), (uint32_t)vertices.size() * (uint32_t)sizeof(Vertex));
    group.m_vbh = bgfx::createVertexBuffer(vertexMem, vertexLayout);

    // Allocate and copy index data
    const bgfx::Memory* indexMem = bgfx::alloc((uint32_t)indices.size() * (uint32_t)sizeof(uint16_t));
    memcpy(indexMem->data, indices.data(), (uint32_t)indices.size() * (uint32_t)sizeof(uint16_t));
    group.m_ibh = bgfx::createIndexBuffer(indexMem);

    // Calculate bounding sphere for the group
    float minX = 0.0f, maxX = (m_xSteps - 1) * m_stepSize;
    float minZ = 0.0f, maxZ = (m_zSteps - 1) * m_stepSize;
    float minY = *std::min_element(m_heightData.begin(), m_heightData.end());
    float maxY = *std::max_element(m_heightData.begin(), m_heightData.end());

    bx::Vec3 center(
        (minX + maxX) * 0.5f,
        (minY + maxY) * 0.5f,
        (minZ + maxZ) * 0.5f
    );

    float dx = (maxX - minX) * 0.5f;
    float dy = (maxY - minY) * 0.5f;
    float dz = (maxZ - minZ) * 0.5f;
    float radius = std::sqrt(dx * dx + dy * dy + dz * dz);

    group.m_sphere.center = center;
    group.m_sphere.radius = radius;

    // Setup AABB
    group.m_aabb.min = bx::Vec3(minX, minY, minZ);
    group.m_aabb.max = bx::Vec3(maxX, maxY, maxZ);

    // Create a single primitive for the entire mesh
    Primitive prim;
    prim.m_startIndex = 0;
    prim.m_numIndices = (uint32_t)indices.size();
    prim.m_startVertex = 0;
    prim.m_numVertices = (uint32_t)vertices.size();
    prim.m_sphere = group.m_sphere;
    prim.m_aabb = group.m_aabb;

    group.m_prims.push_back(prim);

    m_mesh.m_groups.clear();
    m_mesh.m_groups.push_back(group);
}

void CHeightFieldMeshComponent::ClearGeometry()
{
    for (auto& group : m_mesh.m_groups)
    {
        if (bgfx::isValid(group.m_vbh))
        {
            bgfx::destroy(group.m_vbh);
            group.m_vbh.idx = bgfx::kInvalidHandle;
        }

        if (bgfx::isValid(group.m_ibh))
        {
            bgfx::destroy(group.m_ibh);
            group.m_ibh.idx = bgfx::kInvalidHandle;
        }
    }
    m_mesh.m_groups.clear();
}


bool CHeightFieldMeshComponent::SaveMesh(const std::string& filePath)
{
    // If no groups, nothing to save
    if (m_mesh.m_groups.empty())
    {
        std::cerr << "CHeightFieldMeshComponent::SaveMesh: No mesh geometry to save" << std::endl;
        return false;
    }

    BinaryMeshData meshData;
    meshData.layout = m_mesh.m_layout;

    for (const auto& srcGroup : m_mesh.m_groups)
    {
        BinaryMeshGroup dstGroup;
        dstGroup.sphere = srcGroup.m_sphere;
        dstGroup.aabb = srcGroup.m_aabb;
        dstGroup.obb = srcGroup.m_obb;
        dstGroup.materialName.clear();
        dstGroup.numVertices = srcGroup.m_numVertices;
        dstGroup.numIndices = srcGroup.m_numIndices;

        // Leave actual vertex/index payload empty for now. A future change should
        // preserve the CPU-side vertex/index arrays created in GenerateHeightFieldGeometry.
        dstGroup.vertexData.clear();
        dstGroup.indexData.clear();

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

    std::ofstream outFile(filePath, std::ios::binary);
    if (!outFile.is_open())
    {
        std::cerr << "CHeightFieldMeshComponent::SaveMesh: Failed to open file for writing: " << filePath << std::endl;
        return false;
    }

    outFile.write(reinterpret_cast<const char*>(binaryBuffer.data()), static_cast<std::streamsize>(binaryBuffer.size()));
    if (!outFile.good())
    {
        std::cerr << "CHeightFieldMeshComponent::SaveMesh: Failed to write to file: " << filePath << std::endl;
        return false;
    }

    outFile.close();
    std::cout << "CHeightFieldMeshComponent::SaveMesh: Successfully saved mesh to: " << filePath << std::endl;
    return true;
}
