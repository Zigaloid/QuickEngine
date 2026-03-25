#pragma once

#include <functional>

// Forward declarations
struct GLFWwindow;
struct GLFWmonitor;

/**
 * @brief Manages ImGui initialization, rendering, and lifecycle
 * 
 * This class handles all ImGui-related operations including context creation,
 * backend initialization, frame management, DPI scaling, and cleanup.
 */
class ImGuiManager {
public:
    using DockspaceRenderCallback = std::function<void()>;

    ImGuiManager();
    ~ImGuiManager();

    // Core lifecycle
    bool Initialize(GLFWwindow* window, const char* glslVersion = "#version 330");
    void Shutdown();
    
    // Frame management
    void BeginFrame();
    void EndFrame();
    void RenderFrame(const DockspaceRenderCallback& renderCallback = nullptr);
    
    // DPI scaling
    void HandleDPIScaleChange(float xscale, float yscale);
    void UpdateDPIScaling(GLFWwindow* window);
    
    // Dockspace management
    void SetupDockspace();
    
    // State queries
    bool IsInitialized() const { return m_initialized; }
    const char* GetGLSLVersion() const { return m_glslVersion; }
    
    // Configuration
    void SetEnableDocking(bool enable);
    void SetEnableViewports(bool enable);
    void SetEnableKeyboardNav(bool enable);
    void SetEnableGamepadNav(bool enable);

private:
    void setupImGuiStyle();
    void configureIO();
    GLFWmonitor* findWindowMonitor(GLFWwindow* window);
    
    bool m_initialized = false;
    const char* m_glslVersion = "#version 330";
    
    // Configuration flags
    bool m_enableDocking = true;
    bool m_enableViewports = true;
    bool m_enableKeyboardNav = true;
    bool m_enableGamepadNav = true;
};