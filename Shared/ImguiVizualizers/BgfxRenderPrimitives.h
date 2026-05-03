#pragma once

#include <bgfx/bgfx.h>
#include <bx/math.h>
#include <vector>

namespace ImGuiVisualizers {

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
    BgfxRenderPrimitives() = default;
    ~BgfxRenderPrimitives() { Shutdown(); }

    BgfxRenderPrimitives(const BgfxRenderPrimitives&) = delete;
    BgfxRenderPrimitives& operator=(const BgfxRenderPrimitives&) = delete;

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

    // ── Shape-aware wireframe helpers ───────────────────────────────────
    // These take a world-space matrix (rotation + translation) and explicit
    // shape parameters, handling the scale transform internally.

    /**
     * @brief Wireframe axis-aligned box.
     * @param halfX/Y/Z  Half-extents along each local axis.
     */
    void RenderWireBox(bgfx::ViewId viewId, const float* worldMtx,
                       float halfX, float halfY, float halfZ,
                       uint32_t color);

    /**
     * @brief Wireframe sphere built from three great-circle arcs.
     * @param radius  Sphere radius.
     */
    void RenderWireSphere(bgfx::ViewId viewId, const float* worldMtx,
                          float radius, uint32_t color);

    /**
     * @brief Wireframe cylinder aligned to the local Y axis.
     * @param radius      Cylinder radius.
     * @param halfHeight  Half the total cylinder height.
     */
    void RenderWireCylinder(bgfx::ViewId viewId, const float* worldMtx,
                            float radius, float halfHeight,
                            uint32_t color);

    /**
     * @brief Wireframe capsule aligned to the local Y axis.
     * @param radius             Hemisphere radius (also the end-cap radius).
     * @param halfCylinderHeight Half the height of the cylindrical section only,
     *                           matching JPH::CapsuleShapeSettings convention.
     */
    void RenderWireCapsule(bgfx::ViewId viewId, const float* worldMtx,
                           float radius, float halfCylinderHeight,
                           uint32_t color);

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
