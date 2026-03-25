#pragma once

#include <functional>
#include <string>

// Forward declarations
class ProfilerController;
class LevelManager;
class FPSTracker;
class CommandConsole;
class Viewport;
class Window;

namespace ImGuiVisualizers {
    class PropertyInspector;
}

namespace Rendering {
    class RenderComponent;
}

enum class ViewportLayout;

/**
 * @brief Manages all ImGui windows and their visibility states
 * 
 * This class centralizes the management of all UI windows including menus,
 * tool windows, and their show/hide states.
 */
class UIWindowManager {
public:
    UIWindowManager();
    ~UIWindowManager();

    // Initialize with references to components that render windows
    void Initialize(
        ProfilerController* profilerController,
        LevelManager* levelManager,
        FPSTracker* fpsTracker,
        CommandConsole* console,
        ImGuiVisualizers::PropertyInspector* propertyInspector
    );

    // Main rendering methods
    void RenderAllWindows();
    void RenderHeatMap();
    void RenderMenuBar();

    // Window visibility controls
    void SetShowConsole(bool show) { m_showConsole = show; }
    void SetShowPropertyInspector(bool show) { m_showPropertyInspector = show; }

    bool GetShowConsole() const { return m_showConsole; }
    bool GetShowPropertyInspector() const { return m_showPropertyInspector; }

    // Toggle methods
    void ToggleConsole() { m_showConsole = !m_showConsole; }
    void TogglePropertyInspector() { m_showPropertyInspector = !m_showPropertyInspector; }

    // Callbacks for viewport layout management
    using ViewportLayoutCallback = std::function<void(ViewportLayout)>;
    using ViewportInfoCallback = std::function<std::string()>; // Get current layout name
    using ViewportCheckCallback = std::function<bool(ViewportLayout)>; // Check if current layout
    using WindowCloseCallback = std::function<void()>;

    void SetViewportLayoutCallback(ViewportLayoutCallback callback) { m_viewportLayoutCallback = callback; }
    void SetViewportInfoCallback(ViewportInfoCallback callback) { m_viewportInfoCallback = callback; }
    void SetViewportCheckCallback(ViewportCheckCallback callback) { m_viewportCheckCallback = callback; }
    void SetWindowCloseCallback(WindowCloseCallback callback) { m_windowCloseCallback = callback; }

    // Viewport split ratio management
    void SetViewportSplitRatio(float ratio) { m_viewportSplitRatio = ratio; }
    float GetViewportSplitRatio() const { return m_viewportSplitRatio; }

    // Set viewport layout and window references for info display
    void SetViewportLayoutEnum(ViewportLayout currentLayout) { m_currentViewportLayout = currentLayout; }
    void SetWindowReference(Window* window) { m_window = window; }
    void SetActiveViewport(Viewport* viewport) { m_activeViewport = viewport; }

    // Safe method to refresh active viewport from window
    void RefreshActiveViewport();

private:
    // Menu rendering methods
    void renderFileMenu();
    void renderWindowsMenu();
    void renderViewportMenu();

    // Window rendering methods
    void renderConsoleWindow();
    void renderPropertyInspectorWindow();

    // Component references (not owned)
    LevelManager* m_levelManager = nullptr;
    CommandConsole* m_console = nullptr;
    ImGuiVisualizers::PropertyInspector* m_propertyInspector = nullptr;

    // Window visibility states
    bool m_showConsole = false;
    bool m_showPropertyInspector = false;

    // Viewport management
    ViewportLayout m_currentViewportLayout;
    float m_viewportSplitRatio = 0.5f;
    Window* m_window = nullptr;
    Viewport* m_activeViewport = nullptr;

    // Callbacks
    ViewportLayoutCallback m_viewportLayoutCallback;
    ViewportInfoCallback m_viewportInfoCallback;
    ViewportCheckCallback m_viewportCheckCallback;
    WindowCloseCallback m_windowCloseCallback;
};