#include "LevelComponentVisualizer.h"
#include "CoreSystem/CoreSystem.h"
#include "FileSystem/FileSystemManager.h"
#include "EntityComponent.h"
#include "CDeleteEntityCommand.h"
#include <bx/bounds.h>
#include <algorithm>
#include <cfloat>

namespace ImGuiVisualizers {

	bool LevelComponentVisualizer::AttachMeshFromPath(const std::string& levelPath)
	{
		ReleaseLevelComponent();

		m_levelComp = dynamic_cast<CLevelComponent*>(GetEditor().GetObject());
		m_levelComp->Initialize();

		if (!m_levelComp)
		{
			std::cerr << "LevelComponentVisualizer: Failed to cast loaded object to CLevelComponent" << std::endl;
			return false;
		}

		Get3DView().SetRenderCallback([this](bgfx::ViewId viewId, BgfxRenderPrimitives& prims) {
			if (!m_levelComp) return;
			if (!m_levelComp->IsReady()) return;
			RenderComponentHierarchy(viewId, m_levelComp);
			m_selectionManager.RenderSelectionGizmo(viewId, m_gizmoMode, 2.0f);
			});

		RegisterRenderComponents(m_levelComp);

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
	}

	void LevelComponentVisualizer::RegisterRenderComponents(ComponentSystem::Component* root)
	{
		std::vector<CRenderComponent*> renderComps;
		CollectRenderComponents(root, renderComps);

		for (CRenderComponent* rc : renderComps)
		{
			auto selectable = std::make_shared<CRenderComponentSelectable>(rc);
			m_componentSelectables.push_back(selectable);
			m_selectionManager.AddSelectable(selectable);
		}
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

	void LevelComponentVisualizer::CollectRenderComponents(ComponentSystem::Component* comp,
		std::vector<CRenderComponent*>& out)
	{
		if (!comp) return;

		if (auto* rc = dynamic_cast<CRenderComponent*>(comp))
		{
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

		for (const auto& selectable : allSelected)
		{
			auto rc = std::dynamic_pointer_cast<CRenderComponentSelectable>(selectable);
			if (!rc) continue;

			CRenderComponent* comp = rc->GetComponent();
			if (!comp || !comp->IsActive()) continue;

			const Vector4f bs = *comp->GetBoundingSphere();
			const float* m = comp->GetModelMatrix()->data();

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

		// Delete selected entities when the Delete key is pressed inside the viewport.
		if (ImGui::IsKeyPressed(ImGuiKey_Delete, /*repeat=*/false) &&
			m_levelComp && !m_selectionManager.IsGizmoDragging())
		{
			DeleteSelectedEntities();
		}

		// Build window title
		std::string title = m_windowName;
		if (!m_fileName.empty()) {
			title += " - " + m_fileName;
		}

		if (!ImGui::Begin(title.c_str(), isOpen, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
			ImGui::End();
			return false;
		}

		// Render menu bar (includes View > GizmoMode submenu)
		if (ImGui::BeginMenuBar())
		{
			m_editor.GetActionManager().RenderMenuBar();
			ImGui::EndMenuBar();
		}

		// Toolbar
		m_editor.GetActionManager().RenderToolbar();

		// Gizmo mode combo — rendered inline on the same toolbar row
		ImGui::SameLine();
		ImGui::SetNextItemWidth(110.0f);
		static constexpr const char* k_gizmoModeLabels[] = { "Translate", "Scale", "Rotate" };
		int gizmoModeIdx = static_cast<int>(m_gizmoMode);
		if (ImGui::Combo("##GizmoMode", &gizmoModeIdx, k_gizmoModeLabels, 3))
		{
			m_gizmoMode = static_cast<GizmoMode>(gizmoModeIdx);
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Gizmo Mode");
		}

		// Refresh entity assets if needed
		if (m_entityAssetsNeedRefresh) {
			RefreshEntityAssets();
			m_entityAssetsNeedRefresh = false;
		}

		// Main content area
		ImVec2 avail = ImGui::GetContentRegionAvail();

		// Left panel: Entity Assets
		ImGui::BeginChild("##EntityAssets", ImVec2(m_entityPanelWidth, avail.y), true);
		RenderEntityAssetPanel();
		ImGui::EndChild();

		ImGui::SameLine();

		// Splitter handle
		ImGui::Button("##EntitySplitter", ImVec2(4.0f, avail.y));
		if (ImGui::IsItemActive()) {
			m_entityPanelWidth += ImGui::GetIO().MouseDelta.x;
			if (m_entityPanelWidth < 100.0f) m_entityPanelWidth = 100.0f;
			if (m_entityPanelWidth > avail.x * 0.5f) m_entityPanelWidth = avail.x * 0.5f;
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
		}

		ImGui::SameLine();

		// Calculate remaining width for the right side (3D view + inspector)
		float remainingWidth = avail.x - m_entityPanelWidth - 8.0f; // Account for splitter
		float leftW = remainingWidth * 0.65f;
		float rightW = remainingWidth - leftW;

		ImGui::BeginChild("##ContentArea", ImVec2(remainingWidth, avail.y), false);

		// 3D View on the left
		ImGui::BeginChild("##Left3D", ImVec2(leftW, 0), false);
		ImVec2 leftAvail = ImGui::GetContentRegionAvail();
		if (leftAvail.x < 1.0f) leftAvail.x = 1.0f;
		if (leftAvail.y < 1.0f) leftAvail.y = 1.0f;

		m_view.RenderContent(leftAvail);

		// Record the exact screen rect of the rendered image.
		m_viewportMin = ImGui::GetItemRectMin();
		m_viewportSize = ImGui::GetItemRectSize();

		// Update the manager with current view state
		m_selectionManager.SetViewInfo(Get3DView().GetCamera(), m_viewportMin, m_viewportSize);

		// Left-click inside the viewport (not a drag) → pick
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
			!ImGui::IsMouseDragging(ImGuiMouseButton_Left))
		{
			const ImVec2 mouse = ImGui::GetMousePos();
			const bool inViewport = mouse.x >= m_viewportMin.x &&
				mouse.y >= m_viewportMin.y &&
				mouse.x < m_viewportMin.x + m_viewportSize.x &&
				mouse.y < m_viewportMin.y + m_viewportSize.y;
			if (inViewport)
			{
				m_selectionManager.PickAtCursor();
			}
		}

		// Handle entity drop onto the 3D view
		HandleEntityDrop();

		ImGui::EndChild();

		ImGui::SameLine();

		// Inspector on the right
		ImGui::BeginChild("##RightInspector", ImVec2(rightW, 0), false);
		if (!m_editor.IsLoaded()) {
			ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No level loaded");
		}
		else {
			m_editor.RenderInspectorInline();
		}
		ImGui::EndChild();

		ImGui::EndChild(); // ContentArea

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
		std::string entityPath = "./Assets/Entities";
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

				// Create a new EntityComponent as a child of the level
				CEntityComponent* newEntity = m_levelComp->CreateChild<CEntityComponent>();
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

} // namespace ImGuiVisualizers