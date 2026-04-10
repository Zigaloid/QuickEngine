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
        std::vector<std::string> dropdownOptions;  // for Dropdown type
        std::string fileFilter;                     // for FilePicker type (e.g. "*.png;*.jpg")
    };
    class WidgetEntry : public CReflectedBase
    {
    public:
        REFL_DECLARE_OBJECT(WidgetEntry, CReflectedBase);

        EditorWidgetType type = EditorWidgetType::Default;
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
        EditorWidgetType GetWidget(const std::string& memberName) const
        {
            int idx = FindIndex(memberName);
            if (idx >= 0) {
                return m_MemberWidgets[idx]->type;
            }
            return EditorWidgetType::Default;
        }

        // Query optional config (returns nullptr if no config set)
        const WidgetConfig* GetConfig(const std::string& memberName) const
        {
            int idx = FindIndex(memberName);
            if (idx >= 0 && m_MemberWidgets[idx]->hasConfig) {
                return &m_MemberWidgets[idx]->config;
            }
            return nullptr;
        }

        // Check if a member has a custom mapping
        bool HasCustomWidget(const std::string& memberName) const
        {
            return FindIndex(memberName) >= 0;
        }

        // Bulk set from initializer list
        void SetWidgets(std::initializer_list<std::pair<std::string, EditorWidgetType>> mappings)
        {
            for (const auto& p : mappings) {
                SetWidget(p.first, p.second);
            }
        }

        // Deep-copy clone for creating an owned unique_ptr copy.
        std::unique_ptr<PropertyWidgetMap> Clone() const;

    private:

        // Parallel vectors for serialization-friendly layout
        std::vector<std::string> m_MemberNames;
        std::vector<std::unique_ptr<WidgetEntry>> m_MemberWidgets;

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