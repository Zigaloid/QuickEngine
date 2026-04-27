#pragma once

#include "Selectable.h"
#include "../Components/MeshComponent.h"
#include "../Components/EntityComponent.h"

namespace ImGuiVisualizers {

/**
 * @brief Adapts a CRenderComponent* into the CSelectable interface.
 *
 * Shares the component's own transform and bounding-sphere shared_ptrs
 * directly, so the selection manager always reads live data with no
 * explicit sync step required.
 *
 * The owner is set to the first CEntityComponent found by walking up
 * the component hierarchy from the given render component.
 */
class CRenderComponentSelectable final : public CSelectable
{
public:
    explicit CRenderComponentSelectable(CRenderComponent* component)
        : CSelectable(component->GetModelMatrix(),
                      component->GetBoundingSphere(),
                      FindOwnerEntity(component))
        , m_component(component)
    {}

    CRenderComponent* GetComponent() const { return m_component; }

private:
    CRenderComponent* m_component = nullptr;

    /// Walks up the component hierarchy and returns the first CEntityComponent
    /// ancestor, or nullptr if none exists.
    static CEntityComponent* FindOwnerEntity(ComponentSystem::Component* component)
    {
        ComponentSystem::Component* current = component ? component->GetParent() : nullptr;
        while (current)
        {
            if (auto* entity = dynamic_cast<CEntityComponent*>(current))
                return entity;

            current = current->GetParent();
        }
        return nullptr;
    }
};

} // namespace ImGuiVisualizers