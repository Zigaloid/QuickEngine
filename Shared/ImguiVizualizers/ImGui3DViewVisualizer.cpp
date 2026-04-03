#include "ImGui3DViewVisualizer.h"

#include "EntityInstance.h"
#include "CoreSystem/CoreSystem.h"

namespace ImGuiVisualizers {

// ── Construction ────────────────────────────────────────────────────────

ImGui3DViewVisualizer::ImGui3DViewVisualizer(const char* name,
                                             const char* shortcut,
                                             const char* category)
    : m_name(name ? name : "3D View")
    , m_shortcut(shortcut ? shortcut : "")
    , m_category(category ? category : "")
{
}

// ── IImGuiVisualizer lifecycle ──────────────────────────────────────────
CMeshComponent* g_meshComp = nullptr;

void ImGui3DViewVisualizer::Initialize()
{
    m_primitives.Initialize();
    // Viewport is lazily created on first Render() when we know the size.

    auto componentManager = Core::CoreSystem::GetComponentManager();
    g_meshComp = componentManager->CreateComponent<CMeshComponent>();
    g_meshComp->SetMeshPath("./assets/meshes/bunny.bin");
    g_meshComp->SetShaderPath("mesh");
    g_meshComp->Initialize();   
}

void ImGui3DViewVisualizer::Shutdown()
{
    m_viewport.Shutdown();
    m_primitives.Shutdown();
}

void ImGui3DViewVisualizer::Update(float deltaTime)
{
}

// ── Render ──────────────────────────────────────────────────────────────

bool ImGui3DViewVisualizer::Render(bool* isOpen)
{
    if (!ImGui::Begin(m_name.c_str(), isOpen, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        ImGui::End();
        return false;
    }

    RenderToolbar();

    ImVec2 contentSize = ImGui::GetContentRegionAvail();
    if (contentSize.x < 1.0f) contentSize.x = 1.0f;
    if (contentSize.y < 1.0f) contentSize.y = 1.0f;

    uint16_t w = static_cast<uint16_t>(contentSize.x);
    uint16_t h = static_cast<uint16_t>(contentSize.y);

    // Lazy-init or resize
    if (!m_viewport.IsValid()) {
        m_viewport.Initialize(w, h);
    } else if (w != m_viewport.GetWidth() || h != m_viewport.GetHeight()) {
        m_viewport.Resize(w, h);
    }

    if (m_viewport.IsValid()) {
        float aspect = contentSize.x / contentSize.y;

        float viewMtx[16];
        float projMtx[16];
        m_camera.GetViewMatrix(viewMtx);
        m_camera.GetProjectionMatrix(projMtx, aspect);

        m_viewport.BeginFrame(viewMtx, projMtx);

        bgfx::ViewId viewId = m_viewport.GetViewId();

        if (m_showGrid) {
            m_primitives.RenderGrid(viewId, m_gridConfig.size,
                                    m_gridConfig.step, m_gridConfig.color);
        }
        if (m_showAxes) {
            m_primitives.RenderAxes(viewId);
        }

        // User-supplied draw calls
        if (m_renderCallback) {
            m_renderCallback(viewId, m_primitives);
        }

        if (g_meshComp->IsReadyToRender())
        {
            float mtx[16];
            bx::mtxRotateXY(mtx, 0.0f, 0.0f);
            g_meshComp->Render(viewId, mtx);
        }

        // Display the offscreen texture
        ImVec2 cursorPos = ImGui::GetCursorScreenPos();
        ImGui::Image(m_viewport.GetColorTexture(), contentSize);

        // Handle mouse input over the image
        HandleInput(cursorPos, contentSize);
    }

    m_lastContentSize = contentSize;
    ImGui::End();
    return true;
}

// ── Toolbar ─────────────────────────────────────────────────────────────

void ImGui3DViewVisualizer::RenderToolbar()
{
    ImGui::Checkbox("Grid", &m_showGrid);
    ImGui::SameLine();
    ImGui::Checkbox("Axes", &m_showAxes);
    ImGui::SameLine();

    if (ImGui::Button("Reset Camera")) {
        m_camera.Reset();
    }

    ImGui::SameLine();
    float fov = m_camera.GetFov();
    ImGui::SetNextItemWidth(80.0f);
    if (ImGui::SliderFloat("FOV", &fov, 15.0f, 120.0f, "%.0f")) {
        m_camera.SetFov(fov);
    }

    ImGui::Separator();
}

// ── Mouse input ─────────────────────────────────────────────────────────

void ImGui3DViewVisualizer::HandleInput(const ImVec2& regionMin,
                                         const ImVec2& regionSize)
{
    // Determine if the mouse is over the viewport image
    ImVec2 mousePos = ImGui::GetMousePos();
    bool hovered = (mousePos.x >= regionMin.x &&
                    mousePos.x <= regionMin.x + regionSize.x &&
                    mousePos.y >= regionMin.y &&
                    mousePos.y <= regionMin.y + regionSize.y);

    if (!hovered) {
        m_orbiting = false;
        m_panning  = false;
        return;
    }

    ImGuiIO& io = ImGui::GetIO();

    // Orbit – left mouse drag
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        m_orbiting = true;
        ImVec2 delta = io.MouseDelta;
        m_camera.Orbit(-delta.x * 0.005f, -delta.y * 0.005f);
    } else {
        m_orbiting = false;
    }

    // Pan – middle mouse drag
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
        m_panning = true;
        ImVec2 delta = io.MouseDelta;
        float panSpeed = m_camera.GetDistance() * 0.002f;
        m_camera.Pan(-delta.x * panSpeed, delta.y * panSpeed);
    } else {
        m_panning = false;
    }

    // Zoom – scroll wheel
    if (io.MouseWheel != 0.0f) {
        m_camera.Zoom(io.MouseWheel * m_camera.GetDistance() * 0.1f);
    }
}

} // namespace ImGuiVisualizers
