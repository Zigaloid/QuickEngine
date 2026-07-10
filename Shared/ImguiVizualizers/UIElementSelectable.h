#pragma once

#include "RenderComponentSelectable.h"
#include "../Components/UIElementComponent.h"

namespace ImGuiVisualizers {

/**
 * @brief Selectable adapter for CUIElementComponent-derived render components.
 *
 * UI elements render into the dedicated UI bgfx view which uses identity view
 * and identity projection matrices, so the component's model matrix already
 * carries local vertices into clip-space NDC. The selectable therefore lives
 * in NDC/screen space rather than world space, and the selection manager uses
 * a dedicated 2D gizmo path for it.
 */
class CUIElementSelectable final : public CRenderComponentSelectable
{
public:
    explicit CUIElementSelectable(CUIElementComponent* component)
        : CRenderComponentSelectable(component)
    {}

    SpaceKind GetSpace() const override { return SpaceKind::Screen; }
};

} // namespace ImGuiVisualizers