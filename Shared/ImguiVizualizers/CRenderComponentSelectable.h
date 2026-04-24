#pragma once

#include "CSelectable.h"
#include "../Components/MeshComponent.h"

namespace ImGuiVisualizers {

/**
 * @brief Adapts a CRenderComponent* into the CSelectable interface.
 *
 * Shares the component's own transform and bounding-sphere shared_ptrs
 * directly, so the selection manager always reads live data with no
 * explicit sync step required.
 */
class CRenderComponentSelectable final : public CSelectable
{
public:
    explicit CRenderComponentSelectable(CRenderComponent* component)
        : CSelectable(component->GetModelMatrix(), component->GetBoundingSphere())
        , m_component(component)
    {}

    CRenderComponent* GetComponent() const { return m_component; }

private:
    CRenderComponent* m_component = nullptr;
};

} // namespace ImGuiVisualizers