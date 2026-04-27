#include "PropertyWidgetMap.h"
#include "PropertyWidgetMapRegistry.h"
#include "ClassFactory/ClassFactory.h"
#include "Reflection/ReflectionMap.h"

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
        REFL_DEFINE_STRING_MEMBER(WidgetConfig, fileFilter),
        REFL_DEFINE_STRING_MEMBER(WidgetConfig, defaultFolder)
        REFL_DEFINE_END


        REFL_DEFINE_OBJECT(WidgetEntry)
        REFL_DEFINE_INT_MEMBER(WidgetEntry, type),
        REFL_DEFINE_STRING_MEMBER(WidgetEntry, displayName),
        REFL_DEFINE_BOOL_MEMBER(WidgetEntry, isAdvanced),                
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
            newEntry->displayName = e->displayName;
            newEntry->isAdvanced = e->isAdvanced;
            newEntry->hasConfig = e->hasConfig;
            newEntry->config = e->config; // WidgetConfig is copyable
            copy->m_MemberWidgets.push_back(std::move(newEntry));
        }
        return copy;
    }

    // Walk the reflection hierarchy for `className` and return the first mapping found for `memberName`.
    // IMPORTANT: Access map internals directly to avoid calling public getters that perform hierarchy lookup.
    bool PropertyWidgetMap::FindWidgetInHierarchy(const std::string& className, const std::string& memberName,
        EditorWidgetType& outType, WidgetConfig* outConfig, std::string* outOrigin)
    {
        if (className.empty() || memberName.empty()) return false;

        // Create a temp instance to collect the reflection hierarchy.
        std::unique_ptr<CReflectedBase> inst(ClassFactory::CreateObject(className.c_str()));
        if (!inst) {
            // Failed to construct reflection instance - no hierarchy available
            return false;
        }

        std::vector<std::pair<const char*, std::vector<CReflectionMapEntry>*>> hierarchy;
        inst->CollectHierarchyReflectionMaps(hierarchy);

        // hierarchy is ordered derived -> parent -> ...
        for (const auto& pair : hierarchy) {
            if (!pair.first) continue;
            const std::string originClass(pair.first);
            auto map = PropertyWidgetMapRegistry::Instance().Get(originClass);
            if (!map) continue;

            // Use class-private access to inspect the map directly and avoid recursion.
            int idx = map->FindIndex(memberName); // allowed: same class scope
            if (idx < 0) continue;

            // Found a local mapping on this originClass — read it directly.
            if (outType) { /* nothing - outType will be assigned below */ }
            outType = map->m_MemberWidgets[idx]->type;
            if (outConfig != nullptr) {
                if (map->m_MemberWidgets[idx]->hasConfig) {
                    *outConfig = map->m_MemberWidgets[idx]->config;
                }
                else {
                    // leave outConfig unchanged/empty if no config present
                }
            }
            if (outOrigin) *outOrigin = originClass;
            return true;
        }

        return false;
    }

    // Return either a local widget type or a resolved inherited widget type via the hierarchy.
    EditorWidgetType PropertyWidgetMap::GetWidget(const std::string& memberName) const
    {
        int idx = FindIndex(memberName);
        if (idx >= 0) {
            return m_MemberWidgets[idx]->type;
        }

        // Try to find which class this map belongs to
        std::string ownerClass = PropertyWidgetMapRegistry::Instance().FindClassNameForMap(this);
        if (ownerClass.empty()) {
            return EditorWidgetType::Default;
        }

        EditorWidgetType foundType = EditorWidgetType::Default;
        if (FindWidgetInHierarchy(ownerClass, memberName, foundType, nullptr, nullptr)) {
            return foundType;
        }

        return EditorWidgetType::Default;
    }

    // Return pointer to local config if present; otherwise, if an inherited config is found,
    // copy it into a small mutable cache and return a pointer to that cached copy.
    const WidgetConfig* PropertyWidgetMap::GetConfig(const std::string& memberName) const
    {
        int idx = FindIndex(memberName);
        if (idx >= 0 && m_MemberWidgets[idx]->hasConfig) {
            return &m_MemberWidgets[idx]->config;
        }

        std::string ownerClass = PropertyWidgetMapRegistry::Instance().FindClassNameForMap(this);
        if (ownerClass.empty()) return nullptr;

        WidgetConfig tmpCfg;
        EditorWidgetType tmpType;
        std::string origin;
        if (FindWidgetInHierarchy(ownerClass, memberName, tmpType, &tmpCfg, &origin)) {
            // If the found mapping has a config, cache and return it.
            auto originMap = PropertyWidgetMapRegistry::Instance().Get(origin);
            if (originMap) {
                // Access originMap internals directly to confirm config presence without recursion
                int originIdx = originMap->FindIndex(memberName);
                if (originIdx >= 0 && originMap->m_MemberWidgets[originIdx]->hasConfig) {
                    if (!m_CachedInheritedConfig) m_CachedInheritedConfig = std::make_unique<WidgetConfig>();
                    *m_CachedInheritedConfig = originMap->m_MemberWidgets[originIdx]->config;
                    m_CachedInheritedMember = memberName;
                    return m_CachedInheritedConfig.get();
                }
            }
        }

        return nullptr;
    }

    // Return the locally-configured display name for a member, or empty string if none.
    std::string PropertyWidgetMap::GetEntryDisplayNameLocal(const std::string& memberName) const
    {
        int idx = FindIndex(memberName);
        if (idx >= 0 && m_MemberWidgets[idx]) {
            return m_MemberWidgets[idx]->displayName;
        }
        return std::string();
    }

    std::string PropertyWidgetMap::GetEntryDisplayName(const std::string& memberName) const
    {
        // Prefer a locally-set display name, even if it is empty — an explicit
        // empty string means "no override", so only skip it when the entry is absent.
        int idx = FindIndex(memberName);
        if (idx >= 0 && m_MemberWidgets[idx] && !m_MemberWidgets[idx]->displayName.empty()) {
            return m_MemberWidgets[idx]->displayName;
        }

        std::string ownerClass = PropertyWidgetMapRegistry::Instance().FindClassNameForMap(this);
        if (ownerClass.empty()) return std::string();

        EditorWidgetType tmpType = EditorWidgetType::Default;
        std::string origin;
        if (FindWidgetInHierarchy(ownerClass, memberName, tmpType, nullptr, &origin)) {
            auto originMap = PropertyWidgetMapRegistry::Instance().Get(origin);
            if (originMap) {
                return originMap->GetEntryDisplayNameLocal(memberName);
            }
        }
        return std::string();
    }

    // Return the locally-configured isAdvanced flag for a member, or false if none.
    bool PropertyWidgetMap::GetEntryIsAdvancedLocal(const std::string& memberName) const
    {
        int idx = FindIndex(memberName);
        if (idx >= 0 && m_MemberWidgets[idx]) {
            return m_MemberWidgets[idx]->isAdvanced;
        }
        return false;
    }

    bool PropertyWidgetMap::GetEntryIsAdvanced(const std::string& memberName) const
    {
        // A local false could mean "explicitly not advanced" or "not set".
        // Only treat it as authoritative when the entry itself exists locally.
        int idx = FindIndex(memberName);
        if (idx >= 0 && m_MemberWidgets[idx]) {
            return m_MemberWidgets[idx]->isAdvanced;
        }

        std::string ownerClass = PropertyWidgetMapRegistry::Instance().FindClassNameForMap(this);
        if (ownerClass.empty()) return false;

        EditorWidgetType tmpType = EditorWidgetType::Default;
        std::string origin;
        if (FindWidgetInHierarchy(ownerClass, memberName, tmpType, nullptr, &origin)) {
            auto originMap = PropertyWidgetMapRegistry::Instance().Get(origin);
            if (originMap) {
                return originMap->GetEntryIsAdvancedLocal(memberName);
            }
        }
        return false;
    }

    // New overloads for setting displayName/isAdvanced
    void PropertyWidgetMap::SetWidget(const std::string& memberName, EditorWidgetType widgetType, const std::string& displayName, bool isAdvanced)
    {
        int idx = FindIndex(memberName);
        if (idx >= 0) {
            m_MemberWidgets[idx]->type = widgetType;
            m_MemberWidgets[idx]->hasConfig = false;
            m_MemberWidgets[idx]->displayName = displayName;
            m_MemberWidgets[idx]->isAdvanced = isAdvanced;
            return;
        }
        m_MemberNames.push_back(memberName);
        auto entry = std::make_unique<WidgetEntry>();
        entry->type = widgetType;
        entry->displayName = displayName;
        entry->isAdvanced = isAdvanced;
        m_MemberWidgets.push_back(std::move(entry));
    }

    void PropertyWidgetMap::SetWidget(const std::string& memberName, EditorWidgetType widgetType, const WidgetConfig& config, const std::string& displayName, bool isAdvanced)
    {
        int idx = FindIndex(memberName);
        if (idx >= 0) {
            m_MemberWidgets[idx]->type = widgetType;
            m_MemberWidgets[idx]->config = config;
            m_MemberWidgets[idx]->hasConfig = true;
            m_MemberWidgets[idx]->displayName = displayName;
            m_MemberWidgets[idx]->isAdvanced = isAdvanced;
            return;
        }
        m_MemberNames.push_back(memberName);
        auto entry = std::make_unique<WidgetEntry>();
        entry->type = widgetType;
        entry->config = config;
        entry->hasConfig = true;
        entry->displayName = displayName;
        entry->isAdvanced = isAdvanced;
        m_MemberWidgets.push_back(std::move(entry));
    }

} // namespace ImGuiVisualizers