#pragma once

#include "Profiler/Profiler.h"
#include "Profiler/ProfilerAnalyzer.h"
#include "ImGuiVisualizers/ProfilerVisualizer.h"
#include "ImGuiVisualizers/CommandConsole.h"
#include <functional>
#include <string>

// Forward declarations
struct ImVec4;

class ProfilerController {
public:
    ProfilerController();
    ~ProfilerController() = default;

    // Main profiler control
    void StartProfiling();
    void StopProfiling(bool sendAsBinary = false);
    bool IsEnabled() const;
    void Clear();
    
    // Data analysis and visualization
    void ResetView();
    void Initialize();
    void Shutdown();
    void HandleProfilerMessage(const std::string& messageBody);
    
private:
    Profiler::ProfilerVisualizer m_visualizer;
    bool m_showFlameGraph;
    bool m_initialized;
};