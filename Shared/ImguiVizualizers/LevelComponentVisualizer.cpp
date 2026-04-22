#include "LevelComponentVisualizer.h"
#include "CoreSystem/CoreSystem.h"
#include "FileSystem/FileSystemManager.h"
#include "EntityComponent.h"
#include <algorithm>

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

		Get3DView().SetRenderCallback([this](bgfx::ViewId viewId, BgfxRenderPrimitives& /*prims*/) {
			if (!m_levelComp) return;
			if (!m_levelComp->IsReady()) return;
			RenderComponentHierarchy(viewId, m_levelComp);
			});

		// Trigger entity asset refresh
		m_entityAssetsNeedRefresh = true;

		return true;
	}

	void LevelComponentVisualizer::ReleaseLevelComponent()
	{
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

	bool LevelComponentVisualizer::Render(bool* isOpen)
	{
		// Build window title
		std::string title = m_windowName;
		if (!m_fileName.empty()) {
			title += " - " + m_fileName;
		}

		if (!ImGui::Begin(title.c_str(), isOpen, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
			ImGui::End();
			return false;
		}

		// Render menu bar
		if (ImGui::BeginMenuBar())
		{
			m_editor.GetActionManager().RenderMenuBar();
			ImGui::EndMenuBar();
		}

		// Toolbar
		m_editor.GetActionManager().RenderToolbar();

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

						// The component hierarchy has been modified
						// You may want to call m_editor.Save() here if auto-save is desired
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