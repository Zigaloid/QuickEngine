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

		void Register(const std::string& className, std::shared_ptr<PropertyWidgetMap> map);
		std::shared_ptr<PropertyWidgetMap> Get(const std::string& className) const;
		void Unregister(const std::string& className);
		std::string FindClassNameForMap(const PropertyWidgetMap* map) const;

	protected:
		void OnLoaded() override;

	private:
		~PropertyWidgetMapRegistry() = default;
		PropertyWidgetMapRegistry(const PropertyWidgetMapRegistry&) = delete;
		PropertyWidgetMapRegistry& operator=(const PropertyWidgetMapRegistry&) = delete;

		mutable std::mutex m_Mutex;

		std::vector<std::shared_ptr<PropertyWidgetMap>> m_ownedMaps;

		std::vector<std::unique_ptr<PropertyWidgetMap>> m_classWidgets;
		std::vector<std::string> m_classNames;
	};

} // namespace ImGuiVisualizers