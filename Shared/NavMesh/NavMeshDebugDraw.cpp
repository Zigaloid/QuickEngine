#include "NavMeshDebugDraw.h"

#include "Rendering/BgfxRenderPrimitives.h"

#include <Recast.h>

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

} // namespace NavMesh
