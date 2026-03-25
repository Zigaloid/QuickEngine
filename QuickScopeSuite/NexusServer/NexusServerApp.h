#pragma once
#include "ImGuiManager.h"
#include "UIWindowManager.h"
#include "ImGuiVisualizers/CommandConsole.h"
#include "ImGuiVisualizers/NexusFlowGraph.h"
#include "ImGuiVisualizers/NexusLogVisualizer.h"

// Forward declare GLFWwindow to avoid pulling in gl.h before glew.h
struct GLFWwindow;

// Forward declarations

namespace Input {
    class KeyboardShortcutManager;
}

class CNexusServerApp {
public:
    CNexusServerApp();
    ~CNexusServerApp();

    bool Initialize();
    void Run();
    void Shutdown();

protected:
    // Main application methods
    void OnInit();
    void OnUpdate(double deltaTime);
    void OnShutdown();
    void RenderImGui();

private:
    // Window management (raw GLFW, no viewports)
    bool SetupGLFWWindow();
    void HandleWindowCallbacks();
    void OnDPIScaleChanged(float xscale, float yscale);
    void UpdateImGuiDPIScaling();
    
    // ImGui integration
    void SetupImGui();
    void RenderImGuiFrame();
    
    // GLFW window (raw, no Window/Viewport wrapper)
    GLFWwindow* m_glfwWindow = nullptr;
    int m_windowWidth = 1280;
    int m_windowHeight = 720;

    // Application state
    bool m_ShowDemoWindow = false;
    bool m_ShowGLTFDebug = false;
    
    UIWindowManager m_uiWindowManager;
    ImGuiManager m_imguiManager;
    
    // Command console
    CommandConsole m_console;

    // Nexus flow graph visualizer
    NexusFlowGraph m_flowGraph;

    NexusLogVisualizer m_nexusLog;
	   
    bool m_ImGuiInitialized = false;
    const char* m_GlslVersion = "#version 330";

    Input::KeyboardShortcutManager* m_keyboardShortcutManager = nullptr;
    void RegisterKeyboardShortcuts();	
};