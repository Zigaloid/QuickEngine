#pragma once
#include "ICommand.h"
#include "Selectable.h"
#include "../Core/Math/Matrix4f.h"
#include "PhysicsBodySelectable.h"
#include <memory>
#include <vector>

namespace ImGuiVisualizers {

/**
 * @brief Records a before/after transform change on one or more CSelectable objects.
 *
 * Created by CSelectionManager at the end of a gizmo drag. The before-state
 * is captured at drag start (already snapshotted in m_drag.startTransforms)
 * and the after-state is captured when the mouse button is released.
 *
 * Both Execute() (redo) and Undo() write the corresponding snapshot directly
 * into the shared Matrix4f owned by each CSelectable, keeping all external
 * references consistent.
 */
class CTransformCommand final : public ICommand
{
public:
    struct Entry
    {
        std::weak_ptr<CSelectable> selectable;
        Matrix4f                   before;
        Matrix4f                   after;
    };

    explicit CTransformCommand(std::vector<Entry> entries, const char* label = "Transform")
        : m_entries(std::move(entries))
        , m_label(label)
    {}

    // ICommand
    void Execute() override { Apply(&Entry::after);  }
    void Undo()    override { Apply(&Entry::before); }
    const char* GetLabel() const override { return m_label; }

private:
    void Apply(Matrix4f Entry::* which)
    {
        for (const auto& e : m_entries)
        {
            if (auto sel = e.selectable.lock())
            {
                if (Matrix4f* mtx = sel->GetTransformMutable())
                {
                    *mtx = e.*which;

                    // If this selectable adapts a physics body, push the
                    // updated world transform back into the physics component
                    // so that the simulation (and debug rendering) reflect
                    // undo/redo operations as well.
                    if (auto physSel = dynamic_cast<CPhysicsBodySelectable*>(sel.get()))
                    {
                        if (auto* comp = physSel->GetComponent())
                        {
                            comp->SetWorldTransform(*mtx);
                        }
                    }
                }
            }
        }
    }

    std::vector<Entry> m_entries;
    const char*        m_label;
};

} // namespace ImGuiVisualizers