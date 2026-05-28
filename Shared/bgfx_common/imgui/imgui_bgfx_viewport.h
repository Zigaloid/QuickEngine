/*
 * BGFX renderer backend for ImGui multi-viewport (secondary OS windows).
 * Pairs with ImGui_ImplWin32 as the platform backend.
 */

#pragma once

#include <bgfx/bgfx.h>
#include <imgui-docking/imgui.h>

// Register / unregister the BGFX renderer callbacks for secondary viewports.
// _firstViewId is the start of a contiguous block of bgfx ViewIds reserved
// for secondary ImGui windows. The block covers [_firstViewId, 254].
// The main ImGui view uses 255 by default so the range 200-254 is safe.
void imguiBgfxViewportInit(bgfx::ViewId _firstViewId = 200);
void imguiBgfxViewportShutdown();

// Render an ImDrawData payload to an explicit bgfx ViewId.
// Defined in imgui.cpp (shares OcornutImguiContext state) and called
// from imgui_bgfx_viewport.cpp for each secondary viewport.
void imguiBgfxRenderDrawData(ImDrawData* _drawData, bgfx::ViewId _viewId);
