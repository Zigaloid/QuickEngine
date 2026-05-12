#pragma once
// ============================================================
//  NodeGraphNode.h
//  Base node and pin types for the ImGui Node Graph Visualizer.
//
//  Nodes derive from CReflectedBase so they participate in the
//  engine's reflection / serialization system.
// ============================================================

#include "Reflection/ReflectionBase.h"
#include "imgui/imgui.h"

#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace ImGuiVisualizers {

// ?? Pin type identifier ????????????????????????????????????????????????
// Extend this enum (or cast an integer) to represent domain-specific data
// types; the visualizer will colour pins according to their type id.
enum class PinDataType : uint32_t {
    Unknown   = 0,
    Float     = 1,
    Float2    = 2,
    Float3    = 3,
    Float4    = 4,
    Int       = 5,
    Bool      = 6,
    String    = 7,
    Vector    = 8,
    Execution = 9,
    // Values >= 100 are available for user-defined types.
};

// ?? Pin direction ??????????????????????????????????????????????????????
enum class PinDirection { Input, Output };

// ?? NodePin ???????????????????????????????????????????????????????????
/**
 * @brief Represents one connection point on a node.
 *
 * Pins are owned by a NodeGraphNode; external code creates them via
 * NodeGraphNode::AddInputPin / AddOutputPin.
 */
struct NodePin {
    int         id        = -1;
    std::string name;
    PinDirection direction = PinDirection::Input;
    PinDataType  dataType  = PinDataType::Unknown;

    // Runtime state filled in by the visualizer during drawing.
    ImVec2 screenPos  = { 0.f, 0.f };
    bool   connected  = false;

    NodePin() = default;
    NodePin(int anId, std::string aName, PinDirection aDir, PinDataType aType)
        : id(anId), name(std::move(aName)), direction(aDir), dataType(aType) {}
};

// ?? NodeGraphNode ?????????????????????????????????????????????????????
/**
 * @brief Abstract base for all nodes in a NodeGraphVisualizer graph.
 *
 * Derive from this class and use REFL_DECLARE_OBJECT / REFL_DEFINE_OBJECT
 * to expose serialisable properties through the engine reflection system.
 *
 * Minimal concrete example:
 * @code
 *   class MyNode : public NodeGraphNode {
 *   public:
 *       REFL_DECLARE_OBJECT(MyNode, NodeGraphNode)
 *       MyNode() { AddOutputPin("Result", PinDataType::Float); }
 *   private:
 *       float myValue = 0.f;
 *   };
 *   REFL_DEFINE_OBJECT(MyNode)
 *       REFL_DEFINE_FLOAT_MEMBER(MyNode, myValue),
 *   REFL_DEFINE_END
 * @endcode
 */
class NodeGraphNode : public CReflectedBase {
public:
    // ?? Identity ???????????????????????????????????????????????????????
    int         GetId()    const { return m_id; }
    const char* GetTitle() const { return m_title.c_str(); }
    void        SetTitle(const std::string& title) { m_title = title; }

    // ?? Canvas position ????????????????????????????????????????????????
    ImVec2 GetPosition() const { return m_position; }
    void   SetPosition(ImVec2 pos) { m_position = pos; }

    // ?? Pin access ?????????????????????????????????????????????????????
    std::vector<NodePin>& GetInputPins()        { return m_inputPins; }
    std::vector<NodePin>& GetOutputPins()       { return m_outputPins; }
    const std::vector<NodePin>& GetInputPins()  const { return m_inputPins; }
    const std::vector<NodePin>& GetOutputPins() const { return m_outputPins; }

    NodePin* FindPin(int pinId) {
        for (auto& p : m_inputPins)  if (p.id == pinId) return &p;
        for (auto& p : m_outputPins) if (p.id == pinId) return &p;
        return nullptr;
    }

    // ?? Optional per-frame user callback ??????????????????????????????
    // Called by the visualizer after the node body has been drawn so that
    // derived types or external code can push custom ImGui widgets inside
    // the node content area.
    virtual void OnDrawContent() {}

protected:
    // Protected constructor: nodes are created via subclasses or factory.
    NodeGraphNode() = default;
    explicit NodeGraphNode(const std::string& title) : m_title(title) {}

    /**
     * @brief Register an input pin. Call from the subclass constructor.
     * @return The assigned pin id.
     */
    int AddInputPin(const std::string& name,
                    PinDataType dataType = PinDataType::Unknown) {
        int id = s_nextPinId++;
        m_inputPins.emplace_back(id, name, PinDirection::Input, dataType);
        return id;
    }

    /**
     * @brief Register an output pin. Call from the subclass constructor.
     * @return The assigned pin id.
     */
    int AddOutputPin(const std::string& name,
                     PinDataType dataType = PinDataType::Unknown) {
        int id = s_nextPinId++;
        m_outputPins.emplace_back(id, name, PinDirection::Output, dataType);
        return id;
    }

private:
    friend class NodeGraphVisualizer; // visualizer assigns the node id

    int         m_id       = -1;
    std::string m_title;
    ImVec2      m_position = { 100.f, 100.f };

    std::vector<NodePin> m_inputPins;
    std::vector<NodePin> m_outputPins;

    // Simple global pin-id counter; unique across all NodeGraphNode instances.
    inline static int s_nextPinId = 1;
};

// ?? NodeLink ??????????????????????????????????????????????????????????
/**
 * @brief Describes a directed connection between an output pin and an
 *        input pin.
 */
struct NodeLink {
    int id          = -1;
    int outputNodeId = -1;
    int outputPinId  = -1;
    int inputNodeId  = -1;
    int inputPinId   = -1;
};

} // namespace ImGuiVisualizers
