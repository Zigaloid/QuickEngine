#pragma once

#include <functional>
#include <string>

// Forward declarations
class CommandConsole;
class NexusFlowGraph;
class CNexusServer;

namespace ImGuiVisualizers {
    class PropertyInspector;
}

namespace Rendering {
    class RenderComponent;
}
class NexusLogVisualizer;

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
        CommandConsole* console,
        NexusFlowGraph* flowGraph,
        NexusLogVisualizer* nexusLog
    );

    // Main rendering methods
    void RenderAllWindows();
    void RenderHeatMap();
    void RenderMenuBar();

    // Window visibility controls
    void SetShowFrameTimeAnalysis(bool show) { m_showFrameTimeAnalysis = show; }
    void SetShowConsole(bool show) { m_showConsole = show; }
    void SetShowFlameGraph(bool show) { m_showFlameGraph = show; }
    void SetShowFlowGraph(bool show) { m_showFlowGraph = show; }

    bool GetShowFrameTimeAnalysis() const { return m_showFrameTimeAnalysis; }
    bool GetShowConsole() const { return m_showConsole; }
    bool GetShowFlameGraph() const { return m_showFlameGraph; }
    bool GetShowFlowGraph() const { return m_showFlowGraph; }

    // Toggle methods
    void ToggleFrameTimeAnalysis() { m_showFrameTimeAnalysis = !m_showFrameTimeAnalysis; }
    void ToggleConsole() { m_showConsole = !m_showConsole; }    
    void ToggleFlameGraph() { m_showFlameGraph = !m_showFlameGraph; }
    void ToggleFlowGraph() { m_showFlowGraph = !m_showFlowGraph; }

private:
    // Menu rendering methods
    void renderFileMenu();
    void renderWindowsMenu();
    
    // Window rendering methods
    void renderConsoleWindow();
    void renderFlowGraphWindow();
    void renderNexusLog();
    
    // Component references (not owned)
    CommandConsole* m_console = nullptr;
    NexusFlowGraph* m_flowGraph = nullptr;
    NexusLogVisualizer* m_nexusLog = nullptr;
    // Window visibility states
    bool m_showFrameTimeAnalysis = true;
    bool m_showConsole = false;    
    bool m_showFlameGraph = false;
    bool m_showFlowGraph = false;
    bool m_showNexusLog = false;
};