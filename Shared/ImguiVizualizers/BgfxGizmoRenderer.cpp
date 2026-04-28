#include <bgfx/embedded_shader.h>
#include "bgfx_utils.h"

#include <bx/math.h>
#include <vector>
#include <cstring>

#include "debugdraw/vs_debugdraw_lines.bin.h"
#include "debugdraw/fs_debugdraw_lines.bin.h"

#include "BgfxGizmoRenderer.h"

static const bgfx::EmbeddedShader s_gizmoShaders[] =
{
    BGFX_EMBEDDED_SHADER(vs_debugdraw_lines),
    BGFX_EMBEDDED_SHADER(fs_debugdraw_lines),
    BGFX_EMBEDDED_SHADER_END()
};

namespace ImGuiVisualizers {

// ── Colour palette (ABGR) ────────────────────────────────────────────────
static constexpr uint32_t kColorX         = 0xff0000ff;  // X axis    – red
static constexpr uint32_t kColorY         = 0xff00ff00;  // Y axis    – green
static constexpr uint32_t kColorZ         = 0xffff0000;  // Z axis    – blue
static constexpr uint32_t kColorCenter    = 0xffffffff;  // centre    – white
static constexpr uint32_t kColorHighlight = 0xff00ffff;  // hovered   – yellow
static constexpr uint32_t kColorXYPlane   = 0xffaa0000;  // XY handle – dim blue
static constexpr uint32_t kColorYZPlane   = 0xff0000aa;  // YZ handle – dim red
static constexpr uint32_t kColorXZPlane   = 0xff00aa00;  // XZ handle – dim green

static inline uint32_t PickColor(GizmoAxis target, GizmoAxis highlighted, uint32_t base)
{
    return (target == highlighted) ? kColorHighlight : base;
}

// ── Initialisation / Shutdown ────────────────────────────────────────────

bool BgfxGizmoRenderer::Initialize()
{
    if (m_initialized)
        return true;

    m_viewId = BgfxViewIdAllocator::Get().Allocate();
    if (m_viewId == BgfxViewIdAllocator::kInvalidViewId)
        return false;

    // Ensure the shared vertex layout is ready (idempotent).
    PosColorVertex::Init();

    bgfx::RendererType::Enum type = bgfx::getRendererType();
    m_program = bgfx::createProgram(
        bgfx::createEmbeddedShader(s_gizmoShaders, type, "vs_debugdraw_lines"),
        bgfx::createEmbeddedShader(s_gizmoShaders, type, "fs_debugdraw_lines"),
        true);

    if (!bgfx::isValid(m_program))
    {
        BgfxViewIdAllocator::Get().Free(m_viewId);
        m_viewId = BgfxViewIdAllocator::kInvalidViewId;
        return false;
    }

    m_initialized = true;
    return true;
}

void BgfxGizmoRenderer::Shutdown()
{
    if (!m_initialized)
        return;

    if (bgfx::isValid(m_program))
    {
        bgfx::destroy(m_program);
        m_program = BGFX_INVALID_HANDLE;
    }

    if (m_viewId != BgfxViewIdAllocator::kInvalidViewId)
    {
        BgfxViewIdAllocator::Get().Free(m_viewId);
        m_viewId = BgfxViewIdAllocator::kInvalidViewId;
    }

    m_initialized = false;
}

// ── Per-frame view setup ─────────────────────────────────────────────────

void BgfxGizmoRenderer::BeginFrame(bgfx::FrameBufferHandle fbh,
                                    uint16_t                width,
                                    uint16_t                height,
                                    const float*            viewMtx,
                                    const float*            projMtx)
{
    if (!m_initialized)
        return;

    bgfx::setViewName(m_viewId, "GizmoOverlay");
    bgfx::setViewFrameBuffer(m_viewId, fbh);
    bgfx::setViewRect(m_viewId, 0, 0, width, height);
    bgfx::setViewTransform(m_viewId, viewMtx, projMtx);

    // Clear depth only – colour is preserved from the scene pass.
    bgfx::setViewClear(m_viewId, BGFX_CLEAR_DEPTH, 0, 1.0f, 0);

    bgfx::touch(m_viewId);
}

// ── Transient submit helpers ─────────────────────────────────────────────

void BgfxGizmoRenderer::SubmitTransientLines(const float*                        mtx44,
                                              const std::vector<PosColorVertex>& verts)
{
    if (verts.empty())
        return;

    const uint32_t n = static_cast<uint32_t>(verts.size());
    if (!checkAvailTransientBuffers(n, PosColorVertex::ms_layout, 0))
        return;

    bgfx::TransientVertexBuffer tvb;
    bgfx::allocTransientVertexBuffer(&tvb, n, PosColorVertex::ms_layout);
    memcpy(tvb.data, verts.data(), n * sizeof(PosColorVertex));

    bgfx::setTransform(mtx44);
    bgfx::setVertexBuffer(0, &tvb, 0, n);
    bgfx::setState(BGFX_STATE_WRITE_RGB
                 | BGFX_STATE_WRITE_A
                 | BGFX_STATE_WRITE_Z
                 | BGFX_STATE_DEPTH_TEST_LESS
                 | BGFX_STATE_PT_LINES
                 | BGFX_STATE_MSAA);
    bgfx::submit(m_viewId, m_program);
}

void BgfxGizmoRenderer::SubmitTransientTriangles(const float*                        mtx44,
                                                  const std::vector<PosColorVertex>& verts,
                                                  const std::vector<uint16_t>&       indices)
{
    if (verts.empty() || indices.empty())
        return;

    const uint32_t nv = static_cast<uint32_t>(verts.size());
    const uint32_t ni = static_cast<uint32_t>(indices.size());
    if (!checkAvailTransientBuffers(nv, PosColorVertex::ms_layout, ni))
        return;

    bgfx::TransientVertexBuffer tvb;
    bgfx::TransientIndexBuffer  tib;
    bgfx::allocTransientVertexBuffer(&tvb, nv, PosColorVertex::ms_layout);
    bgfx::allocTransientIndexBuffer (&tib, ni);
    memcpy(tvb.data, verts.data(),   nv * sizeof(PosColorVertex));
    memcpy(tib.data, indices.data(), ni * sizeof(uint16_t));

    bgfx::setTransform(mtx44);
    bgfx::setVertexBuffer(0, &tvb);
    bgfx::setIndexBuffer(&tib);
    bgfx::setState(BGFX_STATE_WRITE_RGB
                 | BGFX_STATE_WRITE_A
                 | BGFX_STATE_WRITE_Z
                 | BGFX_STATE_DEPTH_TEST_LESS
                 | BGFX_STATE_CULL_CW
                 | BGFX_STATE_MSAA);
    bgfx::submit(m_viewId, m_program);
}

// ── RenderGizmo dispatcher ───────────────────────────────────────────────

void BgfxGizmoRenderer::RenderGizmo(const float* worldMtx44,
                                      GizmoMode    mode,
                                      GizmoAxis    highlightedAxis,
                                      float        size)
{
    if (!m_initialized)
        return;

    switch (mode)
    {
        case GizmoMode::Translate: RenderGizmoTranslate(worldMtx44, highlightedAxis, size); break;
        case GizmoMode::Scale:     RenderGizmoScale    (worldMtx44, highlightedAxis, size); break;
        case GizmoMode::Rotate:    RenderGizmoRotate   (worldMtx44, highlightedAxis, size); break;
    }
}

// ── Translate gizmo ──────────────────────────────────────────────────────
//
//  Draw 1 – Lines  : 3 axis shafts + 3 planar handle wireframe squares
//  Draw 2 – Tris   : 3 arrowhead cones (one per axis)
//

void BgfxGizmoRenderer::RenderGizmoTranslate(const float* worldMtx44,
                                               GizmoAxis    highlighted,
                                               float        size)
{
    const float shaftEnd = size * 0.78f;
    const float tipEnd   = size * 1.00f;
    const float coneR    = size * 0.055f;
    const float planeMin = size * 0.15f;
    const float planeMax = size * 0.35f;
    constexpr int kSegs  = 12;

    const uint32_t cx  = PickColor(GizmoAxis::X,  highlighted, kColorX);
    const uint32_t cy  = PickColor(GizmoAxis::Y,  highlighted, kColorY);
    const uint32_t cz  = PickColor(GizmoAxis::Z,  highlighted, kColorZ);
    const uint32_t cxy = PickColor(GizmoAxis::XY, highlighted, kColorXYPlane);
    const uint32_t cyz = PickColor(GizmoAxis::YZ, highlighted, kColorYZPlane);
    const uint32_t cxz = PickColor(GizmoAxis::XZ, highlighted, kColorXZPlane);

    // ── Draw 1 : axis shafts + planar handle outlines (lines) ────────────
    {
        std::vector<PosColorVertex> lv;
        lv.reserve(6 + 24);

        // Axis shafts
        lv.push_back({ 0.0f,     0.0f,     0.0f,     cx });
        lv.push_back({ shaftEnd, 0.0f,     0.0f,     cx });
        lv.push_back({ 0.0f,     0.0f,     0.0f,     cy });
        lv.push_back({ 0.0f,     shaftEnd, 0.0f,     cy });
        lv.push_back({ 0.0f,     0.0f,     0.0f,     cz });
        lv.push_back({ 0.0f,     0.0f,     shaftEnd, cz });

        // Planar handle outlines – each is a wireframe square (4 segments = 8 verts)
        // XY plane square (z = 0)
        lv.push_back({ planeMin, planeMin, 0.0f, cxy }); lv.push_back({ planeMax, planeMin, 0.0f, cxy });
        lv.push_back({ planeMax, planeMin, 0.0f, cxy }); lv.push_back({ planeMax, planeMax, 0.0f, cxy });
        lv.push_back({ planeMax, planeMax, 0.0f, cxy }); lv.push_back({ planeMin, planeMax, 0.0f, cxy });
        lv.push_back({ planeMin, planeMax, 0.0f, cxy }); lv.push_back({ planeMin, planeMin, 0.0f, cxy });

        // YZ plane square (x = 0)
        lv.push_back({ 0.0f, planeMin, planeMin, cyz }); lv.push_back({ 0.0f, planeMax, planeMin, cyz });
        lv.push_back({ 0.0f, planeMax, planeMin, cyz }); lv.push_back({ 0.0f, planeMax, planeMax, cyz });
        lv.push_back({ 0.0f, planeMax, planeMax, cyz }); lv.push_back({ 0.0f, planeMin, planeMax, cyz });
        lv.push_back({ 0.0f, planeMin, planeMax, cyz }); lv.push_back({ 0.0f, planeMin, planeMin, cyz });

        // XZ plane square (y = 0)
        lv.push_back({ planeMin, 0.0f, planeMin, cxz }); lv.push_back({ planeMax, 0.0f, planeMin, cxz });
        lv.push_back({ planeMax, 0.0f, planeMin, cxz }); lv.push_back({ planeMax, 0.0f, planeMax, cxz });
        lv.push_back({ planeMax, 0.0f, planeMax, cxz }); lv.push_back({ planeMin, 0.0f, planeMax, cxz });
        lv.push_back({ planeMin, 0.0f, planeMax, cxz }); lv.push_back({ planeMin, 0.0f, planeMin, cxz });

        SubmitTransientLines(worldMtx44, lv);
    }

    // ── Draw 2 : arrowhead cones (triangles) ─────────────────────────────
    //
    //  Each cone: (kSegs+1) verts, kSegs*3 indices.
    //  Tip at tipEnd along axis; base circle at shaftEnd with radius coneR.
    //  Tangent axes for the base circle are the two perpendicular axes.
    {
        const uint32_t coneColors[3] = { cx, cy, cz };

        std::vector<PosColorVertex> tv;
        std::vector<uint16_t>       ti;
        tv.reserve(3 * (kSegs + 1));
        ti.reserve(3 * kSegs * 3);

        for (int axis = 0; axis < 3; ++axis)
        {
            const uint32_t  col  = coneColors[axis];
            const int       t0   = (axis + 1) % 3;
            const int       t1   = (axis + 2) % 3;
            const uint16_t  base = static_cast<uint16_t>(tv.size());

            // Tip vertex
            float tip[3] = {};
            tip[axis] = tipEnd;
            tv.push_back({ tip[0], tip[1], tip[2], col });

            // Base circle vertices
            for (int i = 0; i < kSegs; ++i)
            {
                const float theta = bx::kPi2 * static_cast<float>(i) / static_cast<float>(kSegs);
                float p[3] = {};
                p[axis] = shaftEnd;
                p[t0]   = coneR * bx::cos(theta);
                p[t1]   = coneR * bx::sin(theta);
                tv.push_back({ p[0], p[1], p[2], col });
            }

            // Triangle fan (CCW so faces point outward)
            for (int i = 0; i < kSegs; ++i)
            {
                ti.push_back(base);
                ti.push_back(static_cast<uint16_t>(base + 1 + (i + 1) % kSegs));
                ti.push_back(static_cast<uint16_t>(base + 1 + i));
            }
        }

        SubmitTransientTriangles(worldMtx44, tv, ti);
    }
}

// ── Scale gizmo ──────────────────────────────────────────────────────────
//
//  Draw 1 – Lines  : 3 axis shafts
//  Draw 2 – Tris   : 3 axis box caps + 1 centre box
//

void BgfxGizmoRenderer::RenderGizmoScale(const float* worldMtx44,
                                           GizmoAxis    highlighted,
                                           float        size)
{
    const float shaftEnd  = size * 0.82f;
    const float boxCenter = size * 0.92f;
    const float boxHalf   = size * 0.07f;
    const float ctrHalf   = size * 0.055f;

    const uint32_t cx = PickColor(GizmoAxis::X, highlighted, kColorX);
    const uint32_t cy = PickColor(GizmoAxis::Y, highlighted, kColorY);
    const uint32_t cz = PickColor(GizmoAxis::Z, highlighted, kColorZ);

    // ── Draw 1 : axis shafts (lines) ─────────────────────────────────────
    {
        const std::vector<PosColorVertex> lv = {
            { 0.0f,     0.0f,     0.0f,     cx },
            { shaftEnd, 0.0f,     0.0f,     cx },
            { 0.0f,     0.0f,     0.0f,     cy },
            { 0.0f,     shaftEnd, 0.0f,     cy },
            { 0.0f,     0.0f,     0.0f,     cz },
            { 0.0f,     0.0f,     shaftEnd, cz },
        };
        SubmitTransientLines(worldMtx44, lv);
    }

    // ── Draw 2 : box caps + centre box (triangles) ───────────────────────
    //
    //  Vertex layout per box (CCW, matches BgfxRenderPrimitives convention):
    //   0={-h,-h,+h}  1={+h,-h,+h}  2={+h,+h,+h}  3={-h,+h,+h}
    //   4={-h,-h,-h}  5={+h,-h,-h}  6={+h,+h,-h}  7={-h,+h,-h}
    {
        auto appendBox = [](std::vector<PosColorVertex>& tv,
                             std::vector<uint16_t>&       ti,
                             float ox, float oy, float oz,
                             float h, uint32_t col)
        {
            static constexpr uint16_t kBoxFaces[] = {
                0,1,2, 0,2,3,
                5,4,7, 5,7,6,
                4,0,3, 4,3,7,
                1,5,6, 1,6,2,
                3,2,6, 3,6,7,
                4,5,1, 4,1,0,
            };
            const uint16_t b = static_cast<uint16_t>(tv.size());
            tv.push_back({ ox-h, oy-h, oz+h, col });
            tv.push_back({ ox+h, oy-h, oz+h, col });
            tv.push_back({ ox+h, oy+h, oz+h, col });
            tv.push_back({ ox-h, oy+h, oz+h, col });
            tv.push_back({ ox-h, oy-h, oz-h, col });
            tv.push_back({ ox+h, oy-h, oz-h, col });
            tv.push_back({ ox+h, oy+h, oz-h, col });
            tv.push_back({ ox-h, oy+h, oz-h, col });
            for (uint16_t idx : kBoxFaces)
                ti.push_back(b + idx);
        };

        std::vector<PosColorVertex> tv;
        std::vector<uint16_t>       ti;
        tv.reserve(4 * 8);
        ti.reserve(4 * 36);

        appendBox(tv, ti, boxCenter, 0.0f,      0.0f,      boxHalf, cx);
        appendBox(tv, ti, 0.0f,      boxCenter, 0.0f,      boxHalf, cy);
        appendBox(tv, ti, 0.0f,      0.0f,      boxCenter, boxHalf, cz);
        appendBox(tv, ti, 0.0f,      0.0f,      0.0f,      ctrHalf, kColorCenter);

        SubmitTransientTriangles(worldMtx44, tv, ti);
    }
}

// ── Rotate gizmo ─────────────────────────────────────────────────────────
//
//  Draw 1 – Lines : 3 circular rings, one per axis plane.
//    X ring : circle in the YZ plane  →  (0,  r·cos t,  r·sin t)
//    Y ring : circle in the XZ plane  →  (r·cos t,  0,  r·sin t)
//    Z ring : circle in the XY plane  →  (r·cos t,  r·sin t,  0)
//

void BgfxGizmoRenderer::RenderGizmoRotate(const float* worldMtx44,
                                            GizmoAxis    highlighted,
                                            float        size)
{
    constexpr int kSegs = 32;

    const uint32_t ringColors[3] = {
        PickColor(GizmoAxis::X, highlighted, kColorX),
        PickColor(GizmoAxis::Y, highlighted, kColorY),
        PickColor(GizmoAxis::Z, highlighted, kColorZ),
    };

    // All three rings share one draw call – each vertex carries its own colour.
    std::vector<PosColorVertex> lv;
    lv.reserve(3 * kSegs * 2);

    for (int ring = 0; ring < 3; ++ring)
    {
        // `ring` is the axis the circle is normal to; a0/a1 are its tangent axes.
        const int     a0  = (ring + 1) % 3;
        const int     a1  = (ring + 2) % 3;
        const uint32_t col = ringColors[ring];

        for (int i = 0; i < kSegs; ++i)
        {
            const float t0 = bx::kPi2 * static_cast<float>(i)     / static_cast<float>(kSegs);
            const float t1 = bx::kPi2 * static_cast<float>(i + 1) / static_cast<float>(kSegs);

            float p0[3] = {}, p1[3] = {};
            p0[a0] = size * bx::cos(t0);  p0[a1] = size * bx::sin(t0);
            p1[a0] = size * bx::cos(t1);  p1[a1] = size * bx::sin(t1);

            lv.push_back({ p0[0], p0[1], p0[2], col });
            lv.push_back({ p1[0], p1[1], p1[2], col });
        }
    }

    SubmitTransientLines(worldMtx44, lv);
}

} // namespace ImGuiVisualizers