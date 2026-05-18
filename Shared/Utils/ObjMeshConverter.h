#pragma once

#include "BgfxBinaryMeshWriter.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <cstring>
#include <cmath>

#include <bx/math.h>
#include <bgfx/bgfx.h>

// Forward declared to share options with FbxMeshConverter.h
struct FbxConvertOptions;

namespace ObjMeshConverter
{
    // Simple POD vector types to avoid any bx::Vec2/Vec3 dependency issues.
    struct Float2 { float x, y; };
    struct Float3 { float x, y, z; };

    // Packed vertex matching the bgfx layout we produce.
    struct PackedVertex
    {
        float px, py, pz;
        float nx, ny, nz;
        float u,  v;
        float tx, ty, tz, tw;
    };

    struct VertexHasher
    {
        size_t operator()(const PackedVertex& pv) const noexcept
        {
            const uint8_t* data = reinterpret_cast<const uint8_t*>(&pv);
            size_t hash = 2166136261u;
            for (size_t i = 0; i < sizeof(PackedVertex); ++i)
                hash = (hash ^ data[i]) * 16777619u;
            return hash;
        }
    };

    struct VertexEqual
    {
        bool operator()(const PackedVertex& a, const PackedVertex& b) const noexcept
        {
            return std::memcmp(&a, &b, sizeof(PackedVertex)) == 0;
        }
    };

    // Lightweight .obj loader:
    // - Supports v, vt, vn, f tokens.
    // - Supports f v, f v/vt, f v//vn, f v/vt/vn indexing.
    // - Triangulates polygon faces via a simple triangle fan.
    // - Reuses FbxConvertOptions for consistency (normals, UVs, tangents, scale).
    // - Tangents are not present in .obj so a default tangent frame is used.
    // - When no vt entries are present and generateSphericalUVs is set,
    //   spherical UV coordinates are generated from the normalised direction
    //   from the mesh AABB centre to each vertex position.
    // - Meshes with more than 65535 unique vertices are split into multiple groups
    //   (triangles are never split) because the bgfx Mesh::load chunk format uses
    //   16-bit index buffers.
    inline bool ConvertObjToBgfxMesh(const char*              objPath,
                                     const FbxConvertOptions& opts,
                                     BinaryMeshData&          outMeshData)
    {
        outMeshData.groups.clear();

        std::ifstream ifs(objPath);
        if (!ifs)
            return false;

        std::vector<Float3> positions;
        std::vector<Float2> texcoords;
        std::vector<Float3> normals;

        // ── Pass 1: collect all unique packed vertices and a flat uint32_t index
        //   list.  Using uint32_t here avoids the silent wrap-around that would
        //   occur with uint16_t once the unique vertex count exceeds 65535.
        std::vector<PackedVertex> allVerts;
        std::vector<uint32_t>    allIndices;
        allVerts.reserve(1 << 16);
        allIndices.reserve(1 << 17);

        std::unordered_map<PackedVertex, uint32_t, VertexHasher, VertexEqual> vertexMap;

        struct FaceIndex { int vi, ti, ni; };
        std::vector<FaceIndex> face;

        std::string line;
        while (std::getline(ifs, line))
        {
            if (line.empty() || line[0] == '#')
                continue;

            std::istringstream iss(line);
            std::string tag;
            iss >> tag;

            if (tag == "v")
            {
                Float3 p = {};
                iss >> p.x >> p.y >> p.z;
                p.x *= opts.scaleFactor;
                p.y *= opts.scaleFactor;
                p.z *= opts.scaleFactor;
                positions.push_back(p);
            }
            else if (tag == "vt")
            {
                Float2 tc = {};
                iss >> tc.x >> tc.y;
                tc.y = 1.0f - tc.y; // flip V for bgfx / D3D
                texcoords.push_back(tc);
            }
            else if (tag == "vn")
            {
                Float3 n = {};
                iss >> n.x >> n.y >> n.z;
                normals.push_back(n);
            }
            else if (tag == "f")
            {
                face.clear();

                std::string token;
                while (iss >> token)
                {
                    int vi = 0, ti = 0, ni = 0;
                    const size_t firstSlash  = token.find('/');
                    const size_t secondSlash = (firstSlash != std::string::npos)
                                             ? token.find('/', firstSlash + 1)
                                             : std::string::npos;
                    try
                    {
                        if (firstSlash == std::string::npos)
                        {
                            vi = std::stoi(token);
                        }
                        else if (secondSlash == std::string::npos)
                        {
                            vi = std::stoi(token.substr(0, firstSlash));
                            ti = std::stoi(token.substr(firstSlash + 1));
                        }
                        else if (secondSlash == firstSlash + 1)
                        {
                            vi = std::stoi(token.substr(0, firstSlash));
                            ni = std::stoi(token.substr(secondSlash + 1));
                        }
                        else
                        {
                            vi = std::stoi(token.substr(0, firstSlash));
                            ti = std::stoi(token.substr(firstSlash + 1, secondSlash - firstSlash - 1));
                            ni = std::stoi(token.substr(secondSlash + 1));
                        }
                    }
                    catch (...)
                    {
                        continue; // malformed token, skip
                    }

                    auto fixIndex = [](int idx, size_t count) -> int
                    {
                        if (idx > 0) return idx - 1;
                        if (idx < 0) return static_cast<int>(count) + idx;
                        return -1;
                    };

                    FaceIndex fi;
                    fi.vi = fixIndex(vi, positions.size());
                    fi.ti = ti ? fixIndex(ti, texcoords.size()) : -1;
                    fi.ni = ni ? fixIndex(ni, normals.size())   : -1;
                    face.push_back(fi);
                }

                // Triangle fan triangulation for polygon faces.
                if (face.size() >= 3)
                {
                    const bool needsFlatNormals = opts.includeNormals
                                               && normals.empty()
                                               && opts.generateFlatNormals;

                    for (size_t f = 1; f + 1 < face.size(); ++f)
                    {
                        const FaceIndex tris[3] = { face[0], face[f], face[f + 1] };

                        // ── Compute flat (face) normal once per triangle ─────
                        float fnx = 0.0f, fny = 1.0f, fnz = 0.0f;
                        if (needsFlatNormals)
                        {
                            float p[3][3] = {};
                            for (int k = 0; k < 3; ++k)
                            {
                                if (tris[k].vi >= 0 && static_cast<size_t>(tris[k].vi) < positions.size())
                                {
                                    p[k][0] = positions[tris[k].vi].x;
                                    p[k][1] = positions[tris[k].vi].y;
                                    p[k][2] = positions[tris[k].vi].z;
                                }
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
                            PackedVertex pv = {};

                            if (tris[k].vi >= 0 && static_cast<size_t>(tris[k].vi) < positions.size())
                            {
                                pv.px = positions[tris[k].vi].x;
                                pv.py = positions[tris[k].vi].y;
                                pv.pz = positions[tris[k].vi].z;
                            }

                            if (opts.includeNormals)
                            {
                                if (tris[k].ni >= 0 && static_cast<size_t>(tris[k].ni) < normals.size())
                                {
                                    pv.nx = normals[tris[k].ni].x;
                                    pv.ny = normals[tris[k].ni].y;
                                    pv.nz = normals[tris[k].ni].z;
                                }
                                else if (needsFlatNormals)
                                {
                                    pv.nx = fnx;
                                    pv.ny = fny;
                                    pv.nz = fnz;
                                }
                            }

                            if (opts.includeUVs && tris[k].ti >= 0 && static_cast<size_t>(tris[k].ti) < texcoords.size())
                            {
                                pv.u = texcoords[tris[k].ti].x;
                                pv.v = texcoords[tris[k].ti].y;
                            }

                            if (opts.includeTangents)
                            {
                                pv.tx = 1.0f; pv.ty = 0.0f; pv.tz = 0.0f; pv.tw = 1.0f;
                            }

                            auto it = vertexMap.find(pv);
                            if (it != vertexMap.end())
                            {
                                allIndices.push_back(it->second);
                            }
                            else
                            {
                                const uint32_t newIdx = static_cast<uint32_t>(allVerts.size());
                                allVerts.push_back(pv);
                                vertexMap.emplace(pv, newIdx);
                                allIndices.push_back(newIdx);
                            }
                        }
                    }
                }
            }
        }

        // ── Spherical UV generation ──────────────────────────────────────────
        // Applied when the OBJ has no vt entries and the option is enabled.
        // UVs are projected from the normalised direction from the AABB centre
        // to each vertex position.
        if (opts.includeUVs && opts.generateSphericalUVs && texcoords.empty() && !allVerts.empty())
        {
            // Compute AABB centre from all collected positions.
            float mnx =  FLT_MAX, mny =  FLT_MAX, mnz =  FLT_MAX;
            float mxx = -FLT_MAX, mxy = -FLT_MAX, mxz = -FLT_MAX;
            for (const auto& p : positions)
            {
                mnx = std::min(mnx, p.x); mny = std::min(mny, p.y); mnz = std::min(mnz, p.z);
                mxx = std::max(mxx, p.x); mxy = std::max(mxy, p.y); mxz = std::max(mxz, p.z);
            }
            const float cx = (mnx + mxx) * 0.5f;
            const float cy = (mny + mxy) * 0.5f;
            const float cz = (mnz + mxz) * 0.5f;
            const float scale = opts.sphericalUVScale;

            for (auto& pv : allVerts)
            {
                float dx = pv.px - cx;
                float dy = pv.py - cy;
                float dz = pv.pz - cz;
                const float len = std::sqrt(dx * dx + dy * dy + dz * dz);
                if (len > 1e-6f) { dx /= len; dy /= len; dz /= len; }

                pv.u = (0.5f + std::atan2(dz, dx) / (2.0f * bx::kPi)) * scale;
                pv.v = (0.5f - std::asin(bx::clamp(dy, -1.0f, 1.0f)) / bx::kPi) * scale;
            }
        }

        if (allVerts.empty() || allIndices.empty())
            return false;

        // ── Build VertexLayout ───────────────────────────────────────────────
        bgfx::VertexLayout layout;
        layout.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float);
        if (opts.includeNormals)   layout.add(bgfx::Attrib::Normal,    3, bgfx::AttribType::Float);
        if (opts.includeUVs)       layout.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float);
        if (opts.includeTangents)  layout.add(bgfx::Attrib::Tangent,   4, bgfx::AttribType::Float);
        layout.end();

        outMeshData.groups.clear();
        outMeshData.layout = layout;

        const uint16_t stride = layout.getStride();

        // ── Pass 2: split into groups of at most kMaxVerts unique vertices ───
        //
        // Mesh::load reads the index buffer as 16-bit so each group must have
        // at most 65535 vertices.  Triangles are never split; when a triangle
        // would push a group over the limit a new group is started instead.
        constexpr uint32_t kMaxVerts = 65535u;

        struct GroupState
        {
            std::unordered_map<uint32_t, uint16_t> globalToLocal;
            std::vector<uint32_t>                  globalVerts;  // ordered global indices
            std::vector<uint16_t>                  localIndices;
        };

        std::vector<GroupState> groupStates;
        groupStates.emplace_back();

        for (size_t i = 0; i < allIndices.size(); i += 3)
        {
            const uint32_t gi0 = allIndices[i];
            const uint32_t gi1 = allIndices[i + 1];
            const uint32_t gi2 = allIndices[i + 2];

            GroupState* gs = &groupStates.back();

            // Count how many new vertices this triangle would add.
            uint32_t newVerts = 0;
            if (!gs->globalToLocal.count(gi0)) ++newVerts;
            if (!gs->globalToLocal.count(gi1)) ++newVerts;
            if (!gs->globalToLocal.count(gi2)) ++newVerts;

            if (static_cast<uint32_t>(gs->globalVerts.size()) + newVerts > kMaxVerts)
            {
                groupStates.emplace_back();
                gs = &groupStates.back();
            }

            auto addVert = [&](uint32_t gvi) -> uint16_t
            {
                auto it = gs->globalToLocal.find(gvi);
                if (it != gs->globalToLocal.end())
                    return it->second;
                const uint16_t localIdx = static_cast<uint16_t>(gs->globalVerts.size());
                gs->globalVerts.push_back(gvi);
                gs->globalToLocal.emplace(gvi, localIdx);
                return localIdx;
            };

            gs->localIndices.push_back(addVert(gi0));
            gs->localIndices.push_back(addVert(gi1));
            gs->localIndices.push_back(addVert(gi2));
        }

        // ── Pass 3: build a BinaryMeshGroup per split group ──────────────────
        for (size_t gi = 0; gi < groupStates.size(); ++gi)
        {
            const GroupState& gs = groupStates[gi];
            if (gs.globalVerts.empty())
                continue;

            BinaryMeshGroup group;
            group.numVertices = static_cast<uint16_t>(gs.globalVerts.size());
            group.numIndices  = static_cast<uint32_t>(gs.localIndices.size());
            group.vertexData.resize(group.numVertices * stride);
            group.indexData   = gs.localIndices;

            // Build a contiguous local vertex array so PMP decimation and bounds
            // computation can work without indirecting through globalVerts.
            std::vector<PackedVertex> groupVerts;
            groupVerts.reserve(gs.globalVerts.size());
            for (uint32_t gvi : gs.globalVerts)
                groupVerts.push_back(allVerts[gvi]);

            std::vector<uint16_t> groupIndices = gs.localIndices;

            group.numVertices = static_cast<uint16_t>(groupVerts.size());
            group.numIndices  = static_cast<uint32_t>(groupIndices.size());
            group.vertexData.resize(group.numVertices * stride);
            group.indexData   = std::move(groupIndices);

            // Pack interleaved vertex buffer.
            void* vbPtr = group.vertexData.data();
            for (uint16_t vi = 0; vi < group.numVertices; ++vi)
            {
                const PackedVertex& v = groupVerts[vi];

                float pos[4] = { v.px, v.py, v.pz, 0.0f };
                bgfx::vertexPack(pos, false, bgfx::Attrib::Position, outMeshData.layout, vbPtr, vi);

                if (opts.includeNormals)
                {
                    float nrm[4] = { v.nx, v.ny, v.nz, 0.0f };
                    bgfx::vertexPack(nrm, true, bgfx::Attrib::Normal, outMeshData.layout, vbPtr, vi);
                }
                if (opts.includeUVs)
                {
                    float uv[4] = { v.u, v.v, 0.0f, 0.0f };
                    bgfx::vertexPack(uv, true, bgfx::Attrib::TexCoord0, outMeshData.layout, vbPtr, vi);
                }
                if (opts.includeTangents)
                {
                    float tan[4] = { v.tx, v.ty, v.tz, v.tw };
                    bgfx::vertexPack(tan, true, bgfx::Attrib::Tangent, outMeshData.layout, vbPtr, vi);
                }
            }

            // Compute AABB, bounding sphere, and OBB from the group's positions.
            {
                float mnx = groupVerts[0].px, mny = groupVerts[0].py, mnz = groupVerts[0].pz;
                float mxx = mnx,              mxy = mny,              mxz = mnz;

                for (uint16_t vi = 1; vi < group.numVertices; ++vi)
                {
                    const PackedVertex& vv = groupVerts[vi];
                    mnx = bx::min(mnx, vv.px); mny = bx::min(mny, vv.py); mnz = bx::min(mnz, vv.pz);
                    mxx = bx::max(mxx, vv.px); mxy = bx::max(mxy, vv.py); mxz = bx::max(mxz, vv.pz);
                }

                group.aabb.min = { mnx, mny, mnz };
                group.aabb.max = { mxx, mxy, mxz };

                const float scx = (mnx + mxx) * 0.5f;
                const float scy = (mny + mxy) * 0.5f;
                const float scz = (mnz + mxz) * 0.5f;
                group.sphere.center = { scx, scy, scz };

                float maxDistSq = 0.0f;
                for (uint16_t vi = 0; vi < group.numVertices; ++vi)
                {
                    const PackedVertex& vv = groupVerts[vi];
                    const float dx = vv.px - scx, dy = vv.py - scy, dz = vv.pz - scz;
                    maxDistSq = bx::max(maxDistSq, dx * dx + dy * dy + dz * dz);
                }
                group.sphere.radius = std::sqrt(maxDistSq);

                std::memset(&group.obb, 0, sizeof(bx::Obb));
                group.obb.mtx[ 0] = (mxx - mnx) * 0.5f;
                group.obb.mtx[ 5] = (mxy - mny) * 0.5f;
                group.obb.mtx[10] = (mxz - mnz) * 0.5f;
                group.obb.mtx[12] = scx;
                group.obb.mtx[13] = scy;
                group.obb.mtx[14] = scz;
                group.obb.mtx[15] = 1.0f;
            }

            BinaryMeshPrimitive prim;
            prim.name        = "group_" + std::to_string(gi);
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

        return !outMeshData.groups.empty();
    }

    /// Convenience: convert an OBJ file directly to an in-memory bgfx binary blob.
    inline bool ConvertObjToBgfxBinary(const char*              objPath,
                                       const FbxConvertOptions& opts,
                                       std::vector<uint8_t>&    outBinary)
    {
        outBinary.clear();
        BinaryMeshData meshData;
        if (!ConvertObjToBgfxMesh(objPath, opts, meshData))
            return false;
        return WriteBgfxBinaryMesh(meshData, outBinary);
    }


} // namespace ObjMeshConverter