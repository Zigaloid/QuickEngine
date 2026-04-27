#include "PropertyInspector.h"

#include <algorithm>
#include <sstream>
#include <functional>

//
#include "stringUtils.h"
#include "PropertyWidgetMapRegistry.h"
#include "Math/Vector3f.h"
#include "Math/Vector4f.h"
#include "Math/Matrix4f.h"
#include "CoreSystem/CoreSystem.h"
#include "ClassFactory/ClassFactory.h"
#include "ComponentSystem/ComponentSystem.h"
#include "ComponentSystem/ComponentRegistry.h"
#include "PropertyInspector_Internal.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commdlg.h>
#endif

namespace ImGuiVisualizers
{
	// ═════════════════════════════════════════════════════════════════════════════
	// PropertyInspector
	// ═════════════════════════════════════════════════════════════════════════════

	PropertyInspector::PropertyInspector()
		: m_object(nullptr)
		, m_widgetMap(nullptr)
		, m_readOnly(false)
		, m_showInternalData(true)
		, m_expandByDefault(true)
		, m_displayMode(PropertyDisplayMode::Basic)
		, m_firstRender(true)
		, m_floatBuffer(0.0f)
		, m_intBuffer(0)
		, m_boolBuffer(false)
	{
	}

	bool PropertyInspector::RenderWindow(const char* windowTitle, bool* isOpen)
	{
		if (!ImGui::Begin(windowTitle, isOpen)) {
			ImGui::End();
			return false;
		}

		RenderContent();

		ImGui::End();
		m_firstRender = false;
		return true;
	}

	void PropertyInspector::RenderContent()
	{
		// Toolbar
		if (ImGui::Button("Clear")) {
			ClearObject();
		}
		// Display mode selection
		ImGui::SameLine();
		if (ImGui::Button(m_displayMode == PropertyDisplayMode::Advanced ? "Basic View" : "Advanced View")) {
			m_displayMode = (m_displayMode == PropertyDisplayMode::Advanced) ?
				PropertyDisplayMode::Basic : PropertyDisplayMode::Advanced;
		}

		ImGui::SameLine();
		ImGui::Checkbox("Read Only", &m_readOnly);
		ImGui::SameLine();
		ImGui::Checkbox("Show Internal", &m_showInternalData);
		ImGui::SameLine();
		ImGui::Checkbox("Expand by Default", &m_expandByDefault);


		ImGui::Separator();

		// Content
		if (m_object) {
			RenderObjectProperties(m_object, m_object->GetRflClassName() ? m_object->GetRflClassName() : "Root");
		}
		else {
			ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No object selected");
		}

		// Process any pending component deletions after rendering
		ProcessPendingDeletions();

		m_firstRender = false;
	}

	void PropertyInspector::SetObject(CReflectedBase* object)
	{
		m_object = object;
		m_firstRender = true;

		// Try to get a widget map from the widget map registry.
		m_widgetMap = nullptr;
		if (m_object) {
			const char* className = m_object->GetRflClassName();
			if (className && className[0] != '\0') {
				auto mapPtr = PropertyWidgetMapRegistry::Instance().Get(className);
				if (mapPtr) {
					m_widgetMap = mapPtr.get();
				}
			}
		}

		// Clear expanded state when changing objects
		m_ExpandedNodes.clear();
		m_StringBuffers.clear();
	}

	void PropertyInspector::ClearObject()
	{
		m_object = nullptr;
		m_widgetMap = nullptr;
		m_ExpandedNodes.clear();
		m_StringBuffers.clear();
	}

	void PropertyInspector::RenderObjectProperties(CReflectedBase* object, const std::string& objectName)
	{
		if (!object) {
			RenderNullPointer(objectName);
			return;
		}

		if (m_displayMode == PropertyDisplayMode::Basic) {
			RenderObjectPropertiesFlat(object);
		}
		else {
			RenderObjectPropertiesHierarchy(object);
		}
	}

	void PropertyInspector::RenderObjectPropertiesFlat(CReflectedBase* object)
	{
		// Collect all hierarchy reflection maps
		std::vector<std::pair<const char*, std::vector<CReflectionMapEntry>*>> hierarchyMaps;
		object->CollectHierarchyReflectionMaps(hierarchyMaps);

		if (hierarchyMaps.empty()) {
			ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No reflected properties");
			return;
		}

		// Render all properties in a flat list
		for (const auto& mapPair : hierarchyMaps) {
			const char* className = mapPair.first;
			std::vector<CReflectionMapEntry>* reflectionMap = mapPair.second;

			if (!reflectionMap || reflectionMap->empty()) {
				continue;
			}

			// Render each property
			for (const auto& entry : *reflectionMap) {
				CPropertyBase* property = entry.GetProperty();
				if (property) {
					// Skip internal properties if not showing them
					if (!m_showInternalData && property->GetName().find("m_") == 0) {
						continue;
					}

					RenderProperty(*property, object);
				}
			}
		}
	}

	void PropertyInspector::RenderObjectPropertiesHierarchy(CReflectedBase* object)
	{
		// Collect all hierarchy reflection maps
		std::vector<std::pair<const char*, std::vector<CReflectionMapEntry>*>> hierarchyMaps;
		object->CollectHierarchyReflectionMaps(hierarchyMaps);

		if (hierarchyMaps.empty()) {
			ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No reflected properties");
			return;
		}

		// Render properties grouped by class hierarchy
		for (const auto& mapPair : hierarchyMaps) {
			const char* className = mapPair.first;
			std::vector<CReflectionMapEntry>* reflectionMap = mapPair.second;

			if (!reflectionMap || reflectionMap->empty()) {
				continue;
			}

			RenderClassProperties(className, reflectionMap, object);
		}
	}

	void PropertyInspector::RenderClassProperties(const char* className, std::vector<CReflectionMapEntry>* reflectionMap, CReflectedBase* object)
	{
		if (!className || !reflectionMap || reflectionMap->empty()) {
			return;
		}

		// Generate unique ID for this class section
		const std::string nodeId = GenerateTreeNodeId(className, reflectionMap);
		bool expanded = ShouldExpandNode(nodeId);

		// Count visible properties
		int visiblePropertyCount = 0;
		for (const auto& entry : *reflectionMap) {
			CPropertyBase* property = entry.GetProperty();
			if (property) {
				if (m_showInternalData || property->GetName().find("m_") != 0) {
					visiblePropertyCount++;
				}
			}
		}

		if (visiblePropertyCount == 0) {
			return;
		}

		// Render the class header with property count
		if (ImGui::TreeNodeEx(nodeId.c_str(), expanded ? ImGuiTreeNodeFlags_DefaultOpen : 0,
			"%s (%d properties)", className, visiblePropertyCount)) {
			UpdateExpandedState(nodeId, true);

			// Render each property in this class
			for (const auto& entry : *reflectionMap) {
				CPropertyBase* property = entry.GetProperty();
				if (property) {
					// Skip internal properties if not showing them
					if (!m_showInternalData && property->GetName().find("m_") == 0) {
						continue;
					}

					RenderProperty(*property, object);
				}
			}

			ImGui::TreePop();
		}
		else {
			UpdateExpandedState(nodeId, false);
		}
	}

	void PropertyInspector::RenderProperty(const CPropertyBase& property, CReflectedBase* object, const std::string& prefix)
	{
		const std::string fullName = prefix.empty() ? property.GetName() : prefix + "." + property.GetName();

		// Skip internal properties if not showing them
		if (!m_showInternalData && property.GetName().find("m_") == 0) {
			return;
		}


		// If in Basic display mode, skip properties that are locally marked as "advanced" in the widget map.
		// Uses the local-only query so inherited mappings do not affect this basic/advanced toggle.
		if (m_displayMode == PropertyDisplayMode::Basic && m_widgetMap && m_widgetMap->GetEntryIsAdvanced(property.GetName())) {
			return;
		}

		// Check widget map for a custom widget override
		if (m_widgetMap && m_widgetMap->HasCustomWidget(property.GetName())) {
			EditorWidgetType widgetType = m_widgetMap->GetWidget(property.GetName());
			if (RenderWithCustomWidget(property, object, widgetType)) {
				return;
			}
		}

		switch (property.GetType()) {
		case RT_Float:
			RenderFloatProperty(property, object);
			break;
		case RT_Int:
			RenderIntProperty(property, object);
			break;
		case RT_String:
			RenderStringProperty(property, object);
			break;
		case RT_Bool:
			RenderBoolProperty(property, object);
			break;
		case RT_Vector3:
			RenderVector3Property(property, object);
			break;
		case RT_Vector4:
			RenderVector4Property(property, object);
			break;
		case RT_Matrix4:
			RenderMatrix4Property(property, object);
			break;
		case RT_IntVec:
			RenderIntVectorProperty(property, object);
			break;
		case RT_FloatVec:
			RenderFloatVectorProperty(property, object);
			break;
		case RT_BoolVec:
			RenderBoolVectorProperty(property, object);
			break;
		case RT_StringVec:
			RenderStringVectorProperty(property, object);
			break;
		case RT_Object:
			RenderObjectProperty(property, object);
			break;
		case RT_ObjectPtr:
			RenderObjectPtrProperty(property, object);
			break;
		case RT_ObjectPtrVec:
			RenderObjectPtrVectorProperty(property, object);
			break;
		case RT_Component:
			RenderComponentProperty(property, object);
			break;
		case RT_ComponentPtr:
			RenderComponentPtrProperty(property, object);
			break;
		case RT_ComponentPtrVec:
		case RT_ComponentRawPtrVec:
			RenderComponentPtrVectorProperty(property, object);
			break;
		default:
			ImGui::Text("%s: Unknown type (%d)", property.GetName().c_str(), static_cast<int>(property.GetType()));
			break;
		}
	}
	std::string PropertyInspector::GenerateTreeNodeId(const std::string& name, const void* address)
	{
		std::stringstream ss;
		ss << name << "##" << address;
		return ss.str();
	}

	bool PropertyInspector::ShouldExpandNode(const std::string& nodeId)
	{
		if (m_firstRender && m_expandByDefault) {
			return true;
		}
		return m_ExpandedNodes.find(nodeId) != m_ExpandedNodes.end();
	}

	void PropertyInspector::UpdateExpandedState(const std::string& nodeId, bool expanded)
	{
		if (expanded) {
			m_ExpandedNodes.insert(nodeId);
		}
		else {
			m_ExpandedNodes.erase(nodeId);
		}
	}

	void PropertyInspector::RenderComponentArrayContextMenu(const CPropertyBase& property, CReflectedBase* object)
	{
		// Skip if read-only
		if (m_readOnly) {
			return;
		}

		ImGui::PushID(property.GetName().c_str());

		if (ImGui::BeginPopupContextItem("##ComponentArrayCtx")) {
			const auto& allComponents = GetComponentRegistry().GetAll();

			if (allComponents.empty()) {
				ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "No components registered!");
			}
			else {
				ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Add Component:");
				ImGui::Separator();

				// Group by category if categories are used
				std::unordered_map<std::string, std::vector<const ComponentTypeInfo*>> categorized;
				for (const auto& info : allComponents) {
					categorized[info.category].push_back(&info);
				}

				// Render components grouped by category
				for (const auto& [category, components] : categorized) {
					if (!category.empty()) {
						ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "// %s", category.c_str());
					}

					for (const auto* info : components) {
						if (ImGui::MenuItem(info->displayName.c_str())) {
							AddComponentToArray(property, object, info->className);
						}
					}

					if (!category.empty() && categorized.size() > 1) {
						ImGui::Separator();
					}
				}
			}

			ImGui::EndPopup();
		}

		ImGui::PopID();
	}

	void PropertyInspector::ProcessPendingDeletions()
	{
		if (m_PendingDeletions.empty()) {
			return;
		}

		// Sort deletions by index in descending order to maintain correct indices
		// when deleting multiple items from the same vector
		std::sort(m_PendingDeletions.begin(), m_PendingDeletions.end(),
			[](const PendingComponentDeletion& a, const PendingComponentDeletion& b) {
				// First compare by property address, then by object address, then by index
				if (a.property < b.property) return false;
				if (a.property > b.property) return true;
				if (a.object < b.object) return false;
				if (a.object > b.object) return true;
				return a.index > b.index; // Sort descending by index
			});

		// Process deletions
		for (const auto& deletion : m_PendingDeletions) {
			RemoveComponentFromArray(*deletion.property, deletion.object, deletion.index);
		}

		m_PendingDeletions.clear();
	}

	bool PropertyInspector::RenderWithCustomWidget(const CPropertyBase& property, CReflectedBase* object, EditorWidgetType widgetType)
	{
		const WidgetConfig* config = m_widgetMap ? m_widgetMap->GetConfig(property.GetName()) : nullptr;

		switch (widgetType) {
		case EditorWidgetType::ReadOnly:
			RenderReadOnlyProperty(property, object);
			return true;

		case EditorWidgetType::Slider:
			if (property.GetType() == RT_Float) {
				RenderSliderFloat(property, object, config);
				return true;
			}
			if (property.GetType() == RT_Int) {
				RenderSliderInt(property, object, config);
				return true;
			}
			return false;

		case EditorWidgetType::Drag:
			if (property.GetType() == RT_Float) {
				RenderDragFloat(property, object, config);
				return true;
			}
			if (property.GetType() == RT_Int) {
				RenderDragInt(property, object, config);
				return true;
			}
			return false;

		case EditorWidgetType::ColorPicker:
			if (property.GetType() == RT_Vector3) {
				RenderColorPicker3(property, object);
				return true;
			}
			if (property.GetType() == RT_Vector4) {
				RenderColorPicker4(property, object);
				return true;
			}
			return false;

		case EditorWidgetType::Dropdown:
			if (property.GetType() == RT_String || property.GetType() == RT_Int) {
				RenderDropdown(property, object, config);
				return true;
			}
			return false;

		case EditorWidgetType::TextArea:
			if (property.GetType() == RT_String) {
				RenderTextArea(property, object);
				return true;
			}
			return false;

		case EditorWidgetType::FilePicker:
			if (property.GetType() == RT_String) {
				RenderFilePicker(property, object, config);
				return true;
			}
			return false;
		case EditorWidgetType::InputField:
		case EditorWidgetType::Checkbox:
		case EditorWidgetType::Default:
		default:
			return false;
		}
	}

	void PropertyInspector::RenderSliderFloat(const CPropertyBase& property, CReflectedBase* object, const WidgetConfig* config)
	{
		float* value = reinterpret_cast<float*>(property.GetAddress(object));
		float minVal = config ? config->minValue : 0.0f;
		float maxVal = config ? config->maxValue : 1.0f;

		RenderPropertyLabel(property, object);

		if (m_readOnly) {
			ImGui::Text("%.3f", *value);
		}
		else {
			ImGui::PushID(property.GetName().c_str());
			ImGui::SliderFloat("", value, minVal, maxVal, "%.3f");
			ImGui::PopID();
		}
	}

	void PropertyInspector::RenderSliderInt(const CPropertyBase& property, CReflectedBase* object, const WidgetConfig* config)
	{
		int* value = reinterpret_cast<int*>(property.GetAddress(object));
		int minVal = config ? static_cast<int>(config->minValue) : 0;
		int maxVal = config ? static_cast<int>(config->maxValue) : 100;

		RenderPropertyLabel(property, object);

		if (m_readOnly) {
			ImGui::Text("%d", *value);
		}
		else {
			ImGui::PushID(property.GetName().c_str());
			ImGui::SliderInt("", value, minVal, maxVal);
			ImGui::PopID();
		}
	}

	void PropertyInspector::RenderDragFloat(const CPropertyBase& property, CReflectedBase* object, const WidgetConfig* config)
	{
		float* value = reinterpret_cast<float*>(property.GetAddress(object));
		float speed = config ? config->step : 0.01f;
		float minVal = config ? config->minValue : -FLT_MAX;
		float maxVal = config ? config->maxValue : FLT_MAX;

		RenderPropertyLabel(property, object);

		if (m_readOnly) {
			ImGui::Text("%.3f", *value);
		}
		else {
			ImGui::PushID(property.GetName().c_str());
			ImGui::DragFloat("", value, speed, minVal, maxVal, "%.3f");
			ImGui::PopID();
		}
	}

	void PropertyInspector::RenderDragInt(const CPropertyBase& property, CReflectedBase* object, const WidgetConfig* config)
	{
		int* value = reinterpret_cast<int*>(property.GetAddress(object));
		float speed = config ? config->step : 1.0f;
		int minVal = config ? static_cast<int>(config->minValue) : INT_MIN;
		int maxVal = config ? static_cast<int>(config->maxValue) : INT_MAX;

		RenderPropertyLabel(property, object);

		if (m_readOnly) {
			ImGui::Text("%d", *value);
		}
		else {
			ImGui::PushID(property.GetName().c_str());
			ImGui::DragInt("", value, speed, minVal, maxVal);
			ImGui::PopID();
		}
	}

	void PropertyInspector::RenderColorPicker3(const CPropertyBase& property, CReflectedBase* object)
	{
		Vector3f* value = reinterpret_cast<Vector3f*>(property.GetAddress(object));
		float col[3] = { value->getX(), value->getY(), value->getZ() };

		RenderPropertyLabel(property, object);

		if (m_readOnly) {
			ImGui::ColorButton("##preview", ImVec4(col[0], col[1], col[2], 1.0f));
			ImGui::SameLine();
			ImGui::Text("(%.3f, %.3f, %.3f)", col[0], col[1], col[2]);
		}
		else {
			ImGui::PushID(property.GetName().c_str());
			if (ImGui::ColorEdit3("", col)) {
				value->setX(col[0]);
				value->setY(col[1]);
				value->setZ(col[2]);
			}
			ImGui::PopID();
		}
	}

	void PropertyInspector::RenderColorPicker4(const CPropertyBase& property, CReflectedBase* object)
	{
		Vector4f* value = reinterpret_cast<Vector4f*>(property.GetAddress(object));
		float col[4] = { value->getX(), value->getY(), value->getZ(), value->getW() };

		RenderPropertyLabel(property, object);

		if (m_readOnly) {
			ImGui::ColorButton("##preview", ImVec4(col[0], col[1], col[2], col[3]));
			ImGui::SameLine();
			ImGui::Text("(%.3f, %.3f, %.3f, %.3f)", col[0], col[1], col[2], col[3]);
		}
		else {
			ImGui::PushID(property.GetName().c_str());
			if (ImGui::ColorEdit4("", col)) {
				value->SetX(col[0]);
				value->SetY(col[1]);
				value->SetZ(col[2]);
				value->SetW(col[3]);
			}
			ImGui::PopID();
		}
	}

	void PropertyInspector::RenderDropdown(const CPropertyBase& property, CReflectedBase* object, const WidgetConfig* config)
	{
		RenderPropertyLabel(property, object);

		if (!config || config->dropdownOptions.empty()) {
			ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "(no dropdown options configured)");
			return;
		}

		ImGui::PushID(property.GetName().c_str());

		if (property.GetType() == RT_String) {
			std::string* value = reinterpret_cast<std::string*>(property.GetAddress(object));

			if (m_readOnly) {
				ImGui::Text("%s", value->c_str());
			}
			else {
				SetNextItemWidthToContentRegionAvail();
				if (ImGui::BeginCombo("", value->c_str())) {
					for (int i = 0; i < static_cast<int>(config->dropdownOptions.size()); ++i) {
						bool isSelected = (config->dropdownOptions[i] == *value);
						if (ImGui::Selectable(config->dropdownOptions[i].c_str(), isSelected)) {
							*value = config->dropdownOptions[i];
						}
						if (isSelected) {
							ImGui::SetItemDefaultFocus();
						}
					}
					ImGui::EndCombo();
				}
			}
		}
		else if (property.GetType() == RT_Int) {
			int* value = reinterpret_cast<int*>(property.GetAddress(object));
			int currentIndex = *value;
			if (currentIndex < 0 || currentIndex >= static_cast<int>(config->dropdownOptions.size())) {
				currentIndex = 0;
			}

			if (m_readOnly) {
				ImGui::Text("%s", config->dropdownOptions[currentIndex].c_str());
			}
			else {
				const char* previewValue = config->dropdownOptions[currentIndex].c_str();
				SetNextItemWidthToContentRegionAvail();
				if (ImGui::BeginCombo("", previewValue)) {
					for (int i = 0; i < static_cast<int>(config->dropdownOptions.size()); ++i) {
						bool isSelected = (i == *value);
						if (ImGui::Selectable(config->dropdownOptions[i].c_str(), isSelected)) {
							*value = i;
						}
						if (isSelected) {
							ImGui::SetItemDefaultFocus();
						}
					}
					ImGui::EndCombo();
				}
			}
		}

		ImGui::PopID();
	}

	void PropertyInspector::RenderTextArea(const CPropertyBase& property, CReflectedBase* object)
	{
		std::string* value = reinterpret_cast<std::string*>(property.GetAddress(object));

		RenderPropertyLabel(property, object);

		if (m_readOnly) {
			ImGui::TextWrapped("%s", value->c_str());
		}
		else {
			void* propAddr = property.GetAddress(object);
			StringEditBuffer& buf = m_StringBuffers[propAddr];

			if (!buf.m_isBeingEdited) {
				strncpy_s(buf.data, value->c_str(), sizeof(buf.data) - 1);
				buf.data[sizeof(buf.data) - 1] = '\0';
			}

			ImGui::PushID(property.GetName().c_str());
			if (ImGui::InputTextMultiline("", buf.data, sizeof(buf.data), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 6))) {
				*value = buf.data;
			}
			buf.m_isBeingEdited = ImGui::IsItemActive();
			ImGui::PopID();
		}
	}

	void PropertyInspector::RenderReadOnlyProperty(const CPropertyBase& property, CReflectedBase* object)
	{
		RenderPropertyLabel(property, object);

		switch (property.GetType()) {
		case RT_Float: {
			float* value = reinterpret_cast<float*>(property.GetAddress(object));
			ImGui::Text("%.3f", *value);
			break;
		}
		case RT_Int: {
			int* value = reinterpret_cast<int*>(property.GetAddress(object));
			ImGui::Text("%d", *value);
			break;
		}
		case RT_String: {
			std::string* value = reinterpret_cast<std::string*>(property.GetAddress(object));
			ImGui::Text("\"%s\"", value->c_str());
			break;
		}
		case RT_Bool: {
			bool* value = reinterpret_cast<bool*>(property.GetAddress(object));
			ImGui::Text("%s", *value ? "true" : "false");
			break;
		}
		case RT_Vector3: {
			Vector3f* value = reinterpret_cast<Vector3f*>(property.GetAddress(object));
			ImGui::Text("(%.3f, %.3f, %.3f)", value->getX(), value->getY(), value->getZ());
			break;
		}
		case RT_Vector4: {
			Vector4f* value = reinterpret_cast<Vector4f*>(property.GetAddress(object));
			ImGui::Text("(%.3f, %.3f, %.3f, %.3f)", value->getX(), value->getY(), value->getZ(), value->getW());
			break;
		}
		default:
			ImGui::Text("(read-only)");
			break;
		}
	}
	void PropertyInspector::RenderObjectArrayContextMenu(const CPropertyBase& property, CReflectedBase* object)
	{
		// Skip if read-only
		if (m_readOnly) {
			return;
		}

		ImGui::PushID((property.GetName() + "##ObjectArrayCtx").c_str());

		if (ImGui::BeginPopupContextItem("##ObjectArrayCtx")) {
			auto classNames = ClassFactory::GetRegisteredClassNames();

			if (classNames.empty()) {
				ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "No classes registered!");
			}
			else {
				ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Create Object:");
				ImGui::Separator();

				for (const auto& cname : classNames) {
					if (ImGui::MenuItem(cname.c_str())) {
						AddObjectToArray(property, object, cname);
					}
				}
			}

			ImGui::EndPopup();
		}

		ImGui::PopID();
	}

	bool PropertyInspector::AddObjectToArray(const CPropertyBase& property, CReflectedBase* object, const std::string& className)
	{
		auto objectVector = reinterpret_cast<std::vector<std::unique_ptr<CReflectedBase>>*>(property.GetAddress(object));
		if (!objectVector)
			return false;

		CReflectedBase* newObj = ClassFactory::CreateObject(className.c_str());
		if (!newObj)
			return false;

		objectVector->push_back(std::unique_ptr<CReflectedBase>(newObj));
		return true;
	}

	bool PropertyInspector::RemoveObjectFromArray(const CPropertyBase& property, CReflectedBase* object, size_t index)
	{
		auto objectVector = reinterpret_cast<std::vector<std::unique_ptr<CReflectedBase>>*>(property.GetAddress(object));
		if (!objectVector) return false;
		if (index >= objectVector->size()) return false;

		objectVector->erase(objectVector->begin() + index);
		return true;
	}

	// Implement component add/remove and item-context helpers

	void PropertyInspector::RenderComponentItemContextMenu(const CPropertyBase& property, CReflectedBase* object, size_t index)
	{
		// Skip if read-only
		if (m_readOnly) {
			return;
		}

		ImGui::PushID(static_cast<int>(index));

		if (ImGui::BeginPopupContextItem("##ComponentItemCtx")) {
			if (ImGui::MenuItem("Delete Component")) {
				// Defer deletion to avoid modifying vector during iteration
				m_PendingDeletions.push_back({ &property, object, index });
			}
			ImGui::EndPopup();
		}

		ImGui::PopID();
	}

	bool PropertyInspector::AddComponentToArray(const CPropertyBase& property, CReflectedBase* object, const std::string& componentClassName)
	{
		std::vector<ComponentSystem::Component*>* componentVector =
			reinterpret_cast<std::vector<ComponentSystem::Component*>*>(property.GetAddress(object));

		if (!componentVector) {
			return false;
		}

		// Create the component using ClassFactory
		CReflectedBase* newObject = ClassFactory::CreateObject(componentClassName.c_str());
		if (!newObject) {
			return false;
		}

		// Ensure it's actually a Component
		ComponentSystem::Component* newComponent = dynamic_cast<ComponentSystem::Component*>(newObject);
		if (!newComponent) {
			delete newObject;
			return false;
		}

		// Add to the vector
		componentVector->push_back(newComponent);

		return true;
	}

	bool PropertyInspector::RemoveComponentFromArray(const CPropertyBase& property, CReflectedBase* object, size_t index)
	{
		std::vector<ComponentSystem::Component*>* componentVector =
			reinterpret_cast<std::vector<ComponentSystem::Component*>*>(property.GetAddress(object));

		// Validate index
		if (!componentVector || index >= componentVector->size()) {
			return false;
		}

		// Get the component to delete
		ComponentSystem::Component* componentToDelete = (*componentVector)[index];

		// Remove from vector
		componentVector->erase(componentVector->begin() + index);

		// Delete the component object
		if (componentToDelete) {
			delete componentToDelete;
		}

		return true;
	}

	void PropertyInspector::RenderFilePicker(const CPropertyBase& property, CReflectedBase* object, const WidgetConfig* config)
	{
		std::string* value = reinterpret_cast<std::string*>(property.GetAddress(object));

		RenderPropertyLabel(property, object);

		if (m_readOnly) {
			ImGui::Text("%s", value->c_str());
			return;
		}

		// Use per-property edit buffer (same approach as other string widgets)
		void* propAddr = property.GetAddress(object);
		StringEditBuffer& buf = m_StringBuffers[propAddr];

		if (!buf.m_isBeingEdited) {
			strncpy_s(buf.data, value->c_str(), sizeof(buf.data) - 1);
			buf.data[sizeof(buf.data) - 1] = '\0';
		}

		ImGui::PushID(property.GetName().c_str());

		// Text input
		SetNextItemWidthToContentRegionAvail();

		if (ImGui::InputText("", buf.data, sizeof(buf.data))) {
			*value = buf.data;
		}

		ImGui::SameLine();

		// Browse button - opens native dialog on Windows
		if (ImGui::Button("...")) {
#ifdef _WIN32
			// Build filter buffer. WidgetConfig::fileFilter is expected as semicolon-separated patterns like "*.png;*.jpg"
			std::vector<char> filterBuf;
			auto appendNullTerm = [&](const std::string& s) {
				filterBuf.insert(filterBuf.end(), s.begin(), s.end());
				filterBuf.push_back('\0');
				};

			if (config && !config->fileFilter.empty()) {
				appendNullTerm("Files");
				appendNullTerm(config->fileFilter);
				appendNullTerm("All Files");
				appendNullTerm("*.*");
				filterBuf.push_back('\0'); // final double-null terminator
			}
			else {
				const char def[] = "All Files\0*.*\0\0";
				filterBuf.assign(def, def + sizeof(def));
			}

			char filePath[MAX_PATH] = { 0 };
			OPENFILENAMEA ofn = {};
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = nullptr; // no owner
			ofn.lpstrFile = filePath;
			ofn.nMaxFile = MAX_PATH;
			ofn.lpstrFilter = filterBuf.data();
			ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

			// Build initial directory: <current working dir>\assets\ + optional WidgetConfig.defaultFolder
			char initialDirBuf[MAX_PATH] = { 0 };
			DWORD got = GetCurrentDirectoryA(MAX_PATH, initialDirBuf);
			std::string initDir;
			if (got != 0) {
				initDir = std::string(initialDirBuf);
			}
			else {
				initDir = std::string("."); // fallback
			}

			// Normalize trailing slash and append assets
			if (!initDir.empty() && (initDir.back() == '\\' || initDir.back() == '/')) {
				initDir.pop_back();
			}
			initDir += "\\assets\\";

			// Append widget-config default folder if provided (strip any leading slashes)
			if (config && !config->defaultFolder.empty()) {
				std::string df = config->defaultFolder;
				while (!df.empty() && (df.front() == '\\' || df.front() == '/')) df.erase(df.begin());
				// Don't add an extra backslash if defaultFolder already contains it at end
				initDir += df;
			}

			// Copy into buffer for OPENFILENAME
			strcpy_s(initialDirBuf, sizeof(initialDirBuf), initDir.c_str());
			ofn.lpstrInitialDir = initialDirBuf;

			if (GetOpenFileNameA(&ofn))
			{
				// Strip the path to be relative to the project directory.
				std::string  fixedFilePath = filePath;
				pathSanitize(fixedFilePath);
				fixedFilePath.erase(0, fixedFilePath.find("assets/"));
				fixedFilePath = "./" + fixedFilePath; // ensure it starts with ./			

				*value = fixedFilePath;
				strncpy_s(buf.data, value->c_str(), sizeof(buf.data) - 1);
				buf.data[sizeof(buf.data) - 1] = '\0';
			}
#else
			// Non-Windows: no native dialog implemented here. You can hook into the project's AssetBrowser or implement a platform file dialog.
			// For now the user can paste a path into the input box.
#endif
		}

		// Track whether the text input is active
		buf.m_isBeingEdited = ImGui::IsItemActive();

		ImGui::PopID();
	}

}
