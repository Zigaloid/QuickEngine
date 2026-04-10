#include "PropertyWidgetMap.h"

namespace ImGuiVisualizers {
    REFL_DEFINE_OBJECT(PropertyWidgetMap)
        REFL_DEFINE_OBJECT_PTR_VECTOR_MEMBER(PropertyWidgetMap, m_MemberWidgets),
        REFL_DEFINE_STRING_VECTOR_MEMBER(PropertyWidgetMap, m_MemberNames)
    REFL_DEFINE_END


    REFL_DEFINE_OBJECT(WidgetConfig)
        REFL_DEFINE_FLOAT_MEMBER(WidgetConfig, minValue),
        REFL_DEFINE_FLOAT_MEMBER(WidgetConfig, maxValue),
        REFL_DEFINE_FLOAT_MEMBER(WidgetConfig, step),
        REFL_DEFINE_STRING_VECTOR_MEMBER(WidgetConfig, dropdownOptions),
        REFL_DEFINE_STRING_MEMBER(WidgetConfig, fileFilter)
    REFL_DEFINE_END


    REFL_DEFINE_OBJECT(WidgetEntry)
        REFL_DEFINE_INT_MEMBER(WidgetEntry, type),
        REFL_DEFINE_BOOL_MEMBER(WidgetEntry, hasConfig),
        REFL_DEFINE_OBJECT_MEMBER(WidgetEntry, config)
    REFL_DEFINE_END

        // Clone implementation: deep-copy member names and widget entries.
    std::unique_ptr<PropertyWidgetMap> PropertyWidgetMap::Clone() const
    {
        auto copy = std::make_unique<PropertyWidgetMap>();
        copy->m_MemberNames = m_MemberNames;
        copy->m_MemberWidgets.reserve(m_MemberWidgets.size());
        for (const auto& e : m_MemberWidgets) {
            if (!e) {
                copy->m_MemberWidgets.push_back(nullptr);
                continue;
            }
            auto newEntry = std::make_unique<WidgetEntry>();
            newEntry->type = e->type;
            newEntry->hasConfig = e->hasConfig;
            newEntry->config = e->config; // WidgetConfig is copyable
            copy->m_MemberWidgets.push_back(std::move(newEntry));
        }
        return copy;
    }

} // namespace ImGuiVisualizers


