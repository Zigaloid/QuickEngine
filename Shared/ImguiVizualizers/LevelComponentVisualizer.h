#pragma once
#include "CombinedObjJson3DVisualizer.h"
#include "../Components/LevelComponent.h"
#include "../Components/MeshComponent.h"

namespace ImGuiVisualizers {

/**
 * @brief An editor visualizer for level files.
 *
 * Loads a level file into a CLevelComponent and renders all
 * CRenderComponent-derived descendants each frame via the
 * embedded ImGui3DViewVisualizer.
 */
class LevelComponentVisualizer : public CombinedObjJson3DVisualizer
{
public:
    explicit LevelComponentVisualizer(const char* name = "Level Editor")
        : CombinedObjJson3DVisualizer(name)
        , m_levelComp(nullptr)
    {}

    ~LevelComponentVisualizer() override
    {
        ReleaseLevelComponent();
    }

protected:
    bool AttachMeshFromPath(const std::string& levelPath) override;

private:
    void ReleaseLevelComponent();

    /// Recursively walks the component hierarchy and calls Render() on any
    /// CRenderComponent-derived component that is active.
    void RenderComponentHierarchy(bgfx::ViewId viewId, ComponentSystem::Component* comp);

    CLevelComponent* m_levelComp;
};

} // namespace ImGuiVisualizers