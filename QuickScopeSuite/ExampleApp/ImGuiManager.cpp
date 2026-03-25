#include "ImGuiManager.h"
#include "DebugChannel/DebugChannel.h"
#include "Profiler/Profiler.h"
// ImGui includes
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

// GLFW for window management
#include <GLFW/glfw3.h>

// Standard library
#include <algorithm>

// External debug channel - adjust this include based on your debug system
extern DebugChannels::CDebugChannel AppDebug;

ImGuiManager::ImGuiManager()
{
}

ImGuiManager::~ImGuiManager()
{
    Shutdown();
}

bool ImGuiManager::Initialize(GLFWwindow* window, const char* glslVersion)
{
    if (m_initialized) {
        AppDebug.printf("ImGuiManager already initialized\n");
        return true;
    }
    
    if (!window) {
        AppDebug.printf("Cannot initialize ImGuiManager: Window is null\n");
        return false;
    }
    
    m_glslVersion = glslVersion;
    
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    
    // Configure ImGui IO
    configureIO();
    
    // Setup style
    setupImGuiStyle();
    
    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    if (!ImGui_ImplOpenGL3_Init(m_glslVersion)) {
        AppDebug.printf("Failed to initialize ImGui OpenGL3 backend\n");
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        return false;
    }
    
    m_initialized = true;
    AppDebug.printf("ImGui initialized successfully with GLSL version: %s\n", m_glslVersion);
    
    return true;
}

void ImGuiManager::Shutdown()
{
    if (!m_initialized) return;
    
    // Cleanup ImGui
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    m_initialized = false;
    AppDebug.printf("ImGui shutdown complete\n");
}

void ImGuiManager::BeginFrame()
{
    if (!m_initialized) return;
    
    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiManager::EndFrame()
{
    if (!m_initialized) return;
    
    // Rendering
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    
    // Handle multiple viewports if enabled
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        GLFWwindow* backup_current_context = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backup_current_context);
    }
}

void ImGuiManager::RenderFrame(const DockspaceRenderCallback& renderCallback)
{
    DECLARE_FUNC_LOW();
    if (!m_initialized) return;
    
    BeginFrame();
    
    // Setup dockspace
    SetupDockspace();
    
    // Execute user rendering callback if provided
    if (renderCallback) {
        renderCallback();
    }
    
    EndFrame();
}

void ImGuiManager::SetupDockspace()
{
    if (!m_initialized) return;
    
    // Create dockspace - simplified version since we're rendering over viewports
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;
    window_flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    window_flags |= ImGuiWindowFlags_NoBackground; // Make background transparent over viewport

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("DockSpace", nullptr, window_flags);
    ImGui::PopStyleVar(3);

    ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");

    // Try using PassthruCentralNode to let viewport content show through
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
    
    // Note: The caller should handle menu bar and windows rendering
    // ImGui::End() will be called by the caller after rendering content
}

void ImGuiManager::HandleDPIScaleChange(float xscale, float yscale)
{
    float dpiScale = std::max(xscale, yscale);
    dpiScale = std::clamp(dpiScale, 0.5f, 4.0f);

    AppDebug.printf("DPI scale changed: %.2f (x=%.2f, y=%.2f)\n", dpiScale, xscale, yscale);

    if (!m_initialized) return;

    // Update ImGui scaling
    ImGuiIO& io = ImGui::GetIO();
    ImGuiStyle& style = ImGui::GetStyle();

    // Reset style to default and re-apply scaling
    ImGui::StyleColorsDark();
    style.ScaleAllSizes(dpiScale);
    io.FontGlobalScale = dpiScale;

    // Note: For best quality, you might want to reload fonts at the new scale
    // This would require rebuilding the font atlas
}

void ImGuiManager::UpdateDPIScaling(GLFWwindow* window)
{
    if (!m_initialized || !window) return;

    GLFWmonitor* monitor = findWindowMonitor(window);
    
    // Get the DPI scale for the current monitor
    float xscale, yscale;
    glfwGetMonitorContentScale(monitor, &xscale, &yscale);

    HandleDPIScaleChange(xscale, yscale);
}

void ImGuiManager::SetEnableDocking(bool enable)
{
    m_enableDocking = enable;
    if (m_initialized) {
        ImGuiIO& io = ImGui::GetIO();
        if (enable) {
            io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        } else {
            io.ConfigFlags &= ~ImGuiConfigFlags_DockingEnable;
        }
    }
}

void ImGuiManager::SetEnableViewports(bool enable)
{
    m_enableViewports = enable;
    if (m_initialized) {
        ImGuiIO& io = ImGui::GetIO();
        if (enable) {
            io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        } else {
            io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;
        }
        setupImGuiStyle(); // Re-setup style for viewport changes
    }
}

void ImGuiManager::SetEnableKeyboardNav(bool enable)
{
    m_enableKeyboardNav = enable;
    if (m_initialized) {
        ImGuiIO& io = ImGui::GetIO();
        if (enable) {
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        } else {
            io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
        }
    }
}

void ImGuiManager::SetEnableGamepadNav(bool enable)
{
    m_enableGamepadNav = enable;
    if (m_initialized) {
        ImGuiIO& io = ImGui::GetIO();
        if (enable) {
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
        } else {
            io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;
        }
    }
}

void ImGuiManager::configureIO()
{
    ImGuiIO& io = ImGui::GetIO();
    
    // Configure flags based on settings
    if (m_enableKeyboardNav) {
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    }
    if (m_enableGamepadNav) {
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    }
    if (m_enableDocking) {
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    }
    if (m_enableViewports) {
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    }
}

void ImGuiManager::setupImGuiStyle()
{
    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    if (m_enableViewports) {
        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            style.WindowRounding = 0.0f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }
    }
}

GLFWmonitor* ImGuiManager::findWindowMonitor(GLFWwindow* window)
{
    // Get current window's monitor
    GLFWmonitor* monitor = glfwGetWindowMonitor(window);
    if (!monitor) {
        // Window is not fullscreen, find the monitor it's mostly on
        int windowX, windowY, windowWidth, windowHeight;
        glfwGetWindowPos(window, &windowX, &windowY);
        glfwGetWindowSize(window, &windowWidth, &windowHeight);

        int monitorCount;
        GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);

        monitor = glfwGetPrimaryMonitor(); // Fallback

        // Find the monitor that contains the largest part of the window
        int maxOverlap = 0;
        for (int i = 0; i < monitorCount; ++i) {
            const GLFWvidmode* mode = glfwGetVideoMode(monitors[i]);
            int monitorX, monitorY;
            glfwGetMonitorPos(monitors[i], &monitorX, &monitorY);

            // Calculate overlap
            int overlapX = std::max(0, std::min(windowX + windowWidth, monitorX + mode->width) - std::max(windowX, monitorX));
            int overlapY = std::max(0, std::min(windowY + windowHeight, monitorY + mode->height) - std::max(windowY, monitorY));
            int overlap = overlapX * overlapY;

            if (overlap > maxOverlap) {
                maxOverlap = overlap;
                monitor = monitors[i];
            }
        }
    }
    return monitor;
}