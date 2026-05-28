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

        void Shutdown() override
        {
            if (m_heightFieldComp) m_heightFieldComp->Shutdown();
            ReleaseHeightFieldComponent();            
            CombinedObjJson3DVisualizer::Shutdown();
        }

        void RecalculateMeshNormals(Group& group, const bgfx::VertexLayout& layout);
    protected:
        bool AttachMeshFromPath(const std::string& meshPath) override;
        void ReleaseHeightFieldComponent();
        
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

        // Brush tool state (airbrush for height)
        float m_brushRadius = 1.0f;       // world units
        float m_brushIntensity = 0.05f;   // delta Y applied per brush sample
        bool  m_brushInvert = false;      // raise (false) / lower (true)
        bool  m_brushPainting = false;    // true while mouse is down and painting
        std::vector<CHeightFieldEditCommand::Entry> m_brushInitialEntries; // snapshot for undo
        bool  m_wasBrushMouseDown = false;  // was mouse down last frame

        // Apply the brush at a given world-space position.
        // If recordInitial==true, the function will snapshot 'before' values for undo.
        void ApplyBrushAtWorldPosition(const Vector3f& worldPos, float radius, float intensity, bool invert, bool recordInitial);

        void RegisterHeightFieldPoints();
        void ClearHeightFieldPoints();
        void RenderHeightFieldPointSelection(bgfx::ViewId viewId, Rendering::BgfxRenderPrimitives& prims);
        void RenderBrushVisualization(bgfx::ViewId viewId, Rendering::BgfxRenderPrimitives & prims);
        void UpdateHeightFieldFromGizmoDrag();
        void RegisterHeightFieldActions();
        void RegenerateGridMesh();

        // Helper to create a relative asset path from an absolute path
        std::string MakeAssetPath(const std::string& absolutePath) const;
    };

} // namespace ImGuiVisualizers