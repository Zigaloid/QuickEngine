#pragma once

#include "BgfxBinaryMeshWriter.h"
#include <ufbx.h>
#include <bgfx/bgfx.h>
#include <bx/bounds.h>
#include <bx/math.h>

#include <cfloat>
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
    /// When true and the source mesh has no UV data, spherical UVs are
    /// generated from each vertex's normalised direction from the mesh centre.
    bool  generateSphericalUVs = false;
    /// Uniform scale applied to the generated spherical UV coordinates.
    float sphericalUVScale     = 1.0f;
    /// When true and the source mesh has no normal data, flat (face) normals
    /// are computed from each triangle's geometry via a cross-product.
    bool  generateFlatNormals  = false;
    /// Fraction of triangles to KEEP after decimation (0.01 – 1.0).
    /// e.g. 0.5 = keep 50% of the original triangle count.
    float decimationKeepRatio = 0.5f;
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

/// Compute spherical UV coordinates for a position relative to a centre.
/// The result is scaled by @p scale and maps the full sphere to [0,scale].
inline void ComputeSphericalUV(float px, float py, float pz,
                                float cx, float cy, float cz,
                                float scale,
                                float& outU, float& outV)
{
    float dx = px - cx;
    float dy = py - cy;
    float dz = pz - cz;
    const float len = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (len > 1e-6f) { dx /= len; dy /= len; dz /= len; }

    outU = (0.5f + std::atan2(dz, dx) / (2.0f * bx::kPi)) * scale;
    outV = (0.5f - std::asin(bx::clamp(dy, -1.0f, 1.0f)) / bx::kPi) * scale;
}

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

    // ── Scan all meshes to determine which attributes are actually present.
    // Attributes requested in opts but absent from the FBX would produce
    // zero-filled channels in every vertex (e.g. all-zero tangents), which
    // corrupts tangent-space shading.  Build the layout only from attributes
    // that are both requested AND available.
    bool anyNormals  = false;
    bool anyUVs      = false;
    bool anyTangents = false;
    for (size_t mi = 0; mi < scene->meshes.count; ++mi)
    {
        const ufbx_mesh* m = scene->meshes.data[mi];
        if (m->vertex_normal.exists)  anyNormals  = true;
        if (m->vertex_uv.exists)      anyUVs      = true;
        if (m->vertex_tangent.exists) anyTangents = true;
    }

    FbxConvertOptions effectiveOpts = opts;
    if (opts.includeNormals  && !anyNormals  && !opts.generateFlatNormals)   effectiveOpts.includeNormals  = false;
    if (opts.includeUVs      && !anyUVs      && !opts.generateSphericalUVs) effectiveOpts.includeUVs      = false;
    if (opts.includeTangents && !anyTangents)                               effectiveOpts.includeTangents = false;

    // Build the vertex layout.
    outMeshData.layout = BuildLayout(effectiveOpts);
    outMeshData.groups.clear();

    const uint16_t stride = outMeshData.layout.getStride();

    // ── Iterate every mesh in the scene ─────────────────────────────
    for (size_t mi = 0; mi < scene->meshes.count; ++mi)
    {
        const ufbx_mesh* fbxMesh = scene->meshes.data[mi];

        // ── Pre-compute mesh AABB centre for spherical UV fallback ───
        const bool needsSphericalUV = effectiveOpts.includeUVs
                                   && effectiveOpts.generateSphericalUVs
                                   && !fbxMesh->vertex_uv.exists;
        float meshCX = 0.0f, meshCY = 0.0f, meshCZ = 0.0f;
        if (needsSphericalUV && fbxMesh->num_vertices > 0)
        {
            float mnx =  FLT_MAX, mny =  FLT_MAX, mnz =  FLT_MAX;
            float mxx = -FLT_MAX, mxy = -FLT_MAX, mxz = -FLT_MAX;
            for (size_t vi = 0; vi < fbxMesh->num_vertices; ++vi)
            {
                const float vpx = static_cast<float>(fbxMesh->vertices.data[vi].x) * effectiveOpts.scaleFactor;
                const float vpy = static_cast<float>(fbxMesh->vertices.data[vi].y) * effectiveOpts.scaleFactor;
                const float vpz = static_cast<float>(fbxMesh->vertices.data[vi].z) * effectiveOpts.scaleFactor;
                mnx = std::min(mnx, vpx); mny = std::min(mny, vpy); mnz = std::min(mnz, vpz);
                mxx = std::max(mxx, vpx); mxy = std::max(mxy, vpy); mxz = std::max(mxz, vpz);
            }
            meshCX = (mnx + mxx) * 0.5f;
            meshCY = (mny + mxy) * 0.5f;
            meshCZ = (mnz + mxz) * 0.5f;
        }

        // Triangulate the mesh.
        size_t maxTriIndices = fbxMesh->max_face_triangles * 3;
        std::vector<uint32_t> triIndices(maxTriIndices);

        std::vector<PackedVertex> uniqueVerts;
        std::vector<uint16_t>    indices;
        uniqueVerts.reserve(fbxMesh->num_vertices);
        indices.reserve(fbxMesh->num_triangles * 3);

        std::unordered_map<PackedVertex, uint16_t,
                           VertexHasher, VertexEqual> vertexMap;

        const bool needsFlatNormals = effectiveOpts.includeNormals
                                   && !fbxMesh->vertex_normal.exists
                                   && effectiveOpts.generateFlatNormals;

        for (size_t fi = 0; fi < fbxMesh->num_faces; ++fi)
        {
            ufbx_face face = fbxMesh->faces.data[fi];
            uint32_t numTris = ufbx_triangulate_face(
                triIndices.data(), maxTriIndices, fbxMesh, face);

            for (uint32_t ti = 0; ti < numTris; ++ti)
            {
                // ── Compute flat (face) normal once per triangle ─────────────
                float fnx = 0.0f, fny = 1.0f, fnz = 0.0f;
                if (needsFlatNormals)
                {
                    float p[3][3] = {};
                    for (int k = 0; k < 3; ++k)
                    {
                        ufbx_vec3 pos = ufbx_get_vertex_vec3(
                            &fbxMesh->vertex_position, triIndices[ti * 3 + k]);
                        p[k][0] = static_cast<float>(pos.x) * effectiveOpts.scaleFactor;
                        p[k][1] = static_cast<float>(pos.y) * effectiveOpts.scaleFactor;
                        p[k][2] = static_cast<float>(pos.z) * effectiveOpts.scaleFactor;
                    }
                    const float e0x = p[1][0] - p[0][0], e0y = p[1][1] - p[0][1], e0z = p[1][2] - p[0][2];
                    const float e1x = p[2][0] - p[0][0], e1y = p[2][1] - p[0][1], e1z = p[2][2] - p[0][2];
                    fnx = e0y * e1z - e0z * e1y;
                    fny = e0z * e1x - e0x * e1z;
                    fnz = e0x * e1y - e0y * e1x;
                    const float len = std::sqrt(fnx * fnx + fny * fny + fnz * fnz);
                    if (len > 1e-6f) { fnx /= len; fny /= len; fnz /= len; }
                }

                for (int k = 0; k < 3; ++k)
                {
                    const uint32_t idx = triIndices[ti * 3 + k];

                    PackedVertex pv = {};

                    // Position
                    ufbx_vec3 pos = ufbx_get_vertex_vec3(&fbxMesh->vertex_position, idx);
                    pv.px = static_cast<float>(pos.x) * effectiveOpts.scaleFactor;
                    pv.py = static_cast<float>(pos.y) * effectiveOpts.scaleFactor;
                    pv.pz = static_cast<float>(pos.z) * effectiveOpts.scaleFactor;

                    // Normal — use mesh data when present, otherwise flat-generated.
                    if (effectiveOpts.includeNormals)
                    {
                        if (fbxMesh->vertex_normal.exists)
                        {
                            ufbx_vec3 n = ufbx_get_vertex_vec3(&fbxMesh->vertex_normal, idx);
                            pv.nx = static_cast<float>(n.x);
                            pv.ny = static_cast<float>(n.y);
                            pv.nz = static_cast<float>(n.z);
                        }
                        else if (needsFlatNormals)
                        {
                            pv.nx = fnx;
                            pv.ny = fny;
                            pv.nz = fnz;
                        }
                    }

                    // UV — use mesh data when available, fall back to spherical projection.
                    if (effectiveOpts.includeUVs)
                    {
                        if (fbxMesh->vertex_uv.exists)
                        {
                            ufbx_vec2 uv = ufbx_get_vertex_vec2(&fbxMesh->vertex_uv, idx);
                            pv.u = static_cast<float>(uv.x);
                            pv.v = 1.0f - static_cast<float>(uv.y); // flip V for D3D / bgfx
                        }
                        else if (effectiveOpts.generateSphericalUVs)
                        {
                            ComputeSphericalUV(pv.px, pv.py, pv.pz,
                                               meshCX, meshCY, meshCZ,
                                               effectiveOpts.sphericalUVScale,
                                               pv.u, pv.v);
                        }
                    }

                    // Tangent
                    if (effectiveOpts.includeTangents && fbxMesh->vertex_tangent.exists)
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

            if (effectiveOpts.includeNormals)
            {
                float nrm[4] = { v.nx, v.ny, v.nz, 0.0f };
                bgfx::vertexPack(nrm, true, bgfx::Attrib::Normal,
                                 outMeshData.layout, vbPtr, vi);
            }

            if (effectiveOpts.includeUVs)
            {
                float uv[4] = { v.u, v.v, 0.0f, 0.0f };
                bgfx::vertexPack(uv, true, bgfx::Attrib::TexCoord0,
                                 outMeshData.layout, vbPtr, vi);
            }

            if (effectiveOpts.includeTangents)
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

/// Lightweight stats returned by PreviewFbxMeshStats.
struct FbxMeshPreviewStats
{
    uint32_t totalVertices  = 0;
    uint32_t totalTriangles = 0;
    bool     valid          = false;
};

/// Load an FBX and count vertices/triangles without running decimation.
/// Useful for displaying polygon counts in the UI before a full conversion.
inline FbxMeshPreviewStats PreviewFbxMeshStats(const char*              fbxPath,
                                               const FbxConvertOptions& opts)
{
    FbxConvertOptions previewOpts = opts;
    BinaryMeshData meshData;
    FbxMeshPreviewStats stats;
    if (!ConvertFbxToBgfxMesh(fbxPath, previewOpts, meshData))
        return stats;

    for (const auto& g : meshData.groups)
    {
        stats.totalVertices  += g.numVertices;
        stats.totalTriangles += g.numIndices / 3;
    }
    stats.valid = true;
    return stats;
}