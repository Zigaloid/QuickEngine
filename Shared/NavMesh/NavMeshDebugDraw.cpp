#include "NavMeshDebugDraw.h"

#include "Rendering/BgfxRenderPrimitives.h"

#include <Recast.h>
#include <DetourNavMesh.h>

namespace NavMesh {

void NavMeshDebugDraw::Draw(bgfx::ViewId viewId,
                             Rendering::BgfxRenderPrimitives& prims,
                             const rcPolyMesh* polyMesh,
                             uint32_t abgrColor)
{
    if (!polyMesh || polyMesh->npolys == 0)
        return;

    const int    nvp   = polyMesh->nvp;
    const float  cs    = polyMesh->cs;
    const float  ch    = polyMesh->ch;
    const float* bmin  = polyMesh->bmin;

    // Each vertex in rcPolyMesh is stored as three unsigned shorts (x,y,z)
    // relative to bmin, scaled by cs (x,z) and ch (y).
    auto vertWorld = [&](int vi, float& wx, float& wy, float& wz)
    {
        const unsigned short* v = &polyMesh->verts[vi * 3];
        wx = bmin[0] + v[0] * cs;
        wy = bmin[1] + (v[1] + 1) * ch;   // +1 so it floats slightly above ground
        wz = bmin[2] + v[2] * cs;
    };

    for (int pi = 0; pi < polyMesh->npolys; ++pi)
    {
        const unsigned short* poly = &polyMesh->polys[pi * nvp * 2];

        // Collect valid verts for this polygon
        int vertCount = 0;
        for (int vi = 0; vi < nvp; ++vi)
        {
            if (poly[vi] == RC_MESH_NULL_IDX)
                break;
            ++vertCount;
        }
        if (vertCount < 3)
            continue;

        // Draw each edge of the polygon as a line
        for (int vi = 0; vi < vertCount; ++vi)
        {
            int v0 = poly[vi];
            int v1 = poly[(vi + 1) % vertCount];

            float x0, y0, z0, x1, y1, z1;
            vertWorld(v0, x0, y0, z0);
            vertWorld(v1, x1, y1, z1);

            prims.RenderLine(viewId, x0, y0, z0, x1, y1, z1, abgrColor);
        }
    }
}

void NavMeshDebugDraw::Draw(bgfx::ViewId viewId,
                             Rendering::BgfxRenderPrimitives& prims,
                             const dtNavMesh* navMesh,
                             uint32_t abgrColor)
{
    if (!navMesh)
        return;

    for (int ti = 0; ti < navMesh->getMaxTiles(); ++ti)
    {
        const dtMeshTile* tile = navMesh->getTile(ti);
        if (!tile || !tile->header || tile->header->polyCount == 0)
            continue;

        for (int pi = 0; pi < tile->header->polyCount; ++pi)
        {
            const dtPoly* poly = &tile->polys[pi];
            if (poly->getType() == DT_POLYTYPE_OFFMESH_CONNECTION)
                continue;

            const dtPolyDetail* detail = tile->detailMeshes ? &tile->detailMeshes[pi] : nullptr;
            if (!detail) continue;

            for (int ti2 = 0; ti2 < detail->triCount; ++ti2)
            {
                const unsigned char* tri = &tile->detailTris[(detail->triBase + ti2) * 4];
                float v[3][3];
                for (int vi = 0; vi < 3; ++vi)
                {
                    if (tri[vi] < poly->vertCount)
                    {
                        const float* src = &tile->verts[poly->verts[tri[vi]] * 3];
                        v[vi][0] = src[0]; v[vi][1] = src[1]; v[vi][2] = src[2];
                    }
                    else
                    {
                        const float* src = &tile->detailVerts[(detail->vertBase + tri[vi] - poly->vertCount) * 3];
                        v[vi][0] = src[0]; v[vi][1] = src[1]; v[vi][2] = src[2];
                    }
                }
                prims.RenderLine(viewId, v[0][0], v[0][1] + 0.05f, v[0][2],
                                          v[1][0], v[1][1] + 0.05f, v[1][2], abgrColor);
                prims.RenderLine(viewId, v[1][0], v[1][1] + 0.05f, v[1][2],
                                          v[2][0], v[2][1] + 0.05f, v[2][2], abgrColor);
                prims.RenderLine(viewId, v[2][0], v[2][1] + 0.05f, v[2][2],
                                          v[0][0], v[0][1] + 0.05f, v[0][2], abgrColor);
            }
        }
    }
}

} // namespace NavMesh
