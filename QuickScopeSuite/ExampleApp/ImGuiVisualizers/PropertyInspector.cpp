#include "PropertyInspector.h"

#include "Math/Vector3f.h"
#include "Math/Vector4f.h"
#include "Math/Matrix4f.h"
#include <algorithm>
#include <sstream>
#include "ComponentSystem/ComponentSystem.h"

namespace ImGuiVisualizers {
// Colors for different types
static const ImVec4 COLOR_TYPE_PRIMITIVE = ImVec4(0.7f, 0.9f, 0.7f, 1.0f);
static const ImVec4 COLOR_TYPE_OBJECT = ImVec4(0.7f, 0.7f, 0.9f, 1.0f);
static const ImVec4 COLOR_TYPE_POINTER = ImVec4(0.9f, 0.7f, 0.7f, 1.0f);
static const ImVec4 COLOR_TYPE_VECTOR = ImVec4(0.9f, 0.9f, 0.7f, 1.0f);
static const ImVec4 COLOR_TYPE_COMPONENT = ImVec4(0.9f, 0.7f, 0.9f, 1.0f);

PropertyInspector::PropertyInspector()
    : m_Object(nullptr)
    , m_ReadOnly(false)
    , m_ShowInternalData(true)
    , m_ExpandByDefault(true)
    , m_DisplayMode(PropertyDisplayMode::Hierarchy)
    , m_FirstRender(true)
    , m_FloatBuffer(0.0f)
    , m_IntBuffer(0)
    , m_BoolBuffer(false)
{
    memset(m_StringBuffer, 0, sizeof(m_StringBuffer));
}

bool PropertyInspector::RenderWindow(const char* windowTitle, bool* isOpen)
{
    if (!ImGui::Begin(windowTitle, isOpen)) {
        ImGui::End();
        return false;
    }
    
    // Toolbar
    if (ImGui::Button("Clear")) {
        ClearObject();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Read Only", &m_ReadOnly);
    ImGui::SameLine();
    ImGui::Checkbox("Show Internal", &m_ShowInternalData);
    ImGui::SameLine();
    ImGui::Checkbox("Expand by Default", &m_ExpandByDefault);
    
    // Display mode selection
    ImGui::SameLine();
    if (ImGui::Button(m_DisplayMode == PropertyDisplayMode::Hierarchy ? "Hierarchy" : "Flat List")) {
        m_DisplayMode = (m_DisplayMode == PropertyDisplayMode::Hierarchy) ? 
            PropertyDisplayMode::FlatList : PropertyDisplayMode::Hierarchy;
    }
    
    ImGui::Separator();
    
    // Content
    if (m_Object) {
        ImGui::Text("Object: %s", m_Object->GetRflClassName() ? m_Object->GetRflClassName() : "Unknown");
        ImGui::Text("Address: %p", m_Object);
        ImGui::Separator();
        
        RenderObjectProperties(m_Object, m_Object->GetRflClassName() ? m_Object->GetRflClassName() : "Root");
    } else {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No object selected");
    }
    
    ImGui::End();
    m_FirstRender = false;
    return true;
}

void PropertyInspector::SetObject(CReflectedBase* object)
{
    m_Object = object;
    m_FirstRender = true;
    
    // Clear expanded state when changing objects
    m_ExpandedNodes.clear();
}

void PropertyInspector::ClearObject()
{
    m_Object = nullptr;
    m_ExpandedNodes.clear();
}

void PropertyInspector::RenderObjectProperties(CReflectedBase* object, const std::string& objectName)
{
    if (!object) {
        RenderNullPointer(objectName);
        return;
    }
    
    if (m_DisplayMode == PropertyDisplayMode::FlatList) {
        RenderObjectPropertiesFlat(object);
    } else {
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
                if (!m_ShowInternalData && property->GetName().find("m_") == 0) {
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
            if (m_ShowInternalData || property->GetName().find("m_") != 0) {
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
                if (!m_ShowInternalData && property->GetName().find("m_") == 0) {
                    continue;
                }
                
                RenderProperty(*property, object);
            }
        }
        
        ImGui::TreePop();
    } else {
        UpdateExpandedState(nodeId, false);
    }
}

void PropertyInspector::RenderProperty(const CPropertyBase& property, CReflectedBase* object, const std::string& prefix)
{
    const std::string fullName = prefix.empty() ? property.GetName() : prefix + "." + property.GetName();
    
    // Skip internal properties if not showing them
    if (!m_ShowInternalData && property.GetName().find("m_") == 0) {
        return;
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
    
    if (m_ReadOnly) {
        ImGui::Text("%.3f", *value);
    } else {
        ImGui::PushID(property.GetName().c_str());
        if (ImGui::InputFloat("", value, 0.0f, 0.0f, "%.3f")) {
            // Value changed - could add validation here
        }
        ImGui::PopID();
    }
}

void PropertyInspector::RenderIntProperty(const CPropertyBase& property, CReflectedBase* object)
{
    int* value = reinterpret_cast<int*>(property.GetAddress(object));
    
    RenderPropertyLabel(property.GetName(), property.GetType());
    
    if (m_ReadOnly) {
        ImGui::Text("%d", *value);
    } else {
        ImGui::PushID(property.GetName().c_str());
        ImGui::InputInt("", value);
        ImGui::PopID();
    }
}

void PropertyInspector::RenderStringProperty(const CPropertyBase& property, CReflectedBase* object)
{
    std::string* value = reinterpret_cast<std::string*>(property.GetAddress(object));
    
    RenderPropertyLabel(property.GetName(), property.GetType());
    
    if (m_ReadOnly) {
        ImGui::Text("\"%s\"", value->c_str());
    } else {
        // Copy to buffer for editing
        strncpy_s(m_StringBuffer, value->c_str(), sizeof(m_StringBuffer) - 1);
        m_StringBuffer[sizeof(m_StringBuffer) - 1] = '\0';
        
        ImGui::PushID(property.GetName().c_str());
        if (ImGui::InputText("", m_StringBuffer, sizeof(m_StringBuffer))) {
            *value = m_StringBuffer;
        }
        ImGui::PopID();
    }
}

void PropertyInspector::RenderBoolProperty(const CPropertyBase& property, CReflectedBase* object)
{
    bool* value = reinterpret_cast<bool*>(property.GetAddress(object));
    
    RenderPropertyLabel(property.GetName(), property.GetType());
    
    if (m_ReadOnly) {
        ImGui::Text("%s", *value ? "true" : "false");
    } else {
        ImGui::PushID(property.GetName().c_str());
        ImGui::Checkbox("", value);
        ImGui::PopID();
    }
}

void PropertyInspector::RenderVector3Property(const CPropertyBase& property, CReflectedBase* object)
{
    Vector3f* value = reinterpret_cast<Vector3f*>(property.GetAddress(object));
    
    RenderPropertyLabel(property.GetName(), property.GetType());
    
    if (m_ReadOnly) {
        ImGui::Text("(%.3f, %.3f, %.3f)", value->getX(), value->getY(), value->getZ());
    } else {
        float vec3[3] = { value->getX(), value->getY(), value->getZ() };
        ImGui::PushID(property.GetName().c_str());
        if (ImGui::InputFloat3("", vec3)) {
            value->setX(vec3[0]);
            value->setY(vec3[1]);
            value->setZ(vec3[2]);
        }
        ImGui::PopID();
    }
}

void PropertyInspector::RenderVector4Property(const CPropertyBase& property, CReflectedBase* object)
{
    Vector4f* value = reinterpret_cast<Vector4f*>(property.GetAddress(object));
    
    RenderPropertyLabel(property.GetName(), property.GetType());
    
    if (m_ReadOnly) {
        ImGui::Text("(%.3f, %.3f, %.3f, %.3f)", value->getX(), value->getY(), value->getZ(), value->getW());
    } else {
        float vec4[4] = { value->getX(), value->getY(), value->getZ(), value->getW() };
        ImGui::PushID(property.GetName().c_str());
        if (ImGui::InputFloat4("", vec4)) {
            value->setX(vec4[0]);
            value->setY(vec4[1]);
            value->setZ(vec4[2]);
            value->setW(vec4[3]);
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

		if (m_ReadOnly) {
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

void PropertyInspector::RenderObjectProperty(const CPropertyBase& property, CReflectedBase* object)
{
    CReflectedBase* subObject = reinterpret_cast<CReflectedBase*>(property.GetAddress(object));
    
    const std::string nodeId = GenerateTreeNodeId(property.GetName(), subObject);
    bool expanded = ShouldExpandNode(nodeId);
    
    if (ImGui::TreeNodeEx(nodeId.c_str(), expanded ? ImGuiTreeNodeFlags_DefaultOpen : 0, "%s", property.GetName().c_str())) {
        UpdateExpandedState(nodeId, true);
        
        ImGui::SameLine();
        ImGui::TextColored(GetTypeColor(property.GetType()), "[Object]");
        
        if (subObject) {
            ImGui::Text("Type: %s", subObject->GetRflClassName() ? subObject->GetRflClassName() : "Unknown");
            RenderObjectProperties(subObject, property.GetName());
        } else {
            RenderNullPointer("Embedded object");
        }
        
        ImGui::TreePop();
    } else {
        UpdateExpandedState(nodeId, false);
        ImGui::SameLine();
        ImGui::TextColored(GetTypeColor(property.GetType()), "[Object]");
    }
}

void PropertyInspector::RenderObjectPtrProperty(const CPropertyBase& property, CReflectedBase* object)
{
    std::unique_ptr<CReflectedBase>* objectPtr = 
        reinterpret_cast<std::unique_ptr<CReflectedBase>*>(property.GetAddress(object));
    
    const std::string nodeId = GenerateTreeNodeId(property.GetName(), objectPtr->get());
    bool expanded = ShouldExpandNode(nodeId);
    
    if (ImGui::TreeNodeEx(nodeId.c_str(), expanded ? ImGuiTreeNodeFlags_DefaultOpen : 0, "%s", property.GetName().c_str())) {
        UpdateExpandedState(nodeId, true);
        
        ImGui::SameLine();
        ImGui::TextColored(GetTypeColor(property.GetType()), "[Object*]");
        
        if (*objectPtr) {
            ImGui::Text("Type: %s", (*objectPtr)->GetRflClassName() ? (*objectPtr)->GetRflClassName() : "Unknown");
            ImGui::Text("Address: %p", objectPtr->get());
            RenderObjectProperties(objectPtr->get(), property.GetName());
        } else {
            RenderNullPointer("Object pointer");
        }
        
        ImGui::TreePop();
    } else {
        UpdateExpandedState(nodeId, false);
        ImGui::SameLine();
        ImGui::TextColored(GetTypeColor(property.GetType()), "[Object*]");
        if (!*objectPtr) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.7f, 0.3f, 0.3f, 1.0f), "(null)");
        }
    }
}

void PropertyInspector::RenderObjectPtrVectorProperty(const CPropertyBase& property, CReflectedBase* object)
{
    std::vector<std::unique_ptr<CReflectedBase>>* objectVector = 
        reinterpret_cast<std::vector<std::unique_ptr<CReflectedBase>>*>(property.GetAddress(object));
    
    const std::string nodeId = GenerateTreeNodeId(property.GetName(), objectVector);
    bool expanded = ShouldExpandNode(nodeId);
    
    if (ImGui::TreeNodeEx(nodeId.c_str(), expanded ? ImGuiTreeNodeFlags_DefaultOpen : 0, "%s", property.GetName().c_str())) {
        UpdateExpandedState(nodeId, true);
        
        ImGui::SameLine();
        ImGui::TextColored(GetTypeColor(property.GetType()), "[Vector<Object*>]");
        
        ImGui::Text("Size: %zu", objectVector->size());
        
        for (size_t i = 0; i < objectVector->size(); ++i) {
            const std::string elementName = "[" + std::to_string(i) + "]";
            const std::string elementNodeId = GenerateTreeNodeId(elementName, (*objectVector)[i].get());
            bool elementExpanded = ShouldExpandNode(elementNodeId);
            
            if (ImGui::TreeNodeEx(elementNodeId.c_str(), elementExpanded ? ImGuiTreeNodeFlags_DefaultOpen : 0, "%s", elementName.c_str())) {
                UpdateExpandedState(elementNodeId, true);
                
                if ((*objectVector)[i]) {
                    ImGui::Text("Type: %s", (*objectVector)[i]->GetRflClassName() ? (*objectVector)[i]->GetRflClassName() : "Unknown");
                    ImGui::Text("Address: %p", (*objectVector)[i].get());
                    RenderObjectProperties((*objectVector)[i].get(), elementName);
                } else {
                    RenderNullPointer("Vector element");
                }
                
                ImGui::TreePop();
            } else {
                UpdateExpandedState(elementNodeId, false);
                if (!(*objectVector)[i]) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.7f, 0.3f, 0.3f, 1.0f), "(null)");
                }
            }
        }
        
        ImGui::TreePop();
    } else {
        UpdateExpandedState(nodeId, false);
        ImGui::SameLine();
        ImGui::TextColored(GetTypeColor(property.GetType()), "[Vector<Object*>] (%zu)", objectVector->size());
    }
}

void PropertyInspector::RenderComponentProperty(const CPropertyBase& property, CReflectedBase* object)
{
    ComponentSystem::Component* component = reinterpret_cast<ComponentSystem::Component*>(property.GetAddress(object));
    
    const std::string nodeId = GenerateTreeNodeId(property.GetName(), component);
    bool expanded = ShouldExpandNode(nodeId);
    
    if (ImGui::TreeNodeEx(nodeId.c_str(), expanded ? ImGuiTreeNodeFlags_DefaultOpen : 0, "%s", property.GetName().c_str())) {
        UpdateExpandedState(nodeId, true);
        
        ImGui::SameLine();
        ImGui::TextColored(GetTypeColor(property.GetType()), "[Component]");
        
        if (component) {
            ImGui::Text("Type: %s", component->GetRflClassName() ? component->GetRflClassName() : "Unknown");
            ImGui::Text("ID: %u", component->GetId());
            ImGui::Text("Active: %s", component->IsActive() ? "true" : "false");
            ImGui::Text("Initialized: %s", component->IsInitialized() ? "true" : "false");
            RenderObjectProperties(component, property.GetName());
        } else {
            RenderNullPointer("Component");
        }
        
        ImGui::TreePop();
    } else {
        UpdateExpandedState(nodeId, false);
        ImGui::SameLine();
        ImGui::TextColored(GetTypeColor(property.GetType()), "[Component]");
    }
}

void PropertyInspector::RenderComponentPtrProperty(const CPropertyBase& property, CReflectedBase* object)
{
    ComponentSystem::Component** componentPtr = reinterpret_cast<ComponentSystem::Component**>(property.GetAddress(object));
    
    const std::string nodeId = GenerateTreeNodeId(property.GetName(), *componentPtr);
    bool expanded = ShouldExpandNode(nodeId);
    
    if (ImGui::TreeNodeEx(nodeId.c_str(), expanded ? ImGuiTreeNodeFlags_DefaultOpen : 0, "%s", property.GetName().c_str())) {
        UpdateExpandedState(nodeId, true);
        
        ImGui::SameLine();
        ImGui::TextColored(GetTypeColor(property.GetType()), "[Component*]");
        
        if (*componentPtr) {
            ImGui::Text("Type: %s", (*componentPtr)->GetRflClassName() ? (*componentPtr)->GetRflClassName() : "Unknown");
            ImGui::Text("ID: %u", (*componentPtr)->GetId());
            ImGui::Text("Active: %s", (*componentPtr)->IsActive() ? "true" : "false");
            ImGui::Text("Address: %p", *componentPtr);
            RenderObjectProperties(*componentPtr, property.GetName());
        } else {
            RenderNullPointer("Component pointer");
        }
        
        ImGui::TreePop();
    } else {
        UpdateExpandedState(nodeId, false);
        ImGui::SameLine();
        ImGui::TextColored(GetTypeColor(property.GetType()), "[Component*]");
        if (!*componentPtr) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.7f, 0.3f, 0.3f, 1.0f), "(null)");
        }
    }
}

void PropertyInspector::RenderComponentPtrVectorProperty(const CPropertyBase& property, CReflectedBase* object)
{
    std::vector<ComponentSystem::Component*>* componentVector = 
        reinterpret_cast<std::vector<ComponentSystem::Component*>*>(property.GetAddress(object));
    
    const std::string nodeId = GenerateTreeNodeId(property.GetName(), componentVector);
    bool expanded = ShouldExpandNode(nodeId);
    
    if (ImGui::TreeNodeEx(nodeId.c_str(), expanded ? ImGuiTreeNodeFlags_DefaultOpen : 0, "%s", property.GetName().c_str())) {
        UpdateExpandedState(nodeId, true);
        
        ImGui::SameLine();
        ImGui::TextColored(GetTypeColor(property.GetType()), "[Vector<Component*>]");
        
        ImGui::Text("Size: %zu", componentVector->size());
        
        for (size_t i = 0; i < componentVector->size(); ++i) {
            const std::string elementName = "[" + std::to_string(i) + "]";
            const std::string elementNodeId = GenerateTreeNodeId(elementName, (*componentVector)[i]);
            bool elementExpanded = ShouldExpandNode(elementNodeId);
            
            if (ImGui::TreeNodeEx(elementNodeId.c_str(), elementExpanded ? ImGuiTreeNodeFlags_DefaultOpen : 0, "%s", elementName.c_str())) {
                UpdateExpandedState(elementNodeId, true);
                
                if ((*componentVector)[i]) {
                    ComponentSystem::Component* comp = (*componentVector)[i];
                    ImGui::Text("Type: %s", comp->GetRflClassName() ? comp->GetRflClassName() : "Unknown");
                    ImGui::Text("ID: %u", comp->GetId());
                    ImGui::Text("Active: %s", comp->IsActive() ? "true" : "false");
                    ImGui::Text("Address: %p", comp);
                    RenderObjectProperties(comp, elementName);
                } else {
                    RenderNullPointer("Vector element");
                }
                
                ImGui::TreePop();
            } else {
                UpdateExpandedState(elementNodeId, false);
                if (!(*componentVector)[i]) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.7f, 0.3f, 0.3f, 1.0f), "(null)");
                }
            }
        }
        
        ImGui::TreePop();
    } else {
        UpdateExpandedState(nodeId, false);
        ImGui::SameLine();
        ImGui::TextColored(GetTypeColor(property.GetType()), "[Vector<Component*>] (%zu)", componentVector->size());
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
    if (m_FirstRender && m_ExpandByDefault) {
        return true;
    }
    return m_ExpandedNodes.find(nodeId) != m_ExpandedNodes.end();
}

void PropertyInspector::UpdateExpandedState(const std::string& nodeId, bool expanded)
{
    if (expanded) {
        m_ExpandedNodes.insert(nodeId);
    } else {
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
        case RT_Object: return "Object";
        case RT_ObjectPtr: return "Object*";
        case RT_ObjectPtrVec: return "Vector<Object*>";
        case RT_Component: return "Component";
        case RT_ComponentPtr: return "Component*";
        case RT_ComponentPtrVec: return "Vector<Component*>";
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
            return COLOR_TYPE_VECTOR;
        case RT_Object:
            return COLOR_TYPE_OBJECT;
        case RT_ObjectPtr:
        case RT_ObjectPtrVec:
            return COLOR_TYPE_POINTER;
        case RT_Component:
        case RT_ComponentPtr:
        case RT_ComponentPtrVec:
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

} // namespace ImGuiVisualizers