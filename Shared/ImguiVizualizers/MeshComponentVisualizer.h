#pragma once

#include "CombinedObjJson3DVisualizer.h"

#include "MeshComponent.h"
#include "CoreSystem/CoreSystem.h"

namespace ImGuiVisualizers {

class MeshComponentVisualizer : public CombinedObjJson3DVisualizer
{
public:
    explicit MeshComponentVisualizer(const char* name = "Mesh Editor")
        : CombinedObjJson3DVisualizer(name)
        , m_meshComp(nullptr)
    {}

    ~MeshComponentVisualizer() override
    {
        ReleaseMeshComponent();
    }

protected:
    bool AttachMeshFromPath(const std::string& meshPath) override;    
    void ReleaseMeshComponent();
private:
    CMeshComponent* m_meshComp;
};

} // namespace ImGuiVisualizers