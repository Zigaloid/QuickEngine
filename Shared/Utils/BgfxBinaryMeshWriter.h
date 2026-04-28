#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <bx/bounds.h>
#include <bx/error.h>
#include <bx/readerwriter.h>
#include <bgfx/bgfx.h>

// ── Chunk FourCC codes matching Mesh::load() ────────────────────────────
#define BGFX_CHUNK_MAGIC_VB  BX_MAKEFOURCC('V', 'B', ' ', 0x1)
#define BGFX_CHUNK_MAGIC_IB  BX_MAKEFOURCC('I', 'B', ' ', 0x0)
#define BGFX_CHUNK_MAGIC_PRI BX_MAKEFOURCC('P', 'R', 'I', 0x0)

// bgfx::write(VertexLayout) serialises field-by-field:
//   uint8_t  numAttrs
//   uint16_t stride
//   per attr: uint16_t offset, uint16_t attribId, uint8_t num,
//             uint16_t typeId, bool normalized, bool asInt
//
// This is the only format bgfx::read(VertexLayout) in Mesh::load accepts.
// A raw sizeof(bgfx::VertexLayout) == 78 byte dump is NOT compatible and
// will corrupt every field that follows, causing the reader to desynchronise
// and never reach the IB/PRI chunks — leaving m_groups empty.
namespace bgfx
{
    int32_t write(bx::WriterI* _writer, const bgfx::VertexLayout& _layout, bx::Error* _err);
}

// ── bx::WriterI adapter that appends into a std::vector<uint8_t> ────────
struct VectorWriter final : public bx::WriterI
{
    std::vector<uint8_t>& m_buf;
    explicit VectorWriter(std::vector<uint8_t>& buf) : m_buf(buf) {}

    int32_t write(const void* _data, int32_t _size, bx::Error* /*_err*/) override
    {
        const uint8_t* p = static_cast<const uint8_t*>(_data);
        m_buf.insert(m_buf.end(), p, p + _size);
        return _size;
    }
};

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

/// One mesh group mapping 1-to-1 with a bgfx Group.
struct BinaryMeshGroup
{
    bx::Sphere sphere = {};
    bx::Aabb   aabb   = {};
    bx::Obb    obb    = {};

    std::vector<uint8_t>  vertexData;
    uint16_t              numVertices = 0;

    std::vector<uint16_t> indexData;
    uint32_t              numIndices  = 0;

    std::string                       materialName;
    std::vector<BinaryMeshPrimitive>  primitives;
};

/// Intermediate mesh data that serialises to the bgfx binary format.
struct BinaryMeshData
{
    bgfx::VertexLayout            layout;
    std::vector<BinaryMeshGroup>  groups;
};

/// Write meshData to outBuffer in the exact chunk format that Mesh::load() reads.
//
/// Per-group chunk order:  VB → IB → PRI
///
/// VB  : [sphere][aabb][obb][vertexLayout*][numVertices u16][vertexBytes]
/// IB  : [numIndices u32][indexBytes]
/// PRI : [matLen u16][material][numPrims u16]
///         × N: [nameLen u16][name][startIndex u32][numIndices u32]
///              [startVertex u32][numVertices u32][sphere][aabb][obb]
///
/// (*) VertexLayout is written via bgfx::write — NOT a raw struct copy.
inline bool WriteBgfxBinaryMesh(const BinaryMeshData& meshData,
                                std::vector<uint8_t>&  outBuffer)
{
    outBuffer.clear();

    VectorWriter vw(outBuffer);
    bx::Error    err;

    auto writeRaw = [&](const void* src, size_t size)
    {
        const uint8_t* p = static_cast<const uint8_t*>(src);
        outBuffer.insert(outBuffer.end(), p, p + size);
    };
    auto writeVal = [&](auto val) { writeRaw(&val, sizeof(val)); };

    for (const auto& group : meshData.groups)
    {
        // ── VB chunk ────────────────────────────────────────────────────
        writeVal(static_cast<uint32_t>(BGFX_CHUNK_MAGIC_VB));
        writeRaw(&group.sphere, sizeof(bx::Sphere));
        writeRaw(&group.aabb,   sizeof(bx::Aabb));
        writeRaw(&group.obb,    sizeof(bx::Obb));
        // Use bgfx::write so the layout byte stream matches bgfx::read exactly.
        bgfx::write(&vw, meshData.layout, &err);
        writeVal(group.numVertices);                                    // uint16_t
        writeRaw(group.vertexData.data(), group.vertexData.size());

        // ── IB chunk ────────────────────────────────────────────────────
        writeVal(static_cast<uint32_t>(BGFX_CHUNK_MAGIC_IB));
        writeVal(group.numIndices);                                     // uint32_t
        writeRaw(group.indexData.data(), group.numIndices * sizeof(uint16_t));

        // ── PRI chunk ───────────────────────────────────────────────────
        // m_groups.push_back(group) in Mesh::load fires here — must come last.
        writeVal(static_cast<uint32_t>(BGFX_CHUNK_MAGIC_PRI));

        const auto matLen = static_cast<uint16_t>(group.materialName.size());
        writeVal(matLen);
        writeRaw(group.materialName.data(), matLen);

        const auto numPrims = static_cast<uint16_t>(group.primitives.size());
        writeVal(numPrims);

        for (const auto& prim : group.primitives)
        {
            const auto nameLen = static_cast<uint16_t>(prim.name.size());
            writeVal(nameLen);
            writeRaw(prim.name.data(), nameLen);
            writeVal(prim.startIndex);                                  // uint32_t
            writeVal(prim.numIndices);                                  // uint32_t
            writeVal(prim.startVertex);                                 // uint32_t
            writeVal(prim.numVertices);                                 // uint32_t
            writeRaw(&prim.sphere, sizeof(bx::Sphere));
            writeRaw(&prim.aabb,   sizeof(bx::Aabb));
            writeRaw(&prim.obb,    sizeof(bx::Obb));
        }
    }

    return err.isOk();
}