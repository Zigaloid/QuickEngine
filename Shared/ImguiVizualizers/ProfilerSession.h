#pragma once

#include "Profiler/Profiler.h"
#include "Profiler/ProfilerAnalyzer.h"
#include "ProfilerVisualizer.h"
#include "ProfilerTreeView.h"
#include "ProfilerFrameComparisonView.h"
#include "ProfilerOutlierView.h"
#include "UnifiedActionManager.h"

#include <functional>
#include <string>

/**
 * @brief Specialized FPS tracking class derived from VisualizableMetricHeuristic
 *
 * This class provides FPS-specific functionality including automatic bucket setup,
 * performance-based coloring, and FPS-specific statistics display.
 */
class ProfilerSession
{
public:
    enum class ViewMode {
        FlameGraph,
        TreeView,
        OutlierAnalysis
    };

    ProfilerSession();
    ~ProfilerSession() = default;
     /**
    * @brief Called once after the visualizer is registered with the manager.
    * Use this for deferred setup that requires external systems to be ready.
    */
    void Initialize();

    /**
     * @brief Called once when the visualizer is unregistered or the manager
     * is destroyed. Use this to release resources.
     */
    void Shutdown();

    /**
     * @brief Called every frame regardless of visibility.
     * Use this for logic updates that must run continuously (e.g. data polling).
     * @param deltaTime Elapsed time in seconds since the last update.
     */
    void Update(float deltaTime) { (void)deltaTime; }

    /**
     * @brief Render the visualizer's ImGui window.
     * Only called when the visualizer is visible.
     * @param isOpen Pointer to a bool controlling window visibility.
     *              Set to false to hide the window.
     * @return true if the window was rendered, false otherwise.
     */
    bool Render(bool* isOpen)
    {
        ImGuiWindowFlags wflags = ImGuiWindowFlags_MenuBar;

        if (ImGui::Begin(GetName(), isOpen, wflags))
        {
            // Render profiler-specific menu bar
            if (ImGui::BeginMenuBar())
            {
                m_actionManager.RenderMenuBar();
                ImGui::EndMenuBar();
            }

            RenderSessionContent();
        }
        ImGui::End();
        return true;
    }

    /**
     * @brief Return a display name used in menus and window titles.
     */
    const char* GetName() const
    {
        return "Profiler";
    }

    /**
     * @brief Optional keyboard shortcut label shown in the Windows menu.
     * Return nullptr if no shortcut is assigned.
     */
    virtual const char* GetShortcut() const { return nullptr; }
    virtual const char* GetMenuCategory() const { return "Show"; }

    // Main profiler control
    void StartProfiling();
    void StopProfiling();
    void Clear();
    
    // Data analysis and visualization
    void ResetView();
    
    // UI rendering    
    void RenderFlameGraphWindow(const std::string& windowTitle, bool* showWindow);

    // Render only the per-session content (no ImGui window frame or shared menu bar).
    // Must be called between ImGui::Begin / ImGui::End.
    void RenderSessionContent();
    void RenderThreadFilter();

    // State management
    void SetShowFlameGraph(bool show) { m_showFlameGraph = show; }
    bool ShouldShowFlameGraph() const { return m_showFlameGraph; }

    void SetViewMode(ViewMode mode) { m_viewMode = mode; }
    ViewMode GetViewMode() const { return m_viewMode; }

    // Action manager - owned by ProfilerSession
    UI::UnifiedActionManager& GetActionManager() { return m_actionManager; }

    // Session save/load
    bool SaveSession(const std::string& filePath);
    bool LoadSession(const std::string& filePath);
    bool HasSessionData() const { return !m_sessionPackets.empty(); }

    // File dialog driven save/load
    void SaveSessionAs();
    void SaveSessionCurrent();
    void LoadSessionDialog();

    void HandleProfilerPacket(const std::string& messageBody);
    void HandleProfilerBinaryPacket(const std::vector<uint8_t>& messageBody);

    // Display name for tab labels
    void SetDisplayName(const std::string& name) { m_displayName = name; }
    const std::string& GetDisplayName() const { return m_displayName; }

    // Whether this session receives live capture data
    void SetLiveSession(bool live) { m_isLiveSession = live; }
    bool IsLiveSession() const { return m_isLiveSession; }

    // Whether the session has been modified since last save
    bool IsDirty() const { return m_dirty; }
    void SetDirty(bool dirty) { m_dirty = dirty; }

    // Current file path (empty = unsaved)
    const std::string& GetSessionPath() const { return m_currentSessionPath; }

    // Profiling state (shared across sessions, but Start/Stop lives on the live session)
    bool IsProfilingEnabled() const { return m_enabled; }

    // Access the timeline data for cross-session comparison
    Profiler::TimelineFlameGraphData* GetTimelineData() { return m_visualizer.GetTimelineData(); }

private:
    void registerProfilerActions();
    void registerFileActions();

    UI::UnifiedActionManager m_actionManager;
    Profiler::ProfilerVisualizer m_visualizer;
    Profiler::ProfilerTreeView m_treeView;
    Profiler::ProfilerFrameComparisonView m_frameComparisonView;
    Profiler::ProfilerOutlierView m_outlierView;
    Profiler::ProfilerAnalyzer m_analyzer;
    bool m_showFlameGraph = true;
    bool m_showFrameComparison = false;
    bool m_initialized;
    bool m_enabled = false;
    bool m_dirty = false;
    bool m_isLiveSession = false;
    ViewMode m_viewMode = ViewMode::FlameGraph;

    // Retained binary packets for session save/load
    std::vector<std::vector<uint8_t>> m_sessionPackets;

    // Current session file path (empty = unsaved / no file loaded)
    std::string m_currentSessionPath;

    // Display name for multi-document tabs
    std::string m_displayName = "Untitled";

    std::vector<std::string> m_threadNames;
    std::string m_selectedThreadId; // empty string means "All"
    std::vector<std::string> m_selectedThreadIds;
    std::string m_threadFilterText;
};