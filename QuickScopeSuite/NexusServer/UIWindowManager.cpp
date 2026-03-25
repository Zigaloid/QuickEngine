#include "UIWindowManager.h"
#include "ImGuiVisualizers/CommandConsole.h"
#include "ImGuiVisualizers/NexusFlowGraph.h"
#include "Analysis/HeatMapContainer.h"
#include "ImGuiVisualizers/NexusLogVisualizer.h"

#include <random>
#include <string>

// ImGui includes
#include "imgui.h"


// GLFW for window close
#include <GLFW/glfw3.h>

UIWindowManager::UIWindowManager()
{
}

UIWindowManager::~UIWindowManager()
{
}

// Simple test function that populates a HeatMapContainer with synthetic data.
// Creates series: "Temperature", "Density", "Humidity" with varying spatial patterns.
static void CreateTestHeatMap(HeatMapContainer& map)
{
    const std::size_t cols = map.GetColumns();
    const std::size_t rows = map.GetRows();

    std::mt19937 rng(12345);
    std::normal_distribution<double> noise(0.0, 0.2);

    for (std::size_t r = 0; r < rows; ++r) {
        for (std::size_t c = 0; c < cols; ++c) {
            // Center of the cell in meters
            float x = map.GetOriginX() + (static_cast<float>(c) + 0.5f) * map.GetCellSizeMeters();
            float y = map.GetOriginY() + (static_cast<float>(r) + 0.5f) * map.GetCellSizeMeters();

            // Spatial patterns
            double temp = 20.0 + 5.0 * std::sin(0.4 * x) * std::cos(0.3 * y) + noise(rng);
            double dens = 1.0 + 0.5 * std::cos(0.25 * x) + 0.3 * std::sin(0.35 * y) + noise(rng);
            double hum = 50.0 + 20.0 * std::exp(-((x - 5.0) * (x - 5.0) + (y - 4.0) * (y - 4.0)) / 10.0) + noise(rng);

            // Multiple samples per cell to exercise history/averaging
            map.AddValueByIndex(c, r, "Temperature", temp);
            map.AddValueByIndex(c, r, "Temperature", temp + noise(rng));

            map.AddValueByIndex(c, r, "Density", dens);
            map.AddValueByIndex(c, r, "Density", dens + noise(rng));

            map.AddValueByIndex(c, r, "Humidity", hum);
            map.AddValueByIndex(c, r, "Humidity", hum + noise(rng));
        }
    }

    return;
}

void UIWindowManager::Initialize(
    CommandConsole* console,
    NexusFlowGraph* flowGraph,
    NexusLogVisualizer* nexusLog)
{
    m_console = console;
    m_flowGraph = flowGraph;
    m_nexusLog = nexusLog;
}

void UIWindowManager::RenderAllWindows()
{
    // Render menu bar first
    RenderMenuBar();

    // Render all windows
    renderConsoleWindow();
    renderFlowGraphWindow();
    renderNexusLog();
    RenderHeatMap();
}

void UIWindowManager::RenderHeatMap()
{

}

void UIWindowManager::RenderMenuBar()
{
    if (ImGui::BeginMenuBar()) {
        renderFileMenu();
        
        renderWindowsMenu();        
        ImGui::EndMenuBar();
    }
}

void UIWindowManager::renderNexusLog()
{
    if (m_showNexusLog && m_nexusLog)
        m_nexusLog->RenderWindow();
}

void UIWindowManager::renderFileMenu()
{
}

void UIWindowManager::renderWindowsMenu()
{
    if (ImGui::BeginMenu("Windows")) {
        // Console
        ImGui::MenuItem("Console", "C", &m_showConsole);
        ImGui::MenuItem("Nexus Flow Graph", nullptr, &m_showFlowGraph);
        ImGui::MenuItem("Nexus Log", nullptr, &m_showNexusLog);
        ImGui::Separator();

        ImGui::EndMenu();
    }
}

void UIWindowManager::renderConsoleWindow()
{
    if (m_showConsole && m_console) {
        m_console->RenderConsoleWindow("Console", &m_showConsole);
    }
}

void UIWindowManager::renderFlowGraphWindow()
{
    if (m_showFlowGraph && m_flowGraph) {
        m_flowGraph->RenderFlowGraphWindow("Nexus Flow Graph", &m_showFlowGraph);
    }
}
