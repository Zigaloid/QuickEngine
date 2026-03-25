#include "NexusFlowGraph.h"
#include "Net/NexusServer.h"
#include "Profiler/Profiler.h"

#include <cmath>
#include <sstream>
#include <unordered_set>
#include <unordered_map>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ---------------------------------------------------------------------------
// Color palette
// ---------------------------------------------------------------------------

static constexpr ImU32 kColorServerNode     = IM_COL32(80, 160, 255, 255);
static constexpr ImU32 kColorServerBorder   = IM_COL32(120, 200, 255, 255);
static constexpr ImU32 kColorClientReg      = IM_COL32(100, 200, 100, 255);
static constexpr ImU32 kColorClientUnreg    = IM_COL32(200, 200, 100, 255);
static constexpr ImU32 kColorClientBorder   = IM_COL32(180, 180, 180, 255);
static constexpr ImU32 kColorPipeLine       = IM_COL32(255, 180, 60, 200);
static constexpr ImU32 kColorWildcardLine   = IM_COL32(180, 120, 255, 200);
static constexpr ImU32 kColorConnectionLine = IM_COL32(100, 100, 100, 180);
static constexpr ImU32 kColorText           = IM_COL32(255, 255, 255, 255);
static constexpr ImU32 kColorTextDim        = IM_COL32(180, 180, 180, 255);
static constexpr ImU32 kColorBackground     = IM_COL32(30, 30, 30, 255);
static constexpr ImU32 kColorGrid           = IM_COL32(50, 50, 50, 255);

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

NexusFlowGraph::NexusFlowGraph() = default;

void NexusFlowGraph::SetServer(CNexusServer* server)
{
    m_server = server;
}

// ---------------------------------------------------------------------------
// Main window
// ---------------------------------------------------------------------------

bool NexusFlowGraph::RenderFlowGraphWindow(const char* windowTitle, bool* isOpen)
{
    DECLARE_FUNC_LOW();

    bool windowShouldBeOpen = (isOpen == nullptr) || *isOpen;
    if (!windowShouldBeOpen) {
        return false;
    }

    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin(windowTitle, isOpen, ImGuiWindowFlags_None)) {
        ImGui::End();
        return false;
    }

    // Refresh data on a timer
    ImGuiIO& io = ImGui::GetIO();
    m_timeSinceRefresh += io.DeltaTime;
    if (m_timeSinceRefresh >= m_refreshInterval) {
        m_timeSinceRefresh = 0.0f;
        if (m_server) {
            m_cachedSnapshots = m_server->GetClientSnapshots();
        }
    }

    // Header info
    bool serverRunning = m_server && m_server->IsRunning();
    ImGui::TextColored(serverRunning ? ImVec4(0.4f, 1.0f, 0.4f, 1.0f) : ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                       serverRunning ? "Server: RUNNING" : "Server: STOPPED");
    ImGui::SameLine();
    ImGui::Text("| Clients: %d", static_cast<int>(m_cachedSnapshots.size()));
    ImGui::SameLine();
    ImGui::SliderFloat("Refresh (s)", &m_refreshInterval, 0.1f, 5.0f, "%.1f");

    ImGui::Separator();

    // Render the graph canvas
    RenderGraph();

    ImGui::End();
    return true;
}

// ---------------------------------------------------------------------------
// Graph canvas
// ---------------------------------------------------------------------------

void NexusFlowGraph::RenderGraph()
{
    DECLARE_FUNC_LOW();

    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();

    if (canvasSize.x < 50.0f) canvasSize.x = 50.0f;
    if (canvasSize.y < 50.0f) canvasSize.y = 50.0f;

    // Invisible button for interaction
    ImGui::InvisibleButton("FlowGraphCanvas", canvasSize,
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    bool isHovered = ImGui::IsItemHovered();
    bool isActive = ImGui::IsItemActive();

    // Handle panning
    ImGuiIO& io = ImGui::GetIO();
    if (isActive && ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
        m_scrollOffset.x += io.MouseDelta.x;
        m_scrollOffset.y += io.MouseDelta.y;
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Clip to canvas
    drawList->PushClipRect(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), true);

    // Draw background
    drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), kColorBackground);

    // Draw grid
    float gridSize = 40.0f;
    for (float x = fmodf(m_scrollOffset.x, gridSize); x < canvasSize.x; x += gridSize) {
        drawList->AddLine(ImVec2(canvasPos.x + x, canvasPos.y),
                          ImVec2(canvasPos.x + x, canvasPos.y + canvasSize.y), kColorGrid);
    }
    for (float y = fmodf(m_scrollOffset.y, gridSize); y < canvasSize.y; y += gridSize) {
        drawList->AddLine(ImVec2(canvasPos.x, canvasPos.y + y),
                          ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + y), kColorGrid);
    }

    // Center of canvas (with scroll offset)
    ImVec2 center(canvasPos.x + canvasSize.x * 0.5f + m_scrollOffset.x,
                  canvasPos.y + canvasSize.y * 0.5f + m_scrollOffset.y);

    int clientCount = static_cast<int>(m_cachedSnapshots.size());

    // Adjust orbit radius based on client count
    float orbitRadius = (clientCount <= 4) ? m_orbitRadius :
                        m_orbitRadius + static_cast<float>(clientCount - 4) * 30.0f;

    // Draw connections first (behind nodes)
    for (int i = 0; i < clientCount; ++i) {
        ImVec2 clientPos = CalculateClientPosition(i, clientCount, center, orbitRadius);
        ImVec2 clientCenter(clientPos.x + m_clientNodeWidth * 0.5f,
                            clientPos.y + m_clientNodeHeight * 0.5f);

        // Base connection line
        RenderConnection(drawList, center, clientCenter, "", kColorConnectionLine);

        // Subscription pipe lines
        const auto& client = m_cachedSnapshots[i];
        float subOffset = 0.0f;
        for (const auto& sub : client.subscriptions) {
            ImU32 lineColor = (sub.pipeName == "ANY" || sub.appName == "ANY") ? kColorWildcardLine : kColorPipeLine;

            // Offset parallel lines slightly for readability
            ImVec2 dir(clientCenter.x - center.x, clientCenter.y - center.y);
            float len = sqrtf(dir.x * dir.x + dir.y * dir.y);
            if (len > 0.0f) {
                ImVec2 perp(-dir.y / len, dir.x / len);
                float offset = (subOffset - static_cast<float>(client.subscriptions.size() - 1) * 0.5f) * 4.0f;
                ImVec2 from(center.x + perp.x * offset, center.y + perp.y * offset);
                ImVec2 to(clientCenter.x + perp.x * offset, clientCenter.y + perp.y * offset);

                std::string label = sub.pipeName;
                if (sub.appName != "ANY") {
                    label += " (" + sub.appName + ")";
                }
                RenderConnection(drawList, from, to, label, lineColor);
            }
            subOffset += 1.0f;
        }
    }

    // Draw server node
    RenderServerNode(drawList, center);

    // Draw client nodes
    for (int i = 0; i < clientCount; ++i) {
        ImVec2 clientPos = CalculateClientPosition(i, clientCount, center, orbitRadius);
        RenderClientNode(drawList, clientPos, m_cachedSnapshots[i], i);
    }

    // Draw legend
    RenderLegend(drawList, ImVec2(canvasPos.x + 10.0f, canvasPos.y + canvasSize.y - 80.0f));

    // Instructions
    drawList->AddText(ImVec2(canvasPos.x + 10.0f, canvasPos.y + 5.0f),
                      kColorTextDim, "Right-click drag to pan");

    drawList->PopClipRect();
}

// ---------------------------------------------------------------------------
// Node rendering
// ---------------------------------------------------------------------------

void NexusFlowGraph::RenderServerNode(ImDrawList* drawList, ImVec2 center)
{
    // Outer glow
    drawList->AddCircleFilled(center, m_nodeRadius + 4.0f, IM_COL32(80, 160, 255, 60), 32);
    // Main circle
    drawList->AddCircleFilled(center, m_nodeRadius, kColorServerNode, 32);
    drawList->AddCircle(center, m_nodeRadius, kColorServerBorder, 32, 2.0f);

    // Label
    const char* label = "NexusServer";
    ImVec2 textSize = ImGui::CalcTextSize(label);
    drawList->AddText(ImVec2(center.x - textSize.x * 0.5f, center.y - textSize.y * 0.5f),
                      kColorText, label);

    // Client count below
    std::string countStr = std::to_string(m_cachedSnapshots.size()) + " client(s)";
    ImVec2 countSize = ImGui::CalcTextSize(countStr.c_str());
    drawList->AddText(ImVec2(center.x - countSize.x * 0.5f, center.y + m_nodeRadius + 5.0f),
                      kColorTextDim, countStr.c_str());
}

void NexusFlowGraph::RenderClientNode(ImDrawList* drawList, ImVec2 pos, const SNexusClientSnapshot& client, int index)
{
    ImVec2 max(pos.x + m_clientNodeWidth, pos.y + m_clientNodeHeight);

    ImU32 fillColor = client.registered ? kColorClientReg : kColorClientUnreg;
    ImU32 fillBg = IM_COL32(
        (fillColor & 0xFF),
        ((fillColor >> 8) & 0xFF),
        ((fillColor >> 16) & 0xFF),
        60);

    // Background
    drawList->AddRectFilled(pos, max, fillBg, 8.0f);
    // Border
    drawList->AddRect(pos, max, kColorClientBorder, 8.0f, 0, 1.5f);
    // Status bar at top
    drawList->AddRectFilled(pos, ImVec2(max.x, pos.y + 4.0f), fillColor, 8.0f, 0);

    // App name
    std::string appName = client.appName.empty() ? ("Client #" + std::to_string(index)) : client.appName;
    ImVec2 textSize = ImGui::CalcTextSize(appName.c_str());
    float textX = pos.x + (m_clientNodeWidth - textSize.x) * 0.5f;
    drawList->AddText(ImVec2(textX, pos.y + 8.0f), kColorText, appName.c_str());

    // Status
    const char* statusText = client.registered ? "Registered" : "Pending";
    ImVec2 statusSize = ImGui::CalcTextSize(statusText);
    drawList->AddText(ImVec2(pos.x + (m_clientNodeWidth - statusSize.x) * 0.5f, pos.y + 26.0f),
                      kColorTextDim, statusText);

    // Subscription count
    std::string subStr = std::to_string(client.subscriptions.size()) + " sub(s)";
    ImVec2 subSize = ImGui::CalcTextSize(subStr.c_str());
    drawList->AddText(ImVec2(pos.x + (m_clientNodeWidth - subSize.x) * 0.5f, pos.y + 42.0f),
                      kColorTextDim, subStr.c_str());
}

void NexusFlowGraph::RenderConnection(ImDrawList* drawList, ImVec2 from, ImVec2 to, const std::string& label, ImU32 color)
{
    // Draw a bezier curve between the two points
    ImVec2 mid((from.x + to.x) * 0.5f, (from.y + to.y) * 0.5f);

    // Simple straight line with arrowhead
    drawList->AddLine(from, to, color, 2.0f);

    // Arrowhead
    ImVec2 dir(to.x - from.x, to.y - from.y);
    float len = sqrtf(dir.x * dir.x + dir.y * dir.y);
    if (len > 0.0f) {
        dir.x /= len;
        dir.y /= len;

        float arrowSize = 8.0f;
        ImVec2 arrowBase(to.x - dir.x * arrowSize, to.y - dir.y * arrowSize);
        ImVec2 perp(-dir.y * arrowSize * 0.5f, dir.x * arrowSize * 0.5f);

        ImVec2 arrowPoints[3] = {
            to,
            ImVec2(arrowBase.x + perp.x, arrowBase.y + perp.y),
            ImVec2(arrowBase.x - perp.x, arrowBase.y - perp.y)
        };
        drawList->AddTriangleFilled(arrowPoints[0], arrowPoints[1], arrowPoints[2], color);
    }

    // Label at midpoint
    if (!label.empty()) {
        ImVec2 labelSize = ImGui::CalcTextSize(label.c_str());
        // Background behind text
        ImVec2 labelPos(mid.x - labelSize.x * 0.5f, mid.y - labelSize.y * 0.5f - 8.0f);
        drawList->AddRectFilled(
            ImVec2(labelPos.x - 2.0f, labelPos.y - 1.0f),
            ImVec2(labelPos.x + labelSize.x + 2.0f, labelPos.y + labelSize.y + 1.0f),
            IM_COL32(20, 20, 20, 200), 3.0f);
        drawList->AddText(labelPos, color, label.c_str());
    }
}

void NexusFlowGraph::RenderLegend(ImDrawList* drawList, ImVec2 pos)
{
    float lineHeight = 16.0f;
    float dotRadius = 5.0f;
    float textOffset = 14.0f;

    // Background
    drawList->AddRectFilled(pos, ImVec2(pos.x + 180.0f, pos.y + lineHeight * 4.5f),
                            IM_COL32(20, 20, 20, 200), 4.0f);

    // Registered client
    drawList->AddCircleFilled(ImVec2(pos.x + dotRadius + 4.0f, pos.y + lineHeight * 0.5f + 2.0f), dotRadius, kColorClientReg);
    drawList->AddText(ImVec2(pos.x + textOffset + 4.0f, pos.y + 2.0f), kColorText, "Registered Client");

    // Pending client
    drawList->AddCircleFilled(ImVec2(pos.x + dotRadius + 4.0f, pos.y + lineHeight * 1.5f + 2.0f), dotRadius, kColorClientUnreg);
    drawList->AddText(ImVec2(pos.x + textOffset + 4.0f, pos.y + lineHeight + 2.0f), kColorText, "Pending Client");

    // Pipe subscription
    drawList->AddLine(ImVec2(pos.x + 4.0f, pos.y + lineHeight * 2.5f + 2.0f),
                      ImVec2(pos.x + dotRadius * 2.0f + 4.0f, pos.y + lineHeight * 2.5f + 2.0f), kColorPipeLine, 2.0f);
    drawList->AddText(ImVec2(pos.x + textOffset + 4.0f, pos.y + lineHeight * 2.0f + 2.0f), kColorText, "Pipe Subscription");

    // Wildcard subscription
    drawList->AddLine(ImVec2(pos.x + 4.0f, pos.y + lineHeight * 3.5f + 2.0f),
                      ImVec2(pos.x + dotRadius * 2.0f + 4.0f, pos.y + lineHeight * 3.5f + 2.0f), kColorWildcardLine, 2.0f);
    drawList->AddText(ImVec2(pos.x + textOffset + 4.0f, pos.y + lineHeight * 3.0f + 2.0f), kColorText, "Wildcard (ANY)");
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------

ImVec2 NexusFlowGraph::CalculateClientPosition(int index, int totalClients, ImVec2 center, float radius) const
{
    if (totalClients <= 0) return center;

    float angle = (2.0f * static_cast<float>(M_PI) * static_cast<float>(index)) / static_cast<float>(totalClients)
                  - static_cast<float>(M_PI) * 0.5f; // Start from top

    float x = center.x + cosf(angle) * radius - m_clientNodeWidth * 0.5f;
    float y = center.y + sinf(angle) * radius - m_clientNodeHeight * 0.5f;

    return ImVec2(x, y);
}