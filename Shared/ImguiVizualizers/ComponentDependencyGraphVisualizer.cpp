// ============================================================
//  ComponentDependencyGraphVisualizer.cpp
// ============================================================
#include "CoreSystem/CoreSystem.h"
#include "ComponentDependencyGraphVisualizer.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <queue>

// ?? Reflection definitions ????????????????????????????????????????????
// ComponentNode is a concrete NodeGraphNode subclass; expose the class
// name member so it can be serialised if needed.
REFL_DEFINE_OBJECT(ImGuiVisualizers::ComponentNode)
    REFL_DEFINE_STRING_MEMBER(ImGuiVisualizers::ComponentNode, m_componentClassName),
REFL_DEFINE_END

namespace ImGuiVisualizers {

// ?? Construction ??????????????????????????????????????????????????????

ComponentDependencyGraphVisualizer::ComponentDependencyGraphVisualizer(
    const char* name, ImGuiKey shortcut, const char* category)
    : NodeGraphVisualizer(name, shortcut, category)
{
    // Mark the graph dirty whenever the user creates or removes a link
    // interactively (drag-to-connect or right-click-delete).
    SetOnLinkCreated([this](const NodeLink&) { m_isDirty = true; });
    SetOnLinkDeleted([this](int)             { m_isDirty = true; });

    RefreshClassNames();
}

// ?? Render ????????????????????????????????????????????????????????????

bool ComponentDependencyGraphVisualizer::Render(bool* isOpen)
{
    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

    if (!ImGui::Begin(GetName(), isOpen, flags)) {
        ImGui::End();
        return false;
    }

    RenderLoadBar();
    ImGui::Separator();
    RenderEditBar();
    ImGui::Separator();

    // Status / error feedback
    if (!m_lastError.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 80, 80, 255));
        ImGui::TextWrapped("Error: %s", m_lastError.c_str());
        ImGui::PopStyleColor();
    } else if (!m_statusMessage.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(100, 220, 100, 255));
        ImGui::TextUnformatted(m_statusMessage.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::End();

    return NodeGraphVisualizer::Render(isOpen);
}

// ?? Load bar UI ???????????????????????????????????????????????????????

void ComponentDependencyGraphVisualizer::RenderLoadBar()
{
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 70.f);
    ImGui::InputText("##path", m_pathInputBuf, k_pathBufSize);
    ImGui::SameLine();
    if (ImGui::Button("Load", { 60.f, 0.f })) {
        LoadFromFile(std::string(m_pathInputBuf));
    }

    if (!m_loadedFilePath.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("  %s", m_loadedFilePath.c_str());
    }
}

// ?? Edit bar UI ???????????????????????????????????????????????????????
// Renders the "Add dependency" row and the Save / Refresh buttons.

void ComponentDependencyGraphVisualizer::RenderEditBar()
{
    // ?? Save / dirty indicator ????????????????????????????????????????
    if (m_isDirty) {
        ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(180, 120, 20, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(220, 160, 40, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(255, 200, 60, 255));
    }
    bool saveClicked = ImGui::Button(m_isDirty ? "Save *" : "Save");
    if (m_isDirty) ImGui::PopStyleColor(3);

    if (saveClicked) {
        if (!SaveToFile())
            ImGui::OpenPopup("##SaveErr");
    }
    if (ImGui::BeginPopup("##SaveErr")) {
        ImGui::TextColored({ 1.f, 0.3f, 0.3f, 1.f }, "Save failed: %s", m_lastError.c_str());
        if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button("Refresh Classes")) RefreshClassNames();
    ImGui::SameLine();
    ImGui::TextDisabled("%d classes", static_cast<int>(m_classNames.size()));

    ImGui::Spacing();

    if (m_classNames.empty()) {
        ImGui::TextDisabled("No registered classes found.");
        return;
    }

    // ?? Helper: filtered combo ????????????????????????????????????????
    // Returns true and updates *selectedIdx when the user picks an item.
    auto FilteredCombo = [&](const char* label, int* selectedIdx,
                              char* filterBuf, int filterBufSz) -> bool
    {
        bool changed = false;
        const std::string& current = (*selectedIdx >= 0 &&
                                      *selectedIdx < static_cast<int>(m_classNames.size()))
                                   ? m_classNames[*selectedIdx] : "";

        ImGui::SetNextItemWidth(220.f);
        if (ImGui::BeginCombo(label, current.c_str()))
        {
            // Search filter
            ImGui::SetNextItemWidth(-1.f);
            ImGui::InputText("##filter", filterBuf, filterBufSz);
            ImGui::SetItemDefaultFocus();

            std::string_view filter(filterBuf);
            for (int i = 0; i < static_cast<int>(m_classNames.size()); ++i)
            {
                const std::string& name = m_classNames[i];
                // Case-insensitive substring filter
                if (!filter.empty()) {
                    auto it = std::search(
                        name.begin(), name.end(),
                        filter.begin(), filter.end(),
                        [](char a, char b) {
                            return std::tolower(static_cast<unsigned char>(a)) ==
                                   std::tolower(static_cast<unsigned char>(b));
                        });
                    if (it == name.end()) continue;
                }
                bool selected = (i == *selectedIdx);
                if (ImGui::Selectable(name.c_str(), selected)) {
                    *selectedIdx = i;
                    changed = true;
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        return changed;
    };

    // ?? Dependency row: [dependsOn] --> [dependent] [Add] ????????????
    ImGui::Text("Add dependency:");
    ImGui::SameLine();

    FilteredCombo("##from", &m_addFromIdx, m_fromFilter, k_filterBufSize);
    ImGui::SameLine();
    ImGui::TextUnformatted(" must finish before ");
    ImGui::SameLine();
    FilteredCombo("##to",   &m_addToIdx,   m_toFilter,   k_filterBufSize);
    ImGui::SameLine();

    bool canAdd = (m_addFromIdx != m_addToIdx) && !m_classNames.empty();
    if (!canAdd) ImGui::BeginDisabled();
    bool addClicked = ImGui::Button("Add");
    if (!canAdd) ImGui::EndDisabled();

    if (addClicked && canAdd) {
        const std::string& dependsOn = m_classNames[m_addFromIdx];
        const std::string& dependent = m_classNames[m_addToIdx];
        if (!AddDependency(dependsOn, dependent)) {
            m_statusMessage = "Dependency already exists: "
                            + dependsOn + " -> " + dependent;
        } else {
            m_statusMessage = "Added: " + dependsOn + " -> " + dependent;
            m_lastError.clear();
        }
    }
}

// ?? SaveToFile ????????????????????????????????????????????????????????

bool ComponentDependencyGraphVisualizer::SaveToFile()
{
    m_lastError.clear();

    if (m_loadedFilePath.empty()) {
        m_lastError = "No file path set. Load a file first.";
        return false;
    }

    // Reconstruct a ComponentDependencyDefinitionList from the live graph.
    ComponentDependencyDefinitionList list;
    for (const NodeLink& lnk : GetLinks()) {
        ComponentNode* outNode = static_cast<ComponentNode*>(FindNode(lnk.outputNodeId));
        ComponentNode* inNode  = static_cast<ComponentNode*>(FindNode(lnk.inputNodeId));
        if (outNode && inNode)
            list.AddDependency(inNode->GetComponentClassName(),
                               outNode->GetComponentClassName());
    }

    auto result = list.SafeWrite(m_loadedFilePath);
    if (!result.IsSuccess()) {
        m_lastError = "Save failed: " + result.GetError().ToString();
        return false;
    }

    m_isDirty = false;
    m_statusMessage = "Saved " + std::to_string(list.GetDependencyCount())
                    + " dependenc"
                    + (list.GetDependencyCount() == 1 ? "y" : "ies") + ".";
    return true;
}

// ?? AddDependency ?????????????????????????????????????????????????????

bool ComponentDependencyGraphVisualizer::AddDependency(
    const std::string& dependsOn, const std::string& dependent)
{
    int dependsOnId = GetOrCreateNode(dependsOn);
    int dependentId = GetOrCreateNode(dependent);

    ComponentNode* dependsOnNode = static_cast<ComponentNode*>(FindNode(dependsOnId));
    ComponentNode* dependentNode = static_cast<ComponentNode*>(FindNode(dependentId));
    if (!dependsOnNode || !dependentNode) return false;

    int linkId = AddLink(dependsOnId, dependsOnNode->GetOutputPinId(),
                         dependentId, dependentNode->GetInputPinId());
    if (linkId == -1) return false; // already exists

    m_isDirty = true;
    return true;
}

// ?? GetOrCreateNode ???????????????????????????????????????????????????

int ComponentDependencyGraphVisualizer::GetOrCreateNode(const std::string& className)
{
    auto it = m_nodeIdByClassName.find(className);
    if (it != m_nodeIdByClassName.end()) return it->second;

    auto node  = std::make_unique<ComponentNode>(className);
    int nodeId = AddNode(std::move(node));
    m_nodeIdByClassName[className] = nodeId;
    return nodeId;
}

// ?? RefreshClassNames ?????????????????????????????????????????????????

void ComponentDependencyGraphVisualizer::RefreshClassNames()
{
    m_classNames = ClassFactory::GetRegisteredClassNames();
    std::sort(m_classNames.begin(), m_classNames.end());

    // Clamp selection indices in case the list shrank.
    auto clamp = [&](int& idx) {
        if (!m_classNames.empty())
            idx = std::max(0, std::min(idx, static_cast<int>(m_classNames.size()) - 1));
        else
            idx = 0;
    };
    clamp(m_addFromIdx);
    clamp(m_addToIdx);
}

// ?? LoadFromFile ??????????????????????????????????????????????????????

bool ComponentDependencyGraphVisualizer::LoadFromFile(const std::string& filePath)
{
    m_lastError.clear();
    m_statusMessage.clear();

    if (filePath.empty()) {
        m_lastError = "File path is empty.";
        return false;
    }

    ComponentDependencyDefinitionList depList;
    auto result = depList.SafeRead(filePath);

    if (!result.IsSuccess()) {
        m_lastError = "Failed to read \"" + filePath + "\": " + result.GetError().ToString();
        return false;
    }

    m_loadedFilePath = filePath;
    // Keep the path in the input buffer so the user can see what is loaded.
    std::strncpy(m_pathInputBuf, filePath.c_str(), k_pathBufSize - 1);
    m_pathInputBuf[k_pathBufSize - 1] = '\0';

    BuildGraph(depList);
    return true;
}

// ?? BuildGraph ????????????????????????????????????????????????????????

void ComponentDependencyGraphVisualizer::BuildGraph(
    const ComponentDependencyDefinitionList& list)
{
    // Clear previous graph state.
    Clear();
    m_nodeIdByClassName.clear();

    // Pass 1: create a node for every unique component class name.
    const size_t count = list.GetDependencyCount();
    for (size_t i = 0; i < count; ++i) {
        const ComponentDependencyDefinition& dep = list.GetDependency(i);
        GetOrCreateNode(dep.m_dependent);
        GetOrCreateNode(dep.m_dependsOn);
    }

    // Pass 2: create links (dependsOn -> dependent).
    // An edge from dependsOn's output pin to dependent's input pin reads
    // naturally as "dependsOn must finish before dependent starts".
    for (size_t i = 0; i < count; ++i) {
        const ComponentDependencyDefinition& dep = list.GetDependency(i);

        int dependsOnId = m_nodeIdByClassName.at(dep.m_dependsOn);
        int dependentId = m_nodeIdByClassName.at(dep.m_dependent);

        ComponentNode* dependsOnNode = static_cast<ComponentNode*>(FindNode(dependsOnId));
        ComponentNode* dependentNode = static_cast<ComponentNode*>(FindNode(dependentId));

        if (dependsOnNode && dependentNode) {
            AddLink(dependsOnId, dependsOnNode->GetOutputPinId(),
                    dependentId, dependentNode->GetInputPinId());
        }
    }

    // Pass 3: auto-layout.
    AutoLayout();

    m_isDirty = false;
    m_statusMessage = "Loaded " + std::to_string(count) + " dependenc"
                    + (count == 1 ? "y" : "ies") + ", "
                    + std::to_string(m_nodeIdByClassName.size()) + " component"
                    + (m_nodeIdByClassName.size() == 1 ? "" : "s") + ".";

    FocusAll();
}

// ?? AutoLayout ????????????????????????????????????????????????????????
// Performs a Kahn's-algorithm topological sort on the dependency graph
// and positions nodes in columns (layers) left-to-right, with dependent
// components placed further to the right than their dependencies.

void ComponentDependencyGraphVisualizer::AutoLayout()
{
    // Build adjacency from node id ? successor node ids.
    // An edge dependsOn ? dependent means dependsOn is a predecessor.
    std::unordered_map<int, std::vector<int>> successors; // nodeId ? [nodeId]
    std::unordered_map<int, int>              inDegree;   // nodeId ? count

    for (auto& [name, id] : m_nodeIdByClassName) {
        successors[id]; // ensure entry exists
        inDegree[id] = 0;
    }

    for (auto& lnk : GetLinks()) {
        successors[lnk.outputNodeId].push_back(lnk.inputNodeId);
        inDegree[lnk.inputNodeId]++;
    }

    // Kahn's BFS to assign a column (depth) to each node.
    std::unordered_map<int, int> column; // nodeId ? column index
    std::queue<int> queue;

    for (auto& [id, deg] : inDegree)
        if (deg == 0) { queue.push(id); column[id] = 0; }

    while (!queue.empty()) {
        int cur = queue.front(); queue.pop();
        for (int succ : successors[cur]) {
            column[succ] = std::max(column[succ], column[cur] + 1);
            if (--inDegree[succ] == 0)
                queue.push(succ);
        }
    }

    // Any nodes not yet assigned a column (cycle members) go in column 0.
    for (auto& [name, id] : m_nodeIdByClassName)
        column.emplace(id, 0);

    // Group nodes by column and assign positions.
    std::unordered_map<int, std::vector<int>> byColumn;
    for (auto& [id, col] : column)
        byColumn[col].push_back(id);

    constexpr float colSpacing  = 220.f;
    constexpr float rowSpacing  = 90.f;
    constexpr float originX     = 40.f;
    constexpr float originY     = 40.f;

    for (auto& [col, ids] : byColumn) {
        // Sort ids for deterministic ordering.
        std::sort(ids.begin(), ids.end());
        for (int row = 0; row < static_cast<int>(ids.size()); ++row) {
            NodeGraphNode* node = FindNode(ids[row]);
            if (node) {
                node->SetPosition({
                    originX + col * colSpacing,
                    originY + row * rowSpacing
                });
            }
        }
    }
}

} // namespace ImGuiVisualizers
