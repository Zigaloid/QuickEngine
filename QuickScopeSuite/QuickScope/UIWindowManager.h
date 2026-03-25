#pragma once

#include <functional>
#include <string>
#include <vector>
#include <memory>
#include "ImGuiVisualizers/UnifiedActionManager.h"
#include "ImGuiVisualizers/ImGuiHeatMapVisualizer.h"
#include "Analysis/HeatMapContainer.h"

// Forward declarations
class ProfilerController;
class FPSTracker;
class CommandConsole;
class NexusLogVisualizer;
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
 * tool windows, and their show/hide states. Supports multiple profiler
 * sessions displayed as tabs within a single profiler window.
 */
class UIWindowManager {
public:
    UIWindowManager();
    ~UIWindowManager();

    // Initialize with references to components that render windows.
    // No longer takes a ProfilerController* — sessions are managed internally.
    void Initialize(
        FPSTracker* fpsTracker,
        CommandConsole* console
    );

    // HeatMap setup (call after Initialize)
    void SetupHeatMap(HeatMapContainer* container, const std::string& backgroundTexturePath = "");

    // Main rendering methods
    void RenderAllWindows();
    void RenderHeatMap();
    void RenderMenuBar();
    void RenderToolbar();

    // Window visibility controls
    void SetShowFrameTimeAnalysis(bool show) { m_showFrameTimeAnalysis = show; }
    void SetShowConsole(bool show) { m_showConsole = show; }
    void SetShowHeatMap(bool show) { m_showHeatMap = show; }
    void SetShowProfiler(bool show) { m_showProfiler = show; }

    bool GetShowFrameTimeAnalysis() const { return m_showFrameTimeAnalysis; }
    bool GetShowConsole() const { return m_showConsole; }
    bool GetShowHeatMap() const { return m_showHeatMap; }
    bool GetShowProfiler() const { return m_showProfiler; }

    // Toggle methods
    void ToggleFrameTimeAnalysis() { m_showFrameTimeAnalysis = !m_showFrameTimeAnalysis; }
    void ToggleConsole() { m_showConsole = !m_showConsole; }
    void ToggleHeatMap() { m_showHeatMap = !m_showHeatMap; }
    void ToggleProfiler() { m_showProfiler = !m_showProfiler; }

    // Access the heat map visualizer for additional configuration
    ImGuiVisualizers::ImGuiHeatMapVisualizer& GetHeatMapVisualizer() { return m_heatmapVis; }

    // Access the action manager for external registration
    UI::UnifiedActionManager& GetActionManager() { return m_actionManager; }

    // ---- Multi-document profiler session management ----

    /// Create a new empty profiler session and return a pointer to it.
    ProfilerController* CreateSession(const std::string& displayName = "Untitled");

    /// Create (or return the existing) live capture session.
    ProfilerController* GetOrCreateLiveSession();

    /// Load a profiler session from a file into a new tab.
    void LoadSessionIntoNewTab(const std::string& filePath);

    /// Show a file dialog and load into a new tab.
    void LoadSessionDialog();

    /// Close a session by index. Returns false if index is invalid.
    bool CloseSession(int index);

    /// Get the currently active (focused tab) session. May return nullptr.
    ProfilerController* GetActiveSession();
    const ProfilerController* GetActiveSession() const;

    /// Get the live session (may be nullptr if none created yet).
    ProfilerController* GetLiveSession();

    /// Total number of open sessions.
    int GetSessionCount() const { return static_cast<int>(m_profilerSessions.size()); }

    // Profiling control (routes to live session)
    void StartProfiling();
    void StopProfiling();
    bool IsProfilingEnabled() const;

    // Shutdown all profiler sessions
    void ShutdownProfiler();

    // Initialize profiler networking (call after Nexus is connected)
    void InitializeProfilerNetworking();

private:
    // Window rendering methods
    void renderProfilerWindow();
    void renderFrameTimeAnalysisWindow();
    void renderConsoleWindow();
    void renderHeatMapWindow();

    // Action registration
    void registerWindowActions();
    void registerProfilerActions();

    // Network packet handlers — route to live session
    void handleProfilerPacket(const std::string& messageBody);
    void handleProfilerBinaryPacket(const std::vector<uint8_t>& messageBody);

    // Component references (not owned)
    FPSTracker* m_fpsTracker = nullptr;
    CommandConsole* m_console = nullptr;
    NexusLogVisualizer* m_nexusLog = nullptr;

    // Owned heat map visualizer
    ImGuiVisualizers::ImGuiHeatMapVisualizer m_heatmapVis;

    // Window visibility states
    bool m_showFrameTimeAnalysis = true;
    bool m_showConsole = false;
    bool m_showHeatMap = false;
    bool m_showProfiler = true;

    // App-level action manager (Windows menu only)
    UI::UnifiedActionManager m_actionManager;

    // Profiler-window action manager (rendered only inside the Profiler window menu bar)
    UI::UnifiedActionManager m_profilerActionManager;

    // ---- Multi-document profiler state ----
    std::vector<std::unique_ptr<ProfilerController>> m_profilerSessions;
    int m_activeSessionIndex = -1;   // Index of the currently focused tab
    int m_liveSessionIndex = -1;     // Index of the live capture session (-1 = none)
    int m_nextSessionId = 1;         // Monotonically increasing ID for ImGui tab uniqueness
    bool m_networkInitialized = false;

    // Queue of sessions to close (processed after rendering to avoid mid-loop invalidation)
    std::vector<int> m_pendingCloseIndices;
    void processPendingCloses();
};