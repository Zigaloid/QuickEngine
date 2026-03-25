#pragma once
#include "ImGuiManager.h"
#include "UIWindowManager.h"
#include "ViewportLayoutManager.h"
#include "ImGuiVisualizers/PropertyInspector.h"
#include "ImGuiVisualizers/FPSTracker.h"
#include "ImGuiVisualizers/CommandConsole.h"

#include "ProfilerController.h"
#include "LevelManager.h"
#include "Window.h"
#include "Viewport.h"

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

private:
    ViewportLayoutManager m_viewportLayoutManager;

    // Window and viewport management
    bool SetupWindow();
    void SetupViewports();
    void HandleWindowCallbacks();
	void OnDPIScaleChanged(float xscale, float yscale);
	void UpdateImGuiDPIScaling();
    
    void OnRenderComponentSelectionChanged(Rendering::RenderComponent* selectedComponent);
    // ImGui integration with Window/Viewport system
    void SetupImGuiForWindow();
    void RenderImGuiWithViewports();
    
    // Viewport rendering callback
    void OnViewportRender(Viewport& viewport, const Matrix4f& viewMatrix, const Matrix4f& projectionMatrix);
    
    // Window and rendering
    std::unique_ptr<Window> m_window;
    Viewport* m_mainViewport;
    Viewport* m_activeViewport;

    // Application state
    bool m_ShowDemoWindow = false;     
    bool m_ShowGLTFDebug = false;
    
    UIWindowManager m_uiWindowManager;
    ImGuiManager m_imguiManager;

    // Frame time performance tracker
    FPSTracker m_fpsTracker;

    // Profiler controller - replaces individual profiler components
    ProfilerController m_profilerController;
    
    // Command console
    CommandConsole m_console;
	
    // Property inspector for selected objects
	ImGuiVisualizers::PropertyInspector m_propertyInspector;
    // ImGui integration
    
    bool m_ImGuiInitialized = false;
    const char* m_GlslVersion = "#version 330";

    // Scene components - now managed by LevelManager
    LevelManager m_levelManager;
    Rendering::ManipulatorComponent* m_manipulator = nullptr;
	Rendering::RenderComponentSelector* m_RenderComponentSelector = nullptr;
	Input::KeyboardShortcutManager* m_keyboardShortcutManager = nullptr;
    void RegisterKeyboardShortcuts();	
};