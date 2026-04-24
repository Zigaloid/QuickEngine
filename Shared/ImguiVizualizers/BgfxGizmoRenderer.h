#pragma once
#include <bgfx/bgfx.h>
#include <bx/math.h>
#include <vector>

#include "BgfxRenderPrimitives.h"

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
 *        into a BGFX view using transient vertex/index buffers.
 *
 * The renderer is self-contained: it initialises its own shader program and
 * requires only that bgfx itself has been initialised before calling
 * Initialize().
 *
 * Axis colour convention (ABGR): X = red, Y = green, Z = blue.
 * A highlighted axis or plane is drawn in bright yellow.
 *
 * Typical usage:
 * @code
 *   BgfxGizmoRenderer gizmo;
 *   gizmo.Initialize();
 *   // ... each frame:
 *   gizmo.RenderGizmo(viewId, worldMtx, GizmoMode::Translate,
 *                     GizmoAxis::X);
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
     * @brief Initialise the shader program and vertex layout.
     * @return true on success; false if the BGFX program failed to compile.
     */
    bool Initialize();

    /**
     * @brief Release all GPU resources.
     */
    void Shutdown();

    /**
     * @brief Submit a 3D transform manipulator gizmo at the given world transform.
     *
     * The gizmo is positioned and oriented by @p worldMtx44.  Call once per
     * selected object after the scene geometry has been submitted.
     *
     * @param viewId          BGFX view to submit into.
     * @param worldMtx44      Column-major 4x4 world transform (position + orientation).
     * @param mode            GizmoMode::Translate, Scale, or Rotate.
     * @param highlightedAxis Axis or plane to draw highlighted; GizmoAxis::None for none.
     * @param size            Overall reach of the gizmo in world units.
     */
    void RenderGizmo(bgfx::ViewId viewId,
                     const float* worldMtx44,
                     GizmoMode    mode,
                     GizmoAxis    highlightedAxis = GizmoAxis::None,
                     float        size            = 1.0f);

    bool IsInitialized() const { return m_initialized; }

private:
    // ── Gizmo sub-renderers ─────────────────────────────────────────────
    void RenderGizmoTranslate(bgfx::ViewId viewId, const float* worldMtx44,
                               GizmoAxis highlighted, float size);
    void RenderGizmoScale    (bgfx::ViewId viewId, const float* worldMtx44,
                               GizmoAxis highlighted, float size);
    void RenderGizmoRotate   (bgfx::ViewId viewId, const float* worldMtx44,
                               GizmoAxis highlighted, float size);

    // ── Transient-buffer submit helpers ────────────────────────────────
    void SubmitTransientLines    (bgfx::ViewId viewId, const float* mtx44,
                                  const std::vector<PosColorVertex>& verts);
    void SubmitTransientTriangles(bgfx::ViewId viewId, const float* mtx44,
                                  const std::vector<PosColorVertex>& verts,
                                  const std::vector<uint16_t>&       indices);

    bool                m_initialized = false;
    bgfx::ProgramHandle m_program     = BGFX_INVALID_HANDLE;
};

} // namespace ImGuiVisualizers