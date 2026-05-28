#include "NavMeshBuilder.h"

#include "LevelComponent.h"
#include "HeightFieldPhysicsComponent.h"
#include "HeightFieldMeshComponent.h"
#include "EntityComponent.h"
#include "TransformComponent.h"
#include "Math/Matrix4f.h"
#include "Math/Vector3f.h"

#include <Recast.h>
#include <DetourNavMesh.h>
#include <DetourNavMeshBuilder.h>

#include <algorithm>
#include <cstring>
#include <iostream>

namespace NavMesh {

// ?? Custom deleters ??????????????????????????????????????????????????????????

void NavMeshBuilder::RcPolyMeshDeleter::operator()(rcPolyMesh* p) const
{
    rcFreePolyMesh(p);
}

void NavMeshBuilder::RcPolyMeshDetailDeleter::operator()(rcPolyMeshDetail* p) const
{
    rcFreePolyMeshDetail(p);
}

void NavMeshBuilder::DtNavMeshDeleter::operator()(dtNavMesh* p) const
{
    dtFreeNavMesh(p);
}

// ?? Constructor / destructor ?????????????????????????????????????????????????

NavMeshBuilder::NavMeshBuilder()
    : m_ctx(std::make_unique<rcContext>(/*enableLog=*/true))
{
}

NavMeshBuilder::~NavMeshBuilder() = default;

// ?? Helpers ??????????????????????????????????????????????????????????????????

int NavMeshBuilder::PolyCount() const
{
    return m_polyMesh ? m_polyMesh->npolys : 0;
}

void NavMeshBuilder::Clear()
{
    m_polyMesh.reset();
    m_polyMeshDetail.reset();
    m_navMesh.reset();
    m_lastError.clear();
}

// ?? Geometry collection ??????????????????????????????????????????????????????

bool NavMeshBuilder::CollectGeometry(CLevelComponent* level,
                                     std::vector<float>& outVerts,
                                     std::vector<int>&   outTris)
{
    if (!level)
    {
        m_lastError = "No level component provided.";
        return false;
    }

    auto heightFields = level->FindDescendants<CHeightFieldPhysicsComponent>();
    if (heightFields.empty())
    {
        m_lastError = "No CHeightFieldPhysicsComponent found in level.";
        return false;
    }

    for (CHeightFieldPhysicsComponent* hfComp : heightFields)
    {
        if (!hfComp || !hfComp->IsActive())
            continue;

        // --- Get mesh resource from the sibling CHeightFieldMeshComponent ---
        auto meshRes = hfComp->GetMeshResource();
        if (!meshRes || !meshRes->IsLoaded() || !meshRes->IsFinalized())
            continue;

        const Mesh* mesh = meshRes->GetMesh();
        if (!mesh)
            continue;

        // --- Resolve world transform ---
        // Use the same approach as MeshVertexSelectable: look for a sibling
        // CTransformComponent, then fall back to parent entity.
        Matrix4f worldTransform = Matrix4f::GetIdentity();

        if (auto* tfComp = hfComp->FindSibling<CTransformComponent>())
        {
            worldTransform = tfComp->GetTransform();
        }
        else if (auto* entity = dynamic_cast<CEntityComponent*>(hfComp->GetParent()))
        {
            if (auto* tfComp2 = entity->FindChild<CTransformComponent>())
                worldTransform = tfComp2->GetTransform();
        }

        // --- Flatten each group into world-space verts + tris ---
        for (const Group& group : mesh->m_groups)
        {
            if (!group.m_vertices || !group.m_indices)
                continue;

            const uint32_t vertexBase = static_cast<uint32_t>(outVerts.size() / 3);
            const uint16_t stride     = mesh->m_layout.getStride();

            // Position is assumed to be the first 3 floats in the vertex layout
            // (same assumption used throughout the codebase, e.g. MeshVertexSelectable)
            for (uint16_t vi = 0; vi < group.m_numVertices; ++vi)
            {
                const uint8_t* vertData = group.m_vertices + static_cast<size_t>(vi) * stride;
                const float*   posPtr   = reinterpret_cast<const float*>(vertData);

                Vector3f worldPos = worldTransform.TransformPoint(Vector3f(posPtr[0], posPtr[1], posPtr[2]));
                outVerts.push_back(worldPos.x);
                outVerts.push_back(worldPos.y);
                outVerts.push_back(worldPos.z);
            }

            // Indices are uint16_t; Recast needs int
            const uint32_t triCount = group.m_numIndices / 3;
            for (uint32_t ti = 0; ti < triCount; ++ti)
            {
                outTris.push_back(static_cast<int>(vertexBase + group.m_indices[ti * 3 + 0]));
                outTris.push_back(static_cast<int>(vertexBase + group.m_indices[ti * 3 + 1]));
                outTris.push_back(static_cast<int>(vertexBase + group.m_indices[ti * 3 + 2]));
            }
        }
    }

    if (outVerts.empty() || outTris.empty())
    {
        m_lastError = "Height field geometry is empty (no loaded meshes?).";
        return false;
    }

    return true;
}

// ?? Main build pipeline ??????????????????????????????????????????????????????

bool NavMeshBuilder::Build(CLevelComponent* level, const NavMeshConfig& cfg)
{
    Clear();

    // ?? 1. Collect geometry ??????????????????????????????????????????????????
    std::vector<float> verts;
    std::vector<int>   tris;
    if (!CollectGeometry(level, verts, tris))
        return false;

    const int nverts = static_cast<int>(verts.size() / 3);
    const int ntris  = static_cast<int>(tris.size()  / 3);

    // ?? 2. Compute AABB ??????????????????????????????????????????????????????
    float bmin[3], bmax[3];
    rcCalcBounds(verts.data(), nverts, bmin, bmax);

    // ?? 3. Build config ??????????????????????????????????????????????????????
    rcConfig rcCfg{};
    rcCfg.cs                 = cfg.cellSize;
    rcCfg.ch                 = cfg.cellHeight;
    rcCfg.walkableSlopeAngle = cfg.agentMaxSlope;
    rcCfg.walkableHeight     = static_cast<int>(std::ceil(cfg.agentHeight      / cfg.cellHeight));
    rcCfg.walkableClimb      = static_cast<int>(std::floor(cfg.agentMaxClimb   / cfg.cellHeight));
    rcCfg.walkableRadius     = static_cast<int>(std::ceil(cfg.agentRadius      / cfg.cellSize));
    rcCfg.maxEdgeLen         = static_cast<int>(cfg.edgeMaxLen                 / cfg.cellSize);
    rcCfg.maxSimplificationError = cfg.edgeMaxError;
    rcCfg.minRegionArea      = static_cast<int>(cfg.regionMinSize  * cfg.regionMinSize);
    rcCfg.mergeRegionArea    = static_cast<int>(cfg.regionMergeSize * cfg.regionMergeSize);
    rcCfg.maxVertsPerPoly    = cfg.vertsPerPoly;
    rcCfg.detailSampleDist   = (cfg.detailSampleDist < 0.9f) ? 0.0f
                                 : cfg.cellSize * cfg.detailSampleDist;
    rcCfg.detailSampleMaxError = cfg.cellHeight * cfg.detailSampleMaxError;

    rcCalcGridSize(bmin, bmax, rcCfg.cs, &rcCfg.width, &rcCfg.height);
    rcVcopy(rcCfg.bmin, bmin);
    rcVcopy(rcCfg.bmax, bmax);

    // ?? 4. Heightfield ???????????????????????????????????????????????????????
    rcHeightfield* hf = rcAllocHeightfield();
    if (!hf || !rcCreateHeightfield(m_ctx.get(), *hf, rcCfg.width, rcCfg.height,
                                     rcCfg.bmin, rcCfg.bmax, rcCfg.cs, rcCfg.ch))
    {
        rcFreeHeightField(hf);
        m_lastError = "Failed to create Recast heightfield.";
        return false;
    }

    std::vector<unsigned char> triAreas(static_cast<size_t>(ntris), 0);
    rcMarkWalkableTriangles(m_ctx.get(), rcCfg.walkableSlopeAngle,
                             verts.data(), nverts,
                             tris.data(), ntris,
                             triAreas.data());
    if (!rcRasterizeTriangles(m_ctx.get(),
                               verts.data(), nverts,
                               tris.data(), triAreas.data(), ntris,
                               *hf, rcCfg.walkableClimb))
    {
        rcFreeHeightField(hf);
        m_lastError = "Failed to rasterize triangles into heightfield.";
        return false;
    }

    rcFilterLowHangingWalkableObstacles(m_ctx.get(), rcCfg.walkableClimb, *hf);
    rcFilterLedgeSpans(m_ctx.get(), rcCfg.walkableHeight, rcCfg.walkableClimb, *hf);
    rcFilterWalkableLowHeightSpans(m_ctx.get(), rcCfg.walkableHeight, *hf);

    // ?? 5. Compact heightfield ???????????????????????????????????????????????
    rcCompactHeightfield* chf = rcAllocCompactHeightfield();
    if (!chf || !rcBuildCompactHeightfield(m_ctx.get(), rcCfg.walkableHeight,
                                            rcCfg.walkableClimb, *hf, *chf))
    {
        rcFreeHeightField(hf);
        rcFreeCompactHeightfield(chf);
        m_lastError = "Failed to build compact heightfield.";
        return false;
    }
    rcFreeHeightField(hf);

    if (!rcErodeWalkableArea(m_ctx.get(), rcCfg.walkableRadius, *chf))
    {
        rcFreeCompactHeightfield(chf);
        m_lastError = "Failed to erode walkable area.";
        return false;
    }

    // ?? 6. Regions ???????????????????????????????????????????????????????????
    if (!rcBuildDistanceField(m_ctx.get(), *chf))
    {
        rcFreeCompactHeightfield(chf);
        m_lastError = "Failed to build distance field.";
        return false;
    }
    if (!rcBuildRegions(m_ctx.get(), *chf, 0,
                         rcCfg.minRegionArea, rcCfg.mergeRegionArea))
    {
        rcFreeCompactHeightfield(chf);
        m_lastError = "Failed to build regions.";
        return false;
    }

    // ?? 7. Contours ??????????????????????????????????????????????????????????
    rcContourSet* cset = rcAllocContourSet();
    if (!cset || !rcBuildContours(m_ctx.get(), *chf,
                                   rcCfg.maxSimplificationError,
                                   rcCfg.maxEdgeLen, *cset))
    {
        rcFreeCompactHeightfield(chf);
        rcFreeContourSet(cset);
        m_lastError = "Failed to build contours.";
        return false;
    }
    // chf intentionally kept alive — rcBuildPolyMeshDetail needs it below.

    // ?? 8. Polygon mesh ??????????????????????????????????????????????????????
    rcPolyMesh* polyMesh = rcAllocPolyMesh();
    if (!polyMesh || !rcBuildPolyMesh(m_ctx.get(), *cset, rcCfg.maxVertsPerPoly, *polyMesh))
    {
        rcFreeCompactHeightfield(chf);
        rcFreeContourSet(cset);
        rcFreePolyMesh(polyMesh);
        m_lastError = "Failed to build polygon mesh.";
        return false;
    }
    rcFreeContourSet(cset);

    // ?? 9. Detail mesh ???????????????????????????????????????????????????????
    rcPolyMeshDetail* polyMeshDetail = rcAllocPolyMeshDetail();
    if (!polyMeshDetail ||
        !rcBuildPolyMeshDetail(m_ctx.get(), *polyMesh, *chf,    // chf freed above – use baked data
                                rcCfg.detailSampleDist,
                                rcCfg.detailSampleMaxError,
                                *polyMeshDetail))
    {
        // Detail mesh is optional; proceed without it rather than failing
        rcFreePolyMeshDetail(polyMeshDetail);
        polyMeshDetail = nullptr;
    }
    rcFreeCompactHeightfield(chf);
    chf = nullptr;

    // ?? 10. NavMesh data ?????????????????????????????????????????????????????
    // Mark all polygons walkable before creating NavMesh data
    for (int i = 0; i < polyMesh->npolys; ++i)
        polyMesh->flags[i] = 1;

    dtNavMeshCreateParams params{};
    params.verts            = polyMesh->verts;
    params.vertCount        = polyMesh->nverts;
    params.polys            = polyMesh->polys;
    params.polyAreas        = polyMesh->areas;
    params.polyFlags        = polyMesh->flags;
    params.polyCount        = polyMesh->npolys;
    params.nvp              = polyMesh->nvp;
    if (polyMeshDetail)
    {
        params.detailMeshes     = polyMeshDetail->meshes;
        params.detailVerts      = polyMeshDetail->verts;
        params.detailVertsCount = polyMeshDetail->nverts;
        params.detailTris       = polyMeshDetail->tris;
        params.detailTriCount   = polyMeshDetail->ntris;
    }
    params.walkableHeight   = cfg.agentHeight;
    params.walkableRadius   = cfg.agentRadius;
    params.walkableClimb    = cfg.agentMaxClimb;
    rcVcopy(params.bmin, polyMesh->bmin);
    rcVcopy(params.bmax, polyMesh->bmax);
    params.cs               = rcCfg.cs;
    params.ch               = rcCfg.ch;
    params.buildBvTree      = true;

    unsigned char* navData     = nullptr;
    int            navDataSize = 0;
    if (!dtCreateNavMeshData(&params, &navData, &navDataSize))
    {
        rcFreePolyMesh(polyMesh);
        if (polyMeshDetail) rcFreePolyMeshDetail(polyMeshDetail);
        m_lastError = "Failed to create Detour navmesh data.";
        return false;
    }

    dtNavMesh* navMesh = dtAllocNavMesh();
    if (!navMesh)
    {
        dtFree(navData);
        rcFreePolyMesh(polyMesh);
        if (polyMeshDetail) rcFreePolyMeshDetail(polyMeshDetail);
        m_lastError = "Failed to allocate dtNavMesh.";
        return false;
    }

    dtStatus status = navMesh->init(navData, navDataSize, DT_TILE_FREE_DATA);
    if (dtStatusFailed(status))
    {
        dtFreeNavMesh(navMesh);
        rcFreePolyMesh(polyMesh);
        if (polyMeshDetail) rcFreePolyMeshDetail(polyMeshDetail);
        m_lastError = "Failed to initialise dtNavMesh.";
        return false;
    }

    // Transfer ownership
    m_polyMesh.reset(polyMesh);
    m_polyMeshDetail.reset(polyMeshDetail);
    m_navMesh.reset(navMesh);

    std::cout << "[NavMeshBuilder] Built " << m_polyMesh->npolys << " polygons.\n";
    return true;
}

} // namespace NavMesh
