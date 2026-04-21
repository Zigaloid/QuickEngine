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

	// Colors for different types
	static const ImVec4 COLOR_TYPE_PRIMITIVE = ImVec4(0.7f, 0.9f, 0.7f, 1.0f);
	static const ImVec4 COLOR_TYPE_OBJECT = ImVec4(0.7f, 0.7f, 0.9f, 1.0f);
	static const ImVec4 COLOR_TYPE_POINTER = ImVec4(0.9f, 0.7f, 0.7f, 1.0f);
	static const ImVec4 COLOR_TYPE_VECTOR = ImVec4(0.9f, 0.9f, 0.7f, 1.0f);
	static const ImVec4 COLOR_TYPE_COMPONENT = ImVec4(0.9f, 0.7f, 0.9f, 1.0f);

	// helper that sets next item width to the available content region on the current line
	static void SetNextItemWidthToContentRegionAvail( float scaler = 0.75f)
	{
		float avail = ImGui::GetContentRegionAvail().x;
		if (avail <= 0.0f) {
			// fallback: compute from window width and cursor X
			float winWidth = ImGui::GetWindowWidth();
			float cursorX = ImGui::GetCursorPosX();
			const ImGuiStyle& style = ImGui::GetStyle();
			avail = winWidth - cursorX - style.ItemSpacing.x * 2.0f;			
			if (avail < 0.0f) avail = 0.0f;
		}
		avail = avail * scaler;
		ImGui::SetNextItemWidth(avail);
	}

	// helper to render + / - buttons for vectors (centralized to reduce duplication)
	static void RenderVecAddRemoveButtons(const std::string& idBase, bool readOnly, const std::function<void()>& onAdd, const std::function<void()>& onRemove)
	{
		ImGui::SameLine();
		ImGui::PushID(idBase.c_str());
		if (!readOnly) {
			if (onAdd) {
				if (ImGui::SmallButton(" + ")) {
					onAdd();
				}
			}
			ImGui::SameLine();
			if (onRemove) {
				if (ImGui::SmallButton(" - ")) {
					onRemove();
				}
			}
		}
		ImGui::PopID();
	}

	PropertyInspector::PropertyInspector()
		: m_object(nullptr)
		, m_widgetMap(nullptr)
		, m_readOnly(false)
		, m_showInternalData(true)
		, m_expandByDefault(true)
		, m_displayMode(PropertyDisplayMode::Hierarchy)
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
		ImGui::SameLine();
		ImGui::Checkbox("Read Only", &m_readOnly);
		ImGui::SameLine();
		ImGui::Checkbox("Show Internal", &m_showInternalData);
		ImGui::SameLine();
		ImGui::Checkbox("Expand by Default", &m_expandByDefault);

		// Display mode selection
		ImGui::SameLine();
		if (ImGui::Button(m_displayMode == PropertyDisplayMode::Hierarchy ? "Hierarchy" : "Flat List")) {
			m_displayMode = (m_displayMode == PropertyDisplayMode::Hierarchy) ?
				PropertyDisplayMode::FlatList : PropertyDisplayMode::Hierarchy;
		}

		ImGui::Separator();

		// Content
		if (m_object) {
			ImGui::Text("Object: %s", m_object->GetRflClassName() ? m_object->GetRflClassName() : "Unknown");
			ImGui::Text("Address: %p", m_object);
			ImGui::Separator();

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

		if (m_displayMode == PropertyDisplayMode::FlatList) {
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

			// Add a subtle header for the class name in flat mode
			if (className) {
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 0.7f));
				ImGui::Text("// %s", className);
				ImGui::PopStyleColor();
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

	void PropertyInspector::RenderFloatProperty(const CPropertyBase& property, CReflectedBase* object)
	{
    float* value = reinterpret_cast<float*>(property.GetAddress(object));

    RenderPropertyLabel(property.GetName(), property.GetType());

    if (m_readOnly) {
        ImGui::Text("%.3f", *value);
    }
    else {
        ImGui::PushID(property.GetName().c_str());
        SetNextItemWidthToContentRegionAvail();
        if (ImGui::InputFloat("", value, 0.0f, 0.0f, "%.3f")) {
            // changed
        }
        ImGui::PopID();
    }
}

	void PropertyInspector::RenderIntProperty(const CPropertyBase& property, CReflectedBase* object)
	{
		int* value = reinterpret_cast<int*>(property.GetAddress(object));

		RenderPropertyLabel(property.GetName(), property.GetType());

		if (m_readOnly) {
			ImGui::Text("%d", *value);
		}
		else {
			ImGui::PushID(property.GetName().c_str());
			ImGui::InputInt("", value);
			ImGui::PopID();
		}
	}

	void PropertyInspector::RenderStringProperty(const CPropertyBase& property, CReflectedBase* object)
	{
		std::string* value = reinterpret_cast<std::string*>(property.GetAddress(object));

		RenderPropertyLabel(property.GetName(), property.GetType());

		if (m_readOnly) {
			ImGui::Text("\"%s\"", value->c_str());
		}
		else {
			void* propAddr = property.GetAddress(object);
			StringEditBuffer& buf = m_StringBuffers[propAddr];

			if (!buf.m_isBeingEdited) {
				strncpy_s(buf.data, value->c_str(), sizeof(buf.data) - 1);
				buf.data[sizeof(buf.data) - 1] = '\0';
			}

			ImGui::PushID(property.GetName().c_str());
			SetNextItemWidthToContentRegionAvail();
			if (ImGui::InputText("", buf.data, sizeof(buf.data))) {
				*value = buf.data;
			}
			buf.m_isBeingEdited = ImGui::IsItemActive();
			ImGui::PopID();
		}
	}

	void PropertyInspector::RenderBoolProperty(const CPropertyBase& property, CReflectedBase* object)
	{
		bool* value = reinterpret_cast<bool*>(property.GetAddress(object));

		RenderPropertyLabel(property.GetName(), property.GetType());

		if (m_readOnly) {
			ImGui::Text("%s", *value ? "true" : "false");
		}
		else {
			ImGui::PushID(property.GetName().c_str());
			ImGui::Checkbox("", value);
			ImGui::PopID();
		}
	}

	void PropertyInspector::RenderVector3Property(const CPropertyBase& property, CReflectedBase* object)
	{
		Vector3f* value = reinterpret_cast<Vector3f*>(property.GetAddress(object));

		RenderPropertyLabel(property.GetName(), property.GetType());

		if (m_readOnly) {
			ImGui::Text("(%.3f, %.3f, %.3f)", value->getX(), value->getY(), value->getZ());
		}
		else {
			float vec3[3] = { value->getX(), value->getY(), value->getZ() };
			ImGui::PushID(property.GetName().c_str());
			SetNextItemWidthToContentRegionAvail();
			if (ImGui::InputFloat3("", vec3)) {
				value->SetX(vec3[0]);
				value->SetY(vec3[1]);
				value->SetZ(vec3[2]);
			}
			ImGui::PopID();
		}
	}

	void PropertyInspector::RenderVector4Property(const CPropertyBase& property, CReflectedBase* object)
	{
		Vector4f* value = reinterpret_cast<Vector4f*>(property.GetAddress(object));

		RenderPropertyLabel(property.GetName(), property.GetType());

		if (m_readOnly) {
			ImGui::Text("(%.3f, %.3f, %.3f, %.3f)", value->getX(), value->getY(), value->getZ(), value->getW());
		}
		else {
			float vec4[4] = { value->getX(), value->getY(), value->getZ(), value->getW() };
			ImGui::PushID(property.GetName().c_str());
			if (ImGui::InputFloat4("", vec4)) {
				value->SetX(vec4[0]);
				value->SetY(vec4[1]);
				value->SetZ(vec4[2]);
				value->SetW(vec4[3]);
			}
			ImGui::PopID();
		}
	}

	void PropertyInspector::RenderMatrix4Property(const CPropertyBase& property, CReflectedBase* object)
	{
		Matrix4f* value = reinterpret_cast<Matrix4f*>(property.GetAddress(object));

		const std::string nodeId = GenerateTreeNodeId(property.GetName(), value);
		bool expanded = ShouldExpandNode(nodeId);

		if (ImGui::TreeNodeEx(nodeId.c_str(), expanded ? ImGuiTreeNodeFlags_DefaultOpen : 0, "%s", property.GetName().c_str())) {
			UpdateExpandedState(nodeId, true);

			ImGui::SameLine();
			ImGui::TextColored(GetTypeColor(property.GetType()), "[Matrix4]");

			if (m_readOnly) {
				// Display matrix in a readable format
				for (int row = 0; row < 4; ++row) {
					ImGui::Text("[%.3f, %.3f, %.3f, %.3f]",
						(*value)(row, 0), (*value)(row, 1),
						(*value)(row, 2), (*value)(row, 3));
				}
			}
			else {
				// Edit matrix elements
				for (int row = 0; row < 4; ++row) {
					ImGui::PushID(row);
					float rowData[4] = {
						(*value)(row, 0), (*value)(row, 1),
						(*value)(row, 2), (*value)(row, 3)
					};
					if (ImGui::InputFloat4("", rowData)) {
						for (int col = 0; col < 4; ++col) {
							(*value)(row, col) = rowData[col];
						}
					}
					ImGui::PopID();
				}
			}

			ImGui::TreePop();
		}
		else {
			UpdateExpandedState(nodeId, false);
			ImGui::SameLine();
			ImGui::TextColored(GetTypeColor(property.GetType()), "[Matrix4]");
		}
	}
	void PropertyInspector::RenderReflectedObjectCommon(
		const CPropertyBase& property,
		CReflectedBase* subObject,
		const void* idSource,
		const char* typeTag,
		bool showAddress,
		const char* nullNameOverride)
	{
		const std::string nodeId = GenerateTreeNodeId(property.GetName(), idSource);
		bool expanded = ShouldExpandNode(nodeId);

		if (ImGui::TreeNodeEx(nodeId.c_str(), expanded ? ImGuiTreeNodeFlags_DefaultOpen : 0, "%s", property.GetName().c_str())) {
			UpdateExpandedState(nodeId, true);

			ImGui::SameLine();
			ImGui::TextColored(GetTypeColor(property.GetType()), "%s", typeTag);

			if (!subObject) {
				RenderNullPointer(nullNameOverride ? nullNameOverride : property.GetName().c_str());
			}
			else {
				// Set widget map for the duration of this scope (restored automatically)
				WidgetMapScope wms(this, subObject);

				if (m_showDetails)
					ImGui::Text("Type: %s", subObject->GetRflClassName() ? subObject->GetRflClassName() : "Unknown");
				if (showAddress)
					ImGui::Text("Address: %p", subObject);

				// Render child properties
				RenderObjectProperties(subObject, property.GetName());
			}

			ImGui::TreePop();
		}
		else {
			UpdateExpandedState(nodeId, false);
			ImGui::SameLine();
			ImGui::TextColored(GetTypeColor(property.GetType()), "%s", typeTag);
			if (!subObject) {
				ImGui::SameLine();
				ImGui::TextColored(ImVec4(0.7f, 0.3f, 0.3f, 1.0f), "(null)");
			}
		}
	}

	void PropertyInspector::RenderObjectProperty(const CPropertyBase& property, CReflectedBase* object)
	{
		// Compute address and collect diagnostics
		void* addr = property.GetAddress(object);
		uintptr_t memberAddr = reinterpret_cast<uintptr_t>(addr);

		// Basic sanity checks: avoid obvious invalid addresses.
		if (addr == nullptr) {
			RenderNullPointer("Embedded object");
			return;
		}
		// Reject tiny addresses (kernel / null-zone) and extremely large addresses (very likely bogus)
		const uintptr_t kLow = 0x10000u;                           // below this is almost always invalid for user memory
		const uintptr_t kHigh = (uintptr_t)0x00007fffffffffffULL;  // rough user-space high bound on x64
		if (memberAddr < kLow || memberAddr > kHigh) {
			RenderNullPointer("Embedded object (invalid address)");
			return;
		}

		CReflectedBase* subObject = reinterpret_cast<CReflectedBase*>(addr);
		RenderReflectedObjectCommon(property, subObject, subObject, "[Object]", true, "Embedded object");
	}

	void PropertyInspector::RenderObjectPtrProperty(const CPropertyBase& property, CReflectedBase* object)
	{
		std::unique_ptr<CReflectedBase>* objectPtr =
			reinterpret_cast<std::unique_ptr<CReflectedBase>*>(property.GetAddress(object));

		CReflectedBase* pointed = objectPtr ? objectPtr->get() : nullptr;
		RenderReflectedObjectCommon(property, pointed, pointed ? pointed : static_cast<const void*>(objectPtr), "[Object*]", true, "Object pointer");
	}

	void PropertyInspector::RenderObjectPtrVectorProperty(const CPropertyBase& property, CReflectedBase* object)
	{
		std::vector<std::unique_ptr<CReflectedBase>>* objectVector =
			reinterpret_cast<std::vector<std::unique_ptr<CReflectedBase>>*>(property.GetAddress(object));

		const std::string nodeId = GenerateTreeNodeId(property.GetName(), objectVector);
		bool expanded = ShouldExpandNode(nodeId);

		// persistent selection per-property
		static std::unordered_map<std::string, int> s_objectSelection;

		if (ImGui::TreeNodeEx(nodeId.c_str(), expanded ? ImGuiTreeNodeFlags_DefaultOpen : 0, "%s", property.GetName().c_str())) {
			UpdateExpandedState(nodeId, true);

			// Context menu for creating objects (right-click on header)
			RenderObjectArrayContextMenu(property, object);

			ImGui::SameLine();
			ImGui::TextColored(GetTypeColor(property.GetType()), "[Vector<Object*>]");

			// Add / Remove controls (inline)
			{
				auto classNames = ClassFactory::GetRegisteredClassNames();
				int& selIdx = s_objectSelection[property.GetName()];
				if (selIdx < 0) selIdx = 0;

				if (classNames.empty()) {
					ImGui::SameLine();
					ImGui::PushID((property.GetName() + "##vec_buttons").c_str());
					if (!m_readOnly) {
						ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "(no classes)");
					}
					ImGui::PopID();
				}
				else 
				{
					RenderVecAddRemoveButtons(property.GetName() + "##vec_buttons", m_readOnly,
						[&]() {
							auto classNamesAdd = ClassFactory::GetRegisteredClassNames();
							if (!classNamesAdd.empty()) {
								int indexToAdd = std::clamp(selIdx, 0, static_cast<int>(classNamesAdd.size()) - 1);
								AddObjectToArray(property, object, classNamesAdd[indexToAdd]);
							}
						},
						[&]() {
							if (!objectVector->empty()) {
								RemoveObjectFromArray(property, object, objectVector->size() - 1);
							}
						});
					ImGui::SameLine();
					const char* preview = classNames[std::min(selIdx, static_cast<int>(classNames.size() - 1))].c_str();
					std::string comboId = property.GetName() + "##class_combo";
					SetNextItemWidthToContentRegionAvail();
					if (ImGui::BeginCombo(comboId.c_str(), preview)) {
						for (int i = 0; i < static_cast<int>(classNames.size()); ++i) {
							bool isSelected = (i == selIdx);
							if (ImGui::Selectable(classNames[i].c_str(), isSelected)) {
								selIdx = i;
							}
							if (isSelected) ImGui::SetItemDefaultFocus();
						}
						ImGui::EndCombo();
					}
				}
			}

			ImGui::Text("Size: %zu", objectVector->size());

			for (size_t i = 0; i < objectVector->size(); ++i) {
				const std::string elementName = "[" + std::to_string(i) + "]";
				const std::string elementNodeId = GenerateTreeNodeId(elementName, (*objectVector)[i].get());
				bool elementExpanded = ShouldExpandNode(elementNodeId);

				if (ImGui::TreeNodeEx(elementNodeId.c_str(), elementExpanded ? ImGuiTreeNodeFlags_DefaultOpen : 0, "%s", elementName.c_str())) {
					UpdateExpandedState(elementNodeId, true);

					if ((*objectVector)[i]) {
						// set widget map to the element's class map while rendering (RAII)
						WidgetMapScope wms(this, (*objectVector)[i].get());

						ImGui::Text("Type: %s", (*objectVector)[i]->GetRflClassName() ? (*objectVector)[i]->GetRflClassName() : "Unknown");
						ImGui::Text("Address: %p", (*objectVector)[i].get());
						RenderObjectProperties((*objectVector)[i].get(), elementName);
					}
					else {
						RenderNullPointer("Vector element");
					}

					ImGui::TreePop();
				}
				else {
					UpdateExpandedState(elementNodeId, false);
					if (!(*objectVector)[i]) {
						ImGui::SameLine();
						ImGui::TextColored(ImVec4(0.7f, 0.3f, 0.3f, 1.0f), "(null)");
					}
				}
			}

			ImGui::TreePop();
		}
		else {
			UpdateExpandedState(nodeId, false);
			ImGui::SameLine();
			ImGui::TextColored(GetTypeColor(property.GetType()), "[Vector<Object*>] (%zu)", objectVector->size());

			// Allow quick add/remove when collapsed
			{
				auto classNames = ClassFactory::GetRegisteredClassNames();
				int& selIdx = s_objectSelection[property.GetName()];
				if (selIdx < 0) selIdx = 0;

				if (classNames.empty()) {
					ImGui::SameLine();
					ImGui::PushID((property.GetName() + "##vec_buttons_collapsed").c_str());
					if (!m_readOnly) {
						ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "(no classes)");
					}
					ImGui::PopID();
				}
				else {
					RenderVecAddRemoveButtons(property.GetName() + "##vec_buttons_collapsed", m_readOnly,
						[&]() {
							auto classNamesAdd = ClassFactory::GetRegisteredClassNames();
							if (!classNamesAdd.empty()) {
								int indexToAdd = std::clamp(selIdx, 0, static_cast<int>(classNamesAdd.size()) - 1);
								AddObjectToArray(property, object, classNamesAdd[indexToAdd]);
							}
						},
						[&]() {
							if (!objectVector->empty()) {
								RemoveObjectFromArray(property, object, objectVector->size() - 1);
							}
						});
					ImGui::SameLine();

					const char* preview = classNames[std::min(selIdx, static_cast<int>(classNames.size() - 1))].c_str();
					std::string comboId = property.GetName() + "##class_combo_collapsed";
					SetNextItemWidthToContentRegionAvail();
					if (ImGui::BeginCombo(comboId.c_str(), preview)) {
						for (int i = 0; i < static_cast<int>(classNames.size()); ++i) {
							bool isSelected = (i == selIdx);
							if (ImGui::Selectable(classNames[i].c_str(), isSelected)) {
								selIdx = i;
							}
							if (isSelected) ImGui::SetItemDefaultFocus();
						}
						ImGui::EndCombo();
					}
				}
			}
		}
	}
	void PropertyInspector::RenderComponentCommon(
		const CPropertyBase& property,
		ComponentSystem::Component* comp,
		const void* idSource,
		const char* typeTag,
		bool showAddress,
		const char* nullNameOverride)
	{
		const std::string nodeId = GenerateTreeNodeId(property.GetName(), idSource);
		bool expanded = ShouldExpandNode(nodeId);

		if (ImGui::TreeNodeEx(nodeId.c_str(), expanded ? ImGuiTreeNodeFlags_DefaultOpen : 0, "%s", property.GetName().c_str())) {
			UpdateExpandedState(nodeId, true);

			ImGui::SameLine();
			ImGui::TextColored(GetTypeColor(property.GetType()), "%s", typeTag);

			if (!comp) {
				RenderNullPointer(nullNameOverride ? nullNameOverride : property.GetName().c_str());
			}
			else {
				// Temporarily switch widget map to the component's class while rendering (RAII)
				WidgetMapScope wms(this, comp);

				ImGui::Text("Type: %s", comp->GetRflClassName() ? comp->GetRflClassName() : "Unknown");
				ImGui::Text("ID: %u", comp->GetId());
				ImGui::Text("Active: %s", comp->IsActive() ? "true" : "false");
				ImGui::Text("Initialized: %s", comp->IsInitialized() ? "true" : "false");
				if (showAddress) {
					ImGui::Text("Address: %p", comp);
				}

				RenderObjectProperties(comp, property.GetName());
			}

			ImGui::TreePop();
		}
		else {
			UpdateExpandedState(nodeId, false);
			ImGui::SameLine();
			ImGui::TextColored(GetTypeColor(property.GetType()), "%s", typeTag);
			if (!comp) {
				ImGui::SameLine();
				ImGui::TextColored(ImVec4(0.7f, 0.3f, 0.3f, 1.0f), "(null)");
			}
		}
	}
	void PropertyInspector::RenderComponentProperty(const CPropertyBase& property, CReflectedBase* object)
	{
		ComponentSystem::Component* component = reinterpret_cast<ComponentSystem::Component*>(property.GetAddress(object));
		RenderComponentCommon(property, component, component, "[Component]", false, "Component");
	}

	void PropertyInspector::RenderComponentPtrProperty(const CPropertyBase& property, CReflectedBase* object)
	{
		ComponentSystem::Component** componentPtr = reinterpret_cast<ComponentSystem::Component**>(property.GetAddress(object));
		ComponentSystem::Component* pointed = componentPtr ? *componentPtr : nullptr;
		RenderComponentCommon(property, pointed, pointed ? pointed : static_cast<const void*>(componentPtr), "[Component*]", true, "Component pointer");
	}

	void PropertyInspector::RenderComponentPtrVectorProperty(const CPropertyBase& property, CReflectedBase* object)
	{
		std::vector<ComponentSystem::Component*>* componentVector =
			reinterpret_cast<std::vector<ComponentSystem::Component*>*>(property.GetAddress(object));

		const std::string nodeId = GenerateTreeNodeId(property.GetName(), componentVector);
		bool expanded = ShouldExpandNode(nodeId);

		// persistent selection per-property
		static std::unordered_map<std::string, int> s_componentSelection;

		if (ImGui::TreeNodeEx(nodeId.c_str(), expanded ? ImGuiTreeNodeFlags_DefaultOpen : 0, "%s", property.GetName().c_str())) {
			UpdateExpandedState(nodeId, true);

			// Right-click context menu for adding components (must be called right after TreeNodeEx)
			RenderComponentArrayContextMenu(property, object);

			if (m_showDetails) {
				ImGui::SameLine();
				ImGui::TextColored(GetTypeColor(property.GetType()), "[Vector<Component*>]");
			}

			// Add / Remove controls (inline)
			{
				const auto& allComponents = GetComponentRegistry().GetAll();
				int& selIdx = s_componentSelection[property.GetName()];
				if (selIdx < 0) selIdx = 0;

				if (allComponents.empty()) {
					ImGui::SameLine();
					ImGui::PushID((property.GetName() + "##vec_buttons").c_str());
					if (!m_readOnly) {
						ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "(no components)");
					}
					ImGui::PopID();
				}
				else {
					RenderVecAddRemoveButtons(property.GetName() + "##vec_buttons", m_readOnly,
						[&]() {
							const auto& all = GetComponentRegistry().GetAll();
							if (!all.empty()) {
								int indexToAdd = std::clamp(selIdx, 0, static_cast<int>(all.size()) - 1);
								AddComponentToArray(property, object, all[indexToAdd].className);
							}
						},
						[&]() {
							if (!componentVector->empty()) {
								RemoveComponentFromArray(property, object, componentVector->size() - 1);
							}
						});
					ImGui::SameLine();
					const char* preview = allComponents[std::min(selIdx, static_cast<int>(allComponents.size() - 1))].displayName.c_str();
					std::string comboId = property.GetName() + "##comp_combo";
					SetNextItemWidthToContentRegionAvail();
					if (ImGui::BeginCombo(comboId.c_str(), preview)) {
						for (int i = 0; i < static_cast<int>(allComponents.size()); ++i) {
							bool isSelected = (i == selIdx);
							if (ImGui::Selectable(allComponents[i].displayName.c_str(), isSelected)) {
								selIdx = i;
							}
							if (isSelected) ImGui::SetItemDefaultFocus();
						}
						ImGui::EndCombo();
					}
				}
			}

			ImGui::Text("Size: %zu", componentVector->size());

			for (size_t i = 0; i < componentVector->size(); ++i) {
				const std::string elementName = "[" + std::to_string(i) + "]";
				const std::string elementNodeId = GenerateTreeNodeId(elementName, (*componentVector)[i]);
				bool elementExpanded = ShouldExpandNode(elementNodeId);

				bool nodeOpen = ImGui::TreeNodeEx(elementNodeId.c_str(), elementExpanded ? ImGuiTreeNodeFlags_DefaultOpen : 0, "%s", elementName.c_str());

				// Right-click context menu for individual component (works both expanded and collapsed)
				RenderComponentItemContextMenu(property, object, i);

				if (nodeOpen) {
					UpdateExpandedState(elementNodeId, true);

					if ((*componentVector)[i]) {
						ComponentSystem::Component* comp = (*componentVector)[i];

						// temporarily switch widget map to the component's class (RAII)
						WidgetMapScope wms(this, comp);

						if (m_showDetails) {
							ImGui::Text("Type: %s", comp->GetRflClassName() ? comp->GetRflClassName() : "Unknown");
							ImGui::Text("ID: %u", comp->GetId());
							ImGui::Text("Active: %s", comp->IsActive() ? "true" : "false");
							ImGui::Text("Address: %p", comp);
						}
						RenderObjectProperties(comp, elementName);
					}
					else {
						RenderNullPointer("Vector element");
					}

					ImGui::TreePop();
				}
				else {
					UpdateExpandedState(elementNodeId, false);
					if (!(*componentVector)[i]) {
						ImGui::SameLine();
						ImGui::TextColored(ImVec4(0.7f, 0.3f, 0.3f, 1.0f), "(null)");
					}
				}
			}

			ImGui::TreePop();
		}
		else {
			UpdateExpandedState(nodeId, false);
			ImGui::SameLine();
			ImGui::TextColored(GetTypeColor(property.GetType()), "[Vector<Component*>] (%zu)", componentVector->size());

			// Allow quick add/remove when collapsed
			{
				const auto& allComponents = GetComponentRegistry().GetAll();
				int& selIdx = s_componentSelection[property.GetName()];
				if (selIdx < 0) selIdx = 0;

				if (allComponents.empty()) {
					ImGui::SameLine();
					ImGui::PushID((property.GetName() + "##vec_buttons_collapsed").c_str());
					if (!m_readOnly) {
						ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "(no components)");
					}
					ImGui::PopID();
				}
				else {
					RenderVecAddRemoveButtons(property.GetName() + "##vec_buttons_collapsed", m_readOnly,
						[&]() {
							const auto& all = GetComponentRegistry().GetAll();
							if (!all.empty()) {
								int indexToAdd = std::clamp(selIdx, 0, static_cast<int>(all.size()) - 1);
								AddComponentToArray(property, object, all[indexToAdd].className);
							}
						},
						[&]() {
							if (!componentVector->empty()) {
								RemoveComponentFromArray(property, object, componentVector->size() - 1);
							}
						});
					ImGui::SameLine();
					const char* preview = allComponents[std::min(selIdx, static_cast<int>(allComponents.size() - 1))].displayName.c_str();
					std::string comboId = property.GetName() + "##comp_combo_collapsed";
					SetNextItemWidthToContentRegionAvail();
					if (ImGui::BeginCombo(comboId.c_str(), preview)) {
						for (int i = 0; i < static_cast<int>(allComponents.size()); ++i) {
							bool isSelected = (i == selIdx);
							if (ImGui::Selectable(allComponents[i].displayName.c_str(), isSelected)) {
								selIdx = i;
							}
							if (isSelected) ImGui::SetItemDefaultFocus();
						}
						ImGui::EndCombo();
					}
				}
			}
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

	const char* PropertyInspector::GetTypeDisplayName(RFL_Type type)
	{
		switch (type) {
		case RT_Int: return "int";
		case RT_Float: return "float";
		case RT_String: return "string";
		case RT_Bool: return "bool";
		case RT_Vector3: return "Vector3";
		case RT_Vector4: return "Vector4";
		case RT_Matrix4: return "Matrix4";
		case RT_IntVec: return "Vector<int>";
		case RT_FloatVec: return "Vector<float>";
		case RT_BoolVec: return "Vector<bool>";
		case RT_StringVec: return "Vector<string>";
		case RT_Object: return "Object";
		case RT_ObjectPtr: return "Object*";
		case RT_ObjectPtrVec: return "Vector<Object*>";
		case RT_Component: return "Component";
		case RT_ComponentPtr: return "Component*";
		case RT_ComponentPtrVec: return "Vector<Component*>";
		case RT_ComponentRawPtrVec: return "Vector<Component*>";
		default: return "Unknown";
		}
	}

	ImVec4 PropertyInspector::GetTypeColor(RFL_Type type)
	{
		switch (type) {
		case RT_Int:
		case RT_Float:
		case RT_String:
		case RT_Bool:
			return COLOR_TYPE_PRIMITIVE;
		case RT_Vector3:
		case RT_Vector4:
		case RT_Matrix4:
		case RT_IntVec:
		case RT_FloatVec:
		case RT_BoolVec:
		case RT_StringVec:
			return COLOR_TYPE_VECTOR;
		case RT_Object:
			return COLOR_TYPE_OBJECT;
		case RT_ObjectPtr:
		case RT_ObjectPtrVec:
			return COLOR_TYPE_POINTER;
		case RT_Component:
		case RT_ComponentPtr:
		case RT_ComponentPtrVec:
		case RT_ComponentRawPtrVec:
			return COLOR_TYPE_COMPONENT;
		default:
			return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
		}
	}

	void PropertyInspector::RenderPropertyLabel(const std::string& name, RFL_Type type)
	{
		ImGui::TextColored(GetTypeColor(type), "%s", name.c_str());
		ImGui::SameLine();
		ImGui::Text(":");
		ImGui::SameLine();
	}

	void PropertyInspector::RenderNullPointer(const std::string& name)
	{
		ImGui::TextColored(ImVec4(0.7f, 0.3f, 0.3f, 1.0f), "%s: (null)", name.c_str());
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

	bool PropertyInspector::ValidateFloatInput(float& value, float min, float max)
	{
		if (value < min) {
			value = min;
			return false;
		}
		if (value > max) {
			value = max;
			return false;
		}
		return true;
	}

	bool PropertyInspector::ValidateIntInput(int& value, int min, int max)
	{
		if (value < min) {
			value = min;
			return false;
		}
		if (value > max) {
			value = max;
			return false;
		}
		return true;
	}



// ═════════════════════════════════════════════════════════════════════════════
	// ════════════════════════════════════════════════════════════════════════════
	// Widget-specific rendering methods
	// ════════════════════════════════════════════════════════════════════════════

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

		RenderPropertyLabel(property.GetName(), property.GetType());

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

		RenderPropertyLabel(property.GetName(), property.GetType());

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

		RenderPropertyLabel(property.GetName(), property.GetType());

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

		RenderPropertyLabel(property.GetName(), property.GetType());

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

		RenderPropertyLabel(property.GetName(), property.GetType());

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

		RenderPropertyLabel(property.GetName(), property.GetType());

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
		RenderPropertyLabel(property.GetName(), property.GetType());

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

		RenderPropertyLabel(property.GetName(), property.GetType());

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
		RenderPropertyLabel(property.GetName(), property.GetType());

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
	// Replace the free helper with a templated member implementation that uses vec pointer+index
	template<typename ValueT, typename DrawElemFn>
	void PropertyInspector::RenderVectorGeneric(
		const CPropertyBase& property,
		CReflectedBase* object,
		const char* vectorTag,
		DrawElemFn drawElement,
		std::function<ValueT()> makeDefault,
		std::function<void(void*)> removeCleanup)
	{
		auto vec = reinterpret_cast<std::vector<ValueT>*>(property.GetAddress(object));
		if (!vec) return;

		const std::string nodeId = GenerateTreeNodeId(property.GetName(), vec);
		bool expanded = ShouldExpandNode(nodeId);

		if (ImGui::TreeNodeEx(nodeId.c_str(), expanded ? ImGuiTreeNodeFlags_DefaultOpen : 0, "%s", property.GetName().c_str())) {
			UpdateExpandedState(nodeId, true);

			ImGui::SameLine();
			if( m_showDetails )
				ImGui::TextColored(GetTypeColor(property.GetType()), "%s", vectorTag);

			// Add / Remove controls (use the existing free function)
			RenderVecAddRemoveButtons(property.GetName() + "##vec_buttons", m_readOnly,
				[&]() {
					if (!m_readOnly) {
						if (makeDefault) vec->push_back(makeDefault());
						else vec->push_back(ValueT());
					}
				},
				[&]() {
					if (!m_readOnly && !vec->empty()) {
						if (removeCleanup) {
							// avoid taking address-of for vector<bool> proxy
							if constexpr (!std::is_same<ValueT, bool>::value) {
								void* lastAddr = &(*vec)[vec->size() - 1];
								removeCleanup(lastAddr);
							}
						}
						vec->pop_back();
					}
				});

			ImGui::SameLine();
			ImGui::Text("Size: %zu", vec->size());

			for (size_t i = 0; i < vec->size(); ++i) {
				ImGui::PushID(static_cast<int>(i));
				ImGui::Text("[%zu]:", i);
				ImGui::SameLine();
				drawElement(vec, i, m_readOnly);
				ImGui::PopID();
			}

			ImGui::TreePop();
		}
		else {
			UpdateExpandedState(nodeId, false);
			if (m_showDetails)
			{
				ImGui::SameLine();
				ImGui::TextColored(GetTypeColor(property.GetType()), "%s (%zu)", vectorTag, vec->size());
			}

			// Quick add/remove when collapsed
			RenderVecAddRemoveButtons(property.GetName() + "##vec_buttons_collapsed", m_readOnly,
				[&]() {
					if (!m_readOnly) {
						if (makeDefault) vec->push_back(makeDefault());
						else vec->push_back(ValueT());
					}
				},
				[&]() {
					if (!m_readOnly && !vec->empty()) {
						if (removeCleanup) {
							// avoid taking address-of for vector<bool> proxy
							if constexpr (!std::is_same<ValueT, bool>::value) {
								void* lastAddr = &(*vec)[vec->size() - 1];
								removeCleanup(lastAddr);
							}
						}
						vec->pop_back();
					}
				});
			ImGui::SameLine();
			ImGui::Text("Size: %zu", vec->size());
		}
	}

	// int vector
	void PropertyInspector::RenderIntVectorProperty(const CPropertyBase& property, CReflectedBase* object)
	{
		RenderVectorGeneric<int>(property, object, "[Vector<int>]",
			[&](std::vector<int>* vec, size_t i, bool readOnly) {
				if (readOnly) {
					ImGui::Text("%d", (*vec)[i]);
				}
				else {
					ImGui::PushID("val");
					ImGui::InputInt("##val", &(*vec)[i]);
					ImGui::PopID();
				}
			},
			[]() { return 0; });
	}

	// float vector
	void PropertyInspector::RenderFloatVectorProperty(const CPropertyBase& property, CReflectedBase* object)
	{
		RenderVectorGeneric<float>(property, object, "[Vector<float>]",
			[&](std::vector<float>* vec, size_t i, bool readOnly) {
				if (readOnly) {
					ImGui::Text("%.3f", (*vec)[i]);
				}
				else {
					float v = (*vec)[i];
					ImGui::PushID("val");
					if (ImGui::InputFloat("##val", &v, 0.0f, 0.0f, "%.3f")) {
						(*vec)[i] = v;
					}
					ImGui::PopID();
				}
			},
			[]() { return 0.0f; });
	}

	// bool vector (handle proxy by using a local temp for editing)
	void PropertyInspector::RenderBoolVectorProperty(const CPropertyBase& property, CReflectedBase* object)
	{
		RenderVectorGeneric<bool>(property, object, "[Vector<bool>]",
			[&](std::vector<bool>* vec, size_t i, bool readOnly) {
				bool val = (*vec)[i]; // copy out (works with vector<bool> proxy)
				if (readOnly) {
					ImGui::Text("%s", val ? "true" : "false");
				}
				else {
					ImGui::PushID("val");
					if (ImGui::Checkbox("##val", &val)) {
						(*vec)[i] = val; // assign back
					}
					ImGui::PopID();
				}
			},
			[]() { return false; });
	}

	// string vector (preserve per-element edit buffers; provide cleanup lambda)
	void PropertyInspector::RenderStringVectorProperty(const CPropertyBase& property, CReflectedBase* object)
	{
		RenderVectorGeneric<std::string>(property, object, "[Vector<string>]",
			[&](std::vector<std::string>* vec, size_t i, bool readOnly) {
				void* elemAddr = &(*vec)[i];
				StringEditBuffer& buf = m_StringBuffers[elemAddr];

				if (!buf.m_isBeingEdited) {
					strncpy_s(buf.data, (*vec)[i].c_str(), sizeof(buf.data) - 1);
					buf.data[sizeof(buf.data) - 1] = '\0';
				}

				ImGui::PushID(static_cast<int>(i));
				if (readOnly) {
					ImGui::Text("%s", (*vec)[i].c_str());
				}
				else {
					if (ImGui::InputText("##val", buf.data, sizeof(buf.data))) {
						(*vec)[i] = buf.data;
					}
					buf.m_isBeingEdited = ImGui::IsItemActive();
				}
				ImGui::PopID();
			},
			[]() { return std::string(); },
			[&](void* lastAddr) {
				auto it = m_StringBuffers.find(lastAddr);
				if (it != m_StringBuffers.end()) m_StringBuffers.erase(it);
			});
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

	RenderPropertyLabel(property.GetName(), property.GetType());

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