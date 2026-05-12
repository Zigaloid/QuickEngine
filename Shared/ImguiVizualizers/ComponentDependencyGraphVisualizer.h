#pragma once
// ============================================================
//  ComponentDependencyGraphVisualizer.h
//
//  A NodeGraphVisualizer that reads a serialised
//  ComponentDependencyDefinitionList and displays each
//  component type as a node, with directed edges representing
//  "must-complete-before" relationships.
//
//  Usage:
//    auto* vis = new ComponentDependencyGraphVisualizer();
//    manager.Register(vis);
//    vis->LoadFromFile("Data/component_dependencies.json");
// ============================================================

#include "NodeGraphVisualizer.h"
#include "ComponentSystem/ComponentDependencyDefinition.h"
#include "ClassFactory/ClassFactory.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace ImGuiVisualizers {

// ?? ComponentNode ?????????????????????????????????????????????????????
/**
 * @brief A NodeGraphNode that represents a single component type.
 *
 * Has one execution-type input pin ("In") and one execution-type output
 * pin ("Out") so that dependency arrows can flow left-to-right.
 * The component class name is stored as the node title and is also
 * exposed to the reflection system so it round-trips through JSON.
 */
class ComponentNode : public NodeGraphNode {
public:
    REFL_DECLARE_OBJECT(ComponentNode, NodeGraphNode)

    ComponentNode() { Init(""); }
    explicit ComponentNode(const std::string& componentClassName)
    {
        Init(componentClassName);
    }

    const std::string& GetComponentClassName() const { return m_componentClassName; }

    /** Pin ids are stable after construction. */
    int GetInputPinId()  const { return m_inputPinId;  }
    int GetOutputPinId() const { return m_outputPinId; }

private:
    void Init(const std::string& className)
    {
        m_componentClassName = className;
        SetTitle(className);
        m_inputPinId  = AddInputPin ("In",  PinDataType::Execution);
        m_outputPinId = AddOutputPin("Out", PinDataType::Execution);
    }

    std::string m_componentClassName;
    int         m_inputPinId  = -1;
    int         m_outputPinId = -1;
};

// ?? ComponentDependencyGraphVisualizer ????????????????????????????????
/**
 * @brief Visualizes a ComponentDependencyDefinitionList as a directed
 *        acyclic node graph.
 *
 * Each unique component class name becomes a node.  An edge
 *   dependency ? dependent
 * is drawn for every definition, reflecting "dependency must finish
 * before dependent starts".
 *
 * Call LoadFromFile() with the path to a JSON file that was serialised
 * from ComponentDependencyDefinitionList, or call BuildGraph() directly
 * with an already-populated list object.
 */
class ComponentDependencyGraphVisualizer : public NodeGraphVisualizer {
public:
    explicit ComponentDependencyGraphVisualizer(
        const char* name     = "Component Dependencies",
        ImGuiKey    shortcut = ImGuiKey_None,
        const char* category = "Visualizers");

    ~ComponentDependencyGraphVisualizer() override = default;

    // ?? IImGuiVisualizer overrides ????????????????????????????????????
    bool Render(bool* isOpen) override;

    // ?? Public API ????????????????????????????????????????????????????

    /**
     * @brief Serialise-reads a ComponentDependencyDefinitionList from the
     *        given JSON file path and rebuilds the graph.
     * @param filePath  Absolute or engine-relative path to the JSON file.
     * @return true on success; on failure an error message is stored and
     *         displayed inside the window.
     */
    bool LoadFromFile(const std::string& filePath);

    /**
     * @brief Rebuilds the graph from an already-populated definition list.
     *        Clears any previously displayed graph first.
     */
    void BuildGraph(const ComponentDependencyDefinitionList& list);

    /** @brief Returns the last error message, or empty string if none. */
    const std::string& GetLastError() const { return m_lastError; }

    /**
     * @brief Save the current graph state back to the loaded file path.
     * @return true on success.
     */
    bool SaveToFile();

    /**
     * @brief Add a single dependency edge to the live graph.
     *        Gets-or-creates nodes as needed and marks the graph dirty.
     * @param dependsOn  Class that must complete first (source / left node).
     * @param dependent  Class that waits for it      (target / right node).
     * @return true if a new link was actually created (false if it already exists).
     */
    bool AddDependency(const std::string& dependsOn, const std::string& dependent);

private:
    void RenderLoadBar();
    void RenderEditBar();
    void AutoLayout();

    /// Gets-or-creates a ComponentNode for the given class name.
    int GetOrCreateNode(const std::string& className);

    /// Rebuild the registered-class cache from ClassFactory.
    void RefreshClassNames();

    // Maps component class name -> node id inside the NodeGraphVisualizer graph.
    std::unordered_map<std::string, int> m_nodeIdByClassName;

    // Last loaded file path (shown in the UI).
    std::string m_loadedFilePath;

    // Error / status feedback.
    std::string m_lastError;
    std::string m_statusMessage;

    // True when the graph has unsaved changes.
    bool m_isDirty = false;

    // Cached list of all registered class names (for the right-click add menu).
    std::vector<std::string> m_classNames;

    // Search filter buffer for the canvas right-click "Add Node" popup.
    static constexpr int k_filterBufSize = 128;
    char m_canvasCtxFilter[k_filterBufSize] = {};

    // Path input buffer for the inline file loader UI.
    static constexpr int k_pathBufSize = 512;
    char m_pathInputBuf[k_pathBufSize] = {};
};

} // namespace ImGuiVisualizers
