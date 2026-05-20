#include "CombinedObjJson3DVisualizer.h"
#include "PropertyInspector.h"
#include "SelectionManager.h"
#include "MeshVertexSelectable.h"
#include "CommandHistory.h"
#include "HeightFieldEditCommand.h"

#include "HeightFieldMeshComponent.h"
#include "../Components/EntityComponent.h"
#include "../Components/TransformComponent.h"
#include "CoreSystem/CoreSystem.h"

namespace ImGuiVisualizers {

    class HeightFieldMeshComponentVisualizer : public CombinedObjJson3DVisualizer
    {
    public:
        explicit HeightFieldMeshComponentVisualizer(const char* name = "Height Field Mesh Editor")
            : CombinedObjJson3DVisualizer(name)
            , m_heightFieldComp(nullptr)
            , m_history{ 100 }
        {
        }

        ~HeightFieldMeshComponentVisualizer() override
        {
            ReleaseHeightFieldComponent();
        }

        // Override render to use custom property inspector
        bool Render(bool* isOpen) override;

        void Initialize() override
        {
            CombinedObjJson3DVisualizer::Initialize();
            m_selectionManager.SetCommandHistory(&m_history);
            RegisterHeightFieldActions();
        }

    protected:
        bool AttachMeshFromPath(const std::string& meshPath) override;
        void ReleaseHeightFieldComponent();
        void RecalculateMeshNormals(Group& group, const bgfx::VertexLayout& layout);
    private:
        CHeightFieldMeshComponent* m_heightFieldComp;
        PropertyInspector m_propertyInspector;
        CSelectionManager m_selectionManager;
        std::vector<std::shared_ptr<CMeshVertexSelectable>> m_pointSelectables;

        CCommandHistory m_history; // command history

        // Gizmo drag undo state:
        bool m_wasGizmoDragging = false;
        std::vector<CHeightFieldEditCommand::Entry> m_gizmoInitialEntries;

        // Initialization state
        bool m_selectablesRegistered = false;

        // Persistent buffers for regenerated mesh data
        std::vector<uint8_t> m_regeneratedVertexBuffer;
        std::vector<uint16_t> m_regeneratedIndexBuffer;

        void RegisterHeightFieldPoints();
        void ClearHeightFieldPoints();
        void RenderHeightFieldPointSelection(bgfx::ViewId viewId, Rendering::BgfxRenderPrimitives& prims);
        void UpdateHeightFieldFromGizmoDrag();
        void RegisterHeightFieldActions();
        void RegenerateGridMesh();

        // Helper to create a relative asset path from an absolute path
        std::string MakeAssetPath(const std::string& absolutePath) const;
    };

} // namespace ImGuiVisualizers