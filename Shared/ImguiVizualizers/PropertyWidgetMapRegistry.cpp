#include "PropertyWidgetMapRegistry.h"
#include <algorithm>

namespace ImGuiVisualizers {


    // ── CResourceReference ────────────────────────────────────────────
    REFL_DEFINE_OBJECT(PropertyWidgetMapRegistry)
        REFL_DEFINE_OBJECT_PTR_VECTOR_MEMBER(PropertyWidgetMapRegistry, m_classWidgets),
        REFL_DEFINE_STRING_VECTOR_MEMBER(PropertyWidgetMapRegistry, m_classNames)
        REFL_DEFINE_END


        PropertyWidgetMapRegistry& PropertyWidgetMapRegistry::Instance()
    {
        static PropertyWidgetMapRegistry instance;
        return instance;
    }

    PropertyWidgetMapRegistry::PropertyWidgetMapRegistry()
    {
        this->SafeRead("./Assets/Editor/ClassWidgetRegistry.widgets.obj.json");

        // Create owned shared_ptr copies from the deserialized unique_ptrs so
        // the registry holds ownership while preserving m_classWidgets for serialization.
        m_ownedMaps.reserve(m_classWidgets.size());
        for (const auto& w : m_classWidgets) {
            if (w) {
                m_ownedMaps.push_back(std::shared_ptr<PropertyWidgetMap>(w->Clone().release()));
            }
            else {
                m_ownedMaps.push_back(nullptr);
            }
        }
    }

    void PropertyWidgetMapRegistry::Register(const std::string& className, std::shared_ptr<PropertyWidgetMap> map)
    {
        std::lock_guard<std::mutex> lock(m_Mutex);

        auto it = std::find(m_classNames.begin(), m_classNames.end(), className);
        if (it != m_classNames.end()) {
            // replace existing
            size_t idx = static_cast<size_t>(std::distance(m_classNames.begin(), it));
            m_ownedMaps[idx] = std::move(map);
            // create an owned unique_ptr clone for serialization/iteration vector
            m_classWidgets[idx] = m_ownedMaps[idx] ? m_ownedMaps[idx]->Clone() : nullptr;
        }
        else {
            // append new entry
            m_classNames.push_back(className);
            m_ownedMaps.push_back(std::move(map));
            m_classWidgets.push_back(m_ownedMaps.back() ? m_ownedMaps.back()->Clone() : nullptr);
        }
    }

    std::shared_ptr<PropertyWidgetMap> PropertyWidgetMapRegistry::Get(const std::string& className) const
    {
        std::lock_guard<std::mutex> lock(m_Mutex);

        auto it = std::find(m_classNames.begin(), m_classNames.end(), className);
        if (it != m_classNames.end()) {
            size_t idx = static_cast<size_t>(std::distance(m_classNames.begin(), it));
            return m_ownedMaps[idx];
        }
        return nullptr;
    }

    void PropertyWidgetMapRegistry::Unregister(const std::string& className)
    {
        std::lock_guard<std::mutex> lock(m_Mutex);

        auto it = std::find(m_classNames.begin(), m_classNames.end(), className);
        if (it != m_classNames.end()) {
            size_t idx = static_cast<size_t>(std::distance(m_classNames.begin(), it));
            // erase the associated entries from all vectors
            m_classNames.erase(m_classNames.begin() + idx);
            m_classWidgets.erase(m_classWidgets.begin() + idx);
            m_ownedMaps.erase(m_ownedMaps.begin() + idx);
        }
    }

} // namespace ImGuiVisualizers