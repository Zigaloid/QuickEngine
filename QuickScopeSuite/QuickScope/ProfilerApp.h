#pragma once

#include "ImGuiManager.h"
#include "UIWindowManager.h"
#include "ImGuiVisualizers/FPSTracker.h"
#include "ImGuiVisualizers/CommandConsole.h"
#include "ImGuiVisualizers/ImGuiHeatMapVisualizer.h"

#include "Analysis/HeatMapContainer.h"

#include <mutex>
#include <vector>


// Forward declare GLFWwindow to avoid pulling in gl.h before glew.h
struct GLFWwindow;

// Forward declarations
namespace Rendering {
    class RenderPrimitives;
    class RenderComponent;
    class Manipulators;
    class ManipulatorComponent;
    class RenderComponentSelector;
}
namespace Input {
    class KeyboardShortcutManager;
}

class ProfilerApp {
public:
    ProfilerApp();
    ~ProfilerApp();

    bool Initialize();
    void Run();
    void Shutdown();

protected:
    // Main application methods
    void OnInit();
    void OnUpdate(double deltaTime);
    void OnShutdown();
    void RenderImGui();
    void HandleFPSMessage(const std::string& messageBody);
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

    // Frame time performance tracker
    FPSTracker m_fpsTracker;
    
    // Command console
    CommandConsole m_console;
      
    bool m_ImGuiInitialized = false;
    const char* m_GlslVersion = "#version 330";

    HeatMapContainer m_TestHeatMap = { 1000.0f, 1000.0f, 10.0f };
    ImGuiVisualizers::ImGuiHeatMapVisualizer m_heatmapVis;

    Input::KeyboardShortcutManager* m_keyboardShortcutManager = nullptr;
    void RegisterKeyboardShortcuts();	
    double m_lastFrameTime = 0.0;

    // Saved ImGui GLFW key callback for proper chaining
    // Using raw function pointer type instead of GLFWkeyfun to avoid including glfw3.h
    void(*m_previousKeyCallback)(GLFWwindow*, int, int, int, int) = nullptr;

    std::mutex m_fpsMutex;
    std::vector<float> m_pendingFPSUpdates;    
};

