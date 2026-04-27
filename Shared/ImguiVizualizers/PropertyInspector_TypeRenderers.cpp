// PropertyInspector_TypeRenderers.cpp
// Type-specific property rendering methods for PropertyInspector.
// Extracted from PropertyInspector.cpp to reduce translation-unit size.

#include <algorithm>
#include <sstream>
#include <functional>
#include <unordered_map>

#include "PropertyInspector.h"
#include "PropertyInspector_Internal.h"
#include "PropertyWidgetMapRegistry.h"
#include "Math/Vector3f.h"
#include "Math/Vector4f.h"
#include "Math/Matrix4f.h"
#include "ClassFactory/ClassFactory.h"
#include "ComponentSystem/ComponentSystem.h"
#include "ComponentSystem/ComponentRegistry.h"

namespace ImGuiVisualizers {

    // ???????????????????????????????????????????????????????????????????????????
    // Display-name / label helpers
    // ???????????????????????????????????????????????????????????????????????????

    std::string PropertyInspector::GetDisplayName(const CPropertyBase& property, CReflectedBase* ownerObject) const
    {
        // Local map override (widget map currently set for the parent / context)
        if (m_widgetMap) {
            std::string localName = m_widgetMap->GetEntryDisplayNameLocal(property.GetName());
            if (!localName.empty()) return localName;
        }

        // Try to find an inherited mapping in the registry/hierarchy for the owner object's class.
        if (ownerObject) {
            const char* className = ownerObject->GetRflClassName();
            if (className && className[0] != '\0') {
                EditorWidgetType tmpType;
                WidgetConfig tmpCfg;
                std::string originClass;
                if (PropertyWidgetMap::FindWidgetInHierarchy(className, property.GetName(), tmpType, &tmpCfg, &originClass)) {
                    if (!originClass.empty()) {
                        auto originMap = PropertyWidgetMapRegistry::Instance().Get(originClass);
                        if (originMap) {
                            std::string inheritedName = originMap->GetEntryDisplayName(property.GetName());
                            if (!inheritedName.empty()) return inheritedName;
                        }
                    }
                }
            }
        }

        return property.GetName();
    }

    void PropertyInspector::RenderPropertyLabel(const CPropertyBase& property, CReflectedBase* ownerObject)
    {
        std::string display = GetDisplayName(property, ownerObject);
        ImGui::TextColored(GetTypeColor(property.GetType()), "%s", display.c_str());
        ImGui::SameLine();
        ImGui::Text(":");
        ImGui::SameLine();
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

    const char* PropertyInspector::GetTypeDisplayName(RFL_Type type)
    {
        switch (type) {
        case RT_Int:              return "int";
        case RT_Float:            return "float";
        case RT_String:           return "string";
        case RT_Bool:             return "bool";
        case RT_Vector3:          return "Vector3";
        case RT_Vector4:          return "Vector4";
        case RT_Matrix4:          return "Matrix4";
        case RT_IntVec:           return "Vector<int>";
        case RT_FloatVec:         return "Vector<float>";
        case RT_BoolVec:          return "Vector<bool>";
        case RT_StringVec:        return "Vector<string>";
        case RT_Object:           return "Object";
        case RT_ObjectPtr:        return "Object*";
        case RT_ObjectPtrVec:     return "Vector<Object*>";
        case RT_Component:        return "Component";
        case RT_ComponentPtr:     return "Component*";
        case RT_ComponentPtrVec:  return "Vector<Component*>";
        case RT_ComponentRawPtrVec: return "Vector<Component*>";
        default:                  return "Unknown";
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

    // ???????????????????????????????????????????????????????????????????????????
    // Validation helpers
    // ???????????????????????????????????????????????????????????????????????????

    bool PropertyInspector::ValidateFloatInput(float& value, float min, float max)
    {
        if (value < min) { value = min; return false; }
        if (value > max) { value = max; return false; }
        return true;
    }

    bool PropertyInspector::ValidateIntInput(int& value, int min, int max)
    {
        if (value < min) { value = min; return false; }
        if (value > max) { value = max; return false; }
        return true;
    }

    // ???????????????????????????????????????????????????????????????????????????
    // Primitive type renderers
    // ???????????????????????????????????????????????????????????????????????????

    void PropertyInspector::RenderFloatProperty(const CPropertyBase& property, CReflectedBase* object)
    {
        float* value = reinterpret_cast<float*>(property.GetAddress(object));

        RenderPropertyLabel(property, object);

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

        RenderPropertyLabel(property, object);

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

        RenderPropertyLabel(property, object);

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

        RenderPropertyLabel(property, object);

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

        RenderPropertyLabel(property, object);

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

        RenderPropertyLabel(property, object);

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

        std::string display = GetDisplayName(property, object);

        if (ImGui::TreeNodeEx(nodeId.c_str(), expanded ? ImGuiTreeNodeFlags_DefaultOpen : 0, "%s", display.c_str())) {
            UpdateExpandedState(nodeId, true);

            ImGui::SameLine();
            ImGui::TextColored(GetTypeColor(property.GetType()), "[Matrix4]");

            if (m_readOnly) {
                for (int row = 0; row < 4; ++row) {
                    ImGui::Text("[%.3f, %.3f, %.3f, %.3f]",
                        (*value)(row, 0), (*value)(row, 1),
                        (*value)(row, 2), (*value)(row, 3));
                }
            }
            else {
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

    // ???????????????????????????????????????????????????????????????????????????
    // Reflected-object renderers
    // ???????????????????????????????????????????????????????????????????????????

    void PropertyInspector::RenderReflectedObjectCommon(
        const CPropertyBase& property,
        CReflectedBase*      subObject,
        const void*          idSource,
        const char*          typeTag,
        bool                 showAddress,
        const char*          nullNameOverride)
    {
        const std::string nodeId = GenerateTreeNodeId(property.GetName(), idSource);
        bool expanded = ShouldExpandNode(nodeId);

        std::string display = GetDisplayName(property, nullptr);

        if (ImGui::TreeNodeEx(nodeId.c_str(), expanded ? ImGuiTreeNodeFlags_DefaultOpen : 0, "%s", display.c_str())) {
            UpdateExpandedState(nodeId, true);

            ImGui::SameLine();
            ImGui::TextColored(GetTypeColor(property.GetType()), "%s", typeTag);

            if (!subObject) {
                RenderNullPointer(nullNameOverride ? nullNameOverride : property.GetName().c_str());
            }
            else {
                WidgetMapScope wms(this, subObject);

                if (m_showDetails)
                    ImGui::Text("Type: %s", subObject->GetRflClassName() ? subObject->GetRflClassName() : "Unknown");
                if (showAddress)
                    ImGui::Text("Address: %p", subObject);

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
        void* addr = property.GetAddress(object);
        uintptr_t memberAddr = reinterpret_cast<uintptr_t>(addr);

        if (addr == nullptr) {
            RenderNullPointer("Embedded object");
            return;
        }
        const uintptr_t kLow  = 0x10000u;
        const uintptr_t kHigh = (uintptr_t)0x00007fffffffffffULL;
        if (memberAddr < kLow || memberAddr > kHigh) {
            RenderNullPointer("Embedded object (invalid address)");
            return;
        }

        CReflectedBase* subObject = reinterpret_cast<CReflectedBase*>(addr);
        RenderReflectedObjectCommon(property, subObject, subObject, "[Object]", false, "Embedded object");
    }

    void PropertyInspector::RenderObjectPtrProperty(const CPropertyBase& property, CReflectedBase* object)
    {
        std::unique_ptr<CReflectedBase>* objectPtr =
            reinterpret_cast<std::unique_ptr<CReflectedBase>*>(property.GetAddress(object));

        CReflectedBase* pointed = objectPtr ? objectPtr->get() : nullptr;
        RenderReflectedObjectCommon(property, pointed,
            pointed ? pointed : static_cast<const void*>(objectPtr),
            "[Object*]", true, "Object pointer");
    }

    void PropertyInspector::RenderObjectPtrVectorProperty(const CPropertyBase& property, CReflectedBase* object)
    {
        std::vector<std::unique_ptr<CReflectedBase>>* objectVector =
            reinterpret_cast<std::vector<std::unique_ptr<CReflectedBase>>*>(property.GetAddress(object));

        const std::string nodeId = GenerateTreeNodeId(property.GetName(), objectVector);
        bool expanded = ShouldExpandNode(nodeId);

        std::string display = GetDisplayName(property, object);

        static std::unordered_map<std::string, int> s_objectSelection;

        if (ImGui::TreeNodeEx(nodeId.c_str(), expanded ? ImGuiTreeNodeFlags_DefaultOpen : 0, "%s", display.c_str())) {
            UpdateExpandedState(nodeId, true);

            RenderObjectArrayContextMenu(property, object);

            ImGui::SameLine();
            ImGui::TextColored(GetTypeColor(property.GetType()), "[Vector<Object*>]");

            {
                auto classNames = ClassFactory::GetRegisteredClassNames();
                int& selIdx = s_objectSelection[property.GetName()];
                if (selIdx < 0) selIdx = 0;

                if (classNames.empty()) {
                    ImGui::SameLine();
                    ImGui::PushID((property.GetName() + "##vec_buttons").c_str());
                    if (!m_readOnly)
                        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "(no classes)");
                    ImGui::PopID();
                }
                else {
                    RenderVecAddRemoveButtons(property.GetName() + "##vec_buttons", m_readOnly,
                        [&]() {
                            auto classNamesAdd = ClassFactory::GetRegisteredClassNames();
                            if (!classNamesAdd.empty()) {
                                int idx = std::clamp(selIdx, 0, static_cast<int>(classNamesAdd.size()) - 1);
                                AddObjectToArray(property, object, classNamesAdd[idx]);
                            }
                        },
                        [&]() {
                            if (!objectVector->empty())
                                RemoveObjectFromArray(property, object, objectVector->size() - 1);
                        });
                    ImGui::SameLine();
                    const char* preview = classNames[std::min(selIdx, static_cast<int>(classNames.size() - 1))].c_str();
                    std::string comboId = property.GetName() + "##class_combo";
                    SetNextItemWidthToContentRegionAvail();
                    if (ImGui::BeginCombo(comboId.c_str(), preview)) {
                        for (int i = 0; i < static_cast<int>(classNames.size()); ++i) {
                            bool isSelected = (i == selIdx);
                            if (ImGui::Selectable(classNames[i].c_str(), isSelected))
                                selIdx = i;
                            if (isSelected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                }
            }

            if (objectVector->size())
                ImGui::Text("Size: %zu", objectVector->size());

            for (size_t i = 0; i < objectVector->size(); ++i) {
                const std::string elementName   = "[" + std::to_string(i) + "]";
                const std::string elementNodeId = GenerateTreeNodeId(elementName, (*objectVector)[i].get());
                bool elementExpanded = ShouldExpandNode(elementNodeId);

                if (ImGui::TreeNodeEx(elementNodeId.c_str(), elementExpanded ? ImGuiTreeNodeFlags_DefaultOpen : 0, "%s", elementName.c_str())) {
                    UpdateExpandedState(elementNodeId, true);

                    if ((*objectVector)[i]) {
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

            {
                auto classNames = ClassFactory::GetRegisteredClassNames();
                int& selIdx = s_objectSelection[property.GetName()];
                if (selIdx < 0) selIdx = 0;

                if (classNames.empty()) {
                    ImGui::SameLine();
                    ImGui::PushID((property.GetName() + "##vec_buttons_collapsed").c_str());
                    if (!m_readOnly)
                        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "(no classes)");
                    ImGui::PopID();
                }
                else {
                    RenderVecAddRemoveButtons(property.GetName() + "##vec_buttons_collapsed", m_readOnly,
                        [&]() {
                            auto classNamesAdd = ClassFactory::GetRegisteredClassNames();
                            if (!classNamesAdd.empty()) {
                                int idx = std::clamp(selIdx, 0, static_cast<int>(classNamesAdd.size()) - 1);
                                AddObjectToArray(property, object, classNamesAdd[idx]);
                            }
                        },
                        [&]() {
                            if (!objectVector->empty())
                                RemoveObjectFromArray(property, object, objectVector->size() - 1);
                        });
                    ImGui::SameLine();
                    const char* preview = classNames[std::min(selIdx, static_cast<int>(classNames.size() - 1))].c_str();
                    std::string comboId = property.GetName() + "##class_combo_collapsed";
                    SetNextItemWidthToContentRegionAvail();
                    if (ImGui::BeginCombo(comboId.c_str(), preview)) {
                        for (int i = 0; i < static_cast<int>(classNames.size()); ++i) {
                            bool isSelected = (i == selIdx);
                            if (ImGui::Selectable(classNames[i].c_str(), isSelected))
                                selIdx = i;
                            if (isSelected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                }
            }
        }
    }

    // ???????????????????????????????????????????????????????????????????????????
    // Component renderers
    // ???????????????????????????????????????????????????????????????????????????

    void PropertyInspector::RenderComponentCommon(
        const CPropertyBase&        property,
        ComponentSystem::Component* comp,
        const void*                 idSource,
        const char*                 typeTag,
        bool                        showAddress,
        const char*                 nullNameOverride)
    {
        const std::string nodeId = GenerateTreeNodeId(property.GetName(), idSource);
        bool expanded = ShouldExpandNode(nodeId);

        std::string display = GetDisplayName(property, nullptr);

        if (ImGui::TreeNodeEx(nodeId.c_str(), expanded ? ImGuiTreeNodeFlags_DefaultOpen : 0, "%s", display.c_str())) {
            UpdateExpandedState(nodeId, true);

            ImGui::SameLine();
            ImGui::TextColored(GetTypeColor(property.GetType()), "%s", typeTag);

            if (!comp) {
                RenderNullPointer(nullNameOverride ? nullNameOverride : property.GetName().c_str());
            }
            else {
                WidgetMapScope wms(this, comp);

                ImGui::Text("Type: %s", comp->GetRflClassName() ? comp->GetRflClassName() : "Unknown");
                ImGui::Text("ID: %u", comp->GetId());
                ImGui::Text("Active: %s", comp->IsActive() ? "true" : "false");
                ImGui::Text("Initialized: %s", comp->IsInitialized() ? "true" : "false");
                if (showAddress)
                    ImGui::Text("Address: %p", comp);

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
        ComponentSystem::Component*  pointed      = componentPtr ? *componentPtr : nullptr;
        RenderComponentCommon(property, pointed,
            pointed ? pointed : static_cast<const void*>(componentPtr),
            "[Component*]", true, "Component pointer");
    }

    void PropertyInspector::RenderComponentPtrVectorProperty(const CPropertyBase& property, CReflectedBase* object)
    {
        std::vector<ComponentSystem::Component*>* componentVector =
            reinterpret_cast<std::vector<ComponentSystem::Component*>*>(property.GetAddress(object));

        const std::string nodeId = GenerateTreeNodeId(property.GetName(), componentVector);
        bool expanded = ShouldExpandNode(nodeId);

        static std::unordered_map<std::string, int> s_componentSelection;

        std::string display = GetDisplayName(property, object);

        if (ImGui::TreeNodeEx(nodeId.c_str(), expanded ? ImGuiTreeNodeFlags_DefaultOpen : 0, "%s", display.c_str())) {
            UpdateExpandedState(nodeId, true);

            RenderComponentArrayContextMenu(property, object);

            if (m_showDetails) {
                ImGui::SameLine();
                ImGui::TextColored(GetTypeColor(property.GetType()), "[Vector<Component*>]");
            }

            {
                const auto& allComponents = GetComponentRegistry().GetAll();
                int& selIdx = s_componentSelection[property.GetName()];
                if (selIdx < 0) selIdx = 0;

                if (allComponents.empty()) {
                    ImGui::SameLine();
                    ImGui::PushID((property.GetName() + "##vec_buttons").c_str());
                    if (!m_readOnly)
                        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "(no components)");
                    ImGui::PopID();
                }
                else {
                    RenderVecAddRemoveButtons(property.GetName() + "##vec_buttons", m_readOnly,
                        [&]() {
                            const auto& all = GetComponentRegistry().GetAll();
                            if (!all.empty()) {
                                int idx = std::clamp(selIdx, 0, static_cast<int>(all.size()) - 1);
                                AddComponentToArray(property, object, all[idx].className);
                            }
                        },
                        [&]() {
                            if (!componentVector->empty())
                                RemoveComponentFromArray(property, object, componentVector->size() - 1);
                        });
                    ImGui::SameLine();
                    const char* preview = allComponents[std::min(selIdx, static_cast<int>(allComponents.size() - 1))].displayName.c_str();
                    std::string comboId = property.GetName() + "##comp_combo";
                    SetNextItemWidthToContentRegionAvail();
                    if (ImGui::BeginCombo(comboId.c_str(), preview)) {
                        for (int i = 0; i < static_cast<int>(allComponents.size()); ++i) {
                            bool isSelected = (i == selIdx);
                            if (ImGui::Selectable(allComponents[i].displayName.c_str(), isSelected))
                                selIdx = i;
                            if (isSelected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                }
            }

            if (componentVector->size())
                ImGui::Text("Size: %zu", componentVector->size());

            for (size_t i = 0; i < componentVector->size(); ++i) {
                const std::string elementName   = "[" + std::to_string(i) + "]";
                const std::string elementNodeId = GenerateTreeNodeId(elementName, (*componentVector)[i]);
                bool elementExpanded = ShouldExpandNode(elementNodeId);

                bool nodeOpen = ImGui::TreeNodeEx(elementNodeId.c_str(),
                    elementExpanded ? ImGuiTreeNodeFlags_DefaultOpen : 0,
                    "%s", elementName.c_str());

                RenderComponentItemContextMenu(property, object, i);

                if (nodeOpen) {
                    UpdateExpandedState(elementNodeId, true);

                    if ((*componentVector)[i]) {
                        ComponentSystem::Component* comp = (*componentVector)[i];
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

            {
                const auto& allComponents = GetComponentRegistry().GetAll();
                int& selIdx = s_componentSelection[property.GetName()];
                if (selIdx < 0) selIdx = 0;

                if (allComponents.empty()) {
                    ImGui::SameLine();
                    ImGui::PushID((property.GetName() + "##vec_buttons_collapsed").c_str());
                    if (!m_readOnly)
                        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "(no components)");
                    ImGui::PopID();
                }
                else {
                    RenderVecAddRemoveButtons(property.GetName() + "##vec_buttons_collapsed", m_readOnly,
                        [&]() {
                            const auto& all = GetComponentRegistry().GetAll();
                            if (!all.empty()) {
                                int idx = std::clamp(selIdx, 0, static_cast<int>(all.size()) - 1);
                                AddComponentToArray(property, object, all[idx].className);
                            }
                        },
                        [&]() {
                            if (!componentVector->empty())
                                RemoveComponentFromArray(property, object, componentVector->size() - 1);
                        });
                    ImGui::SameLine();
                    const char* preview = allComponents[std::min(selIdx, static_cast<int>(allComponents.size() - 1))].displayName.c_str();
                    std::string comboId = property.GetName() + "##comp_combo_collapsed";
                    SetNextItemWidthToContentRegionAvail();
                    if (ImGui::BeginCombo(comboId.c_str(), preview)) {
                        for (int i = 0; i < static_cast<int>(allComponents.size()); ++i) {
                            bool isSelected = (i == selIdx);
                            if (ImGui::Selectable(allComponents[i].displayName.c_str(), isSelected))
                                selIdx = i;
                            if (isSelected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                }
            }
        }
    }

    // ???????????????????????????????????????????????????????????????????????????
    // Generic vector renderer + primitive-vector specialisations
    // ???????????????????????????????????????????????????????????????????????????

    template<typename ValueT, typename DrawElemFn>
    void PropertyInspector::RenderVectorGeneric(
        const CPropertyBase&           property,
        CReflectedBase*                object,
        const char*                    vectorTag,
        DrawElemFn                     drawElement,
        std::function<ValueT()>        makeDefault,
        std::function<void(void*)>     removeCleanup)
    {
        auto vec = reinterpret_cast<std::vector<ValueT>*>(property.GetAddress(object));
        if (!vec) return;

        const std::string nodeId  = GenerateTreeNodeId(property.GetName(), vec);
        bool expanded             = ShouldExpandNode(nodeId);
        std::string display       = GetDisplayName(property, object);

        if (ImGui::TreeNodeEx(nodeId.c_str(), expanded ? ImGuiTreeNodeFlags_DefaultOpen : 0, "%s", display.c_str())) {
            UpdateExpandedState(nodeId, true);

            ImGui::SameLine();
            if (m_showDetails)
                ImGui::TextColored(GetTypeColor(property.GetType()), "%s", vectorTag);

            RenderVecAddRemoveButtons(property.GetName() + "##vec_buttons", m_readOnly,
                [&]() {
                    if (!m_readOnly) {
                        if (makeDefault) vec->push_back(makeDefault());
                        else             vec->push_back(ValueT());
                    }
                },
                [&]() {
                    if (!m_readOnly && !vec->empty()) {
                        if (removeCleanup) {
                            if constexpr (!std::is_same<ValueT, bool>::value) {
                                void* lastAddr = &(*vec)[vec->size() - 1];
                                removeCleanup(lastAddr);
                            }
                        }
                        vec->pop_back();
                    }
                });

            if (vec->size()) {
                ImGui::SameLine();
                ImGui::Text("Size: %zu", vec->size());
            }

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
            if (m_showDetails) {
                ImGui::SameLine();
                ImGui::TextColored(GetTypeColor(property.GetType()), "%s (%zu)", vectorTag, vec->size());
            }

            RenderVecAddRemoveButtons(property.GetName() + "##vec_buttons_collapsed", m_readOnly,
                [&]() {
                    if (!m_readOnly) {
                        if (makeDefault) vec->push_back(makeDefault());
                        else             vec->push_back(ValueT());
                    }
                },
                [&]() {
                    if (!m_readOnly && !vec->empty()) {
                        if (removeCleanup) {
                            if constexpr (!std::is_same<ValueT, bool>::value) {
                                void* lastAddr = &(*vec)[vec->size() - 1];
                                removeCleanup(lastAddr);
                            }
                        }
                        vec->pop_back();
                    }
                });

            if (vec->size()) {
                ImGui::SameLine();
                ImGui::Text("Size: %zu", vec->size());
            }
        }
    }

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
                    if (ImGui::InputFloat("##val", &v, 0.0f, 0.0f, "%.3f"))
                        (*vec)[i] = v;
                    ImGui::PopID();
                }
            },
            []() { return 0.0f; });
    }

    void PropertyInspector::RenderBoolVectorProperty(const CPropertyBase& property, CReflectedBase* object)
    {
        RenderVectorGeneric<bool>(property, object, "[Vector<bool>]",
            [&](std::vector<bool>* vec, size_t i, bool readOnly) {
                bool val = (*vec)[i];
                if (readOnly) {
                    ImGui::Text("%s", val ? "true" : "false");
                }
                else {
                    ImGui::PushID("val");
                    if (ImGui::Checkbox("##val", &val))
                        (*vec)[i] = val;
                    ImGui::PopID();
                }
            },
            []() { return false; });
    }

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
                    if (ImGui::InputText("##val", buf.data, sizeof(buf.data)))
                        (*vec)[i] = buf.data;
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

} // namespace ImGuiVisualizers
