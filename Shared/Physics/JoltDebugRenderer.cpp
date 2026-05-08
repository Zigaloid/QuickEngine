#include <Jolt/Jolt.h>

#ifdef JPH_DEBUG_RENDERER

#include "JoltDebugRenderer.h"
#include "Rendering/BgfxRenderPrimitives.h"
#include <bgfx/bgfx.h>

JoltDebugRenderer::JoltDebugRenderer()
{
    // Initialize the base class (registers internal geometry for helper draw calls)
    Initialize();
}

void JoltDebugRenderer::SetCamera(const float* viewMtx)
{
    // Extract camera position from the inverse of the view matrix.
    // For a standard view matrix the translation is encoded in column 3 of the inverse,
    // but a simpler extraction: cam pos = -transpose(R) * T
    // However DebugRendererSimple just needs an approximate position for LOD.
    // The camera "eye" is at (-dot(right,T), -dot(up,T), -dot(fwd,T))
    float cx = -(viewMtx[0] * viewMtx[12] + viewMtx[1] * viewMtx[13] + viewMtx[2] * viewMtx[14]);
    float cy = -(viewMtx[4] * viewMtx[12] + viewMtx[5] * viewMtx[13] + viewMtx[6] * viewMtx[14]);
    float cz = -(viewMtx[8] * viewMtx[12] + viewMtx[9] * viewMtx[13] + viewMtx[10] * viewMtx[14]);

    SetCameraPos(JPH::RVec3(cx, cy, cz));
}

void JoltDebugRenderer::DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor)
{
    if (!m_enabled)
        return;

    uint32_t abgr = inColor.GetUInt32();

    m_lines.push_back({ inFrom.GetX(), inFrom.GetY(), inFrom.GetZ(), abgr });
    m_lines.push_back({ inTo.GetX(),   inTo.GetY(),   inTo.GetZ(),   abgr });
}

void JoltDebugRenderer::DrawTriangle(JPH::RVec3Arg inV1, JPH::RVec3Arg inV2, JPH::RVec3Arg inV3, JPH::ColorArg inColor, ECastShadow /*inCastShadow*/)
{
    if (!m_enabled)
        return;

    // Render triangles as wireframe lines for simplicity
    DrawLine(inV1, inV2, inColor);
    DrawLine(inV2, inV3, inColor);
    DrawLine(inV3, inV1, inColor);
}

void JoltDebugRenderer::DrawText3D(JPH::RVec3Arg /*inPosition*/, const JPH::string_view& /*inString*/, JPH::ColorArg /*inColor*/, float /*inHeight*/)
{
    // Text rendering not implemented — silently ignore.
}

void JoltDebugRenderer::Flush()
{
    if (!m_enabled || m_lines.empty())
        return;

    auto& primitives = Rendering::BgfxRenderPrimitives::Instance();

    // Submit lines in pairs
    for (size_t i = 0; i + 1 < m_lines.size(); i += 2)
    {
        const auto& a = m_lines[i];
        const auto& b = m_lines[i + 1];
        primitives.RenderLine(m_viewId, a.x, a.y, a.z, b.x, b.y, b.z, a.abgr);
    }

    m_lines.clear();

    // Tell the base class to release unused cached geometry
    NextFrame();
}

#endif // JPH_DEBUG_RENDERER
