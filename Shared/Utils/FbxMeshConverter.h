#pragma once

#include "BgfxBinaryMeshWriter.h"

#include <ufbx.h>
#include <bgfx/bgfx.h>
#include <bx/bounds.h>
#include <bx/math.h>

#include <cmath>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

/// Options that control what vertex attributes are extracted.
struct FbxConvertOptions
{
    bool includeNormals  = true;
    bool includeUVs      = true;
    bool includeTangents = true;
    /// Global scale applied to every position.
    float scaleFactor    = 1.0f;
};

// ── Internal helpers ────────────────────────────────────────────────────
namespace FbxMeshConverterDetail
{

/// Interleaved vertex that matches the layout we build below.
struct PackedVertex
{
    float px, py, pz;       // Position
    float nx, ny, nz;       // Normal
    float u, v;             // TexCoord0
    float tx, ty, tz, tw;   // Tangent
};

/// Build the bgfx::VertexLayout that corresponds to PackedVertex.
inline bgfx::VertexLayout BuildLayout(const FbxConvertOptions& opts)
{
    bgfx::VertexLayout layout;
    layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float);

    if (opts.includeNormals)
        layout.add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float);

    if (opts.includeUVs)
        layout.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float);

    if (opts.includeTangents)
        layout.add(bgfx::Attrib::Tangent, 4, bgfx::AttribType::Float);

    layout.end();
    return layout;
}

/// Hash for de-duplication of vertices.
struct VertexHasher
{
    size_t operator()(const PackedVertex& v) const
    {
        // Simple FNV-1a over the raw bytes.
        const uint8_t* data = reinterpret_cast<const uint8_t*>(&v);
        size_t hash = 2166136261u;
        for (size_t i = 0; i < sizeof(PackedVertex); ++i)
            hash = (hash ^ data[i]) * 16777619u;
        return hash;
    }
};

struct VertexEqual
{
    bool operator()(const PackedVertex& a, const PackedVertex& b) const
    {
        return std::memcmp(&a, &b, sizeof(PackedVertex)) == 0;
    }
};

/// Compute AABB, Sphere, and OBB from a set of positions.
inline void ComputeBounds(const std::vector<PackedVertex>& verts,
                          bx::Aabb& aabb, bx::Sphere& sphere, bx::Obb& obb)
{
    if (verts.empty())
    {
        aabb   = {};
        sphere = {};
        obb    = {};
        return;
    }

    // AABB
    bx::Vec3 mn = { verts[0].px, verts[0].py, verts[0].pz };
    bx::Vec3 mx = mn;
    for (const auto& v : verts)
    {
        mn.x = bx::min(mn.x, v.px);
        mn.y = bx::min(mn.y, v.py);
        mn.z = bx::min(mn.z, v.pz);
        mx.x = bx::max(mx.x, v.px);
        mx.y = bx::max(mx.y, v.py);
        mx.z = bx::max(mx.z, v.pz);
    }
    aabb.min = mn;
    aabb.max = mx;

    // Bounding sphere (centre of AABB, radius = max distance from centre).
    bx::Vec3 centre = {
        (mn.x + mx.x) * 0.5f,
        (mn.y + mx.y) * 0.5f,
        (mn.z + mx.z) * 0.5f
    };
    float maxDistSq = 0.0f;
    for (const auto& v : verts)
    {
        float dx = v.px - centre.x;
        float dy = v.py - centre.y;
        float dz = v.pz - centre.z;
        maxDistSq = bx::max(maxDistSq, dx * dx + dy * dy + dz * dz);
    }
    sphere.center = centre;
    sphere.radius = std::sqrt(maxDistSq);

    // OBB – axis-aligned approximation (sufficient for most uses).
    std::memset(&obb, 0, sizeof(bx::Obb));
    obb.mtx[ 0] = (mx.x - mn.x) * 0.5f;
    obb.mtx[ 5] = (mx.y - mn.y) * 0.5f;
    obb.mtx[10] = (mx.z - mn.z) * 0.5f;
    obb.mtx[12] = centre.x;
    obb.mtx[13] = centre.y;
    obb.mtx[14] = centre.z;
    obb.mtx[15] = 1.0f;
}

} // namespace FbxMeshConverterDetail

// ── Public API ──────────────────────────────────────────────────────────

/// Load an FBX file via ufbx and produce a BinaryMeshData that can be
/// written with WriteBgfxBinaryMesh() and read back by Mesh::load().
///
/// @param fbxPath      File system path to the .fbx file.
/// @param opts         Conversion options (scale, attribute toggles).
/// @param outMeshData  Receives the converted mesh data.
/// @return             true on success.
inline bool ConvertFbxToBgfxMesh(const char*            fbxPath,
                                 const FbxConvertOptions& opts,
                                 BinaryMeshData&         outMeshData)
{
    using namespace FbxMeshConverterDetail;

    // ── Load the FBX scene ──────────────────────────────────────────
    ufbx_load_opts ufbxOpts = {};
    ufbxOpts.target_axes    = ufbx_axes_right_handed_y_up;
    ufbxOpts.target_unit_meters = 1.0f;

    ufbx_error error;
    ufbx_scene* scene = ufbx_load_file(fbxPath, &ufbxOpts, &error);
    if (!scene)
        return false;

    // Build the vertex layout.
    outMeshData.layout = BuildLayout(opts);
    outMeshData.groups.clear();

    const uint16_t stride = outMeshData.layout.getStride();

    // ── Iterate every mesh in the scene ─────────────────────────────
    for (size_t mi = 0; mi < scene->meshes.count; ++mi)
    {
        const ufbx_mesh* fbxMesh = scene->meshes.data[mi];

        // Triangulate the mesh.
        size_t maxTriIndices = fbxMesh->max_face_triangles * 3;
        std::vector<uint32_t> triIndices(maxTriIndices);

        std::vector<PackedVertex> uniqueVerts;
        std::vector<uint16_t>    indices;
        uniqueVerts.reserve(fbxMesh->num_vertices);
        indices.reserve(fbxMesh->num_triangles * 3);

        std::unordered_map<PackedVertex, uint16_t,
                           VertexHasher, VertexEqual> vertexMap;

        for (size_t fi = 0; fi < fbxMesh->num_faces; ++fi)
        {
            ufbx_face face = fbxMesh->faces.data[fi];
            uint32_t numTris = ufbx_triangulate_face(
                triIndices.data(), maxTriIndices, fbxMesh, face);

            for (uint32_t ti = 0; ti < numTris * 3; ++ti)
            {
                uint32_t idx = triIndices[ti];

                PackedVertex pv = {};

                // Position
                ufbx_vec3 pos = ufbx_get_vertex_vec3(&fbxMesh->vertex_position, idx);
                pv.px = static_cast<float>(pos.x) * opts.scaleFactor;
                pv.py = static_cast<float>(pos.y) * opts.scaleFactor;
                pv.pz = static_cast<float>(pos.z) * opts.scaleFactor;

                // Normal
                if (opts.includeNormals && fbxMesh->vertex_normal.exists)
                {
                    ufbx_vec3 n = ufbx_get_vertex_vec3(&fbxMesh->vertex_normal, idx);
                    pv.nx = static_cast<float>(n.x);
                    pv.ny = static_cast<float>(n.y);
                    pv.nz = static_cast<float>(n.z);
                }

                // UV
                if (opts.includeUVs && fbxMesh->vertex_uv.exists)
                {
                    ufbx_vec2 uv = ufbx_get_vertex_vec2(&fbxMesh->vertex_uv, idx);
                    pv.u = static_cast<float>(uv.x);
                    pv.v = 1.0f - static_cast<float>(uv.y); // flip V for D3D / bgfx
                }

                // Tangent
                if (opts.includeTangents && fbxMesh->vertex_tangent.exists)
                {
                    ufbx_vec3 t = ufbx_get_vertex_vec3(&fbxMesh->vertex_tangent, idx);
                    pv.tx = static_cast<float>(t.x);
                    pv.ty = static_cast<float>(t.y);
                    pv.tz = static_cast<float>(t.z);

                    // Bitangent sign
                    if (fbxMesh->vertex_bitangent.exists)
                    {
                        ufbx_vec3 bt = ufbx_get_vertex_vec3(
                            &fbxMesh->vertex_bitangent, idx);
                        ufbx_vec3 n = fbxMesh->vertex_normal.exists
                            ? ufbx_get_vertex_vec3(&fbxMesh->vertex_normal, idx)
                            : ufbx_vec3{0, 1, 0};
                        // cross(N, T) · B < 0 → handedness = -1
                        float cx = static_cast<float>(n.y * t.z - n.z * t.y);
                        float cy = static_cast<float>(n.z * t.x - n.x * t.z);
                        float cz = static_cast<float>(n.x * t.y - n.y * t.x);
                        float dot = cx * static_cast<float>(bt.x)
                                  + cy * static_cast<float>(bt.y)
                                  + cz * static_cast<float>(bt.z);
                        pv.tw = (dot < 0.0f) ? -1.0f : 1.0f;
                    }
                    else
                    {
                        pv.tw = 1.0f;
                    }
                }

                // De-duplicate vertex
                auto it = vertexMap.find(pv);
                if (it != vertexMap.end())
                {
                    indices.push_back(it->second);
                }
                else
                {
                    uint16_t newIdx = static_cast<uint16_t>(uniqueVerts.size());
                    uniqueVerts.push_back(pv);
                    vertexMap[pv] = newIdx;
                    indices.push_back(newIdx);
                }
            }
        }

        if (uniqueVerts.empty())
            continue;

        // ── Pack interleaved vertex buffer using bgfx::vertexPack ───
        BinaryMeshGroup group;
        group.numVertices = static_cast<uint16_t>(uniqueVerts.size());
        group.numIndices  = static_cast<uint32_t>(indices.size());
        group.vertexData.resize(group.numVertices * stride);
        group.indexData   = std::move(indices);

        void* vbPtr = group.vertexData.data();
        for (uint16_t vi = 0; vi < group.numVertices; ++vi)
        {
            const PackedVertex& v = uniqueVerts[vi];

            float pos[4] = { v.px, v.py, v.pz, 0.0f };
            bgfx::vertexPack(pos, false, bgfx::Attrib::Position,
                             outMeshData.layout, vbPtr, vi);

            if (opts.includeNormals)
            {
                float nrm[4] = { v.nx, v.ny, v.nz, 0.0f };
                bgfx::vertexPack(nrm, true, bgfx::Attrib::Normal,
                                 outMeshData.layout, vbPtr, vi);
            }

            if (opts.includeUVs)
            {
                float uv[4] = { v.u, v.v, 0.0f, 0.0f };
                bgfx::vertexPack(uv, true, bgfx::Attrib::TexCoord0,
                                 outMeshData.layout, vbPtr, vi);
            }

            if (opts.includeTangents)
            {
                float tan[4] = { v.tx, v.ty, v.tz, v.tw };
                bgfx::vertexPack(tan, true, bgfx::Attrib::Tangent,
                                 outMeshData.layout, vbPtr, vi);
            }
        }

        // ── Compute bounding volumes ────────────────────────────────
        ComputeBounds(uniqueVerts, group.aabb, group.sphere, group.obb);

        // Single primitive covering the whole group.
        BinaryMeshPrimitive prim;
        prim.name        = fbxMesh->name.length > 0
                             ? std::string(fbxMesh->name.data, fbxMesh->name.length)
                             : "mesh_" + std::to_string(mi);
        prim.startIndex  = 0;
        prim.numIndices  = group.numIndices;
        prim.startVertex = 0;
        prim.numVertices = group.numVertices;
        prim.sphere      = group.sphere;
        prim.aabb        = group.aabb;
        prim.obb         = group.obb;

        group.materialName = "default";
        group.primitives.push_back(std::move(prim));

        outMeshData.groups.push_back(std::move(group));
    }

    ufbx_free_scene(scene);
    return !outMeshData.groups.empty();
}

/// Convenience: convert an FBX file directly to an in-memory bgfx binary blob.
inline bool ConvertFbxToBgfxBinary(const char*             fbxPath,
                                   const FbxConvertOptions& opts,
                                   std::vector<uint8_t>&    outBinary)
{
    BinaryMeshData meshData;
    if (!ConvertFbxToBgfxMesh(fbxPath, opts, meshData))
        return false;

    return WriteBgfxBinaryMesh(meshData, outBinary);
}