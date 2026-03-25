#include "UIWindowManager.h"
#include "ProfilerController.h"
#include "LevelManager.h"
#include "ImGuiVisualizers/FPSTracker.h"
#include "ImGuiVisualizers/CommandConsole.h"
#include "ImGuiVisualizers/PropertyInspector.h"
#include "ImGuiVisualizers/ImGuiHeatMapVisualizer.h"
#include "Analysis/HeatMapContainer.h"
#include "Window.h"
#include "Viewport.h"
#include "ProfilerApp.h" // For ViewportLayout enum

#include <random>
#include <string>

// ImGui includes
#include "imgui.h"


// GLFW for window close
#include <GLFW/glfw3.h>

// External debug channel
extern DebugChannels::CDebugChannel AppDebug;



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
    ProfilerController* profilerController,
    LevelManager* levelManager,
    FPSTracker* fpsTracker,
    CommandConsole* console,
    ImGuiVisualizers::PropertyInspector* propertyInspector)
{
    m_levelManager = levelManager;
    m_console = console;
    m_propertyInspector = propertyInspector;

    AppDebug.printf("UIWindowManager initialized\n");
}

void UIWindowManager::RenderAllWindows()
{
    // Render menu bar first
    RenderMenuBar();

    // Render all windows
    renderConsoleWindow();
    renderPropertyInspectorWindow();
    RenderHeatMap();
}

void UIWindowManager::RenderHeatMap()
{

}

void UIWindowManager::RenderMenuBar()
{
    if (ImGui::BeginMenuBar()) {
        renderFileMenu();
        
        // Level Menu - handled by LevelManager
        // Add safety check for active viewport before passing it to LevelManager
        if (m_levelManager) {
            Viewport* safeActiveViewport = (m_activeViewport && m_window) ? m_activeViewport : nullptr;
            m_levelManager->RenderLevelMenu(safeActiveViewport);
        }

        renderWindowsMenu();
        renderViewportMenu();

        ImGui::EndMenuBar();
    }
}

void UIWindowManager::renderFileMenu()
{
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Exit")) {
            if (m_windowCloseCallback) {
                m_windowCloseCallback();
            }
        }
        ImGui::EndMenu();
    }
}

void UIWindowManager::renderWindowsMenu()
{
    if (ImGui::BeginMenu("Windows")) {
        // Console
        ImGui::MenuItem("Console", "C", &m_showConsole);
        
        // Property Inspector
        ImGui::MenuItem("Property Inspector", "I", &m_showPropertyInspector);

        ImGui::Separator();

        ImGui::EndMenu();
    }
}

void UIWindowManager::renderViewportMenu()
{
    if (ImGui::BeginMenu("Viewport")) {
        // Current viewport layout indicator
        if (m_viewportInfoCallback) {
            ImGui::Text("Current Layout: %s", m_viewportInfoCallback().c_str());
        }
        ImGui::Separator();

        // Viewport layout options
        if (m_viewportCheckCallback && m_viewportLayoutCallback) {
            if (ImGui::MenuItem("Single Viewport", "Ctrl+1", m_viewportCheckCallback(ViewportLayout::Single))) {
                m_viewportLayoutCallback(ViewportLayout::Single);
                // Clear active viewport reference since it will be updated after layout change
                m_activeViewport = nullptr;
            }

            if (ImGui::MenuItem("Horizontal Split", "Ctrl+2", m_viewportCheckCallback(ViewportLayout::HorizontalSplit))) {
                m_viewportLayoutCallback(ViewportLayout::HorizontalSplit);
                // Clear active viewport reference since it will be updated after layout change
                m_activeViewport = nullptr;
            }

            if (ImGui::MenuItem("Vertical Split", "Ctrl+3", m_viewportCheckCallback(ViewportLayout::VerticalSplit))) {
                m_viewportLayoutCallback(ViewportLayout::VerticalSplit);
                // Clear active viewport reference since it will be updated after layout change
                m_activeViewport = nullptr;
            }

            if (ImGui::MenuItem("Quad Layout", "Ctrl+4", m_viewportCheckCallback(ViewportLayout::Quad))) {
                m_viewportLayoutCallback(ViewportLayout::Quad);
                // Clear active viewport reference since it will be updated after layout change
                m_activeViewport = nullptr;
            }
        }

        ImGui::Separator();

        // Split ratio controls for split layouts
        if (m_currentViewportLayout == ViewportLayout::HorizontalSplit ||
            m_currentViewportLayout == ViewportLayout::VerticalSplit) {

            ImGui::Text("Split Ratio:");
            if (ImGui::SliderFloat("##SplitRatio", &m_viewportSplitRatio, 0.1f, 0.9f, "%.2f")) {
                // Trigger layout reapplication via callback
                if (m_viewportLayoutCallback) {
                    m_viewportLayoutCallback(m_currentViewportLayout);
                    // Clear active viewport reference since it will be updated after layout change
                    m_activeViewport = nullptr;
                }
            }
        }

        ImGui::Separator();

        // Viewport information
        if (m_window) {
            ImGui::Text("Total Viewports: %zu", m_window->getViewportCount());

            // Add null check for active viewport to prevent crash
            if (m_activeViewport) {
                ImGui::Text("Active Viewport: %s", m_activeViewport->getName().c_str());
            } else {
                ImGui::Text("Active Viewport: None");
            }
        }

        ImGui::EndMenu();
    }
}

void UIWindowManager::renderConsoleWindow()
{
    if (m_showConsole && m_console) {
        m_console->RenderConsoleWindow("Console", &m_showConsole);
    }
}

void UIWindowManager::renderPropertyInspectorWindow()
{
    if (m_showPropertyInspector && m_propertyInspector) {
        m_propertyInspector->RenderWindow("Property Inspector", &m_showPropertyInspector);
    }
}

void UIWindowManager::RefreshActiveViewport()
{
    if (m_window) {
        m_activeViewport = m_window->getActiveViewport();
    }
}