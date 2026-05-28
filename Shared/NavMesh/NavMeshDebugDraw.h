#pragma once

#include <bgfx/bgfx.h>
#include <cstdint>

struct rcPolyMesh;

namespace Rendering { class BgfxRenderPrimitives; }

namespace NavMesh {

/// Draws the walkable polygons of a built rcPolyMesh as a wireframe overlay
/// using the existing BgfxRenderPrimitives line renderer.
///
/// Polygon edges are drawn in a semi-transparent blue/teal colour.  The
/// interior of each polygon is NOT filled (no solid-colour pass) because the
/// existing BgfxRenderPrimitives API only supports line primitives; filling
/// would require a separate draw call and state change that is left as a
/// future extension.
struct NavMeshDebugDraw
{
    /// Draw the polygon-mesh wireframe at the given BGFX view.
    /// Does nothing when @p polyMesh is null.
    static void Draw(bgfx::ViewId viewId,
                     Rendering::BgfxRenderPrimitives& prims,
                     const rcPolyMesh* polyMesh,
                     uint32_t abgrColor = 0xcc22ee44);
};

} // namespace NavMesh
