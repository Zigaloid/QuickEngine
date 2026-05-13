#pragma once
#include "LevelComponent.h"
#include "EntityComponent.h"
#include "SelectionManager.h"
#include <string>
#include <vector>
#include <memory>

namespace ImGuiVisualizers {

/**
 * @brief A UI panel for managing layers in the level editor.
 * 
 * Layers are represented as child CLevelComponent instances.
 * This panel provides functionality to:
 * - Display a hierarchy of layers
 * - Select layers
 * - Show/hide layers (via SetActive)
 * - Add new layers
 * - Delete layers
 * - Rename layers (via m_name member)
 */
class LayersPanel
{
public:
    LayersPanel() = default;
    ~LayersPanel() = default;

    /**
     * @brief Set the root level component.
     * Layers are direct children of this component.
     */
    void SetLevelComponent(CLevelComponent* level) { m_levelComponent = level; }

    /**
     * @brief Set the selection manager used to select entities from the panel.
     */
    void SetSelectionManager(CSelectionManager* selectionManager) { m_selectionManager = selectionManager; }

    /**
     * @brief Get the currently selected layer, or nullptr if none selected.
     */
    CLevelComponent* GetSelectedLayer() const { return m_selectedLayer; }

    /**
     * @brief Set the currently selected layer.
     */
    void SetSelectedLayer(CLevelComponent* layer) { m_selectedLayer = layer; }

    /**
     * @brief Render the layers panel UI.
     * Should be called within a tab or child window.
     */
    void Render();

    /**
     * @brief Clear internal state (e.g., when level changes).
     */
    void Clear()
    {
        m_selectedLayer = nullptr;
        m_expandedLayers.clear();
        m_renamingLayer = nullptr;
        m_renameBuffer[0] = '\0';
    }

private:
    /**
     * @brief Recursively render a layer node and its children.
     */
    void RenderLayerNode(CLevelComponent* layer, int nodeId);

    /**
     * @brief Create a new layer as a child of the given parent.
     */
    void CreateNewLayer(CLevelComponent* parent);

    /**
     * @brief Delete a layer and all its children.
     */
    void DeleteLayer(CLevelComponent* layer);

    /**
     * @brief Start renaming a layer.
     */
    void StartRenaming(CLevelComponent* layer);

    /**
     * @brief Finish renaming a layer.
     */
    void FinishRenaming();

    /**
     * @brief Render a leaf entity node within a layer.
     */
    void RenderEntityNode(CEntityComponent* entity, int nodeId);

    /**
     * @brief Select all selectable components belonging to the given entity.
     * @param additive  If true, adds to the current selection; otherwise replaces it.
     */
    void SelectEntity(CEntityComponent* entity, bool additive);

    /**
     * @brief Select all entities that are direct or indirect children of a layer.
     */
    void SelectAllEntitiesInLayer(CLevelComponent* layer);

    CLevelComponent*   m_levelComponent   = nullptr;
    CSelectionManager* m_selectionManager = nullptr;
    CLevelComponent* m_selectedLayer = nullptr;
    CLevelComponent* m_renamingLayer = nullptr;
    std::vector<CLevelComponent*> m_expandedLayers;
    char m_renameBuffer[256];
    int m_nextNodeId = 0;
};

} // namespace ImGuiVisualizers
