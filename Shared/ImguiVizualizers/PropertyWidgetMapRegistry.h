#pragma once

#include <string>
#include <memory>
#include <mutex>
#include <vector>
#include "PropertyWidgetMap.h"
#include "Reflection/Reflection.h"

namespace ImGuiVisualizers {

    class PropertyWidgetMapRegistry : public CReflectedBase
    {
    public:        
        REFL_DECLARE_OBJECT(PropertyWidgetMapRegistry, CReflectedBase);
        PropertyWidgetMapRegistry();

        static PropertyWidgetMapRegistry& Instance();

        // Register a widget map for a reflected class name. Ownership is held by the registry.
        void Register(const std::string& className, std::shared_ptr<PropertyWidgetMap> map);

        // Get the widget map for a class name, or nullptr if none registered.
        std::shared_ptr<PropertyWidgetMap> Get(const std::string& className) const;

        // Unregister a widget map.
        void Unregister(const std::string& className);

    private:        
        ~PropertyWidgetMapRegistry() = default;
        PropertyWidgetMapRegistry(const PropertyWidgetMapRegistry&) = delete;
        PropertyWidgetMapRegistry& operator=(const PropertyWidgetMapRegistry&) = delete;

        mutable std::mutex m_Mutex;

        // Ownership storage for registered maps (keeps them alive)
        std::vector<std::shared_ptr<PropertyWidgetMap>> m_ownedMaps;

        // Parallel vectors used for serialization / iteration
        std::vector<std::unique_ptr<PropertyWidgetMap>> m_classWidgets;
        std::vector<std::string> m_classNames; 
    };

} // namespace ImGuiVisualizers