#include "PropertyWidgetMap.h"

namespace ImGuiVisualizers {

    void PropertyWidgetMap::SetWidget(const std::string& memberName, EditorWidgetType widgetType)
    {
        WidgetEntry entry;
        entry.type = widgetType;
        entry.hasConfig = false;
        m_WidgetMap[memberName] = entry;
    }

    void PropertyWidgetMap::SetWidget(const std::string& memberName, EditorWidgetType widgetType, const WidgetConfig& config)
    {
        WidgetEntry entry;
        entry.type = widgetType;
        entry.hasConfig = true;
        entry.config = config;
        m_WidgetMap[memberName] = entry;
    }

    EditorWidgetType PropertyWidgetMap::GetWidget(const std::string& memberName) const
    {
        auto it = m_WidgetMap.find(memberName);
        if (it != m_WidgetMap.end()) {
            return it->second.type;
        }
        return EditorWidgetType::Default;
    }

    const WidgetConfig* PropertyWidgetMap::GetConfig(const std::string& memberName) const
    {
        auto it = m_WidgetMap.find(memberName);
        if (it != m_WidgetMap.end() && it->second.hasConfig) {
            return &it->second.config;
        }
        return nullptr;
    }

    bool PropertyWidgetMap::HasCustomWidget(const std::string& memberName) const
    {
        auto it = m_WidgetMap.find(memberName);
        return it != m_WidgetMap.end() && it->second.type != EditorWidgetType::Default;
    }

    void PropertyWidgetMap::SetWidgets(std::initializer_list<std::pair<std::string, EditorWidgetType>> mappings)
    {
        for (const auto& mapping : mappings) {
            SetWidget(mapping.first, mapping.second);
        }
    }

} // namespace ImGuiVisualizers
