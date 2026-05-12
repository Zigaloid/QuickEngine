#pragma once
// ============================================================
//  NodeGraphVisualizer.h
//  IImGuiVisualizer that renders an interactive node-graph
//  canvas using only Dear ImGui draw-list primitives.
// ============================================================

#include "IImGuiVisualizer.h"
#include "NodeGraphNode.h"

#include "imgui/imgui.h"

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>

namespace ImGuiVisualizers {

/**
 * @brief An IImGuiVisualizer that renders an interactive node graph.
 *
 * Usage:
 * @code
 *   auto* vis = new NodeGraphVisualizer("My Graph");
 *   manager.Register(vis);
 *
 *   // Add a node
 *   auto node = std::make_unique<MyNode>();
 *   int nodeId = vis->AddNode(std::move(node));
 *
 *   // Connect pins
 *   vis->AddLink(outNodeId, outPinId, inNodeId, inPinId);
 * @endcode
 *
 * The visualizer supports:
 *  - Pan (middle-mouse or Alt+left-drag)
 *  - Zoom (scroll wheel)
 *  - Node drag
 *  - Link creation by dragging from an output pin to an input pin
 *  - Link deletion (right-click a link)
 *  - Optional callbacks for link creation/deletion events
 */
class NodeGraphVisualizer : public IImGuiVisualizer {
public:
    // ?? Callbacks ??????????????????????????????????????????????????????
    /// Called when the user successfully draws a new link.
    using OnLinkCreated = std::function<void(const NodeLink&)>;
    /// Called when the user deletes a link (right-click).
    using OnLinkDeleted = std::function<void(int linkId)>;
    /// Called when the user right-clicks a node; lets hosts show a context menu.
    using OnNodeContextMenu = std::function<void(int nodeId, const std::string& title)>;

    // ?? Construction ???????????????????????????????????????????????????
    explicit NodeGraphVisualizer(const char* name     = "Node Graph",
                                 ImGuiKey    shortcut = ImGuiKey_None,
                                 const char* category = "Visualizers");
    ~NodeGraphVisualizer() override = default;

    // ?? IImGuiVisualizer ???????????????????????????????????????????????
    void        Initialize()              override {}
    void        Shutdown()                override {}
    void        Update(float /*dt*/)      override {}
    bool        Render(bool* isOpen)      override;
    const char* GetName()           const override { return m_name.c_str(); }
    ImGuiKey    GetShortcut()       const override { return m_shortcutKey; }
    const char* GetMenuCategory()   const override { return m_category.empty() ? nullptr : m_category.c_str(); }

    // ?? Graph management ???????????????????????????????????????????????

    /**
     * @brief Add a node to the graph.
     * @param node  Ownership transfers to the visualizer.
     * @return      The node's assigned id.
     */
    int  AddNode(std::unique_ptr<NodeGraphNode> node);

    /**
     * @brief Remove a node (and all links connected to it) by id.
     */
    void RemoveNode(int nodeId);

    /**
     * @brief Create a link between an output pin and an input pin.
     * @return The assigned link id, or -1 if the connection is invalid.
     */
    int  AddLink(int outputNodeId, int outputPinId,
                 int inputNodeId,  int inputPinId);

    /** @brief Remove a link by id. */
    void RemoveLink(int linkId);

    /** @brief Remove all nodes and links. */
    void Clear();

    // ?? Queries ????????????????????????????????????????????????????????
    NodeGraphNode* FindNode(int nodeId) const;
    const NodeLink* FindLink(int linkId) const;
    const std::vector<NodeLink>& GetLinks() const { return m_links; }

    // ?? View control ??????????????????????????????????????????????????
    void FocusAll();
    void FocusOnNode(int nodeId);
    void SetZoom(float zoom);
    float GetZoom() const { return m_zoom; }

    // ?? Callbacks ?????????????????????????????????????????????????????
    void SetOnLinkCreated    (OnLinkCreated     cb) { m_onLinkCreated     = std::move(cb); }
    void SetOnLinkDeleted    (OnLinkDeleted     cb) { m_onLinkDeleted     = std::move(cb); }
    void SetOnNodeContextMenu(OnNodeContextMenu cb) { m_onNodeContextMenu = std::move(cb); }

    // ?? Style tweaks ???????????????????????????????????????????????????
    struct Style {
        float  nodeRounding      = 4.f;
        float  nodePadding       = 8.f;
        float  pinRadius         = 5.f;
        float  linkThickness     = 2.5f;
        float  gridStep          = 64.f;
        float  zoomMin           = 0.2f;
        float  zoomMax           = 4.0f;
        float  zoomStepFactor    = 1.15f;
        ImU32  colorGrid         = IM_COL32(60,  60,  60,  255);
        ImU32  colorBackground   = IM_COL32(30,  30,  30,  255);
        ImU32  colorNodeBg       = IM_COL32(55,  55,  65,  255);
        ImU32  colorNodeBorder   = IM_COL32(100, 100, 110, 255);
        ImU32  colorNodeTitle    = IM_COL32(70,  70,  80,  255);
        ImU32  colorNodeHover    = IM_COL32(80,  80,  90,  255);
        ImU32  colorNodeSelected = IM_COL32(80,  120, 160, 255);
        ImU32  colorPinDefault   = IM_COL32(150, 150, 200, 255);
        ImU32  colorPinConnected = IM_COL32(100, 200, 100, 255);
        ImU32  colorPinHover     = IM_COL32(220, 220, 255, 255);
        ImU32  colorLinkDefault  = IM_COL32(180, 180, 180, 255);
        ImU32  colorLinkHover    = IM_COL32(255, 200, 100, 255);
        ImU32  colorLinkDrag     = IM_COL32(200, 200,  50, 255);
        ImU32  colorText         = IM_COL32(220, 220, 220, 255);
    };
    Style& GetStyle() { return m_style; }

private:
    // ?? Internal helpers ???????????????????????????????????????????????
    void RenderToolbar();
    void RenderCanvas();

    // Drawing
    void DrawGrid(ImDrawList* dl, ImVec2 canvasOrigin);
    void DrawLinks(ImDrawList* dl);
    void DrawNodes(ImDrawList* dl);
    void DrawDragLink(ImDrawList* dl);
    void DrawSingleNode(ImDrawList* dl, NodeGraphNode* node);

    float  NodeWidth(NodeGraphNode* node) const;
    float  NodeHeight(NodeGraphNode* node) const;
    ImVec2 InputPinPos (NodeGraphNode* node, int pinIndex) const;
    ImVec2 OutputPinPos(NodeGraphNode* node, int pinIndex) const;
    void   DrawPin(ImDrawList* dl, NodePin& pin, bool hovered);
    void   DrawBezierLink(ImDrawList* dl, ImVec2 p0, ImVec2 p1, ImU32 color);

    // Coordinate helpers
    ImVec2 CanvasToScreen(ImVec2 c) const {
        return { c.x * m_zoom + m_canvasOrigin.x + m_scrollOffset.x,
                 c.y * m_zoom + m_canvasOrigin.y + m_scrollOffset.y };
    }
    ImVec2 ScreenToCanvas(ImVec2 s) const {
        return { (s.x - m_canvasOrigin.x - m_scrollOffset.x) / m_zoom,
                 (s.y - m_canvasOrigin.y - m_scrollOffset.y) / m_zoom };
    }

    // Interaction
    void HandleInput(bool canvasHovered);
    void HandleNodeDrag();
    void HandleLinkDrag(bool canvasHovered);
    void HandleLinkHover();
    void HandlePinInteractions(bool canvasHovered);
    void HandleContextMenus();
    bool TryBeginLinkFrom(NodePin* pin, NodeGraphNode* node);
    bool TryCompleteLinkAt(NodePin* pin, NodeGraphNode* node);
    void CancelLinkDrag();
    NodePin* HitTestPin(ImVec2 screenPos, NodeGraphNode*& outNode) const;
    int      HitTestLink(ImVec2 screenPos) const;
    bool     PointOnBezier(ImVec2 p, ImVec2 p0, ImVec2 p1, float threshold) const;
    ImU32    DataTypeColor(PinDataType dt) const;
    bool     LinkExists(int outPin, int inPin) const;
    void     UpdatePinConnectedState();

    // Pin screen-position cache (rebuilt each frame during DrawNodes)
    struct PinScreenInfo { ImVec2 pos; NodeGraphNode* node; };
    std::unordered_map<int, PinScreenInfo> m_pinScreenPos; // pin id -> info

    // ?? Data ??????????????????????????????????????????????????????????
    std::string  m_name;
    ImGuiKey     m_shortcutKey = ImGuiKey_None;
    std::string  m_category;

    std::vector<std::unique_ptr<NodeGraphNode>> m_nodes;
    std::vector<NodeLink> m_links;
    int m_nextNodeId = 1;
    int m_nextLinkId = 1;

    // Canvas state
    ImVec2 m_canvasOrigin  = { 0.f, 0.f }; // screen-space top-left of canvas
    ImVec2 m_canvasSize    = { 0.f, 0.f };
    ImVec2 m_scrollOffset  = { 0.f, 0.f }; // pan offset (screen pixels)
    float  m_zoom          = 1.f;

    // Interaction state
    int    m_hoveredNodeId  = -1;
    int    m_selectedNodeId = -1;
    int    m_dragNodeId     = -1;
    ImVec2 m_dragNodeOffset = { 0.f, 0.f };
    bool   m_isPanning      = false;
    ImVec2 m_panStartMouse  = { 0.f, 0.f };
    ImVec2 m_panStartOffset = { 0.f, 0.f };

    // Link drag state
    bool         m_isDraggingLink   = false;
    int          m_dragSourcePinId  = -1;
    int          m_dragSourceNodeId = -1;
    PinDirection m_dragSourceDir    = PinDirection::Output;

    // Hovered link (for highlight + right-click deletion)
    int  m_hoveredLinkId = -1;

    // Context menu state
    int  m_contextNodeId = -1;
    int  m_contextLinkId = -1;

    // Style
    Style m_style;

    // Callbacks
    OnLinkCreated      m_onLinkCreated;
    OnLinkDeleted      m_onLinkDeleted;
    OnNodeContextMenu  m_onNodeContextMenu;

    // Layout constants
    static constexpr float k_titleBarH   = 26.f;
    static constexpr float k_pinRowH     = 20.f;
    static constexpr float k_minNodeW    = 140.f;
};

} // namespace ImGuiVisualizers
