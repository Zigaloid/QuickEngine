#pragma once

#include "Profiler/Profiler.h"
#include "Profiler/ProfilerAnalyzer.h"
#include "ImGuiVisualizers/ProfilerVisualizer.h"
#include "ImGuiVisualizers/ProfilerTreeView.h"
#include "ImGuiVisualizers/ProfilerFrameComparisonView.h"
#include "ImGuiVisualizers/ProfilerOutlierView.h"
#include "ImGuiVisualizers/UnifiedActionManager.h"
#include "ImGuiVisualizers/CommandConsole.h"
#include <functional>
#include <string>

// Forward declarations
struct ImVec4;

class ProfilerController {
public:
    enum class ViewMode {
        FlameGraph,
        TreeView,
        OutlierAnalysis
    };

    ProfilerController();
    ~ProfilerController() = default;

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

    // Action manager - owned by ProfilerController
    UI::UnifiedActionManager& GetActionManager() { return m_actionManager; }

    // Session save/load
    bool SaveSession(const std::string& filePath);
    bool LoadSession(const std::string& filePath);
    bool HasSessionData() const { return !m_sessionPackets.empty(); }

    // File dialog driven save/load
    void SaveSessionAs();
    void SaveSessionCurrent();
    void LoadSessionDialog();

    void Initialize();
    void Shutdown();
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