#pragma once
#include "ICommand.h"
#include "CSelectionManager.h"
#include "LevelComponent.h"
#include "EntityComponent.h"
#include <functional>
#include <string>

namespace ImGuiVisualizers {

/**
 * @brief Records the deletion of a CEntityComponent from a CLevelComponent.
 *
 * The full entity state (including all reflected children) is serialised to a
 * JSON string at construction time. Execute() removes the entity from the
 * level; Undo() recreates it by deserialising that snapshot into a fresh child.
 *
 * Two callbacks let the owning visualizer keep its selectable registry in sync
 * without the command depending on LevelComponentVisualizer internals:
 *
 *   deregisterFn(entity)  — called on Execute to unregister all selectables
 *                           that belong to the entity being deleted.
 *   registerFn(entity)    — called on Undo to register selectables for the
 *                           newly recreated entity.
 */
class CDeleteEntityCommand final : public ICommand
{
public:
    using RegisterFn   = std::function<void(ComponentSystem::Component*)>;
    using DeregisterFn = std::function<void(CEntityComponent*)>;

    /// @param level           The level that owns the entity.
    /// @param entity          The entity to delete (must be a direct child of level).
    /// @param selectionMgr    Used to clear the selection when the entity is removed.
    /// @param registerFn      Callback to re-add render-component selectables after undo.
    /// @param deregisterFn    Callback to remove render-component selectables on delete.
    CDeleteEntityCommand(CLevelComponent*   level,
                         CEntityComponent*  entity,
                         CSelectionManager* selectionMgr,
                         RegisterFn         registerFn,
                         DeregisterFn       deregisterFn)
        : m_level(level)
        , m_entity(entity)
        , m_selectionMgr(selectionMgr)
        , m_registerFn(std::move(registerFn))
        , m_deregisterFn(std::move(deregisterFn))
    {
        // Snapshot the full entity state before it is ever deleted.
        auto result = entity->WriteToJsonString();
        if (result.IsSuccess())
            m_jsonSnapshot = result.GetValue();
    }

    // ICommand ─────────────────────────────────────────────────────────

    /// Removes the entity from the level and clears related selectables.
    void Execute() override
    {
        if (!m_entity || !m_level)
            return;

        m_deregisterFn(m_entity);
        m_selectionMgr->ClearSelection();
        m_level->RemoveChild(m_entity);
        m_entity = nullptr;   // entity is now destroyed
    }

    /// Recreates the entity from the JSON snapshot and re-registers its selectables.
    void Undo() override
    {
        if (!m_level || m_jsonSnapshot.empty())
            return;

        m_entity = m_level->CreateChild<CEntityComponent>();
        if (!m_entity)
            return;

        if (!m_entity->ReadFromJsonString(m_jsonSnapshot))
        {
            // Snapshot restore failed — remove the empty shell we just created.
            m_level->RemoveChild(m_entity);
            m_entity = nullptr;
            return;
        }

        m_registerFn(m_entity);
    }

    const char* GetLabel() const override { return "Delete Entity"; }

private:
    CLevelComponent*   m_level       = nullptr;
    CEntityComponent*  m_entity      = nullptr;   ///< Valid only between Undo/Execute pairs.
    CSelectionManager* m_selectionMgr = nullptr;
    RegisterFn         m_registerFn;
    DeregisterFn       m_deregisterFn;
    std::string        m_jsonSnapshot;             ///< Full JSON state captured at construction.
};

} // namespace ImGuiVisualizers