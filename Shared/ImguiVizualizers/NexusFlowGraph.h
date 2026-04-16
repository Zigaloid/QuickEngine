#pragma once

#include "imgui.h"
#include "IImGuiVisualizer.h"
#include <string>
#include <vector>
#include "Net/NexusServer.h" // Required: SNexusClientSnapshot must be a complete type for std::vector

// Forward declarations
class CNexusServer;
struct SNexusSubscription;

/**
 * @brief ImGui-based flow graph visualizer for NexusServer connections
 * 
 * Renders a visual flow graph showing the NexusServer at the center,
 * connected clients as nodes, and their pipe subscriptions as edges.
 * Automatically refreshes from a live CNexusServer instance.
 */
class NexusFlowGraph : public ImGuiVisualizers::IImGuiVisualizer
{
public:
    explicit NexusFlowGraph();
    
    /**
     * @brief Called once after the visualizer is registered with the manager.
     * Use this for deferred setup that requires external systems to be ready.
     */
    void Initialize() override {}
    void Shutdown() override {}
    void Update(float deltaTime) override { (void)deltaTime; }

    bool Render(bool* isOpen) override
    {
        RenderFlowGraphWindow(GetName(), isOpen);
        return true;
    }

    const char* GetName() const override { return "Nexus Flow Graph"; }
    const char* GetShortcut() const override { return nullptr; }
    const char* GetMenuCategory() const override { return "Show"; }

    /**
     * @brief Set the server to visualize
     * @param server Pointer to the CNexusServer instance (not owned)
     */
    void SetServer(CNexusServer* server);

    /**
     * @brief Render the flow graph window
     * @param windowTitle Title for the ImGui window
     * @param isOpen Pointer to bool controlling window visibility
     * @return true if window is open and rendering
     */
    bool RenderFlowGraphWindow(const char* windowTitle = "Nexus Flow Graph", bool* isOpen = nullptr);

private:
    // Rendering helpers
    void RenderGraph();
    void RenderServerNode(ImDrawList* drawList, ImVec2 center);
    void RenderClientNode(ImDrawList* drawList, ImVec2 pos, const SNexusClientSnapshot& client, int index);
    void RenderConnection(ImDrawList* drawList, ImVec2 from, ImVec2 to, const std::string& label, ImU32 color);
    void RenderLegend(ImDrawList* drawList, ImVec2 pos);

    // Layout helpers
    ImVec2 CalculateClientPosition(int index, int totalClients, ImVec2 center, float radius) const;

    // Data
    CNexusServer* m_server = nullptr;
    std::vector<SNexusClientSnapshot> m_cachedSnapshots;

    // Refresh timing
    float m_refreshInterval = 0.5f; // seconds
    float m_timeSinceRefresh = 0.0f;

    // Visual settings
    float m_nodeRadius = 40.0f;
    float m_clientNodeWidth = 150.0f;
    float m_clientNodeHeight = 60.0f;
    float m_orbitRadius = 200.0f;
    ImVec2 m_scrollOffset = ImVec2(0.0f, 0.0f);
    bool m_isDragging = false;
    ImVec2 m_dragStart = ImVec2(0.0f, 0.0f);
};