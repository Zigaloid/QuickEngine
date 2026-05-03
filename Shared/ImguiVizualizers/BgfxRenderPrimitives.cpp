#include "BgfxRenderPrimitives.h"

// bgfx helpers
#include <bgfx/embedded_shader.h>
#include "bgfx_utils.h"

// External
#include <bx/math.h>
#include <vector>
#include <cmath>

// Re-use the pre-compiled debug-draw position+color shaders.
// They transform with u_modelViewProj and pass a_color0 → v_color0.
#include "debugdraw/vs_debugdraw_lines.bin.h"
#include "debugdraw/fs_debugdraw_lines.bin.h"

static const bgfx::EmbeddedShader s_3dviewShaders[] =
{
    BGFX_EMBEDDED_SHADER(vs_debugdraw_lines),
    BGFX_EMBEDDED_SHADER(fs_debugdraw_lines),
    BGFX_EMBEDDED_SHADER_END()
};

namespace ImGuiVisualizers {

bgfx::VertexLayout PosColorVertex::ms_layout;

// ── File-scope geometry helpers ─────────────────────────────────────────

static constexpr int kWireSegments = 16; // segments per wire circle

/// Transform a local-space point by a column-major 4x4 matrix (bgfx convention).
/// out = mtx * (x, y, z, 1)
static void TransformPoint(float out[3], const float p[3], const float m[16])
{
    out[0] = m[0] * p[0] + m[4] * p[1] + m[8]  * p[2] + m[12];
    out[1] = m[1] * p[0] + m[5] * p[1] + m[9]  * p[2] + m[13];
    out[2] = m[2] * p[0] + m[6] * p[1] + m[10] * p[2] + m[14];
}

/// Emit one full wire circle lying in the local XZ plane at y = yOffset.
/// Points are transformed into world space via mtx before storing.
/// Returns the updated vertex index.
static uint32_t EmitWireCircle(PosColorVertex* v, uint32_t idx,
                                const float* mtx,
                                float radius, float yOffset,
                                uint32_t color)
{
    for (int i = 0; i < kWireSegments; ++i)
    {
        const float t0 = bx::kPi2 * static_cast<float>(i)     / kWireSegments;
        const float t1 = bx::kPi2 * static_cast<float>(i + 1) / kWireSegments;

        const float lp0[3] = { radius * bx::cos(t0), yOffset, radius * bx::sin(t0) };
        const float lp1[3] = { radius * bx::cos(t1), yOffset, radius * bx::sin(t1) };

        float wp0[3], wp1[3];
        TransformPoint(wp0, lp0, mtx);
        TransformPoint(wp1, lp1, mtx);

        v[idx++] = { wp0[0], wp0[1], wp0[2], color };
        v[idx++] = { wp1[0], wp1[1], wp1[2], color };
    }
    return idx;
}

/// Emit a hemisphere as two orthogonal semicircles (XY and ZY planes).
/// yOffset is the base of the hemisphere (centre of the sphere cap).
/// yDir = +1.0f for upper cap, -1.0f for lower cap.
/// Returns the updated vertex index.
static uint32_t EmitHemisphereArcs(PosColorVertex* v, uint32_t idx,
                                    const float* mtx,
                                    float radius, float yOffset, float yDir,
                                    uint32_t color)
{
    const int halfSegs = kWireSegments / 2;

    // Two orthogonal arcs: XY plane and ZY plane
    for (int arc = 0; arc < 2; ++arc)
    {
        for (int i = 0; i < halfSegs; ++i)
        {
            const float t0 = bx::kPi * static_cast<float>(i)     / halfSegs;
            const float t1 = bx::kPi * static_cast<float>(i + 1) / halfSegs;

            float lp0[3], lp1[3];
            if (arc == 0) // XY plane
            {
                lp0[0] = radius * bx::cos(t0); lp0[1] = yOffset + yDir * radius * bx::sin(t0); lp0[2] = 0.0f;
                lp1[0] = radius * bx::cos(t1); lp1[1] = yOffset + yDir * radius * bx::sin(t1); lp1[2] = 0.0f;
            }
            else // ZY plane
            {
                lp0[0] = 0.0f; lp0[1] = yOffset + yDir * radius * bx::sin(t0); lp0[2] = radius * bx::cos(t0);
                lp1[0] = 0.0f; lp1[1] = yOffset + yDir * radius * bx::sin(t1); lp1[2] = radius * bx::cos(t1);
            }

            float wp0[3], wp1[3];
            TransformPoint(wp0, lp0, mtx);
            TransformPoint(wp1, lp1, mtx);

            v[idx++] = { wp0[0], wp0[1], wp0[2], color };
            v[idx++] = { wp1[0], wp1[1], wp1[2], color };
        }
    }
    return idx;
}

// ── Initialisation / shutdown ───────────────────────────────────────────

bool BgfxRenderPrimitives::Initialize()
{
    if (m_initialized)
        return true;

    PosColorVertex::Init();

    bgfx::RendererType::Enum type = bgfx::getRendererType();
    m_program = bgfx::createProgram(
        bgfx::createEmbeddedShader(s_3dviewShaders, type, "vs_debugdraw_lines"),
        bgfx::createEmbeddedShader(s_3dviewShaders, type, "fs_debugdraw_lines"),
        true);

    if (!bgfx::isValid(m_program))
        return false;

    CreateCubeBuffers();
    CreateSphereBuffers();

    m_initialized = true;
    return true;
}

void BgfxRenderPrimitives::Shutdown()
{
    if (!m_initialized)
        return;

    if (bgfx::isValid(m_program))     { bgfx::destroy(m_program);     m_program     = BGFX_INVALID_HANDLE; }
    if (bgfx::isValid(m_cubeVbh))     { bgfx::destroy(m_cubeVbh);     m_cubeVbh     = BGFX_INVALID_HANDLE; }
    if (bgfx::isValid(m_cubeIbh))     { bgfx::destroy(m_cubeIbh);     m_cubeIbh     = BGFX_INVALID_HANDLE; }
    if (bgfx::isValid(m_cubeWireIbh)) { bgfx::destroy(m_cubeWireIbh); m_cubeWireIbh = BGFX_INVALID_HANDLE; }
    if (bgfx::isValid(m_sphereVbh))   { bgfx::destroy(m_sphereVbh);   m_sphereVbh   = BGFX_INVALID_HANDLE; }
    if (bgfx::isValid(m_sphereIbh))   { bgfx::destroy(m_sphereIbh);   m_sphereIbh   = BGFX_INVALID_HANDLE; }

    m_initialized = false;
}

// ── Submit helper ───────────────────────────────────────────────────────

void BgfxRenderPrimitives::SubmitTransientLines(bgfx::ViewId viewId,
                                                 const PosColorVertex* verts,
                                                 uint32_t count) const
{
    if (!checkAvailTransientBuffers(count, PosColorVertex::ms_layout, 0))
        return;

    bgfx::TransientVertexBuffer tvb;
    bgfx::allocTransientVertexBuffer(&tvb, count, PosColorVertex::ms_layout);
    bx::memCopy(tvb.data, verts, count * sizeof(PosColorVertex));

    float identity[16];
    bx::mtxIdentity(identity);
    bgfx::setTransform(identity);

    bgfx::setVertexBuffer(0, &tvb, 0, count);
    bgfx::setState(BGFX_STATE_WRITE_RGB
                 | BGFX_STATE_WRITE_A
                 | BGFX_STATE_WRITE_Z
                 | BGFX_STATE_DEPTH_TEST_LESS
                 | BGFX_STATE_PT_LINES
                 | BGFX_STATE_MSAA);

    bgfx::submit(viewId, m_program);
}

// ── Grid ────────────────────────────────────────────────────────────────

void BgfxRenderPrimitives::RenderGrid(bgfx::ViewId viewId, float size,
                                       float step, uint32_t color)
{
    if (!m_initialized)
        return;

    int lineCount = static_cast<int>(size / step);
    uint32_t numVertices = (lineCount * 2 + 1) * 2 * 2; // two axes, two verts per line

    if (!checkAvailTransientBuffers(numVertices, PosColorVertex::ms_layout, 0))
        return;

    bgfx::TransientVertexBuffer tvb;
    bgfx::allocTransientVertexBuffer(&tvb, numVertices, PosColorVertex::ms_layout);

    auto* verts = reinterpret_cast<PosColorVertex*>(tvb.data);
    uint32_t idx = 0;

    for (int i = -lineCount; i <= lineCount; ++i)
    {
        float pos = static_cast<float>(i) * step;

        // Line parallel to Z
        verts[idx++] = { pos, 0.0f, -size, color };
        verts[idx++] = { pos, 0.0f,  size, color };

        // Line parallel to X
        verts[idx++] = { -size, 0.0f, pos, color };
        verts[idx++] = {  size, 0.0f, pos, color };
    }

    float identity[16];
    bx::mtxIdentity(identity);
    bgfx::setTransform(identity);

    bgfx::setVertexBuffer(0, &tvb, 0, idx);
    bgfx::setState(BGFX_STATE_WRITE_RGB
                 | BGFX_STATE_WRITE_A
                 | BGFX_STATE_WRITE_Z
                 | BGFX_STATE_DEPTH_TEST_LESS
                 | BGFX_STATE_PT_LINES
                 | BGFX_STATE_MSAA);

    bgfx::submit(viewId, m_program);
}

// ── Axes ────────────────────────────────────────────────────────────────

void BgfxRenderPrimitives::RenderAxes(bgfx::ViewId viewId, float length)
{
    if (!m_initialized)
        return;

    const uint32_t numVerts = 6;
    if (!checkAvailTransientBuffers(numVerts, PosColorVertex::ms_layout, 0))
        return;

    bgfx::TransientVertexBuffer tvb;
    bgfx::allocTransientVertexBuffer(&tvb, numVerts, PosColorVertex::ms_layout);

    auto* v = reinterpret_cast<PosColorVertex*>(tvb.data);

    // X axis – red (ABGR: 0xff0000ff)
    v[0] = { 0.0f, 0.0f, 0.0f, 0xff0000ff };
    v[1] = { length, 0.0f, 0.0f, 0xff0000ff };

    // Y axis – green (ABGR: 0xff00ff00)
    v[2] = { 0.0f, 0.0f, 0.0f, 0xff00ff00 };
    v[3] = { 0.0f, length, 0.0f, 0xff00ff00 };

    // Z axis – blue (ABGR: 0xffff0000)
    v[4] = { 0.0f, 0.0f, 0.0f, 0xffff0000 };
    v[5] = { 0.0f, 0.0f, length, 0xffff0000 };

    float identity[16];
    bx::mtxIdentity(identity);
    bgfx::setTransform(identity);

    bgfx::setVertexBuffer(0, &tvb);
    bgfx::setState(BGFX_STATE_WRITE_RGB
                 | BGFX_STATE_WRITE_A
                 | BGFX_STATE_WRITE_Z
                 | BGFX_STATE_DEPTH_TEST_LESS
                 | BGFX_STATE_PT_LINES
                 | BGFX_STATE_MSAA);

    bgfx::submit(viewId, m_program);
}

// ── Line ────────────────────────────────────────────────────────────────

void BgfxRenderPrimitives::RenderLine(bgfx::ViewId viewId,
                                       float x0, float y0, float z0,
                                       float x1, float y1, float z1,
                                       uint32_t abgrColor)
{
    if (!m_initialized)
        return;

    if (!checkAvailTransientBuffers(2, PosColorVertex::ms_layout, 0))
        return;

    bgfx::TransientVertexBuffer tvb;
    bgfx::allocTransientVertexBuffer(&tvb, 2, PosColorVertex::ms_layout);
    auto* v = reinterpret_cast<PosColorVertex*>(tvb.data);
    v[0] = { x0, y0, z0, abgrColor };
    v[1] = { x1, y1, z1, abgrColor };

    float identity[16];
    bx::mtxIdentity(identity);
    bgfx::setTransform(identity);

    bgfx::setVertexBuffer(0, &tvb);
    bgfx::setState(BGFX_STATE_WRITE_RGB
                 | BGFX_STATE_WRITE_A
                 | BGFX_STATE_WRITE_Z
                 | BGFX_STATE_DEPTH_TEST_LESS
                 | BGFX_STATE_PT_LINES
                 | BGFX_STATE_MSAA);

    bgfx::submit(viewId, m_program);
}

// ── Cube ────────────────────────────────────────────────────────────────

void BgfxRenderPrimitives::CreateCubeBuffers()
{
    // Unit cube centred at origin (-0.5 to +0.5)
    static const PosColorVertex cubeVerts[] = {
        // Front face
        { -0.5f, -0.5f,  0.5f, 0xffffffff },
        {  0.5f, -0.5f,  0.5f, 0xffffffff },
        {  0.5f,  0.5f,  0.5f, 0xffffffff },
        { -0.5f,  0.5f,  0.5f, 0xffffffff },
        // Back face
        { -0.5f, -0.5f, -0.5f, 0xffffffff },
        {  0.5f, -0.5f, -0.5f, 0xffffffff },
        {  0.5f,  0.5f, -0.5f, 0xffffffff },
        { -0.5f,  0.5f, -0.5f, 0xffffffff },
    };

    static const uint16_t cubeIndices[] = {
        0,1,2, 0,2,3,   // front
        5,4,7, 5,7,6,   // back
        4,0,3, 4,3,7,   // left
        1,5,6, 1,6,2,   // right
        3,2,6, 3,6,7,   // top
        4,5,1, 4,1,0,   // bottom
    };

    static const uint16_t cubeWireIndices[] = {
        0,1, 1,2, 2,3, 3,0,   // front
        4,5, 5,6, 6,7, 7,4,   // back
        0,4, 1,5, 2,6, 3,7,   // connecting
    };

    m_cubeVbh = bgfx::createVertexBuffer(
        bgfx::makeRef(cubeVerts, sizeof(cubeVerts)),
        PosColorVertex::ms_layout);

    m_cubeIbh = bgfx::createIndexBuffer(
        bgfx::makeRef(cubeIndices, sizeof(cubeIndices)));

    m_cubeWireIbh = bgfx::createIndexBuffer(
        bgfx::makeRef(cubeWireIndices, sizeof(cubeWireIndices)));
}

void BgfxRenderPrimitives::RenderCube(bgfx::ViewId viewId,
                                       const float* mtx44,
                                       uint32_t abgrColor)
{
    if (!m_initialized || !bgfx::isValid(m_cubeVbh))
        return;

    bgfx::setTransform(mtx44);
    bgfx::setVertexBuffer(0, m_cubeVbh);
    bgfx::setIndexBuffer(m_cubeIbh);
    bgfx::setState(BGFX_STATE_WRITE_RGB
                 | BGFX_STATE_WRITE_A
                 | BGFX_STATE_WRITE_Z
                 | BGFX_STATE_DEPTH_TEST_LESS
                 | BGFX_STATE_CULL_CW
                 | BGFX_STATE_MSAA);

    bgfx::submit(viewId, m_program);
}

void BgfxRenderPrimitives::RenderWireCube(bgfx::ViewId viewId,
                                           const float* mtx44,
                                           uint32_t abgrColor)
{
    if (!m_initialized || !bgfx::isValid(m_cubeVbh))
        return;

    bgfx::setTransform(mtx44);
    bgfx::setVertexBuffer(0, m_cubeVbh);
    bgfx::setIndexBuffer(m_cubeWireIbh);
    bgfx::setState(BGFX_STATE_WRITE_RGB
                 | BGFX_STATE_WRITE_A
                 | BGFX_STATE_WRITE_Z
                 | BGFX_STATE_DEPTH_TEST_LESS
                 | BGFX_STATE_PT_LINES
                 | BGFX_STATE_MSAA);

    bgfx::submit(viewId, m_program);
}

// ── Sphere ──────────────────────────────────────────────────────────────

void BgfxRenderPrimitives::CreateSphereBuffers(int rings, int segments)
{
    std::vector<PosColorVertex> verts;
    std::vector<uint16_t> indices;

    for (int r = 0; r <= rings; ++r)
    {
        float phi = bx::kPi * static_cast<float>(r) / static_cast<float>(rings);
        float y   = bx::cos(phi);
        float rr  = bx::sin(phi);

        for (int s = 0; s <= segments; ++s)
        {
            float theta = 2.0f * bx::kPi * static_cast<float>(s) / static_cast<float>(segments);
            float x = rr * bx::cos(theta);
            float z = rr * bx::sin(theta);
            verts.push_back({ x * 0.5f, y * 0.5f, z * 0.5f, 0xffffffff });
        }
    }

    for (int r = 0; r < rings; ++r)
    {
        for (int s = 0; s < segments; ++s)
        {
            uint16_t a = static_cast<uint16_t>(r * (segments + 1) + s);
            uint16_t b = static_cast<uint16_t>(a + segments + 1);
            indices.push_back(a);
            indices.push_back(b);
            indices.push_back(static_cast<uint16_t>(a + 1));

            indices.push_back(static_cast<uint16_t>(a + 1));
            indices.push_back(b);
            indices.push_back(static_cast<uint16_t>(b + 1));
        }
    }

    m_sphereVbh = bgfx::createVertexBuffer(
        bgfx::copy(verts.data(), static_cast<uint32_t>(verts.size() * sizeof(PosColorVertex))),
        PosColorVertex::ms_layout);

    m_sphereIbh = bgfx::createIndexBuffer(
        bgfx::copy(indices.data(), static_cast<uint32_t>(indices.size() * sizeof(uint16_t))));
}

void BgfxRenderPrimitives::RenderSphere(bgfx::ViewId viewId,
                                         const float* mtx44,
                                         uint32_t abgrColor)
{
    if (!m_initialized || !bgfx::isValid(m_sphereVbh))
        return;

    bgfx::setTransform(mtx44);
    bgfx::setVertexBuffer(0, m_sphereVbh);
    bgfx::setIndexBuffer(m_sphereIbh);
    bgfx::setState(BGFX_STATE_WRITE_RGB
                 | BGFX_STATE_WRITE_A
                 | BGFX_STATE_WRITE_Z
                 | BGFX_STATE_DEPTH_TEST_LESS
                 | BGFX_STATE_CULL_CW
                 | BGFX_STATE_MSAA);

    bgfx::submit(viewId, m_program);
}

// ── Shape-aware wireframe helpers ───────────────────────────────────────

void BgfxRenderPrimitives::RenderWireBox(bgfx::ViewId viewId,
                                          const float* worldMtx,
                                          float halfX, float halfY, float halfZ,
                                          uint32_t color)
{
    if (!m_initialized)
        return;

    // Scale the world matrix basis vectors by full extents (unit cube has half-size 0.5)
    float scaled[16];
    bx::memCopy(scaled, worldMtx, sizeof(scaled));
    const float sx = halfX * 2.0f, sy = halfY * 2.0f, sz = halfZ * 2.0f;
    scaled[0] *= sx;  scaled[1] *= sx;  scaled[2]  *= sx;
    scaled[4] *= sy;  scaled[5] *= sy;  scaled[6]  *= sy;
    scaled[8] *= sz;  scaled[9] *= sz;  scaled[10] *= sz;

    RenderWireCube(viewId, scaled, color);
}

void BgfxRenderPrimitives::RenderWireSphere(bgfx::ViewId viewId,
                                             const float* worldMtx,
                                             float radius,
                                             uint32_t color)
{
    if (!m_initialized)
        return;

    // 3 great circles: XZ equator, XY, ZY — each kWireSegments line segments
    constexpr uint32_t kNumVerts = kWireSegments * 2 * 3;
    PosColorVertex verts[kNumVerts];
    uint32_t idx = 0;

    idx = EmitWireCircle(verts, idx, worldMtx, radius, 0.0f, color); // XZ equator

    for (int i = 0; i < kWireSegments; ++i)
    {
        const float t0 = bx::kPi2 * static_cast<float>(i)     / kWireSegments;
        const float t1 = bx::kPi2 * static_cast<float>(i + 1) / kWireSegments;

        // XY plane
        const float xy0[3] = { radius * bx::cos(t0), radius * bx::sin(t0), 0.0f };
        const float xy1[3] = { radius * bx::cos(t1), radius * bx::sin(t1), 0.0f };
        float wxy0[3], wxy1[3];
        TransformPoint(wxy0, xy0, worldMtx);
        TransformPoint(wxy1, xy1, worldMtx);
        verts[idx++] = { wxy0[0], wxy0[1], wxy0[2], color };
        verts[idx++] = { wxy1[0], wxy1[1], wxy1[2], color };

        // ZY plane
        const float zy0[3] = { 0.0f, radius * bx::sin(t0), radius * bx::cos(t0) };
        const float zy1[3] = { 0.0f, radius * bx::sin(t1), radius * bx::cos(t1) };
        float wzy0[3], wzy1[3];
        TransformPoint(wzy0, zy0, worldMtx);
        TransformPoint(wzy1, zy1, worldMtx);
        verts[idx++] = { wzy0[0], wzy0[1], wzy0[2], color };
        verts[idx++] = { wzy1[0], wzy1[1], wzy1[2], color };
    }

    SubmitTransientLines(viewId, verts, idx);
}

void BgfxRenderPrimitives::RenderWireCylinder(bgfx::ViewId viewId,
                                               const float* worldMtx,
                                               float radius, float halfHeight,
                                               uint32_t color)
{
    if (!m_initialized)
        return;

    // Top circle + bottom circle + kWireSegments/2 vertical lines
    constexpr int       kVLines   = kWireSegments / 2;
    constexpr uint32_t kNumVerts  = kWireSegments * 2 * 2   // two circles
                                  + kVLines * 2;            // vertical lines
    PosColorVertex verts[kNumVerts];
    uint32_t idx = 0;

    idx = EmitWireCircle(verts, idx, worldMtx, radius,  halfHeight, color);
    idx = EmitWireCircle(verts, idx, worldMtx, radius, -halfHeight, color);

    for (int i = 0; i < kVLines; ++i)
    {
        const float t = bx::kPi2 * static_cast<float>(i) / kVLines;
        const float lTop[3] = { radius * bx::cos(t),  halfHeight, radius * bx::sin(t) };
        const float lBot[3] = { radius * bx::cos(t), -halfHeight, radius * bx::sin(t) };
        float wTop[3], wBot[3];
        TransformPoint(wTop, lTop, worldMtx);
        TransformPoint(wBot, lBot, worldMtx);
        verts[idx++] = { wTop[0], wTop[1], wTop[2], color };
        verts[idx++] = { wBot[0], wBot[1], wBot[2], color };
    }

    SubmitTransientLines(viewId, verts, idx);
}

void BgfxRenderPrimitives::RenderWireCapsule(bgfx::ViewId viewId,
                                              const float* worldMtx,
                                              float radius, float halfCylinderHeight,
                                              uint32_t color)
{
    if (!m_initialized)
        return;

    // Top circle + bottom circle + vertical lines + upper hemisphere arcs + lower hemisphere arcs
    constexpr int       kVLines   = kWireSegments / 2;
    constexpr int       kArcSegs  = kWireSegments / 2;
    constexpr uint32_t kNumVerts  = kWireSegments * 2 * 2       // top + bottom circles
                                  + kVLines * 2                  // vertical lines
                                  + 2 * 2 * kArcSegs * 2;       // 2 caps × 2 arcs × kArcSegs segments × 2 verts
    PosColorVertex verts[kNumVerts];
    uint32_t idx = 0;

    idx = EmitWireCircle(verts, idx, worldMtx, radius,  halfCylinderHeight, color);
    idx = EmitWireCircle(verts, idx, worldMtx, radius, -halfCylinderHeight, color);

    for (int i = 0; i < kVLines; ++i)
    {
        const float t = bx::kPi2 * static_cast<float>(i) / kVLines;
        const float lTop[3] = { radius * bx::cos(t),  halfCylinderHeight, radius * bx::sin(t) };
        const float lBot[3] = { radius * bx::cos(t), -halfCylinderHeight, radius * bx::sin(t) };
        float wTop[3], wBot[3];
        TransformPoint(wTop, lTop, worldMtx);
        TransformPoint(wBot, lBot, worldMtx);
        verts[idx++] = { wTop[0], wTop[1], wTop[2], color };
        verts[idx++] = { wBot[0], wBot[1], wBot[2], color };
    }

    // Upper hemisphere arcs (above +halfCylinderHeight)
    idx = EmitHemisphereArcs(verts, idx, worldMtx, radius,  halfCylinderHeight, +1.0f, color);
    // Lower hemisphere arcs (below -halfCylinderHeight)
    idx = EmitHemisphereArcs(verts, idx, worldMtx, radius, -halfCylinderHeight, -1.0f, color);

    SubmitTransientLines(viewId, verts, idx);
}

} // namespace ImGuiVisualizers
