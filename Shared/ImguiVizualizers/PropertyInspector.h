#pragma once

#include "imgui.h"
#include "Reflection/ReflectionBase.h"
#include "Reflection/ReflectionMap.h"
#include "ComponentSystem/ComponentRegistry.h"
#include "PropertyInspector.h"
#include "PropertyWidgetMap.h"
#include "PropertyWidgetMapRegistry.h"

#include <string>
#include <unordered_set>
#include <unordered_map>
#include <memory>

namespace ImGuiVisualizers {

	enum class PropertyDisplayMode {
		FlatList,
		Hierarchy
	};

	class PropertyInspector
	{
	public:
		PropertyInspector();
		~PropertyInspector() = default;

		// Main rendering method (creates its own window)
		bool RenderWindow(const char* windowTitle, bool* isOpen = nullptr);

		// Render only the content (toolbar + properties) without Begin/End.
		// Use this to embed the inspector inside an existing window.
		void RenderContent();

		// Set the object to inspect
		void SetObject(CReflectedBase* object);
		void ClearObject();

		// Configuration
		void SetReadOnly(bool readOnly) { m_ReadOnly = readOnly; }
		void SetShowInternalData(bool show) { m_ShowInternalData = show; }
		void SetExpandByDefault(bool expand) { m_ExpandByDefault = expand; }
		void SetDisplayMode(PropertyDisplayMode mode) { m_DisplayMode = mode; }
		void SetWidgetMap(const PropertyWidgetMap* widgetMap) { m_WidgetMap = widgetMap; }

		// Getters
		bool IsReadOnly() const { return m_ReadOnly; }
		bool ShowInternalData() const { return m_ShowInternalData; }
		bool ExpandByDefault() const { return m_ExpandByDefault; }
		PropertyDisplayMode GetDisplayMode() const { return m_DisplayMode; }
		CReflectedBase* GetObject() const { return m_Object; }
		const PropertyWidgetMap* GetWidgetMap() const { return m_WidgetMap; }

	private:
		// Core rendering methods
		void RenderObjectProperties(CReflectedBase* object, const std::string& objectName = "");
		void RenderObjectPropertiesFlat(CReflectedBase* object);
		void RenderObjectPropertiesHierarchy(CReflectedBase* object);
		void RenderClassProperties(const char* className, std::vector<CReflectionMapEntry>* reflectionMap, CReflectedBase* object);
		void RenderProperty(const CPropertyBase& property, CReflectedBase* object, const std::string& prefix = "");
		void DumpReflectionMap(CReflectedBase* object);

		// Type-specific rendering methods
		void RenderFloatProperty(const CPropertyBase& property, CReflectedBase* object);
		void RenderIntProperty(const CPropertyBase& property, CReflectedBase* object);
		void RenderStringProperty(const CPropertyBase& property, CReflectedBase* object);
		void RenderBoolProperty(const CPropertyBase& property, CReflectedBase* object);
		void RenderVector3Property(const CPropertyBase& property, CReflectedBase* object);
		void RenderVector4Property(const CPropertyBase& property, CReflectedBase* object);
		void RenderMatrix4Property(const CPropertyBase& property, CReflectedBase* object);
		void RenderReflectedObjectCommon( const CPropertyBase& property, CReflectedBase* subObject, const void* idSource, const char* typeTag, bool showAddress, const char* nullNameOverride);
		void RenderObjectProperty(const CPropertyBase& property, CReflectedBase* object);
		void RenderObjectPtrProperty(const CPropertyBase& property, CReflectedBase* object);
		void RenderObjectPtrVectorProperty(const CPropertyBase& property, CReflectedBase* object);
		void RenderComponentCommon(const CPropertyBase& property, ComponentSystem::Component* comp, const void* idSource, const char* typeTag, bool showAddress, const char* nullNameOverride);
		void RenderComponentProperty(const CPropertyBase& property, CReflectedBase* object);
		void RenderComponentPtrProperty(const CPropertyBase& property, CReflectedBase* object);
		void RenderComponentPtrVectorProperty(const CPropertyBase& property, CReflectedBase* object);
		void RenderIntVectorProperty(const CPropertyBase& property, CReflectedBase* object);
		void RenderFloatVectorProperty(const CPropertyBase& property, CReflectedBase* object);
		void RenderBoolVectorProperty(const CPropertyBase& property, CReflectedBase* object);
		void RenderStringVectorProperty(const CPropertyBase& property, CReflectedBase* object);

		template<typename ValueT, typename DrawElemFn>
		void RenderVectorGeneric(const CPropertyBase& property, CReflectedBase* object, const char* vectorTag, DrawElemFn drawElement, std::function<ValueT()> makeDefault = std::function<ValueT()>(), std::function<void(void*)> removeCleanup = std::function<void(void*)>());

		// Widget-specific rendering methods (used by PropertyWidgetMap overrides)
		
		bool RenderWithCustomWidget(const CPropertyBase& property, CReflectedBase* object, EditorWidgetType widgetType);
		void RenderSliderFloat(const CPropertyBase& property, CReflectedBase* object, const WidgetConfig* config);
		void RenderSliderInt(const CPropertyBase& property, CReflectedBase* object, const WidgetConfig* config);
		void RenderDragFloat(const CPropertyBase& property, CReflectedBase* object, const WidgetConfig* config);
		void RenderDragInt(const CPropertyBase& property, CReflectedBase* object, const WidgetConfig* config);
		void RenderColorPicker3(const CPropertyBase& property, CReflectedBase* object);
		void RenderColorPicker4(const CPropertyBase& property, CReflectedBase* object);
		void RenderDropdown(const CPropertyBase& property, CReflectedBase* object, const WidgetConfig* config);
		void RenderTextArea(const CPropertyBase& property, CReflectedBase* object);
		void RenderReadOnlyProperty(const CPropertyBase& property, CReflectedBase* object);
		void RenderFilePicker(const CPropertyBase& property, CReflectedBase* object, const WidgetConfig* config);

		// Context menu for component arrays
		void RenderComponentArrayContextMenu(const CPropertyBase& property, CReflectedBase* object);
		void RenderComponentItemContextMenu(const CPropertyBase& property, CReflectedBase* object, size_t index);
		void RenderObjectArrayContextMenu(const CPropertyBase& property, CReflectedBase* object);
		
		// Component management
		bool AddComponentToArray(const CPropertyBase& property, CReflectedBase* object, const std::string& componentClassName);
		bool RemoveComponentFromArray(const CPropertyBase& property, CReflectedBase* object, size_t index);
		bool AddObjectToArray(const CPropertyBase& property, CReflectedBase* object, const std::string& className);
		bool RemoveObjectFromArray(const CPropertyBase& property, CReflectedBase* object, size_t index);
		void ProcessPendingDeletions();

		// Helper methods
		std::string GenerateTreeNodeId(const std::string& name, const void* address);
		bool ShouldExpandNode(const std::string& nodeId);
		void UpdateExpandedState(const std::string& nodeId, bool expanded);
		const char* GetTypeDisplayName(RFL_Type type);
		ImVec4 GetTypeColor(RFL_Type type);
		void RenderPropertyLabel(const std::string& name, RFL_Type type);
		void RenderNullPointer(const std::string& name);

		// Validation helpers
		bool ValidateFloatInput(float& value, float min = -FLT_MAX, float max = FLT_MAX);
		bool ValidateIntInput(int& value, int min = INT_MIN, int max = INT_MAX);

	private:
		// Current object being inspected
		CReflectedBase* m_Object;

		// Widget map for custom property widgets
		const PropertyWidgetMap* m_WidgetMap;

		// Configuration options
		bool m_ReadOnly;
		bool m_ShowInternalData;
		bool m_ExpandByDefault;
		PropertyDisplayMode m_DisplayMode;

		// UI state
		std::unordered_set<std::string> m_ExpandedNodes;
		bool m_FirstRender;

		// Per-property string edit buffers keyed by property address
		struct StringEditBuffer {
			char data[1024];
			bool isBeingEdited;
		};
		std::unordered_map<void*, StringEditBuffer> m_StringBuffers;

		// Deferred deletion for components (to avoid modifying vector during iteration)
		struct PendingComponentDeletion {
			const CPropertyBase* property;
			CReflectedBase* object;
			size_t index;
		};
		std::vector<PendingComponentDeletion> m_PendingDeletions;

		// Input buffers for editable properties
		float m_FloatBuffer;
		int m_IntBuffer;
		bool m_BoolBuffer;
		bool m_ShowDetails = false;

		struct WidgetMapScope {
			const PropertyWidgetMap* m_prev;
			PropertyInspector* m_owner;
			WidgetMapScope(PropertyInspector* owner, CReflectedBase* obj)
				: m_prev(owner->m_WidgetMap), m_owner(owner)
			{
				if (obj) {
					const char* subClassName = obj->GetRflClassName();
					if (subClassName && subClassName[0] != '\0') {
						auto mapPtr = PropertyWidgetMapRegistry::Instance().Get(subClassName);
						owner->m_WidgetMap = mapPtr ? mapPtr.get() : nullptr;
					}
					else {
						owner->m_WidgetMap = nullptr;
					}
				}
				else {
					owner->m_WidgetMap = nullptr;
				}
			}
			~WidgetMapScope() {
				m_owner->m_WidgetMap = m_prev;
			}
		};
	};

} // namespace ImGuiVisualizers