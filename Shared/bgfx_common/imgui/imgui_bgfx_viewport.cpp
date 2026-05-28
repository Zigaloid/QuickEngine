/*
 * BGFX renderer backend for ImGui multi-viewport (secondary OS windows).
 *
 * Each secondary ImGui viewport maps to:
 *   - A native OS window created/owned by ImGui_ImplWin32.
 *   - A bgfx FrameBuffer targeting that window's HWND.
 *   - A unique bgfx ViewId drawn to every frame.
 *
 * bgfx::frame() in the main loop flushes all views (including secondary ones)
 * in a single call, so Renderer_SwapBuffers is intentionally a no-op.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <bgfx/bgfx.h>
#include <imgui-docking/imgui.h>

#include "imgui_bgfx_viewport.h"

// ---------------------------------------------------------------------------
// ViewId pool
// ---------------------------------------------------------------------------

static bgfx::ViewId s_viewIdFirst = 200;

struct ViewIdPool
{
    static constexpr int kMax = 55; // 200-254 inclusive

    bool    used[kMax] = {};

    bgfx::ViewId alloc()
    {
        for (int i = 0; i < kMax; ++i)
        {
            if (!used[i])
            {
                used[i] = true;
                return static_cast<bgfx::ViewId>(s_viewIdFirst + i);
            }
        }
        IM_ASSERT(false && "ImGui BGFX viewport: ran out of ViewIds");
        return static_cast<bgfx::ViewId>(s_viewIdFirst);
    }

    void free(bgfx::ViewId id)
    {
        int idx = static_cast<int>(id) - static_cast<int>(s_viewIdFirst);
        if (idx >= 0 && idx < kMax)
            used[idx] = false;
    }
};

static ViewIdPool s_viewIdPool;

// ---------------------------------------------------------------------------
// Per-viewport renderer data
// ---------------------------------------------------------------------------

struct BgfxViewportData
{
    bgfx::FrameBufferHandle frameBuffer = BGFX_INVALID_HANDLE;
    bgfx::ViewId            viewId      = 0;
    uint16_t                width       = 0;
    uint16_t                height      = 0;
};

// ---------------------------------------------------------------------------
// Renderer callbacks
// ---------------------------------------------------------------------------

static void Renderer_CreateWindow(ImGuiViewport* vp)
{
    auto* data    = IM_NEW(BgfxViewportData);
    data->viewId  = s_viewIdPool.alloc();
    data->width   = static_cast<uint16_t>(vp->Size.x);
    data->height  = static_cast<uint16_t>(vp->Size.y);

    HWND hwnd = static_cast<HWND>(vp->PlatformHandleRaw);
    IM_ASSERT(hwnd != nullptr);

    data->frameBuffer = bgfx::createFrameBuffer(hwnd, data->width, data->height);
    bgfx::setViewFrameBuffer(data->viewId, data->frameBuffer);
    bgfx::setViewClear(data->viewId, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x000000ff, 1.0f, 0);

    vp->RendererUserData = data;
}

static void Renderer_DestroyWindow(ImGuiViewport* vp)
{
    if (auto* data = static_cast<BgfxViewportData*>(vp->RendererUserData))
    {
        bgfx::destroy(data->frameBuffer);
        s_viewIdPool.free(data->viewId);
        IM_DELETE(data);
        vp->RendererUserData = nullptr;
    }
}

static void Renderer_SetWindowSize(ImGuiViewport* vp, ImVec2 size)
{
    auto* data = static_cast<BgfxViewportData*>(vp->RendererUserData);
    if (data == nullptr)
        return;

    data->width  = static_cast<uint16_t>(size.x);
    data->height = static_cast<uint16_t>(size.y);

    // Recreate the framebuffer at the new size.
    bgfx::destroy(data->frameBuffer);

    HWND hwnd = static_cast<HWND>(vp->PlatformHandleRaw);
    data->frameBuffer = bgfx::createFrameBuffer(hwnd, data->width, data->height);
    bgfx::setViewFrameBuffer(data->viewId, data->frameBuffer);
}

static void Renderer_RenderWindow(ImGuiViewport* vp, void* /*renderArg*/)
{
    auto* data = static_cast<BgfxViewportData*>(vp->RendererUserData);
    if (data == nullptr)
        return;

    imguiBgfxRenderDrawData(vp->DrawData, data->viewId);
}

static void Renderer_SwapBuffers(ImGuiViewport* /*vp*/, void* /*renderArg*/)
{
    // bgfx::frame() in the main loop handles all swaps — nothing to do here.
}

// ---------------------------------------------------------------------------
// Public init / shutdown
// ---------------------------------------------------------------------------

void imguiBgfxViewportInit(bgfx::ViewId _firstViewId)
{
    s_viewIdFirst = _firstViewId;

    ImGuiPlatformIO& pio         = ImGui::GetPlatformIO();
    pio.Renderer_CreateWindow    = Renderer_CreateWindow;
    pio.Renderer_DestroyWindow   = Renderer_DestroyWindow;
    pio.Renderer_SetWindowSize   = Renderer_SetWindowSize;
    pio.Renderer_RenderWindow    = Renderer_RenderWindow;
    pio.Renderer_SwapBuffers     = Renderer_SwapBuffers;
}

void imguiBgfxViewportShutdown()
{
    ImGuiPlatformIO& pio         = ImGui::GetPlatformIO();
    pio.Renderer_CreateWindow    = nullptr;
    pio.Renderer_DestroyWindow   = nullptr;
    pio.Renderer_SetWindowSize   = nullptr;
    pio.Renderer_RenderWindow    = nullptr;
    pio.Renderer_SwapBuffers     = nullptr;
}
