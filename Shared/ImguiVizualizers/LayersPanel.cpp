#include "imgui.h"
#include <algorithm>
#include <functional>
#include "LayersPanel.h"
namespace ImGuiVisualizers {

void LayersPanel::Render()
{
    if (!m_levelComponent)
    {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No level loaded");
        return;
    }

    // Toolbar
    // Use a fixed frame height for the button so it is visible inside tab layouts
    ImVec2 addBtnSize(-1.0f, ImGui::GetFrameHeight());
    if (ImGui::Button("+ Add Layer", addBtnSize))
    {
        CreateNewLayer(m_levelComponent);
    }

    ImGui::SameLine();

    // Expand / Collapse all buttons
    if (ImGui::SmallButton("Expand All"))
    {
        // Recursively add every layer to m_expandedLayers
        std::function<void(CLevelComponent*)> expandRec = [&](CLevelComponent* layer)
        {
            if (!layer) return;
            if (std::find(m_expandedLayers.begin(), m_expandedLayers.end(), layer) == m_expandedLayers.end())
                m_expandedLayers.push_back(layer);
            for (auto* child : layer->GetChildren())
            {
                if (auto* childLayer = dynamic_cast<CLevelComponent*>(child))
                    expandRec(childLayer);
            }
        };
        expandRec(m_levelComponent);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Collapse All"))
    {
        // Recursively remove every layer from m_expandedLayers
        std::function<void(CLevelComponent*)> collapseRec = [&](CLevelComponent* layer)
        {
            if (!layer) return;
            auto it = std::find(m_expandedLayers.begin(), m_expandedLayers.end(), layer);
            if (it != m_expandedLayers.end())
                m_expandedLayers.erase(it);
            for (auto* child : layer->GetChildren())
            {
                if (auto* childLayer = dynamic_cast<CLevelComponent*>(child))
                    collapseRec(childLayer);
            }
        };
        collapseRec(m_levelComponent);
    }

    ImGui::Separator();

    // Render layer tree
    if (ImGui::BeginChild("##LayersList", ImVec2(0, 0), false))
    {
        const auto& children = m_levelComponent->GetChildren();
        for (size_t i = 0; i < children.size(); ++i)
        {
            auto* layer = dynamic_cast<CLevelComponent*>(children[i]);
            if (layer)
            {
                RenderLayerNode(layer, static_cast<int>(i));
            }
        }
    }
    ImGui::EndChild();
}

void LayersPanel::RenderLayerNode(CLevelComponent* layer, int nodeId)
{
    if (!layer) return;

    ImGui::PushID(layer);

    const auto& childLayers = layer->GetChildren();
    // Consider a node to have children if it has any children (layers or entities).
    bool hasChildren = !childLayers.empty();

    // Determine if this node is expanded
    bool isExpanded = std::find(m_expandedLayers.begin(), m_expandedLayers.end(), layer) 
        != m_expandedLayers.end();

    // Tree node with expand/collapse
    // Avoid SpanFullWidth so inline widgets (checkbox) remain clickable.
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_None;
    if (!hasChildren)
        flags |= ImGuiTreeNodeFlags_Leaf;
    if (layer == m_selectedLayer)
        flags |= ImGuiTreeNodeFlags_Selected;

    // Improve interaction: arrow click and double-click on label toggle.
    flags |= ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;

    // Honor our stored expansion state (e.g. when CreateNewLayer pushed a parent)
    if (isExpanded)
        flags |= ImGuiTreeNodeFlags_DefaultOpen;

    // Render the tree node using the layer name as the label when not renaming so
    // clicking the name toggles expand/collapse. When renaming, show an InputText
    // on the same line instead.
    bool nodeOpen = false;
    if (m_renamingLayer == layer)
    {
        // Create a tree node with an empty label (so the InputText won't be duplicated)
        nodeOpen = ImGui::TreeNodeEx(layer, flags, "");
        ImGui::SameLine();
        ImGui::SetKeyboardFocusHere();
        if (ImGui::InputText("##LayerName", m_renameBuffer, sizeof(m_renameBuffer),
            ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue))
        {
            FinishRenaming();
        }
        // Allow clicking the input text to select the layer
        if (ImGui::IsItemClicked())
        {
            m_selectedLayer = layer;
        }
        if (!ImGui::IsItemActive() && ImGui::IsMouseClicked(0))
        {
            FinishRenaming();
        }
    }
    else
    {
        // Show the layer name inside the TreeNode so clicking it toggles expansion
        nodeOpen = ImGui::TreeNodeEx(layer, flags, "%s", layer->GetName().c_str());

        // Handle selection when clicking the tree node arrow/area or label
        if (ImGui::IsItemClicked())
        {
            m_selectedLayer = layer;
        }

        // Open context menu on right-click of the tree node arrow/area or label
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
        {
            ImGui::OpenPopup("##LayerContextMenu");
        }
    }

    // Context menu — bound to an explicit ID so it is not hijacked by the last item (checkbox)
    // Note: we allow opening via both the tree node and the label above.
    ImGui::SameLine();

    // Visibility toggle (eye icon) — editor-only visibility separate from component active state
    bool isVisible = layer->IsVisibleInEditor();
    if (ImGui::Checkbox(("##Visible" + std::to_string(nodeId)).c_str(), &isVisible))
    {
        layer->SetVisibleInEditor(isVisible);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Toggle layer visibility");
    }

    if (ImGui::BeginPopup("##LayerContextMenu"))
    {
        if (ImGui::MenuItem("Add Child Layer"))
        {
            CreateNewLayer(layer);
        }
        if (ImGui::MenuItem("Rename"))
        {
            StartRenaming(layer);
        }
        if (ImGui::MenuItem("Select All Entities"))
        {
            // Clear existing selection then select every entity in this layer
            if (m_selectionManager)
                m_selectionManager->ClearSelection();
            SelectAllEntitiesInLayer(layer);
        }
        if (ImGui::MenuItem("Delete"))
        {
            DeleteLayer(layer);
            ImGui::EndPopup();
            ImGui::PopID();
            return;
        }
        ImGui::EndPopup();
    }

    // Render child layers and entities if expanded
    if (nodeOpen)
    {
        // Track expansion state
        if (std::find(m_expandedLayers.begin(), m_expandedLayers.end(), layer) 
            == m_expandedLayers.end())
        {
            m_expandedLayers.push_back(layer);
        }

        int childId = 0;
        for (auto* child : childLayers)
        {
            if (auto* childLayer = dynamic_cast<CLevelComponent*>(child))
            {
                RenderLayerNode(childLayer, nodeId * 100 + childId);
                ++childId;
            }
            else if (auto* entity = dynamic_cast<CEntityComponent*>(child))
            {
                RenderEntityNode(entity, nodeId * 100 + childId);
                ++childId;
            }
        }

        ImGui::TreePop();
    }
    else
    {
        // Track collapsed state
        auto it = std::find(m_expandedLayers.begin(), m_expandedLayers.end(), layer);
        if (it != m_expandedLayers.end())
        {
            m_expandedLayers.erase(it);
        }
    }

    ImGui::PopID();
}

void LayersPanel::RenderEntityNode(CEntityComponent* entity, int nodeId)
{
    if (!entity) return;

    ImGui::PushID(entity);

    const std::string& name = entity->GetName();
    const std::string displayName = name.empty() ? "(unnamed entity)" : name;

    // Check if this entity is currently selected (any of its selectables are selected)
    bool isSelected = false;
    if (m_selectionManager)
    {
        for (const auto& sel : m_selectionManager->GetAllSelected())
        {
            if (sel->GetOwner() == entity)
            {
                isSelected = true;
                break;
            }
        }
    }

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf
        | ImGuiTreeNodeFlags_SpanFullWidth
        | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (isSelected)
        flags |= ImGuiTreeNodeFlags_Selected;

    ImGui::TreeNodeEx(entity, flags, "%s", displayName.c_str());

    if (ImGui::IsItemClicked() && m_selectionManager)
    {
        const bool shiftHeld = ImGui::GetIO().KeyShift;
        SelectEntity(entity, shiftHeld);
    }

    ImGui::PopID();
}

void LayersPanel::SelectEntity(CEntityComponent* entity, bool additive)
{
    if (!entity || !m_selectionManager) return;

    // Collect all selectables that belong to this entity
    std::vector<std::shared_ptr<CSelectable>> entitySelectables;
    for (const auto& sel : m_selectionManager->GetAllSelectables())
    {
        if (sel && sel->GetOwner() == entity)
            entitySelectables.push_back(sel);
    }

    if (entitySelectables.empty()) return;

    if (additive)
    {
        for (const auto& sel : entitySelectables)
            m_selectionManager->AddToSelection(sel);
    }
    else
    {
        m_selectionManager->SetAllSelected(entitySelectables);
    }
}

void LayersPanel::SelectAllEntitiesInLayer(CLevelComponent* layer)
{
    if (!layer || !m_selectionManager) return;

    // Collect all CEntityComponent direct children of this layer
    for (auto* child : layer->GetChildren())
    {
        if (auto* entity = dynamic_cast<CEntityComponent*>(child))
            SelectEntity(entity, /*additive=*/true);
    }
}

void LayersPanel::CreateNewLayer(CLevelComponent* parent)
{
    if (!parent) return;

    CLevelComponent* newLayer = parent->CreateChild<CLevelComponent>();
    if (newLayer)
    {
        // Set a default name
        static int layerCount = 0;
        newLayer->SetName("Layer_" + std::to_string(layerCount++));

        // Select the new layer
        m_selectedLayer = newLayer;

        // Expand parent to show the new layer
        if (std::find(m_expandedLayers.begin(), m_expandedLayers.end(), parent) 
            == m_expandedLayers.end())
        {
            m_expandedLayers.push_back(parent);
        }
    }
}

void LayersPanel::DeleteLayer(CLevelComponent* layer)
{
    if (!layer || !m_levelComponent) return;

    // Deselect if it was selected
    if (m_selectedLayer == layer)
    {
        m_selectedLayer = nullptr;
    }

    // Remove from expanded list
    auto it = std::find(m_expandedLayers.begin(), m_expandedLayers.end(), layer);
    if (it != m_expandedLayers.end())
    {
        m_expandedLayers.erase(it);
    }

    // Get the parent and remove this layer
    ComponentSystem::Component* parent = layer->GetParent();
    if (parent)
    {
        parent->RemoveChild(layer);
    }
}

void LayersPanel::StartRenaming(CLevelComponent* layer)
{
    if (!layer) return;

    m_renamingLayer = layer;
    const std::string& name = layer->GetName();
    if (name.size() < sizeof(m_renameBuffer))
    {
        std::copy(name.begin(), name.end(), m_renameBuffer);
        m_renameBuffer[name.size()] = '\0';
    }
    else
    {
        std::copy(name.begin(), name.begin() + (sizeof(m_renameBuffer) - 1), m_renameBuffer);
        m_renameBuffer[sizeof(m_renameBuffer) - 1] = '\0';
    }
}

void LayersPanel::FinishRenaming()
{
    if (m_renamingLayer)
    {
        m_renamingLayer->SetName(m_renameBuffer);
    }
    m_renamingLayer = nullptr;
    m_renameBuffer[0] = '\0';
}

} // namespace ImGuiVisualizers
