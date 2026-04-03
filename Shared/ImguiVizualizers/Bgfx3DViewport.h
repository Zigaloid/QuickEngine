#pragma once

#include <bgfx/bgfx.h>
#include <cstdint>
#include "BgfxViewIdAllocator.h"

namespace ImGuiVisualizers {

/**
 * @brief Manages a BGFX offscreen framebuffer (color + depth) and a dedicated
 *        view ID so that 3D content can be rendered to a texture and then
 *        displayed inside an ImGui window via ImGui::Image().
 *
 * Multiple instances can coexist – each one allocates its own view ID from
 * BgfxViewIdAllocator.
 */
class Bgfx3DViewport {
public:
    Bgfx3DViewport() = default;
    ~Bgfx3DViewport() { Shutdown(); }

    Bgfx3DViewport(const Bgfx3DViewport&) = delete;
    Bgfx3DViewport& operator=(const Bgfx3DViewport&) = delete;

    /**
     * @brief Allocate view ID, create initial framebuffer.
     * @return true on success.
     */
    bool Initialize(uint16_t width, uint16_t height);

    /**
     * @brief Destroy framebuffer and release view ID.
     */
    void Shutdown();

    /**
     * @brief Recreate the framebuffer if the size changed.
     *        Does nothing if the size is unchanged.
     */
    void Resize(uint16_t width, uint16_t height);

    /**
     * @brief Prepare the view for a new frame.
     *        Sets view rect, clear colour, framebuffer, and transforms.
     * @param viewMtx   Row-major 4x4 view matrix (float[16]).
     * @param projMtx   Row-major 4x4 projection matrix (float[16]).
     * @param clearColor RGBA8 packed clear colour (default dark grey).
     */
    void BeginFrame(const float* viewMtx, const float* projMtx,
                    uint32_t clearColor = 0x303030ff);

    // ── Accessors ───────────────────────────────────────────────────────

    bgfx::ViewId          GetViewId()       const { return m_viewId; }
    bgfx::TextureHandle   GetColorTexture() const { return m_colorTex; }
    uint16_t              GetWidth()        const { return m_width; }
    uint16_t              GetHeight()       const { return m_height; }
    bool                  IsValid()         const { return m_initialized; }

private:
    void CreateFramebuffer();
    void DestroyFramebuffer();

    bgfx::ViewId            m_viewId      = BgfxViewIdAllocator::kInvalidViewId;
    bgfx::FrameBufferHandle m_fbh         = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle     m_colorTex    = BGFX_INVALID_HANDLE;
    uint16_t                m_width       = 0;
    uint16_t                m_height      = 0;
    bool                    m_initialized = false;
};

} // namespace ImGuiVisualizers
