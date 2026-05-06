#pragma once
#include <bgfx/bgfx.h>
#include <bx/math.h>
#include <vector>

#include "BgfxRenderPrimitives.h"
#include "BgfxViewIdAllocator.h"
using namespace Rendering;

namespace ImGuiVisualizers {

/**
 * @brief Selects which axis or plane of a gizmo is active / highlighted.
 */
enum class GizmoAxis
{
    None,  ///< Nothing highlighted
    X,     ///< X axis
    Y,     ///< Y axis
    Z,     ///< Z axis
    XY,    ///< XY plane handle
    YZ,    ///< YZ plane handle
    XZ,    ///< XZ plane handle
};

/**
 * @brief Transform manipulation mode for BgfxGizmoRenderer::RenderGizmo().
 */
enum class GizmoMode
{
    Translate,  ///< Arrow shafts + cones + planar handle outlines  (Move)
    Scale,      ///< Axis shafts + box caps + centre box
    Rotate,     ///< Three circular rings, one per axis plane
};

/**
 * @brief Renders a 3D transform manipulator gizmo (Translate / Scale / Rotate)
 *        into a dedicated BGFX view using transient vertex/index buffers.
 *
 * The renderer allocates its own view ID so it always executes after the
 * scene pass, and clears only the depth buffer each frame so the gizmo
 * is drawn in front of all scene geometry.
 *
 * Axis colour convention (ABGR): X = red, Y = green, Z = blue.
 * A highlighted axis or plane is drawn in bright yellow.
 *
 * Typical usage:
 * @code
 *   BgfxGizmoRenderer gizmo;
 *   gizmo.Initialize();
 *   // ... each frame, after scene submission:
 *   gizmo.BeginFrame(framebufferHandle, viewMtx, projMtx);
 *   gizmo.RenderGizmo(worldMtx, GizmoMode::Translate, GizmoAxis::X);
 * @endcode
 */
class BgfxGizmoRenderer
{
public:
    BgfxGizmoRenderer() = default;
    ~BgfxGizmoRenderer() { Shutdown(); }

    BgfxGizmoRenderer(const BgfxGizmoRenderer&)            = delete;
    BgfxGizmoRenderer& operator=(const BgfxGizmoRenderer&) = delete;

    /**
     * @brief Initialise the shader program, vertex layout and allocate a view ID.
     * @return true on success; false if the BGFX program failed to compile.
     */
    bool Initialize();

    /**
     * @brief Release all GPU resources and free the view ID.
     */
    void Shutdown();

    /**
     * @brief Set up the gizmo view for this frame.
     *
     * Must be called once per frame before any RenderGizmo() calls.
     * Binds @p fbh as the render target, sets the viewport rect, and clears
     * only the depth buffer, ensuring the gizmo always renders in front of
     * scene geometry.
     *
     * @param fbh      Framebuffer to render into (same as the scene pass).
     * @param width    Framebuffer width in pixels.
     * @param height   Framebuffer height in pixels.
     * @param viewMtx  Camera view matrix (column-major 4x4).
     * @param projMtx  Camera projection matrix (column-major 4x4).
     */
    void BeginFrame(bgfx::FrameBufferHandle fbh,
                    uint16_t                width,
                    uint16_t                height,
                    const float*            viewMtx,
                    const float*            projMtx);

    /**
     * @brief Submit a 3D transform manipulator gizmo at the given world transform.
     *
     * The gizmo is positioned and oriented by @p worldMtx44.  Call once per
     * selected object after BeginFrame() has been called.
     *
     * @param worldMtx44      Column-major 4x4 world transform (position + orientation).
     * @param mode            GizmoMode::Translate, Scale, or Rotate.
     * @param highlightedAxis Axis or plane to draw highlighted; GizmoAxis::None for none.
     * @param size            Overall reach of the gizmo in world units.
     */
    void RenderGizmo(const float* worldMtx44,
                     GizmoMode    mode,
                     GizmoAxis    highlightedAxis = GizmoAxis::None,
                     float        size            = 1.0f);

    bool IsInitialized() const { return m_initialized; }

private:
    // ── Gizmo sub-renderers ─────────────────────────────────────────────
    void RenderGizmoTranslate(const float* worldMtx44, GizmoAxis highlighted, float size);
    void RenderGizmoScale    (const float* worldMtx44, GizmoAxis highlighted, float size);
    void RenderGizmoRotate   (const float* worldMtx44, GizmoAxis highlighted, float size);

    // ── Transient-buffer submit helpers ────────────────────────────────
    void SubmitTransientLines    (const float* mtx44,
                                  const std::vector<PosColorVertex>& verts);
    void SubmitTransientTriangles(const float* mtx44,
                                  const std::vector<PosColorVertex>& verts,
                                  const std::vector<uint16_t>&       indices);

    bool                m_initialized = false;
    bgfx::ProgramHandle m_program     = BGFX_INVALID_HANDLE;
    bgfx::ViewId        m_viewId      = BgfxViewIdAllocator::kInvalidViewId;
};

} // namespace ImGuiVisualizers