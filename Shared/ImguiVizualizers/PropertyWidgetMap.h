#pragma once

#include <string>
#include <vector>
#include <initializer_list>
#include <utility>
#include <memory>
#include "Reflection\reflection.h"

namespace ImGuiVisualizers {

	enum EditorWidgetType {
		Default,        // Use the current type-based auto widget (preserves existing behavior)
		InputField,     // Standard text/number input box
		Slider,         // Slider for numeric types (float, int)
		Drag,           // Drag-to-edit for numeric types
		ColorPicker,    // Color picker for Vector3/Vector4
		Checkbox,       // Checkbox for bool
		Dropdown,       // Dropdown/combo for string or int (requires dropdownOptions)
		TextArea,       // Multiline text for string
		FilePicker,     // File picker (string path) - new
		ReadOnly,       // Force read-only display regardless of inspector setting
	};

	class WidgetConfig : public CReflectedBase
	{
	public:
		REFL_DECLARE_OBJECT(WidgetConfig, CReflectedBase);

		float minValue = 0.0f;
		float maxValue = 1.0f;
		float step = 0.01f;
		std::vector<std::string> dropdownOptions;	// for Dropdown type
		std::string fileFilter;                     // for FilePicker type (e.g. "*.png;*.jpg")
		std::string defaultFolder;                  // for FilePicker type (e.g. "*.png;*.jpg")
	};
	class WidgetEntry : public CReflectedBase
	{
	public:
		REFL_DECLARE_OBJECT(WidgetEntry, CReflectedBase);
		EditorWidgetType type = EditorWidgetType::Default;
		std::string displayName;                    // Name that shows in the inspector instead of the member name (optional)
		bool isAdvanced = false;                    // can be used to hide complex config options behind an "Advanced" toggle in the UI	
		bool hasConfig = false;
		WidgetConfig config;
	};

	class PropertyWidgetMap : public CReflectedBase
	{
	public:
		REFL_DECLARE_OBJECT(PropertyWidgetMap, CReflectedBase);

		PropertyWidgetMap() = default;
		~PropertyWidgetMap() = default;

		// Set the widget for a specific member name
		void SetWidget(const std::string& memberName, EditorWidgetType widgetType)
		{
			int idx = FindIndex(memberName);
			if (idx >= 0) {
				m_MemberWidgets[idx]->type = widgetType;
				m_MemberWidgets[idx]->hasConfig = false;
				return;
			}
			m_MemberNames.push_back(memberName);
			auto entry = std::make_unique<WidgetEntry>();
			entry->type = widgetType;
			m_MemberWidgets.push_back(std::move(entry));
		}

		// Set widget with extra config (e.g., min/max for sliders, options for dropdowns)
		void SetWidget(const std::string& memberName, EditorWidgetType widgetType, const WidgetConfig& config)
		{
			int idx = FindIndex(memberName);
			if (idx >= 0) {
				m_MemberWidgets[idx]->type = widgetType;
				m_MemberWidgets[idx]->config = config;
				m_MemberWidgets[idx]->hasConfig = true;
				return;
			}
			m_MemberNames.push_back(memberName);
			auto entry = std::make_unique<WidgetEntry>();
			entry->type = widgetType;
			entry->config = config;
			entry->hasConfig = true;
			m_MemberWidgets.push_back(std::move(entry));
		}

		// Set widget and also configure displayName/isAdvanced (no config)
		void SetWidget(const std::string& memberName, EditorWidgetType widgetType, const std::string& displayName, bool isAdvanced);

		// Set widget with config and also configure displayName/isAdvanced
		void SetWidget(const std::string& memberName, EditorWidgetType widgetType, const WidgetConfig& config, const std::string& displayName, bool isAdvanced);

		// Remove a custom widget mapping for a member. Returns true if removed.
		bool RemoveWidget(const std::string& memberName)
		{
			int idx = FindIndex(memberName);
			if (idx < 0) return false;
			m_MemberNames.erase(m_MemberNames.begin() + idx);
			m_MemberWidgets.erase(m_MemberWidgets.begin() + idx);
			return true;
		}

		// Query which widget to use for a given member (returns Default if not mapped)
		EditorWidgetType GetWidget(const std::string& memberName) const;

		// Query optional config (returns nullptr if no config set)
		// If the config is inherited from a parent, a small internal cache is used and a pointer
		// to that cached copy is returned (valid until the next call that returns an inherited config).
		const WidgetConfig* GetConfig(const std::string& memberName) const;

		// Check if a member has a custom mapping (local-only)
		bool HasCustomWidget(const std::string& memberName) const
		{
			return FindIndex(memberName) >= 0;
		}

		// Return local display name for a member if configured, otherwise empty string.
		// This exposes the WidgetEntry::displayName without exposing internal vectors.
		std::string GetEntryDisplayNameLocal(const std::string& memberName) const;

		// Return display name for a member, walking the reflection hierarchy if not found locally.
		std::string GetEntryDisplayName(const std::string& memberName) const;

		// Return local isAdvanced flag for a member if configured, otherwise false.
		bool GetEntryIsAdvancedLocal(const std::string& memberName) const;

		// Return isAdvanced flag for a member, walking the reflection hierarchy if not found locally.
		bool GetEntryIsAdvanced(const std::string& memberName) const;

		// Bulk set from initializer list
		void SetWidgets(std::initializer_list<std::pair<std::string, EditorWidgetType>> mappings)
		{
			for (const auto& p : mappings) {
				SetWidget(p.first, p.second);
			}
		}

		// Deep-copy clone for creating an owned unique_ptr copy.
		std::unique_ptr<PropertyWidgetMap> Clone() const;

		// Resolve a widget for `memberName` starting at `className` and walking the reflection
		// hierarchy (derived -> parent -> ...). Returns true if a mapping was found.
		// If found, outType is set. If outConfig != nullptr and a config exists, it is copied.
		// If outOrigin != nullptr, it will be assigned the class name that declared the mapping.
		static bool FindWidgetInHierarchy(const std::string& className, const std::string& memberName,
			EditorWidgetType& outType, WidgetConfig* outConfig = nullptr, std::string* outOrigin = nullptr);

	private:

		// Parallel vectors for serialization-friendly layout
		std::vector<std::string> m_MemberNames;
		std::vector<std::unique_ptr<WidgetEntry>> m_MemberWidgets;

		// small mutable cache used when returning an inherited config pointer from a const method
		mutable std::unique_ptr<WidgetConfig> m_CachedInheritedConfig;
		mutable std::string m_CachedInheritedMember;

		// Helper: linear search for index, returns -1 if not found
		int FindIndex(const std::string& memberName) const
		{
			for (size_t i = 0; i < m_MemberNames.size(); ++i) {
				if (m_MemberNames[i] == memberName) return static_cast<int>(i);
			}
			return -1;
		}
	};

} // namespace ImGuiVisualizers