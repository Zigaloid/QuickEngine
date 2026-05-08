#pragma once
#include "ICommand.h"
#include "SelectionManager.h"
#include "LevelComponent.h"
#include "EntityComponent.h"
#include <functional>
#include <string>
#include <vector>
#include <memory>

namespace ImGuiVisualizers {

/**
 * @brief Records the duplication of one or more CEntityComponent objects.
 *
 * The full state of each source entity is serialised to a JSON string at
 * construction time. Execute() (and Redo()) recreates the duplicates from
 * those snapshots, registers their render-component selectables via the
 * supplied callback, and makes them the active selection. Undo() removes
 * the duplicate entities and clears the selection.
 *
 * Two callbacks keep the owning visualizer's selectable registry in sync
 * without the command depending on LevelComponentVisualizer internals:
 *
 *   registerFn(entity)    — called on Execute/Redo to register selectables
 *                           for the newly created duplicate and return them.
 *   deregisterFn(entity)  — called on Undo to unregister selectables that
 *                           belong to the duplicate being removed.
 */
class CDuplicateEntitiesCommand final : public ICommand
{
public:
    /// Returns the newly registered CSelectable objects for the given entity root.
    using RegisterFn   = std::function<std::vector<std::shared_ptr<CSelectable>>(ComponentSystem::Component*)>;
    using DeregisterFn = std::function<void(CEntityComponent*)>;

    CDuplicateEntitiesCommand(CLevelComponent*                      level,
                              const std::vector<CEntityComponent*>& sourceEntities,
                              CSelectionManager*                    selectionMgr,
                              RegisterFn                            registerFn,
                              DeregisterFn                          deregisterFn)
        : m_level(level)
        , m_selectionMgr(selectionMgr)
        , m_registerFn(std::move(registerFn))
        , m_deregisterFn(std::move(deregisterFn))
    {
        // Snapshot each source entity before duplication.
        m_jsonSnapshots.reserve(sourceEntities.size());
        for (auto* entity : sourceEntities)
        {
            auto result = entity->WriteToJsonString();
            m_jsonSnapshots.push_back(result.IsSuccess() ? result.GetValue() : std::string{});
        }
    }

    // ICommand ?????????????????????????????????????????????????????????

    /// Creates duplicate entities from the JSON snapshots, registers their
    /// selectables, and makes them the active selection.
    void Execute() override
    {
        m_newEntities.clear();
        std::vector<std::shared_ptr<CSelectable>> allNewSelectables;

        for (const auto& snapshot : m_jsonSnapshots)
        {
            if (snapshot.empty())
                continue;

            CEntityComponent* entity = m_level->CreateChild<CEntityComponent>();
            if (!entity)
                continue;

            if (!entity->ReadFromJsonString(snapshot))
            {
                m_level->RemoveChild(entity);
                continue;
            }

            m_newEntities.push_back(entity);

            auto newSelectables = m_registerFn(entity);
            allNewSelectables.insert(allNewSelectables.end(),
                                     newSelectables.begin(),
                                     newSelectables.end());
        }

        m_selectionMgr->SetAllSelected(allNewSelectables);
    }

    /// Removes all duplicate entities and clears the selection.
    void Undo() override
    {
        m_selectionMgr->ClearSelection();

        for (CEntityComponent* entity : m_newEntities)
        {
            m_deregisterFn(entity);
            m_level->RemoveChild(entity);
        }
        m_newEntities.clear();
    }

    const char* GetLabel() const override { return "Duplicate Entities"; }

private:
    CLevelComponent*               m_level        = nullptr;
    CSelectionManager*             m_selectionMgr = nullptr;
    RegisterFn                     m_registerFn;
    DeregisterFn                   m_deregisterFn;
    std::vector<std::string>       m_jsonSnapshots;
    std::vector<CEntityComponent*> m_newEntities;
};

} // namespace ImGuiVisualizers
