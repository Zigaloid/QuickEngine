// ============================================================
//  NodeGraphVisualizer.cpp
// ============================================================
#include "CoreSystem/CoreSystem.h"

#include "NodeGraphVisualizer.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace ImGuiVisualizers {

// ?? Helpers ????????????????????????????????????????????????????????????
static ImVec2 operator+(ImVec2 a, ImVec2 b) { return { a.x + b.x, a.y + b.y }; }
static ImVec2 operator-(ImVec2 a, ImVec2 b) { return { a.x - b.x, a.y - b.y }; }
static ImVec2 operator*(ImVec2 a, float s)  { return { a.x * s,   a.y * s   }; }
static float  Vec2Len(ImVec2 v) { return std::sqrt(v.x * v.x + v.y * v.y); }

// ?? Construction ???????????????????????????????????????????????????????

NodeGraphVisualizer::NodeGraphVisualizer(const char* name,
                                         ImGuiKey    shortcut,
                                         const char* category)
    : m_name    (name     ? name     : "Node Graph")
    , m_shortcutKey(shortcut)
    , m_category(category ? category : "")
{}

// ?? Graph management ???????????????????????????????????????????????????

int NodeGraphVisualizer::AddNode(std::unique_ptr<NodeGraphNode> node)
{
    node->m_id = m_nextNodeId++;
    int id = node->m_id;
    m_nodes.push_back(std::move(node));
    return id;
}

void NodeGraphVisualizer::RemoveNode(int nodeId)
{
    // Remove all links connected to this node first.
    m_links.erase(
        std::remove_if(m_links.begin(), m_links.end(), [&](const NodeLink& l) {
            return l.outputNodeId == nodeId || l.inputNodeId == nodeId;
        }),
        m_links.end());

    m_nodes.erase(
        std::remove_if(m_nodes.begin(), m_nodes.end(), [&](const std::unique_ptr<NodeGraphNode>& n) {
            return n->GetId() == nodeId;
        }),
        m_nodes.end());

    if (m_selectedNodeId == nodeId) m_selectedNodeId = -1;
    if (m_dragNodeId     == nodeId) m_dragNodeId     = -1;

    if (m_onNodeDeleted) m_onNodeDeleted(nodeId);
}

int NodeGraphVisualizer::AddLink(int outNodeId, int outPinId,
                                  int  inNodeId, int  inPinId)
{
    // Validate nodes exist.
    if (!FindNode(outNodeId) || !FindNode(inNodeId)) return -1;
    // Prevent duplicates.
    if (LinkExists(outPinId, inPinId)) return -1;

    NodeLink lnk;
    lnk.id           = m_nextLinkId++;
    lnk.outputNodeId = outNodeId;
    lnk.outputPinId  = outPinId;
    lnk.inputNodeId  = inNodeId;
    lnk.inputPinId   = inPinId;
    m_links.push_back(lnk);
    UpdatePinConnectedState();
    return lnk.id;
}

void NodeGraphVisualizer::RemoveLink(int linkId)
{
    m_links.erase(
        std::remove_if(m_links.begin(), m_links.end(),
            [&](const NodeLink& l) { return l.id == linkId; }),
        m_links.end());
    UpdatePinConnectedState();
}

void NodeGraphVisualizer::Clear()
{
    m_nodes.clear();
    m_links.clear();
    m_nextNodeId    = 1;
    m_nextLinkId    = 1;
    m_selectedNodeId = -1;
    m_dragNodeId     = -1;
    m_hoveredLinkId  = -1;
    UpdatePinConnectedState();
}

NodeGraphNode* NodeGraphVisualizer::FindNode(int nodeId) const
{
    for (auto& n : m_nodes)
        if (n->GetId() == nodeId) return n.get();
    return nullptr;
}

const NodeLink* NodeGraphVisualizer::FindLink(int linkId) const
{
    for (auto& l : m_links)
        if (l.id == linkId) return &l;
    return nullptr;
}

// ?? View control ??????????????????????????????????????????????????????

void NodeGraphVisualizer::SetZoom(float zoom)
{
    m_zoom = std::max(m_style.zoomMin, std::min(m_style.zoomMax, zoom));
}

void NodeGraphVisualizer::FocusAll()
{
    if (m_nodes.empty()) return;
    ImVec2 mn = m_nodes[0]->GetPosition();
    ImVec2 mx = mn;
    for (auto& n : m_nodes) {
        ImVec2 p = n->GetPosition();
        mn.x = std::min(mn.x, p.x);
        mn.y = std::min(mn.y, p.y);
        mx.x = std::max(mx.x, p.x + NodeWidth(n.get()));
        mx.y = std::max(mx.y, p.y + NodeHeight(n.get()));
    }
    ImVec2 graphCenter = { (mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f };
    ImVec2 canvasCenter = { m_canvasSize.x * 0.5f, m_canvasSize.y * 0.5f };
    m_scrollOffset = { canvasCenter.x - graphCenter.x * m_zoom,
                       canvasCenter.y - graphCenter.y * m_zoom };
}

void NodeGraphVisualizer::FocusOnNode(int nodeId)
{
    NodeGraphNode* n = FindNode(nodeId);
    if (!n) return;
    ImVec2 p = n->GetPosition();
    ImVec2 nodeCenter = { p.x + NodeWidth(n) * 0.5f, p.y + NodeHeight(n) * 0.5f };
    ImVec2 canvasCenter = { m_canvasSize.x * 0.5f, m_canvasSize.y * 0.5f };
    m_scrollOffset = { canvasCenter.x - nodeCenter.x * m_zoom,
                       canvasCenter.y - nodeCenter.y * m_zoom };
}

// ?? IImGuiVisualizer ??????????????????????????????????????????????????

bool NodeGraphVisualizer::Render(bool* isOpen)
{
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    if (!ImGui::Begin(m_name.c_str(), isOpen, flags)) {
        ImGui::End();
        return false;
    }

    RenderToolbar();
    RenderCanvas();

    ImGui::End();
    return true;
}

// ?? Toolbar ???????????????????????????????????????????????????????????

void NodeGraphVisualizer::RenderToolbar()
{
    if (ImGui::Button("Focus All")) FocusAll();
    ImGui::SameLine();
    if (ImGui::Button("Reset Zoom")) SetZoom(1.f);
    ImGui::SameLine();
    ImGui::Text("Zoom: %.0f%%", m_zoom * 100.f);
}

// ?? Canvas ????????????????????????????????????????????????????????????

void NodeGraphVisualizer::RenderCanvas()
{
    ImVec2 canvasPos  = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    if (canvasSize.x < 1.f) canvasSize.x = 1.f;
    if (canvasSize.y < 1.f) canvasSize.y = 1.f;

    m_canvasOrigin = canvasPos;
    m_canvasSize   = canvasSize;

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Background
    dl->AddRectFilled(canvasPos, canvasPos + canvasSize, m_style.colorBackground);

    // Clip drawing to canvas bounds.
    dl->PushClipRect(canvasPos, canvasPos + canvasSize, true);

    DrawGrid(dl, canvasPos);
    DrawLinks(dl);
    DrawNodes(dl);
    DrawDragLink(dl);

    dl->PopClipRect();

    // Invisible button captures all mouse input for the canvas area.
    ImGui::SetCursorScreenPos(canvasPos);
    ImGui::InvisibleButton("##canvas", canvasSize,
        ImGuiButtonFlags_MouseButtonLeft  |
        ImGuiButtonFlags_MouseButtonRight |
        ImGuiButtonFlags_MouseButtonMiddle);
    bool canvasHovered = ImGui::IsItemHovered();

    HandleInput(canvasHovered);
    HandleContextMenus();
}

// ?? Grid ??????????????????????????????????????????????????????????????

void NodeGraphVisualizer::DrawGrid(ImDrawList* dl, ImVec2 /*canvasOrigin*/)
{
    float step = m_style.gridStep * m_zoom;
    if (step < 8.f) return; // Don't draw grid when too zoomed out.

    ImVec2 orig = m_canvasOrigin;
    float  offX = std::fmod(m_scrollOffset.x, step);
    float  offY = std::fmod(m_scrollOffset.y, step);

    for (float x = offX; x < m_canvasSize.x; x += step)
        dl->AddLine({ orig.x + x, orig.y },
                    { orig.x + x, orig.y + m_canvasSize.y },
                    m_style.colorGrid, 1.f);

    for (float y = offY; y < m_canvasSize.y; y += step)
        dl->AddLine({ orig.x,                    orig.y + y },
                    { orig.x + m_canvasSize.x,   orig.y + y },
                    m_style.colorGrid, 1.f);
}

// ?? Layout helpers ????????????????????????????????????????????????????

float NodeGraphVisualizer::NodeWidth(NodeGraphNode* node) const
{
    float w = k_minNodeW;
    // Widen to fit the longest pin label.
    ImFont* font = ImGui::GetFont();
    float  fs    = ImGui::GetFontSize() * m_zoom;
    auto measurePin = [&](const NodePin& p) {
        float tw = font->CalcTextSizeA(fs, FLT_MAX, 0.f, p.name.c_str()).x;
        w = std::max(w, tw + m_style.pinRadius * 3.f + m_style.nodePadding * 2.f);
    };
    for (auto& p : node->GetInputPins())  measurePin(p);
    for (auto& p : node->GetOutputPins()) measurePin(p);
    return w * (1.f / m_zoom); // return in canvas space
}

float NodeGraphVisualizer::NodeHeight(NodeGraphNode* node) const
{
    int rows = static_cast<int>(std::max(node->GetInputPins().size(),
                                         node->GetOutputPins().size()));
    return (k_titleBarH + rows * k_pinRowH + m_style.nodePadding * 2.f) / m_zoom;
}

ImVec2 NodeGraphVisualizer::InputPinPos(NodeGraphNode* node, int pinIndex) const
{
    ImVec2 np = node->GetPosition();
    float x   = np.x;
    float y   = np.y + (k_titleBarH / m_zoom) + m_style.nodePadding / m_zoom
                + (pinIndex + 0.5f) * (k_pinRowH / m_zoom);
    return { x, y };
}

ImVec2 NodeGraphVisualizer::OutputPinPos(NodeGraphNode* node, int pinIndex) const
{
    ImVec2 np = node->GetPosition();
    float x   = np.x + NodeWidth(node);
    float y   = np.y + (k_titleBarH / m_zoom) + m_style.nodePadding / m_zoom
                + (pinIndex + 0.5f) * (k_pinRowH / m_zoom);
    return { x, y };
}

// ?? Data type colour ??????????????????????????????????????????????????

ImU32 NodeGraphVisualizer::DataTypeColor(PinDataType dt) const
{
    switch (dt) {
        case PinDataType::Float:     return IM_COL32(100, 200, 100, 255);
        case PinDataType::Float2:    return IM_COL32(150, 200, 100, 255);
        case PinDataType::Float3:    return IM_COL32(200, 200, 100, 255);
        case PinDataType::Float4:    return IM_COL32(200, 150, 100, 255);
        case PinDataType::Int:       return IM_COL32(100, 150, 200, 255);
        case PinDataType::Bool:      return IM_COL32(200, 100, 100, 255);
        case PinDataType::String:    return IM_COL32(200, 100, 200, 255);
        case PinDataType::Vector:    return IM_COL32(150, 100, 200, 255);
        case PinDataType::Execution: return IM_COL32(255, 255, 255, 255);
        default:                     return m_style.colorPinDefault;
    }
}

// ?? Pin helpers ????????????????????????????????????????????????????????

bool NodeGraphVisualizer::LinkExists(int outPin, int inPin) const
{
    for (auto& l : m_links)
        if (l.outputPinId == outPin && l.inputPinId == inPin) return true;
    return false;
}

void NodeGraphVisualizer::UpdatePinConnectedState()
{
    // Reset all.
    for (auto& n : m_nodes) {
        for (auto& p : n->GetInputPins())  p.connected = false;
        for (auto& p : n->GetOutputPins()) p.connected = false;
    }
    for (auto& lnk : m_links) {
        NodeGraphNode* sn = FindNode(lnk.outputNodeId);
        NodeGraphNode* en = FindNode(lnk.inputNodeId);
        if (sn) { NodePin* p = sn->FindPin(lnk.outputPinId); if (p) p->connected = true; }
        if (en) { NodePin* p = en->FindPin(lnk.inputPinId);  if (p) p->connected = true; }
    }
}

NodePin* NodeGraphVisualizer::HitTestPin(ImVec2 screenPos, NodeGraphNode*& outNode) const
{
    float threshold = m_style.pinRadius + 4.f;
    for (auto& [pinId, info] : m_pinScreenPos) {
        ImVec2 delta = screenPos - info.pos;
        if (Vec2Len(delta) <= threshold) {
            outNode = info.node;
            return outNode ? outNode->FindPin(pinId) : nullptr;
        }
    }
    outNode = nullptr;
    return nullptr;
}

// ?? Bezier link drawing ???????????????????????????????????????????????

void NodeGraphVisualizer::DrawBezierLink(ImDrawList* dl,
                                          ImVec2 p0, ImVec2 p1, ImU32 color)
{
    float dx = std::abs(p1.x - p0.x) * 0.5f;
    ImVec2 cp0 = { p0.x + dx, p0.y };
    ImVec2 cp1 = { p1.x - dx, p1.y };
    dl->AddBezierCubic(p0, cp0, cp1, p1, color, m_style.linkThickness * m_zoom);
}

// ?? Link hover hit-test ????????????????????????????????????????????????

bool NodeGraphVisualizer::PointOnBezier(ImVec2 pt, ImVec2 p0, ImVec2 p1,
                                         float threshold) const
{
    float dx = std::abs(p1.x - p0.x) * 0.5f;
    ImVec2 cp0 = { p0.x + dx, p0.y };
    ImVec2 cp1 = { p1.x - dx, p1.y };

    constexpr int samples = 20;
    ImVec2 prev = p0;
    for (int i = 1; i <= samples; ++i) {
        float t  = static_cast<float>(i) / samples;
        float u  = 1.f - t;
        ImVec2 cur = p0 * (u*u*u)
                   + cp0 * (3.f*u*u*t)
                   + cp1 * (3.f*u*t*t)
                   + p1  * (t*t*t);
        // Point-to-segment distance
        ImVec2 seg = cur - prev;
        float segLen = Vec2Len(seg);
        float dist;
        if (segLen < 1e-4f) {
            dist = Vec2Len(pt - prev);
        } else {
            float tt = std::max(0.f, std::min(1.f,
                ((pt.x - prev.x) * seg.x + (pt.y - prev.y) * seg.y) / (segLen * segLen)));
            ImVec2 proj = { prev.x + tt * seg.x, prev.y + tt * seg.y };
            dist = Vec2Len(pt - proj);
        }
        if (dist <= threshold) return true;
        prev = cur;
    }
    return false;
}

int NodeGraphVisualizer::HitTestLink(ImVec2 screenPos) const
{
    float threshold = 8.f;
    for (auto& lnk : m_links) {
        auto it0 = m_pinScreenPos.find(lnk.outputPinId);
        auto it1 = m_pinScreenPos.find(lnk.inputPinId);
        if (it0 == m_pinScreenPos.end() || it1 == m_pinScreenPos.end()) continue;
        if (PointOnBezier(screenPos, it0->second.pos, it1->second.pos, threshold))
            return lnk.id;
    }
    return -1;
}

// ?? Drawing ???????????????????????????????????????????????????????????

void NodeGraphVisualizer::DrawPin(ImDrawList* dl, NodePin& pin, bool hovered)
{
    ImU32 color = hovered           ? m_style.colorPinHover
                : pin.connected     ? DataTypeColor(pin.dataType)
                                    : m_style.colorPinDefault;
    ImU32 border = IM_COL32(255, 255, 255, 200);

    float r = m_style.pinRadius;
    ImVec2 sp = pin.screenPos;
    if (pin.connected)
        dl->AddCircleFilled(sp, r, color);
    else
        dl->AddCircle(sp, r, color, 12, 1.5f);
    if (hovered)
        dl->AddCircle(sp, r + 2.f, border, 12, 1.5f);
}

void NodeGraphVisualizer::DrawLinks(ImDrawList* dl)
{
    for (auto& lnk : m_links) {
        auto it0 = m_pinScreenPos.find(lnk.outputPinId);
        auto it1 = m_pinScreenPos.find(lnk.inputPinId);
        if (it0 == m_pinScreenPos.end() || it1 == m_pinScreenPos.end()) continue;

        bool hovered = (lnk.id == m_hoveredLinkId);
        ImU32 color  = hovered ? m_style.colorLinkHover : m_style.colorLinkDefault;

        // Tint the link with the output pin's data-type colour.
        NodeGraphNode* outNode = it0->second.node;
        if (outNode) {
            NodePin* outPin = outNode->FindPin(lnk.outputPinId);
            if (outPin && outPin->dataType != PinDataType::Unknown)
                color = hovered ? m_style.colorLinkHover : DataTypeColor(outPin->dataType);
        }
        DrawBezierLink(dl, it0->second.pos, it1->second.pos, color);
    }
}

void NodeGraphVisualizer::DrawDragLink(ImDrawList* dl)
{
    if (!m_isDraggingLink) return;
    auto it = m_pinScreenPos.find(m_dragSourcePinId);
    if (it == m_pinScreenPos.end()) return;

    ImVec2 src = it->second.pos;
    ImVec2 dst = ImGui::GetMousePos();

    if (m_dragSourceDir == PinDirection::Input)
        std::swap(src, dst);

    DrawBezierLink(dl, src, dst, m_style.colorLinkDrag);
}

void NodeGraphVisualizer::DrawNodes(ImDrawList* dl)
{
    m_pinScreenPos.clear();

    // Draw back-to-front so the selected node is on top.
    // Simple approach: draw unselected nodes first, selected last.
    for (auto& n : m_nodes)
        if (n->GetId() != m_selectedNodeId) DrawSingleNode(dl, n.get());

    for (auto& n : m_nodes)
        if (n->GetId() == m_selectedNodeId) DrawSingleNode(dl, n.get());
}

void NodeGraphVisualizer::DrawSingleNode(ImDrawList* dl, NodeGraphNode* node)
{
    float  nw   = NodeWidth(node)  * m_zoom;
    float  nh   = NodeHeight(node) * m_zoom;
    ImVec2 sp   = CanvasToScreen(node->GetPosition());
    ImVec2 ep   = { sp.x + nw, sp.y + nh };
    float  rnd  = m_style.nodeRounding;

    bool isSelected = (node->GetId() == m_selectedNodeId);
    bool isHovered  = (node->GetId() == m_hoveredNodeId);

    ImU32 bgColor    = isSelected ? m_style.colorNodeSelected
                     : isHovered  ? m_style.colorNodeHover
                                  : m_style.colorNodeBg;
    ImU32 borderColor = isSelected ? IM_COL32(120, 160, 200, 255)
                      : isHovered  ? IM_COL32(200, 200, 255, 200)
                                   : m_style.colorNodeBorder;

    // Node body
    dl->AddRectFilled(sp, ep, bgColor, rnd);
    dl->AddRect      (sp, ep, borderColor, rnd, 0, isSelected ? 2.f : 1.f);

    // Title bar
    ImVec2 tbEnd = { ep.x, sp.y + k_titleBarH * m_zoom };
    dl->AddRectFilled(sp, tbEnd, m_style.colorNodeTitle, rnd,
                      ImDrawFlags_RoundCornersTop);

    // Title text
    float titleFontSz = std::max(8.f, ImGui::GetFontSize() * m_zoom);
    dl->AddText(ImGui::GetFont(), titleFontSz,
                { sp.x + m_style.nodePadding * m_zoom, sp.y + (k_titleBarH * m_zoom - titleFontSz) * 0.5f },
                m_style.colorText, node->GetTitle());

    float pinFontSz = std::max(7.f, ImGui::GetFontSize() * m_zoom * 0.88f);
    float pinR      = m_style.pinRadius;

    // Input pins
    for (int i = 0; i < static_cast<int>(node->GetInputPins().size()); ++i) {
        NodePin& pin   = node->GetInputPins()[i];
        ImVec2 canvasP = InputPinPos(node, i);
        ImVec2 screenP = CanvasToScreen(canvasP);
        pin.screenPos  = screenP;

        m_pinScreenPos[pin.id] = { screenP, node };

        NodeGraphNode* hNode = nullptr;
        NodePin* hPin = HitTestPin(ImGui::GetMousePos(), hNode);
        bool hovered = (hPin && hPin->id == pin.id);

        DrawPin(dl, pin, hovered);

        // Pin label to the right of the circle
        dl->AddText(ImGui::GetFont(), pinFontSz,
                    { screenP.x + pinR + 4.f,
                      screenP.y - pinFontSz * 0.5f },
                    m_style.colorText, pin.name.c_str());
    }

    // Output pins
    for (int i = 0; i < static_cast<int>(node->GetOutputPins().size()); ++i) {
        NodePin& pin   = node->GetOutputPins()[i];
        ImVec2 canvasP = OutputPinPos(node, i);
        ImVec2 screenP = CanvasToScreen(canvasP);
        pin.screenPos  = screenP;

        m_pinScreenPos[pin.id] = { screenP, node };

        NodeGraphNode* hNode = nullptr;
        NodePin* hPin = HitTestPin(ImGui::GetMousePos(), hNode);
        bool hovered = (hPin && hPin->id == pin.id);

        DrawPin(dl, pin, hovered);

        // Pin label to the left of the circle
        float labelW = ImGui::GetFont()->CalcTextSizeA(
                           pinFontSz, FLT_MAX, 0.f, pin.name.c_str()).x;
        dl->AddText(ImGui::GetFont(), pinFontSz,
                    { screenP.x - pinR - 4.f - labelW,
                      screenP.y - pinFontSz * 0.5f },
                    m_style.colorText, pin.name.c_str());
    }

    // Custom content: give the node a chance to push extra ImGui widgets.
    // We temporarily set the cursor to a position inside the node body.
    ImVec2 contentCursor = { sp.x + m_style.nodePadding * m_zoom,
                              tbEnd.y + m_style.nodePadding * m_zoom };
    ImGui::SetCursorScreenPos(contentCursor);
    node->OnDrawContent();
}

// ?? Input handling ????????????????????????????????????????????????????

void NodeGraphVisualizer::HandleInput(bool canvasHovered)
{
    ImGuiIO& io = ImGui::GetIO();

    // ?? Zoom (scroll wheel over canvas) ????????????????????????????????
    if (canvasHovered && io.MouseWheel != 0.f) {
        float factor = (io.MouseWheel > 0.f) ? m_style.zoomStepFactor
                                              : 1.f / m_style.zoomStepFactor;
        ImVec2 mouseCanvas = ScreenToCanvas(io.MousePos);
        float  newZoom     = std::max(m_style.zoomMin,
                             std::min(m_style.zoomMax, m_zoom * factor));
        // Zoom toward mouse cursor.
        m_scrollOffset.x += mouseCanvas.x * (m_zoom - newZoom);
        m_scrollOffset.y += mouseCanvas.y * (m_zoom - newZoom);
        m_zoom = newZoom;
    }

    // ?? Pan (middle-mouse or Alt + left-drag) ??????????????????????????
    bool panButton = ImGui::IsMouseDown(ImGuiMouseButton_Middle) ||
                     (ImGui::IsMouseDown(ImGuiMouseButton_Left) && io.KeyAlt);

    if (canvasHovered && panButton && !m_isPanning) {
        m_isPanning      = true;
        m_panStartMouse  = io.MousePos;
        m_panStartOffset = m_scrollOffset;
    }
    if (m_isPanning) {
        if (panButton) {
            m_scrollOffset.x = m_panStartOffset.x + (io.MousePos.x - m_panStartMouse.x);
            m_scrollOffset.y = m_panStartOffset.y + (io.MousePos.y - m_panStartMouse.y);
        } else {
            m_isPanning = false;
        }
    }

    // ?? Node drag / selection ??????????????????????????????????????????
    HandleNodeDrag();

    // ?? Link drag / pin interactions ???????????????????????????????????
    HandlePinInteractions(canvasHovered);

    // ?? Link hover ?????????????????????????????????????????????????????
    HandleLinkHover();
}

void NodeGraphVisualizer::HandleNodeDrag()
{
    ImGuiIO& io = ImGui::GetIO();
    ImVec2   mousePos = io.MousePos;

    // Start dragging a node on left-click (not over a pin).
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.KeyAlt) {
        NodeGraphNode* hNode = nullptr;
        HitTestPin(mousePos, hNode); // just to check if over a pin
        // Only begin node drag if we are NOT over a pin.
        NodePin* hPin = HitTestPin(mousePos, hNode);
        if (!hPin) {
            // Hit-test nodes (top-to-bottom iteration, last wins = topmost drawn).
            m_hoveredNodeId = -1;
            for (auto& n : m_nodes) {
                ImVec2 sp = CanvasToScreen(n->GetPosition());
                ImVec2 ep = { sp.x + NodeWidth(n.get()) * m_zoom,
                              sp.y + NodeHeight(n.get()) * m_zoom };
                if (mousePos.x >= sp.x && mousePos.x <= ep.x &&
                    mousePos.y >= sp.y && mousePos.y <= ep.y) {
                    m_hoveredNodeId = n->GetId();
                }
            }
            if (m_hoveredNodeId != -1) {
                m_selectedNodeId = m_hoveredNodeId;
                m_dragNodeId     = m_hoveredNodeId;
                NodeGraphNode* dn = FindNode(m_dragNodeId);
                ImVec2 canvasP = dn->GetPosition();
                ImVec2 screenP = CanvasToScreen(canvasP);
                m_dragNodeOffset = { mousePos.x - screenP.x, mousePos.y - screenP.y };
            } else {
                // Clicked on empty canvas.
                m_selectedNodeId = -1;
            }
        }
    }

    if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) && m_dragNodeId != -1) {
        NodeGraphNode* dn = FindNode(m_dragNodeId);
        if (dn) {
            ImVec2 newScreen = { mousePos.x - m_dragNodeOffset.x,
                                 mousePos.y - m_dragNodeOffset.y };
            dn->SetPosition(ScreenToCanvas(newScreen));
        }
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        m_dragNodeId = -1;
    }

    // Update hovered node each frame.
    m_hoveredNodeId = -1;
    for (auto& n : m_nodes) {
        ImVec2 sp = CanvasToScreen(n->GetPosition());
        ImVec2 ep = { sp.x + NodeWidth(n.get()) * m_zoom,
                      sp.y + NodeHeight(n.get()) * m_zoom };
        if (io.MousePos.x >= sp.x && io.MousePos.x <= ep.x &&
            io.MousePos.y >= sp.y && io.MousePos.y <= ep.y) {
            m_hoveredNodeId = n->GetId();
        }
    }
}

void NodeGraphVisualizer::HandlePinInteractions(bool canvasHovered)
{
    if (!canvasHovered && !m_isDraggingLink) return;

    ImVec2 mousePos = ImGui::GetMousePos();
    NodeGraphNode* hNode = nullptr;
    NodePin*       hPin  = HitTestPin(mousePos, hNode);

    // Start drag from a pin on left-click.
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && hPin && hNode) {
        TryBeginLinkFrom(hPin, hNode);
    }

    // Complete or cancel link drag on release.
    if (m_isDraggingLink && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        if (hPin && hNode) {
            TryCompleteLinkAt(hPin, hNode);
        } else {
            CancelLinkDrag();
        }
    }
}

bool NodeGraphVisualizer::TryBeginLinkFrom(NodePin* pin, NodeGraphNode* node)
{
    m_isDraggingLink   = true;
    m_dragSourcePinId  = pin->id;
    m_dragSourceNodeId = node->GetId();
    m_dragSourceDir    = pin->direction;
    m_dragNodeId       = -1; // don't drag the node while dragging a link
    return true;
}

bool NodeGraphVisualizer::TryCompleteLinkAt(NodePin* pin, NodeGraphNode* node)
{
    // Capture source state before CancelLinkDrag() clears it.
    const int          srcPinId  = m_dragSourcePinId;
    const int          srcNodeId = m_dragSourceNodeId;
    const PinDirection srcDir    = m_dragSourceDir;
    CancelLinkDrag();

    if (pin->id == srcPinId)       return false; // same pin
    if (node->GetId() == srcNodeId) return false; // same node

    // Determine which is output, which is input.
    int outNode, outPin, inNode, inPin;

    if (srcDir == PinDirection::Output && pin->direction == PinDirection::Input) {
        outNode = srcNodeId;      outPin = srcPinId;
        inNode  = node->GetId(); inPin  = pin->id;
    } else if (srcDir == PinDirection::Input && pin->direction == PinDirection::Output) {
        outNode = node->GetId(); outPin = pin->id;
        inNode  = srcNodeId;      inPin  = srcPinId;
    } else {
        return false; // same direction – invalid
    }

    int linkId = AddLink(outNode, outPin, inNode, inPin);
    if (linkId != -1 && m_onLinkCreated) {
        if (const NodeLink* lnk = FindLink(linkId))
            m_onLinkCreated(*lnk);
    }
    return linkId != -1;
}

void NodeGraphVisualizer::CancelLinkDrag()
{
    m_isDraggingLink   = false;
    m_dragSourcePinId  = -1;
    m_dragSourceNodeId = -1;
}

void NodeGraphVisualizer::HandleLinkHover()
{
    if (m_isDraggingLink) { m_hoveredLinkId = -1; return; }
    m_hoveredLinkId = HitTestLink(ImGui::GetMousePos());
}

// ?? Context menus ?????????????????????????????????????????????????????

void NodeGraphVisualizer::HandleContextMenus()
{
    // Node context menu
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !m_isDraggingLink) {
        m_contextNodeId = m_hoveredNodeId;
        m_contextLinkId = m_hoveredLinkId;

        if (m_contextNodeId != -1)
            ImGui::OpenPopup("##NodeCtx");
        else if (m_contextLinkId != -1)
            ImGui::OpenPopup("##LinkCtx");
        else if (m_onCanvasContextMenu) {
            m_contextMenuCanvasPos = ScreenToCanvas(ImGui::GetMousePos());
            ImGui::OpenPopup("##CanvasCtx");
        }
    }

    if (ImGui::BeginPopup("##NodeCtx")) {
        NodeGraphNode* n = FindNode(m_contextNodeId);
        if (n) {
            ImGui::TextDisabled("%s", n->GetTitle());
            ImGui::Separator();
            if (m_onNodeContextMenu)
                m_onNodeContextMenu(n->GetId(), n->GetTitle());
            if (ImGui::MenuItem("Delete Node")) {
                RemoveNode(m_contextNodeId);
                m_contextNodeId = -1;
            }
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("##LinkCtx")) {
        ImGui::TextDisabled("Link #%d", m_contextLinkId);
        ImGui::Separator();
        if (ImGui::MenuItem("Delete Link")) {
            if (m_onLinkDeleted) m_onLinkDeleted(m_contextLinkId);
            RemoveLink(m_contextLinkId);
            m_contextLinkId = -1;
        }
        ImGui::EndPopup();
    }

    if (m_onCanvasContextMenu && ImGui::BeginPopup("##CanvasCtx")) {
        m_onCanvasContextMenu(m_contextMenuCanvasPos);
        ImGui::EndPopup();
    }
}

} // namespace ImGuiVisualizers
