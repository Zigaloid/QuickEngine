#include "LevelComponentVisualizer.h"
#include "CoreSystem/CoreSystem.h"

namespace ImGuiVisualizers {

bool LevelComponentVisualizer::AttachMeshFromPath(const std::string& levelPath)
{
    ReleaseLevelComponent();

    auto componentManager = Core::CoreSystem::GetComponentManager();
    if (!componentManager)
        return false;

    m_levelComp = componentManager->CreateComponent<CLevelComponent>();
    if (!m_levelComp)
        return false;

    m_levelComp->SafeRead(levelPath);
    m_levelComp->ReInitialize();

    Get3DView().SetRenderCallback([this](bgfx::ViewId viewId, BgfxRenderPrimitives& /*prims*/) {
        if (!m_levelComp) return;
        if (!m_levelComp->IsReady()) return;
        RenderComponentHierarchy(viewId, m_levelComp);
    });

    return true;
}

void LevelComponentVisualizer::ReleaseLevelComponent()
{
    if (m_levelComp)
    {
        auto componentManager = Core::CoreSystem::GetComponentManager();
        if (componentManager)
        {
            componentManager->ReleaseComponent(m_levelComp);
        }
        m_levelComp = nullptr;
    }
    // Clear render callback to avoid stale calls
    Get3DView().SetRenderCallback(nullptr);
}

void LevelComponentVisualizer::RenderComponentHierarchy(bgfx::ViewId viewId, ComponentSystem::Component* comp)
{
    if (!comp) return;

    if (auto* renderComp = dynamic_cast<CRenderComponent*>(comp))
    {
        if (renderComp->IsActive())
        {
            renderComp->Render(viewId);
        }
    }

    for (auto* child : comp->GetChildren())
    {
        RenderComponentHierarchy(viewId, child);
    }
}

} // namespace ImGuiVisualizers