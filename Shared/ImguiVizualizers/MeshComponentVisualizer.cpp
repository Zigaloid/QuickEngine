#include "MeshComponentVisualizer.h"

namespace ImGuiVisualizers {

bool MeshComponentVisualizer::AttachMeshFromPath(const std::string& meshPath)
{
    // Release previous, if any
    ReleaseMeshComponent();

    auto componentManager = Core::CoreSystem::GetComponentManager();
    if (!componentManager)
        return false;

    // Recognize ".mesh.obj.json" as a prebuilt mesh resource (same logic as before).
    if (meshPath.size() >= 14 && meshPath.compare(meshPath.size() - 14, 14, ".mesh.obj.json") == 0)
    {
        m_meshComp = componentManager->CreateComponent<CMeshComponent>();
        if (!m_meshComp)
            return false;

        m_meshComp->SafeRead(meshPath);
        m_meshComp->ReInitialize();

        // Provide a render callback to the 3D view that invokes the component's Render().
        Get3DView().SetRenderCallback([this](bgfx::ViewId viewId, BgfxRenderPrimitives& /*prims*/) {
            if (!m_meshComp) return;            
                m_meshComp->Render(viewId);
        });

        return true;
    }

    // Not handled here
    return false;
}

void MeshComponentVisualizer::ReleaseMeshComponent()
{
    if (m_meshComp)
    {
        auto componentManager = Core::CoreSystem::GetComponentManager();
        if (componentManager)
        {
            componentManager->ReleaseComponent(m_meshComp);
        }
        m_meshComp = nullptr;
    }
    // Clear render callback to avoid stale calls
    Get3DView().SetRenderCallback(nullptr);
}

} // namespace ImGuiVisualizers