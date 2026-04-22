#pragma once
#include "CombinedObjJson3DVisualizer.h"
#include "../Components/LevelComponent.h"
#include "../Components/MeshComponent.h"
#include <vector>
#include <string>

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
        , m_entityPanelWidth(250.0f)
    {}

    ~LevelComponentVisualizer() override
    {
        ReleaseLevelComponent();
    }

    bool Render(bool* isOpen) override;

protected:
    bool AttachMeshFromPath(const std::string& levelPath) override;

private:
    void ReleaseLevelComponent();

    /// Recursively walks the component hierarchy and calls Render() on any
    /// CRenderComponent-derived component that is active.
    void RenderComponentHierarchy(bgfx::ViewId viewId, ComponentSystem::Component* comp);

    // Entity asset browser panel
    void RenderEntityAssetPanel();
    void RefreshEntityAssets();
    void HandleEntityDrop();

    struct EntityAssetEntry {
        std::string fileName;
        std::string fullPath;
    };

    CLevelComponent* m_levelComp;
    
    // Entity asset browser state
    std::vector<EntityAssetEntry> m_entityAssets;
    bool m_entityAssetsNeedRefresh = true;
    float m_entityPanelWidth;
};

} // namespace ImGuiVisualizers