#pragma once

#include <Jolt/Jolt.h>

#ifdef JPH_DEBUG_RENDERER

#include <Jolt/Renderer/DebugRendererSimple.h>
#include <bgfx/bgfx.h>
#include <vector>

/// Debug renderer implementation that bridges Jolt's debug drawing to bgfx
/// via BgfxRenderPrimitives. Call Render() once per frame after physics update.
class JoltDebugRenderer final : public JPH::DebugRendererSimple
{
public:
    JoltDebugRenderer();
    ~JoltDebugRenderer() override = default;

    /// Set the bgfx view used for debug draw submission (default: 0).
    void SetViewId(bgfx::ViewId viewId) { m_viewId = viewId; }

    /// Enable/disable debug rendering at runtime.
    void SetEnabled(bool enabled) { m_enabled = enabled; }
    bool IsEnabled() const { return m_enabled; }

    /// Call each frame before physics DrawBodies to provide camera position for LOD.
    void SetCamera(const float* viewMtx);

    /// Flush all buffered lines/triangles to bgfx. Call once per frame during Render().
    void Flush();

    // --- DebugRendererSimple interface ---
    void DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor) override;
    void DrawTriangle(JPH::RVec3Arg inV1, JPH::RVec3Arg inV2, JPH::RVec3Arg inV3, JPH::ColorArg inColor, ECastShadow inCastShadow) override;
    void DrawText3D(JPH::RVec3Arg inPosition, const JPH::string_view &inString, JPH::ColorArg inColor, float inHeight) override;

private:
    struct LineVertex
    {
        float x, y, z;
        uint32_t abgr;
    };

    std::vector<LineVertex> m_lines; // pairs of vertices
    bgfx::ViewId m_viewId = 0;
    bool m_enabled = true;
};

#endif // JPH_DEBUG_RENDERER
