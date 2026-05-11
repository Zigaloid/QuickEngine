#pragma once

#include "Selectable.h"
#include "../Components/PhysicsBodyComponent.h"

namespace ImGuiVisualizers {

// Adapts a CPhysicsBodyComponent into the CSelectable interface.
// The selectable owns simple shared_ptrs for a transform and bounding sphere
// which are updated by calling UpdateFromComponent().
class CPhysicsBodySelectable final : public CSelectable
{
public:
    explicit CPhysicsBodySelectable(CPhysicsBodyComponent* component)
        // Use the component's shared pointers so the selectable references the
        // live component data (same pattern as RenderComponentSelectable).
        : CSelectable(component ? component->GetModelMatrix() : std::shared_ptr<Matrix4f>(),
                      component ? component->GetBoundingSphere() : std::shared_ptr<Vector4f>(),
                      component)
        , m_component(component)
    {}

    void UpdateFromComponent()
    {
        if (!m_component) return;

        // The gizmo edits the resource's local transform (m_transform) directly
        // via the shared pointer from GetModelMatrix(). This represents the
        // physics shape's offset relative to the mesh — no physics simulation
        // sync is needed in the editor context.

        // Update bounding sphere from the component's local transform scale.
        auto bsPtr = GetBoundingSpherePtr();
        if (bsPtr && m_component)
        {
            Vector3f scale = m_component->GetScale();
            float maxScale = scale.GetX();
            if (scale.GetY() > maxScale) maxScale = scale.GetY();
            if (scale.GetZ() > maxScale) maxScale = scale.GetZ();

            *bsPtr = Vector4f(0.0f, 0.0f, 0.0f, maxScale * 0.5f);
        }
    }

    CPhysicsBodyComponent* GetComponent() const { return m_component; }

private:
    CPhysicsBodyComponent* m_component = nullptr;
};

} // namespace ImGuiVisualizers
