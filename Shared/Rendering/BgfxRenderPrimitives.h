#pragma once

#include <bgfx/bgfx.h>
#include <bx/math.h>
#include <vector>

namespace Rendering {

/**
 * @brief Vertex layout for simple position + colour rendering via BGFX.
 */
struct PosColorVertex {
    float x, y, z;
    uint32_t abgr;

    static void Init()
    {
        ms_layout
            .begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Color0,   4, bgfx::AttribType::Uint8, true)
            .end();
    }

    static bgfx::VertexLayout ms_layout;
};

/**
 * @brief Renders basic 3D primitives (grid, axes, cubes, spheres, lines)
 *        into a given BGFX view using transient or static buffers.
 *
 * Shaders are loaded once at Initialize() time via the standard BGFX
 * loadProgram() helper.
 */
class BgfxRenderPrimitives  
{
public:
    static BgfxRenderPrimitives& Instance()
    {
        static BgfxRenderPrimitives instance;
        return instance;
    }

    ~BgfxRenderPrimitives() { Shutdown(); }

    BgfxRenderPrimitives(const BgfxRenderPrimitives&) = delete;
    BgfxRenderPrimitives& operator=(const BgfxRenderPrimitives&) = delete;
    BgfxRenderPrimitives(BgfxRenderPrimitives&&) = delete;
    BgfxRenderPrimitives& operator=(BgfxRenderPrimitives&&) = delete;

private:
    BgfxRenderPrimitives() = default;

public:
    /**
     * @brief Create vertex layout, shader programs, and static geometry.
     */
    bool Initialize();

    /**
     * @brief Release all GPU resources.
     */
    void Shutdown();

    // ── Primitive submission ────────────────────────────────────────────

    /**
     * @brief Submit an infinite-style ground grid centred at the origin.
     */
    void RenderGrid(bgfx::ViewId viewId, float size = 20.0f,
                    float step = 1.0f, uint32_t color = 0xff808080);

    /**
     * @brief Submit RGB axes at the origin (X=red, Y=green, Z=blue).
     */
    void RenderAxes(bgfx::ViewId viewId, float length = 2.0f);

    /**
     * @brief Submit a solid-colour unit cube at the given transform.
     */
    void RenderCube(bgfx::ViewId viewId, const float* mtx44, uint32_t abgrColor);

    /**
     * @brief Submit a wireframe unit cube at the given transform.
     */
    void RenderWireCube(bgfx::ViewId viewId, const float* mtx44, uint32_t abgrColor);

    /**
     * @brief Submit a solid-colour unit sphere at the given transform.
     */
    void RenderSphere(bgfx::ViewId viewId, const float* mtx44, uint32_t abgrColor);

    /**
     * @brief Submit a single line segment in world space.
     */
    void RenderLine(bgfx::ViewId viewId,
                    float x0, float y0, float z0,
                    float x1, float y1, float z1,
                    uint32_t abgrColor);


    void RenderWireBox(bgfx::ViewId viewId, const float* worldMtx, uint32_t color);
    void RenderWireSphere(bgfx::ViewId viewId, const float* worldMtx, uint32_t color);
    void RenderWireCylinder(bgfx::ViewId viewId, const float* worldMtx, uint32_t color);
    void RenderWireCapsule(bgfx::ViewId viewId, const float* worldMtx, uint32_t color);

    /**
     * @brief Submit a wireframe unit cone at the given transform.
     *        The cone has its apex at (0, +0.5, 0) and its base circle
     *        at y = -0.5 with radius 0.5, all in local space.
     */
    void RenderWireCone(bgfx::ViewId viewId, const float* worldMtx, uint32_t color);

private:
    void CreateCubeBuffers();
    void CreateSphereBuffers(int rings = 8, int segments = 16);

    void SubmitTransientLines(bgfx::ViewId viewId,
                              const PosColorVertex* verts,
                              uint32_t count) const;

    bool m_initialized = false;

    bgfx::ProgramHandle m_program     = BGFX_INVALID_HANDLE;

    bgfx::VertexBufferHandle m_cubeVbh     = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle  m_cubeIbh     = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle  m_cubeWireIbh = BGFX_INVALID_HANDLE;

    bgfx::VertexBufferHandle m_sphereVbh = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle  m_sphereIbh = BGFX_INVALID_HANDLE;
};

} // namespace ImGuiVisualizers
