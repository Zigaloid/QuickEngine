#include "LevelComponentVisualizer.h"
#include "CoreSystem/CoreSystem.h"
#include "CoreSystem/AppConfig.h"
#include "FileSystem/FileSystemManager.h"
#include "EntityComponent.h"
#include "TransformComponent.h"
#include "PhysicsBodyComponent.h"
#include "CharacterComponent.h"
#include "LightManagerComponent.h"
#include "DeleteEntityCommand.h"
#include <imgui-docking/imgui_internal.h>
#include <bx/bounds.h>
#include <algorithm>
#include <cfloat>
#include "Net/NexusClient.h"
#include "SharedNexusDefines.h"

// Undefine Windows GDI macro that conflicts with our GetObject() method
#undef GetObject

namespace ImGuiVisualizers {

	bool LevelComponentVisualizer::AttachMeshFromPath(const std::string& levelPath)
	{
		// Get the object currently held by the editor first (may be the same instance we already have)
		CLevelComponent* newLevel = dynamic_cast<CLevelComponent*>(GetEditor().GetObject());
		if (!newLevel)
		{
			std::cerr << "LevelComponentVisualizer: Failed to cast loaded object to CLevelComponent" << std::endl;
			return false;
		}

		// If the editor returned a different instance than our current one, release the old one.
		// Do NOT call Shutdown() on the editor-owned instance (newLevel) — ReleaseLevelComponent()
		// will only Shutdown the existing m_levelComp if it is different. This avoids destroying
		// children on the editor's in-memory object when Save triggers a re-attach.
		if (newLevel != m_levelComp)
		{
			ReleaseLevelComponent();
			m_levelComp = newLevel;

			// Initialize only if not already initialized
			if (!m_levelComp->IsInitialized())
			{
				m_levelComp->Initialize();
			}
		}
		else
		{
			// Same instance: clear selection/render state but do not Shutdown the component,
			// which would clear its children.
			m_selectionManager.ClearSelectables();
			m_componentSelectables.clear();
			// Re-register render callback below (keeps rendering / gizmo state consistent).
		}

		// Install render callback and register render components for the (re)attached level
		Get3DView().SetRenderCallback([this](bgfx::ViewId viewId, BgfxRenderPrimitives& prims) {
			if (!m_levelComp) return;
			if (!m_levelComp->IsReady()) return;

			// Tick the light manager explicitly so its resolved light state is
			// up to date before any mesh component reads it during rendering.
			if (auto* lightManager = m_levelComp->FindDescendant<CLightManagerComponent>())
				lightManager->Update(0.0);

			RenderComponentHierarchy(viewId, m_levelComp);
			m_selectionManager.RenderSelectionGizmo(Get3DView().GetFrameBuffer(), m_gizmoMode, 2.0f);
		});

		RegisterRenderComponents(m_levelComp);

		// Initialize the layers panel with the level component
		m_layersPanel.SetLevelComponent(m_levelComp);
		m_layersPanel.SetSelectionManager(&m_selectionManager);

		// Trigger entity asset refresh
		m_entityAssetsNeedRefresh = true;

		return true;
	}

	void LevelComponentVisualizer::ReleaseLevelComponent()
	{
		// Clear render callback to avoid stale calls
		Get3DView().SetRenderCallback(nullptr);

		// Ensure gizmo renderer releases its GPU resources promptly.
		m_selectionManager.ShutdownGizmo();

		// Clear selectable registry and our local references
		m_selectionManager.ClearSelectables();
		m_componentSelectables.clear();

		if (m_levelComp)
		{
			// Only call Shutdown() if the ComponentManager is still alive.
			// If CoreSystem has already shut it down, the component memory has been
			// freed and m_levelComp is a dangling pointer – do not touch it.
			auto* manager = Core::CoreSystem::GetComponentManager();
			if (manager && manager->IsInitialized() && m_levelComp->IsInitialized())
			{
				m_levelComp->Shutdown();
			}
			m_levelComp = nullptr;
		}
	}

	void LevelComponentVisualizer::RegisterLevelActions()
	{
		auto& am = m_editor.GetActionManager();

		am.RegisterAction({
			.path = "View.GizmoMode.Translate",
			.description = "Set gizmo to Translate mode",
			.targets = UI::ActionTarget::Menu | UI::ActionTarget::Console,
			.callback = [this]() { m_gizmoMode = GizmoMode::Translate; },
			.isEnabled = [this]() { return m_levelComp != nullptr; },
			.isChecked = [this]() { return m_gizmoMode == GizmoMode::Translate; },
			.sortPriority = 10
			});

		am.RegisterAction({
			.path = "View.GizmoMode.Scale",
			.description = "Set gizmo to Scale mode",
			.targets = UI::ActionTarget::Menu | UI::ActionTarget::Console,
			.callback = [this]() { m_gizmoMode = GizmoMode::Scale; },
			.isEnabled = [this]() { return m_levelComp != nullptr; },
			.isChecked = [this]() { return m_gizmoMode == GizmoMode::Scale; },
			.sortPriority = 20
			});

		am.RegisterAction({
			.path = "View.GizmoMode.Rotate",
			.description = "Set gizmo to Rotate mode",
			.targets = UI::ActionTarget::Menu | UI::ActionTarget::Console,
			.callback = [this]() { m_gizmoMode = GizmoMode::Rotate; },
			.isEnabled = [this]() { return m_levelComp != nullptr; },
			.isChecked = [this]() { return m_gizmoMode == GizmoMode::Rotate; },
			.sortPriority = 30
			});
		am.RegisterAction({
			.path = "View.FocusOnSelection",
			.description = "Move the camera to focus on the selected objects",
			.shortcutHint = "F",
			.targets = UI::ActionTarget::Menu | UI::ActionTarget::Console,
			.callback = [this]() { m_selectionManager.FocusCameraOnSelection(); },
			.isEnabled = [this]() { return !m_selectionManager.GetAllSelected().empty(); },
			.sortPriority = 40
			});
		am.RegisterAction({
			.path = "File.Save",
			.description = "Save the current level.",
			.targets = UI::ActionTarget::Toolbar | UI::ActionTarget::Menu | UI::ActionTarget::Console,
			.callback = [this]() 
			{ 				
				m_editor.Save();
				NEXUS_SEND_MESSAGE(GAME_PIPE, MSG_TYPE_CONSOLE_COMMAND, "Level_Reload");
			},
			.isEnabled = [this]() { return m_levelComp != nullptr; },
			.isChecked = [this]() { return m_gizmoMode == GizmoMode::Rotate; },
			.sortPriority = 30
			});
	}

	void LevelComponentVisualizer::RegisterRenderComponents(ComponentSystem::Component* root)
	{
		RegisterRenderComponentsReturning(root);
	}

	std::vector<std::shared_ptr<CSelectable>> LevelComponentVisualizer::RegisterRenderComponentsReturning(ComponentSystem::Component* root)
	{
		std::vector<CRenderComponent*> renderComps;
		CollectRenderComponents(root, renderComps);

		std::vector<std::shared_ptr<CSelectable>> result;
		result.reserve(renderComps.size());

		for (CRenderComponent* rc : renderComps)
		{
			auto selectable = std::make_shared<CRenderComponentSelectable>(rc);
			m_componentSelectables.push_back(selectable);
			m_selectionManager.AddSelectable(selectable);
			result.push_back(selectable);
		}

		return result;
	}

	void LevelComponentVisualizer::DuplicateSelectedEntities()
	{
		if (!m_levelComp)
			return;

		// Collect the unique CEntityComponent parents from the current selection.
		std::vector<CEntityComponent*> sourceEntities;
		for (const auto& sel : m_selectionManager.GetAllSelected())
		{
			auto* owner = dynamic_cast<CEntityComponent*>(sel->GetOwner());
			if (!owner)
				continue;
			if (std::find(sourceEntities.begin(), sourceEntities.end(), owner) == sourceEntities.end())
				sourceEntities.push_back(owner);
		}

		if (sourceEntities.empty())
			return;

		auto cmd = std::make_unique<CDuplicateEntitiesCommand>(
			m_levelComp,
			sourceEntities,
			&m_selectionManager,
			[this](ComponentSystem::Component* root) { return RegisterRenderComponentsReturning(root); },
			[this](CEntityComponent* e) { DeregisterEntitySelectables(e); });

		m_history.Push(std::move(cmd));
	}

	void LevelComponentVisualizer::RenderComponentHierarchy(bgfx::ViewId viewId, ComponentSystem::Component* comp)
	{
		if (!comp) return;
	// Helper to test whether the component belongs to a visible-in-editor layer.
	auto IsInVisibleLayer = [](ComponentSystem::Component* c) -> bool
	{
		ComponentSystem::Component* cur = c;
		while (cur)
		{
			if (auto* lvl = dynamic_cast<CLevelComponent*>(cur))
				return lvl->IsVisibleInEditor();
			cur = cur->GetParent();
		}
		return true;
	};

	if (auto* renderComp = dynamic_cast<CRenderComponent*>(comp))
	{
		if (renderComp->IsActive() && IsInVisibleLayer(renderComp))
		{
			renderComp->Render(viewId);
		}
	}
        // For other physics components (e.g. CCharacterComponent), call DebugRender
		// using the sibling CTransformComponent's model matrix as the world base.
		// Skip debug rendering when the physics component (or its ancestors)
		// are inactive so layer visibility is respected.
		if (auto* physComp = dynamic_cast<CPhysicsComponent*>(comp))
		{
			// Respect component active state in the hierarchy (covers layer on/off)
			if (!physComp->IsActive() || !physComp->IsActiveInHierarchy())
			{
				// Do not render debug for inactive components
			}
			else
			{
				Matrix4f transform = physComp->GetObjectMatrix();

				// Use FindSibling helper to locate a CTransformComponent sibling if present
				if (auto* transformComp = comp->FindSibling<CTransformComponent>())
				{
					auto modelMatrix = transformComp->GetTransform();
					transform = modelMatrix * transform;
				}

                // Also respect editor visibility of containing layer
				if (IsInVisibleLayer(physComp))
					physComp->DebugRender(viewId, transform);
			}
		}

		for (auto* child : comp->GetChildren())
		{
			RenderComponentHierarchy(viewId, child);
		}
	}

	void LevelComponentVisualizer::CollectRenderComponents(ComponentSystem::Component* comp,
		std::vector<CRenderComponent*>& out)
	{
		if (!comp) return;

		if (auto* rc = dynamic_cast<CRenderComponent*>(comp))
		{
			rc->OnInitialize(); // Ensure the component is initialized before rendering or selection
			out.push_back(rc);
		}

		for (auto* child : comp->GetChildren())
		{
			CollectRenderComponents(child, out);
		}
	}

	void LevelComponentVisualizer::RenderSelectionHighlight(bgfx::ViewId viewId,
		BgfxRenderPrimitives& prims)
	{
		const auto& allSelected = m_selectionManager.GetAllSelected();
		if (allSelected.empty()) return;

		const std::shared_ptr<CSelectable> lastSelected = m_selectionManager.GetSelected();

	// Helper to test whether the component belongs to a visible-in-editor layer.
	auto IsInVisibleLayer = [](ComponentSystem::Component* c) -> bool
	{
		ComponentSystem::Component* cur = c;
		while (cur)
		{
			if (auto* lvl = dynamic_cast<CLevelComponent*>(cur))
				return lvl->IsVisibleInEditor();
			cur = cur->GetParent();
		}
		return true;
	};

	for (const auto& selectable : allSelected)
		{
			auto rc = std::dynamic_pointer_cast<CRenderComponentSelectable>(selectable);
			if (!rc) continue;

			CRenderComponent* comp = rc->GetComponent();
        if (!comp || !comp->IsActive()) continue;
		if (!IsInVisibleLayer(comp)) continue;

			const Vector4f bs = *comp->GetBoundingSphere();
			auto modelMatrixPtr = comp->GetModelMatrix();
			if (!modelMatrixPtr) continue;
			const float* m = modelMatrixPtr->data();

			const float cx = m[0] * bs.x + m[4] * bs.y + m[8] * bs.z + m[12];
			const float cy = m[1] * bs.x + m[5] * bs.y + m[9] * bs.z + m[13];
			const float cz = m[2] * bs.x + m[6] * bs.y + m[10] * bs.z + m[14];

			const float sx = bx::sqrt(m[0] * m[0] + m[1] * m[1] + m[2] * m[2]);
			const float sy = bx::sqrt(m[4] * m[4] + m[5] * m[5] + m[6] * m[6]);
			const float sz = bx::sqrt(m[8] * m[8] + m[9] * m[9] + m[10] * m[10]);
			const float radius = bs.w * bx::max(sx, bx::max(sy, sz));
			const float r = radius > 0.0f ? radius : 0.5f;

			float highlightMtx[16];
			bx::mtxSRT(highlightMtx, r, r, r, 0.0f, 0.0f, 0.0f, cx, cy, cz);

			// Most-recently-picked object renders in yellow; others in white
			const uint32_t color = (selectable == lastSelected)
				? 0xff00ffff   // yellow (ABGR)
				: 0xffffffff;  // white  (ABGR)

			prims.RenderSphere(viewId, highlightMtx, color);
		}
	}

	void LevelComponentVisualizer::DeregisterEntitySelectables(CEntityComponent* entity)
	{
		if (!entity)
			return;

		// Remove every selectable whose owning entity matches the one being deleted.
		// Walk a copy so we can safely erase from m_componentSelectables mid-loop.
		auto it = m_componentSelectables.begin();
		while (it != m_componentSelectables.end())
		{
			const auto& sel = *it;

			// Walk up the component hierarchy to find the owning CEntityComponent.
			ComponentSystem::Component* current =
				sel->GetComponent() ? sel->GetComponent()->GetParent() : nullptr;

			bool belongsToEntity = false;
			while (current)
			{
				if (current == entity) { belongsToEntity = true; break; }
				current = current->GetParent();
			}

			if (belongsToEntity)
			{
				m_selectionManager.RemoveSelectable(*it);
				it = m_componentSelectables.erase(it);
			}
			else
			{
				++it;
			}
		}
	}

	void LevelComponentVisualizer::DeleteSelectedEntities()
	{
		if (!m_levelComp)
			return;

		// Collect the unique CEntityComponent owners from the current selection.
		// Multiple selected render components may share the same entity.
		std::vector<CEntityComponent*> toDelete;
		for (const auto& sel : m_selectionManager.GetAllSelected())
		{
			auto* owner = dynamic_cast<CEntityComponent*>(sel->GetOwner());
			if (!owner)
				continue;
			if (std::find(toDelete.begin(), toDelete.end(), owner) == toDelete.end())
				toDelete.push_back(owner);
		}

		if (toDelete.empty())
			return;

		for (CEntityComponent* entity : toDelete)
		{
			auto cmd = std::make_unique<CDeleteEntityCommand>(
				m_levelComp,
				entity,
				&m_selectionManager,
				[this](ComponentSystem::Component* root) { RegisterRenderComponents(root); },
				[this](CEntityComponent* e) { DeregisterEntitySelectables(e); });

			m_history.Push(std::move(cmd));
		}
	}

	bool LevelComponentVisualizer::Render(bool* isOpen)
	{
		if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Z)) m_history.Undo();
		if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Y)) m_history.Redo();

		// Build window title
		std::string title = m_windowName;
		if (!m_fileName.empty())
			title += " - " + m_fileName;

		ImGuiWindowFlags hostFlags =
			ImGuiWindowFlags_MenuBar          |
			ImGuiWindowFlags_NoScrollbar      |
			ImGuiWindowFlags_NoScrollWithMouse;

		if (!ImGui::Begin(title.c_str(), isOpen, hostFlags))
		{
			ImGui::End();
			return false;
		}

		// ── Menu bar ─────────────────────────────────────────────────────────
		if (ImGui::BeginMenuBar())
		{
			m_editor.GetActionManager().RenderMenuBar();

			if (ImGui::BeginMenu("View"))
			{
				if (ImGui::MenuItem("Reset Layout"))
					m_dockInitialized = false;
				ImGui::EndMenu();
			}

			ImGui::EndMenuBar();
		}

		// ── Toolbar ───────────────────────────────────────────────────────────
		m_editor.GetActionManager().RenderToolbar();

		ImGui::SameLine();
		ImGui::SetNextItemWidth(110.0f);
		static constexpr const char* k_gizmoModeLabels[] = { "Translate", "Scale", "Rotate" };
		int gizmoModeIdx = static_cast<int>(m_gizmoMode);
		if (ImGui::Combo("##GizmoMode", &gizmoModeIdx, k_gizmoModeLabels, 3))
			m_gizmoMode = static_cast<GizmoMode>(gizmoModeIdx);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Gizmo Mode");

		// ── Refresh entity assets ─────────────────────────────────────────────
		if (m_entityAssetsNeedRefresh)
		{
			RefreshEntityAssets();
			m_entityAssetsNeedRefresh = false;
		}

		// ── DockSpace ─────────────────────────────────────────────────────────
		// Derive the dockspace ID directly from the host window so the ID is
		// stable and absolute — not relative to whatever is on the ID stack.
		// ImGui::GetID() inside a window produces a window-relative hash; using
		// ImHashStr on a fixed string gives a context-independent value that
		// matches what imgui.ini records under [Docking][Data].
		m_dockspaceId = ImHashStr("LevelEditorDockSpace");

		ImVec2 avail = ImGui::GetContentRegionAvail();
		ImGui::DockSpace(m_dockspaceId, avail, ImGuiDockNodeFlags_None);

		// Only seed the default layout when no layout was restored from imgui.ini.
		if (!m_dockInitialized)
		{  
			ImGuiDockNode* node = ImGui::DockBuilderGetNode(m_dockspaceId);
			const bool restoredFromIni = node != nullptr && !node->IsEmpty();

			if (!restoredFromIni)
			{
				ImGui::DockBuilderRemoveNode(m_dockspaceId);
				ImGui::DockBuilderAddNode(m_dockspaceId, ImGuiDockNodeFlags_DockSpace);
				ImGui::DockBuilderSetNodeSize(m_dockspaceId, avail);

				ImGui::DockBuilderDockWindow("Entity Assets", m_dockspaceId);
				ImGui::DockBuilderDockWindow("3D Viewport",   m_dockspaceId);
				ImGui::DockBuilderDockWindow("Layers",        m_dockspaceId);
				ImGui::DockBuilderDockWindow("Properties",    m_dockspaceId);
				ImGui::DockBuilderDockWindow("Object",        m_dockspaceId);

				ImGui::DockBuilderFinish(m_dockspaceId);
			}

			m_dockInitialized = true;
		}

		ImGui::End(); // host window must end before child windows are submitted

		// ── Entity Assets panel ───────────────────────────────────────────────
		ImGui::SetNextWindowDockID(m_dockspaceId, ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(250.0f, 400.0f), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Entity Assets", nullptr, ImGuiWindowFlags_NoCollapse))
		{
			RenderEntityAssetPanel();
		}
		ImGui::End();

		// ── 3-D Viewport panel ────────────────────────────────────────────────
		ImGui::SetNextWindowDockID(m_dockspaceId, ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(800.0f, 600.0f), ImGuiCond_FirstUseEver);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		if (ImGui::Begin("3D Viewport", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
		{
			ImVec2 vpAvail = ImGui::GetContentRegionAvail();
			if (vpAvail.x < 1.0f) vpAvail.x = 1.0f;
			if (vpAvail.y < 1.0f) vpAvail.y = 1.0f;

			m_view.RenderContent(vpAvail);
				
			// Viewport bounds are now cached in the 3D view
			m_selectionManager.SetViewInfo(Get3DView().GetCamera(), Get3DView().GetViewportMin(), Get3DView().GetViewportSize());

			// Delete key — only when mouse is over the viewport
			const ImVec2 mouse = ImGui::GetMousePos();
			if (Get3DView().IsPointInViewport(mouse) &&
 					ImGui::IsKeyPressed(ImGuiKey_Delete, /*repeat=*/false) &&
 					m_levelComp && !m_selectionManager.IsGizmoDragging())
 			{
 				DeleteSelectedEntities();
 			}

			if (Get3DView().IsPointInViewport(mouse) && ImGui::IsKeyPressed(ImGuiKey_F, /*repeat=*/false))
			{
				m_selectionManager.FocusCameraOnSelection();
			}

 			// Left-click pick (Alt = camera pan, skip picking)
 			if (!ImGui::GetIO().KeyAlt)
 			{
 				if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
 					!ImGui::IsMouseDragging(ImGuiMouseButton_Left))
 				{
 					const ImVec2 mouse = ImGui::GetMousePos();
					if (Get3DView().IsPointInViewport(mouse))
 						m_selectionManager.PickAtCursor();
 				}
 			}

			HandleEntityDrop();
		}
		ImGui::End();
		ImGui::PopStyleVar();

		// ── Layers panel ──────────────────────────────────────────────────────
		ImGui::SetNextWindowDockID(m_dockspaceId, ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(250.0f, 400.0f), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Layers", nullptr, ImGuiWindowFlags_NoCollapse))
		{
			m_layersPanel.Render();
		}
		ImGui::End();

		// ── Properties panel ──────────────────────────────────────────────────
		ImGui::SetNextWindowDockID(m_dockspaceId, ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(250.0f, 400.0f), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Properties", nullptr, ImGuiWindowFlags_NoCollapse))
		{
			RenderInspectorPanel();
		}
		ImGui::End();

		// ── Object panel ──────────────────────────────────────────────────────
		ImGui::SetNextWindowDockID(m_dockspaceId, ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(250.0f, 400.0f), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Object", nullptr, ImGuiWindowFlags_NoCollapse))
		{
			m_editor.RenderInspectorInline();
		}
		ImGui::End();

		return true;
	}

	void LevelComponentVisualizer::RenderEntityAssetPanel()
	{
		ImGui::TextColored(ImVec4(0.8f, 0.8f, 1.0f, 1.0f), "Entity Assets");
		ImGui::Separator();

		if (ImGui::Button("Refresh", ImVec2(-1, 0))) {
			m_entityAssetsNeedRefresh = true;
		}

		ImGui::Separator();

		// Render list of entity assets
		if (ImGui::BeginChild("##EntityList", ImVec2(0, 0), false)) {
			for (const auto& asset : m_entityAssets) {
				ImGui::PushID(asset.fullPath.c_str());

				ImGui::Selectable(asset.fileName.c_str(), false);

				// Enable drag-and-drop
				if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
					// Set payload to carry the entity asset full path
					ImGui::SetDragDropPayload("ENTITY_ASSET", asset.fullPath.c_str(), asset.fullPath.size() + 1);
					ImGui::Text("Add: %s", asset.fileName.c_str());
					ImGui::EndDragDropSource();
				}

				ImGui::PopID();
			}
		}
		ImGui::EndChild();
	}

	void LevelComponentVisualizer::RefreshEntityAssets()
	{
		m_entityAssets.clear();

		auto* fileSystem = Core::CoreSystem::GetFileSystemManager();
		if (!fileSystem) return;

		// Look for entity files in the Assets/Entities directory
		std::string entityPath = Core::AppConfig::Instance().ResolvePath("./Assets/Entities");
		auto dirResult = fileSystem->OpenDirectory(entityPath);
		if (!dirResult.IsSuccess()) return;

		auto& dir = dirResult.GetValue();
		auto filesResult = dir->GetFiles();
		if (!filesResult.IsSuccess()) return;

		for (const auto& info : filesResult.GetValue()) {
			// Filter for .entity files (or whatever extension you use)
			if (info.name.ends_with(".entity") || info.name.ends_with(".json")) {
				EntityAssetEntry entry;
				entry.fileName = info.name;
				entry.fullPath = info.fullPath;
				m_entityAssets.push_back(entry);
			}
		}

		// Sort alphabetically
		std::sort(m_entityAssets.begin(), m_entityAssets.end(),
			[](const EntityAssetEntry& a, const EntityAssetEntry& b) {
				return a.fileName < b.fileName;
			});
	}

	void LevelComponentVisualizer::HandleEntityDrop()
	{
		if (!m_levelComp) return;

		// Accept drag-drop payload
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_ASSET")) {
				const char* entityPath = static_cast<const char*>(payload->Data);

				// Add to the selected layer if one is active, otherwise fall back to the level root
				CLevelComponent* targetParent = m_layersPanel.GetSelectedLayer();
				if (!targetParent)
					targetParent = m_levelComp;

				// Create a new EntityComponent as a child of the target layer
				CEntityComponent* newEntity = targetParent->CreateChild<CEntityComponent>();
				if (newEntity) {
					// Load the entity definition from file using SafeRead
					auto result = newEntity->SafeRead(entityPath);
					if (result.IsSuccess()) {
						std::cout << "Successfully added entity from: " << entityPath << std::endl;

						// Register any new render components from the dropped entity
						RegisterRenderComponents(newEntity);
					}
					else {
						std::cerr << "Failed to load entity from: " << entityPath
							<< " - Error: " << result.GetError().message << std::endl;
					}
				}
			}
			ImGui::EndDragDropTarget();
		}
	}

	void LevelComponentVisualizer::RenderInspectorPanel()
	{
		if (!m_editor.IsLoaded())
		{
			ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No level loaded");
			return;
		}

		// Sync the property inspector to the most-recently selected object.
		const auto& lastSelected = m_selectionManager.GetSelected();
		CReflectedBase* owner = lastSelected ? lastSelected->GetOwner() : nullptr;
		if (owner != m_propertyInspector.GetObject())
			m_propertyInspector.SetObject(owner);

		if (owner)
			m_propertyInspector.RenderContent();
		else
			ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No object selected");
	}

} // namespace ImGuiVisualizers