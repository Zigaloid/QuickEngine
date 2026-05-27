#include "ImGui3DViewVisualizer.h"

// Project headers
#include "CoreSystem/CoreSystem.h"
#include "MeshComponent.h"
#include "EntityComponent.h"

// Standard headers
#include <algorithm>
#include <string_view>

namespace ImGuiVisualizers {

// ── Construction ────────────────────────────────────────────────────────

ImGui3DViewVisualizer::ImGui3DViewVisualizer(const char* name,
                                             ImGuiKey shortcut,
                                             const char* category)
    : m_name(name ? name : "3D View")
    , m_shortcutKey(shortcut)
    , m_category(category ? category : "")
{
}

// ── IImGuiVisualizer lifecycle ──────────────────────────────────────────

void ImGui3DViewVisualizer::Initialize()
{
    Rendering::BgfxRenderPrimitives::Instance().Initialize();
}

void ImGui3DViewVisualizer::Shutdown()
{
    m_viewport.Shutdown();
    Rendering::BgfxRenderPrimitives::Instance().Shutdown();
}

void ImGui3DViewVisualizer::Update(float deltaTime)
{
}

// ── Public mesh-loading API ─────────────────────────────────────────────

bool ImGui3DViewVisualizer::Render(bool* isOpen)
{
    if (!ImGui::Begin(m_name.c_str(), isOpen, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) 
    {
        ImGui::End();
        return false;
    }

    // Render toolbar and viewport content into the current window
    ImVec2 contentSize = ImGui::GetContentRegionAvail();
    if (contentSize.x < 1.0f) contentSize.x = 1.0f;
    if (contentSize.y < 1.0f) contentSize.y = 1.0f;

    RenderContent(contentSize);

    m_lastContentSize = contentSize;
    ImGui::End();
    return true;
}

void ImGui3DViewVisualizer::RenderContent(const ImVec2& contentSize)
{
    // Render toolbar (uses ImGui calls)
    RenderToolbar();

    // After rendering the toolbar, the remaining content region is the viewport area.
    // Use the remaining region to size the offscreen framebuffer so we exclude toolbar/menu height.
    ImVec2 remaining = ImGui::GetContentRegionAvail();

    // Defensive: clamp to the originally provided contentSize (in case parent's content constraints differ)
    ImVec2 size;
    size.x = std::min(contentSize.x, remaining.x);
    size.y = std::min(contentSize.y, remaining.y);

    // Ensure non-zero size
    if (size.x < 1.0f) size.x = 1.0f;
    if (size.y < 1.0f) size.y = 1.0f;

    uint16_t w = static_cast<uint16_t>(size.x);
    uint16_t h = static_cast<uint16_t>(size.y);

    // Lazy-init or resize
    if (!m_viewport.IsValid()) 
    {
        m_viewport.Initialize(w, h);
    } 
    else if (w != m_viewport.GetWidth() || h != m_viewport.GetHeight()) 
    {
        m_viewport.Resize(w, h);
    }

    if (m_viewport.IsValid()) {
        float aspect = size.x / size.y;

        float viewMtx[16];
        float projMtx[16];
        m_camera.GetViewMatrix(viewMtx);
        m_camera.GetProjectionMatrix(projMtx, aspect);

        m_viewport.BeginFrame(viewMtx, projMtx);

        bgfx::ViewId viewId = m_viewport.GetViewId();

        if (m_showGrid) 
        {
            Rendering::BgfxRenderPrimitives::Instance().RenderGrid(viewId, m_gridConfig.size,
                                    m_gridConfig.step, m_gridConfig.color);
        }


        // User-supplied draw calls
        if (m_renderCallback) {
            m_renderCallback(viewId, Rendering::BgfxRenderPrimitives::Instance());
        }

        // Display the offscreen texture and create an invisible interactive item
        ImVec2 cursorPos = ImGui::GetCursorScreenPos();
        ImGui::Image(m_viewport.GetColorTexture(), size);

        m_viewportMin = ImGui::GetItemRectMin();
        m_viewportSize = ImGui::GetItemRectSize();

        // place an invisible button exactly over the image so ImGui captures mouse input
        ImGui::SetCursorScreenPos(cursorPos);
        ImGui::InvisibleButton("##viewport", size);
        bool hovered = ImGui::IsItemHovered();
        bool active  = ImGui::IsItemActive();

        // Handle mouse input over the image (now using ImGui's item state)
        HandleInput(cursorPos, size, hovered, active);
    }
}

// ── Toolbar ─────────────────────────────────────────────────────────────

void ImGui3DViewVisualizer::RenderToolbar()
{
    ImGui::Checkbox("Grid", &m_showGrid);
    ImGui::SameLine();
    ImGui::Checkbox("Axes", &m_showAxes);
    ImGui::SameLine();

    if (ImGui::Button("Reset Camera")) 
    {
        m_camera.Reset();
    }

    ImGui::SameLine();
    float fov = m_camera.GetFov();
    ImGui::SetNextItemWidth(80.0f);
    if (ImGui::SliderFloat("FOV", &fov, 15.0f, 120.0f, "%.0f")) 
    {
        m_camera.SetFov(fov);
    }

    ImGui::Separator();
}

// ── Mouse input ─────────────────────────────────────────────────────────

void ImGui3DViewVisualizer::HandleInput(const ImVec2& regionMin,
                                         const ImVec2& regionSize,
                                         bool itemHovered,
                                         bool itemActive)
{
    // If the widget is neither hovered nor active, clear drag state and return
    if (!itemHovered && !itemActive) 
    {
        m_orbiting = false;
        m_panning  = false;
        return;
    }

    ImGuiIO& io = ImGui::GetIO();

    // Camera movement is only applied when the Alt key is held
    if (io.KeyAlt)
    {

        // Orbit – left mouse drag (only when our invisible item is active)
        if ( itemActive && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            m_orbiting = true;
            ImVec2 delta = io.MouseDelta;
            m_camera.Orbit(-delta.x * 0.005f, -delta.y * 0.005f);
        }
        else
        {
            m_orbiting = false;
        }

        // Pan – middle mouse drag (only when our invisible item is active)
        if ((itemHovered || itemActive) && ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
        {
            m_panning = true;
            ImVec2 delta = io.MouseDelta;
            float panSpeed = m_camera.GetDistance() * 0.002f;
            m_camera.Pan(-delta.x * panSpeed, delta.y * panSpeed);
        }
        else
        {
            m_panning = false;
        }

        // Zoom – scroll wheel, only when hovered (so wheel doesn't scroll parent)
        if ( itemHovered && io.MouseWheel != 0.0f)
        {
            m_camera.Zoom(io.MouseWheel * m_camera.GetDistance() * 0.1f);

            // Consume the wheel input so ImGui / parent windows won't also scroll.
            // Clearing both vertical and horizontal wheel values is defensive.
            io.MouseWheel = 0.0f;
            io.MouseWheelH = 0.0f;
        }
    }
}

} // namespace ImGuiVisualizers
