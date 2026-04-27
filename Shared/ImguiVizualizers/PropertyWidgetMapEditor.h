#pragma once

#include "IImGuiVisualizer.h"
#include "Reflection/ReflectionBase.h"
#include "PropertyWidgetMapRegistry.h"
#include "PropertyWidgetMap.h"
#include "UnifiedActionManager.h"

#include <string>
#include <vector>
#include <memory>

namespace ImGuiVisualizers {

	class PropertyWidgetMapEditor : public IImGuiVisualizer {
	public:
		PropertyWidgetMapEditor();
		~PropertyWidgetMapEditor() override;

		// IImGuiVisualizer
		void Initialize() override;
		void Shutdown() override;
		bool Render(bool* isOpen) override;
		const char* GetName() const override { return "Property Widget Map Editor"; }
		const char* GetMenuCategory() const override { return "Tools"; }

		// Document key helper used by DocumentManager
		static std::string MakeDocumentKey(const std::string& assetPath);

		// Load the given widgets file into the singleton PropertyWidgetMapRegistry instance.
		// Returns true on success; on success the editor will refresh its class list.
		bool SetPath(const std::string& path);

		// Save the current registry state back to the last-set path.
		// Returns true on success.
		bool Save();

	private:
		void RefreshClassList();
		void PopulateMemberList(const std::string& className);
		void LoadSelectedMemberMapping(); // copy mapping into UI state
		void ApplySelectedMemberMapping(); // apply UI state back to registry

		// Register/unregister toolbar/menu actions
		void RegisterEditorActions();
		void UnregisterEditorActions();

		std::vector<std::string> m_classNames;
		int m_selectedClassIndex = -1;
		std::string m_selectedClassName;

		// Members of currently selected class (includes inherited members)
		std::vector<std::string> m_memberNames;
		int m_selectedMemberIndex = -1;

		// Temporary instance used only to query reflection maps for the selected class
		std::unique_ptr<CReflectedBase> m_tempInstance;

		// Path used for SafeRead / SafeWrite
		std::string m_currentPath;

		// UI-editable state for the currently selected member
		EditorWidgetType m_editWidgetType = EditorWidgetType::Default;
		bool m_editHasConfig = false;
		WidgetConfig m_editConfig;
		std::string m_editDisplayName;
		bool m_editIsAdvanced = false;
		// If the loaded mapping is inherited from a parent class, these track that origin.
		bool m_mappingIsInherited = false;
		std::string m_mappingOriginClass;

		char m_filterBuf[256] = { 0 };

		// Unified action manager for toolbar/menu integration
		UI::UnifiedActionManager m_actionManager;
	};
} // namespace ImGuiVisualizers