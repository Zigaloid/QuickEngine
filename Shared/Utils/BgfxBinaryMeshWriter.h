#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <bx/bounds.h>
#include <bgfx/bgfx.h>

// ── Chunk FourCC codes matching Mesh::load() ────────────────────────────
#define BGFX_CHUNK_MAGIC_VB  BX_MAKEFOURCC('V', 'B', ' ', 0x1)
#define BGFX_CHUNK_MAGIC_IB  BX_MAKEFOURCC('I', 'B', ' ', 0x0)
#define BGFX_CHUNK_MAGIC_PRI BX_MAKEFOURCC('P', 'R', 'I', 0x0)

/// Lightweight primitive description (mirrors the bgfx Primitive struct).
struct BinaryMeshPrimitive
{
    std::string name;
    uint32_t    startIndex  = 0;
    uint32_t    numIndices  = 0;
    uint32_t    startVertex = 0;
    uint32_t    numVertices = 0;
    bx::Sphere  sphere = {};
    bx::Aabb    aabb   = {};
    bx::Obb     obb    = {};
};

/// One mesh group that maps 1-to-1 with a bgfx Group.
struct BinaryMeshGroup
{
    // Bounding volumes for the whole group.
    bx::Sphere sphere = {};
    bx::Aabb   aabb   = {};
    bx::Obb    obb    = {};

    // Interleaved vertex data laid out according to `layout`.
    std::vector<uint8_t> vertexData;
    uint16_t             numVertices = 0;

    // 16-bit index data.
    std::vector<uint16_t> indexData;
    uint32_t              numIndices = 0;

    // Material name and per-primitive descriptions.
    std::string                       materialName;
    std::vector<BinaryMeshPrimitive>  primitives;
};

/// Intermediate mesh data that can be serialised to the bgfx binary format.
struct BinaryMeshData
{
    bgfx::VertexLayout            layout;
    std::vector<BinaryMeshGroup>  groups;
};

/// Write `meshData` to `outBuffer` in the chunk format that Mesh::load() reads.
inline bool WriteBgfxBinaryMesh(const BinaryMeshData& meshData,
                                std::vector<uint8_t>&  outBuffer)
{
    outBuffer.clear();

    auto writeRaw = [&](const void* src, size_t size) {
        const uint8_t* p = static_cast<const uint8_t*>(src);
        outBuffer.insert(outBuffer.end(), p, p + size);
    };
    auto write = [&](auto val) { writeRaw(&val, sizeof(val)); };

    for (const auto& group : meshData.groups)
    {
        // ── VB chunk ────────────────────────────────────────────────
        {
            uint32_t chunk = BGFX_CHUNK_MAGIC_VB;
            write(chunk);
            writeRaw(&group.sphere, sizeof(bx::Sphere));
            writeRaw(&group.aabb,   sizeof(bx::Aabb));
            writeRaw(&group.obb,    sizeof(bx::Obb));
            writeRaw(&meshData.layout, sizeof(bgfx::VertexLayout));
            write(group.numVertices);
            writeRaw(group.vertexData.data(), group.vertexData.size());
        }

        // ── IB chunk ────────────────────────────────────────────────
        {
            uint32_t chunk = BGFX_CHUNK_MAGIC_IB;
            write(chunk);
            write(group.numIndices);
            writeRaw(group.indexData.data(),
                     group.numIndices * sizeof(uint16_t));
        }

        // ── PRI chunk ───────────────────────────────────────────────
        {
            uint32_t chunk = BGFX_CHUNK_MAGIC_PRI;
            write(chunk);

            uint16_t matLen = static_cast<uint16_t>(group.materialName.size());
            write(matLen);
            writeRaw(group.materialName.data(), matLen);

            uint16_t numPrims = static_cast<uint16_t>(group.primitives.size());
            write(numPrims);

            for (const auto& prim : group.primitives)
            {
                uint16_t nameLen = static_cast<uint16_t>(prim.name.size());
                write(nameLen);
                writeRaw(prim.name.data(), nameLen);

                write(prim.startIndex);
                write(prim.numIndices);
                write(prim.startVertex);
                write(prim.numVertices);
                writeRaw(&prim.sphere, sizeof(bx::Sphere));
                writeRaw(&prim.aabb,   sizeof(bx::Aabb));
                writeRaw(&prim.obb,    sizeof(bx::Obb));
            }
        }
    }

    return true;
}