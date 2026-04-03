#include "BgfxRenderPrimitives.h"

#include <bgfx/embedded_shader.h>
#include "bgfx_utils.h"
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

    for (int i = -lineCount; i <= lineCount; ++i) {
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

    for (int r = 0; r <= rings; ++r) {
        float phi = bx::kPi * static_cast<float>(r) / static_cast<float>(rings);
        float y   = bx::cos(phi);
        float rr  = bx::sin(phi);

        for (int s = 0; s <= segments; ++s) {
            float theta = 2.0f * bx::kPi * static_cast<float>(s) / static_cast<float>(segments);
            float x = rr * bx::cos(theta);
            float z = rr * bx::sin(theta);
            verts.push_back({ x * 0.5f, y * 0.5f, z * 0.5f, 0xffffffff });
        }
    }

    for (int r = 0; r < rings; ++r) {
        for (int s = 0; s < segments; ++s) {
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

} // namespace ImGuiVisualizers
