#include "imgui.h"
#include <algorithm>
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
    bool hasChildren = std::any_of(childLayers.begin(), childLayers.end(),
        [](ComponentSystem::Component* child) { return dynamic_cast<CLevelComponent*>(child) != nullptr; });

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

    bool nodeOpen = ImGui::TreeNodeEx(layer, flags, "");

    // Handle selection when clicking the tree node arrow/area
    if (ImGui::IsItemClicked())
    {
        m_selectedLayer = layer;
    }

    ImGui::SameLine();

    // Layer name (with rename support)
    if (m_renamingLayer == layer)
    {
        ImGui::SetKeyboardFocusHere();
        if (ImGui::InputText("##LayerName", m_renameBuffer, sizeof(m_renameBuffer),
            ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue))
        {
            FinishRenaming();
        }
        // Also allow clicking the input text to select the layer
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
        ImGui::Text("%s", layer->GetName().c_str());
        // Clicking the label should select the layer as well
        if (ImGui::IsItemClicked())
        {
            m_selectedLayer = layer;
        }
    }

    // Visibility toggle (eye icon) — editor-only visibility separate from component active state
    ImGui::SameLine();
    bool isVisible = layer->IsVisibleInEditor();
    if (ImGui::Checkbox(("##Visible" + std::to_string(nodeId)).c_str(), &isVisible))
    {
        layer->SetVisibleInEditor(isVisible);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Toggle layer visibility");
    }

    // Context menu — allow right-click on the last rendered item for this row
    if (ImGui::BeginPopupContextItem())
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
