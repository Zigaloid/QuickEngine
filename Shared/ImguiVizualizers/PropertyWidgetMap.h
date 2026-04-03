#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <initializer_list>
#include <utility>

namespace ImGuiVisualizers {

    enum class EditorWidgetType {
        Default,        // Use the current type-based auto widget (preserves existing behavior)
        InputField,     // Standard text/number input box
        Slider,         // Slider for numeric types (float, int)
        Drag,           // Drag-to-edit for numeric types
        ColorPicker,    // Color picker for Vector3/Vector4
        Checkbox,       // Checkbox for bool
        Dropdown,       // Dropdown/combo for string or int (requires dropdownOptions)
        TextArea,       // Multiline text for string
        ReadOnly,       // Force read-only display regardless of inspector setting
    };

    struct WidgetConfig {
        float minValue = 0.0f;
        float maxValue = 1.0f;
        float step = 0.01f;
        std::vector<std::string> dropdownOptions;  // for Dropdown type
        std::string fileFilter;                     // for FilePicker type
    };

    class PropertyWidgetMap
    {
    public:
        PropertyWidgetMap() = default;
        ~PropertyWidgetMap() = default;

        // Set the widget for a specific member name
        void SetWidget(const std::string& memberName, EditorWidgetType widgetType);

        // Set widget with extra config (e.g., min/max for sliders, options for dropdowns)
        void SetWidget(const std::string& memberName, EditorWidgetType widgetType, const WidgetConfig& config);

        // Query which widget to use for a given member (returns Default if not mapped)
        EditorWidgetType GetWidget(const std::string& memberName) const;

        // Query optional config (returns nullptr if no config set)
        const WidgetConfig* GetConfig(const std::string& memberName) const;

        // Check if a member has a custom mapping
        bool HasCustomWidget(const std::string& memberName) const;

        // Bulk set from initializer list
        void SetWidgets(std::initializer_list<std::pair<std::string, EditorWidgetType>> mappings);

    private:
        struct WidgetEntry {
            EditorWidgetType type = EditorWidgetType::Default;
            bool hasConfig = false;
            WidgetConfig config;
        };

        std::unordered_map<std::string, WidgetEntry> m_WidgetMap;
    };

} // namespace ImGuiVisualizers
