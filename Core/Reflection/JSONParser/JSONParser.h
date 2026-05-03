#pragma once

#include <iostream>
#include <vector>
#include <sstream>
#include <memory>
#include <unordered_map>

#include "Reflection/Reflection.h"
#include "../external/rapidjson/include/prettywriter.h"
#include "../external/rapidjson/include/stringbuffer.h"
#include "../external/rapidjson/include/Document.h"
#include "CoreSystem/CoreSystem.h"
#include "Math/Vector3f.h"
#include "Math/Vector4f.h"
#include "Math/Matrix4f.h"

using namespace rapidjson;

#define JTAG_ELELMENTS  "Elements"
#define JTAG_TYPE       "Type"
#define JTAG_SIZE       "Size"
#define JTAG_VALUE      "Value"
#define JTAG_ROOT       "RootObject"

class CRFL_JSONParser : public IRFL_Parser
{
public:
	void ReadFloat(const CPropertyBase& property, CReflectedBase* obj)
	{
		ReflectionDebug.Assert(obj, "Null Ptr passed to function.\n");
		Value* valueMember = FindValue(property);
		if (valueMember)
		{
			float* value = reinterpret_cast<float*>(property.GetAddress(obj));
			*value = valueMember->GetFloat();
		}
	}
	void ReadInt(const CPropertyBase& property, CReflectedBase* obj)
	{
		Value* valueMember = FindValue(property);
		if (valueMember)
		{
			int* value = reinterpret_cast<int*>(property.GetAddress(obj));
			*value = valueMember->GetInt();
		}
	}
	void ReadString(const CPropertyBase& property, CReflectedBase* obj)
	{
		Value* valueMember = FindValue(property);
		if (valueMember)
		{
			std::string* value = reinterpret_cast<std::string*>(property.GetAddress(obj));
			*value = valueMember->GetString();
		}
	}
	void ReadMatrix4(const CPropertyBase& property, CReflectedBase* obj)
	{
		Value* valueMember = FindValue(property);
		if (valueMember && valueMember->IsArray())
		{
			Matrix4f* value = reinterpret_cast<Matrix4f*>(property.GetAddress(obj));
			auto& floatArray = value->GetWriteData();
			for (uint32_t i = 0; i < valueMember->GetArray().Size(); i++)
			{
				floatArray[i] = valueMember->GetArray()[i].GetFloat();
			}
		}
	}
	void ReadVector4(const CPropertyBase& property, CReflectedBase* obj)
	{
		Value* valueMember = FindValue(property);
		if (valueMember && valueMember->IsArray())
		{
			Vector4f* value = reinterpret_cast<Vector4f*>(property.GetAddress(obj));

			ReflectionDebug.Assert(valueMember->GetArray().Size() == 4, "Bad Vector4 Read");
			value->SetX(valueMember->GetArray()[0].GetFloat());
			value->SetY(valueMember->GetArray()[1].GetFloat());
			value->SetZ(valueMember->GetArray()[2].GetFloat());
			value->SetW(valueMember->GetArray()[3].GetFloat());
		}
	}
	void ReadVector3(const CPropertyBase& property, CReflectedBase* obj)
	{
		Value* valueMember = FindValue(property);
		if (valueMember && valueMember->IsArray())
		{
			Vector3f* value = reinterpret_cast<Vector3f*>(property.GetAddress(obj));

			ReflectionDebug.Assert(valueMember->GetArray().Size() == 3, "Bad Vector3 Read");
			value->SetX(valueMember->GetArray()[0].GetFloat());
			value->SetY(valueMember->GetArray()[1].GetFloat());
			value->SetZ(valueMember->GetArray()[2].GetFloat());
		}
	}
	void ReadObject(const CPropertyBase& property, CReflectedBase* obj)
	{
		Value* valueMember = FindValue(property);
		if (valueMember)
		{
			Value* member = &m_currentObject->FindMember(property.GetName().c_str())->value;
			Push(member);
			ScopedPop guard(*this);
			CReflectedBase* subObj = reinterpret_cast<CReflectedBase*>(property.GetAddress(obj));
			subObj->ReadMembers(*this);			
		}
	}

	// Refactored to use unique_ptr
	void ReadObjectPtr(const CPropertyBase& property, CReflectedBase* obj)
	{
		Value* valueMember = FindValue(property);
		if (valueMember)
		{
			Value* member = &m_currentObject->FindMember(property.GetName().c_str())->value;
			Push(member);
			ScopedPop guard(*this);
			std::unique_ptr<CReflectedBase>* subObjPtr =
				reinterpret_cast<std::unique_ptr<CReflectedBase>*>(property.GetAddress(obj));

			auto subObj = CreateObjectAsUniquePtr(valueMember->GetString());
			if (subObj) {
				subObj->ReadMembers(*this);
				*subObjPtr = std::move(subObj);
			}
		}
	}

	// Refactored to use vector of unique_ptr
	void ReadObjectPtrArray(const CPropertyBase& property, CReflectedBase* obj)
	{
		Value* member = &m_currentObject->FindMember(property.GetName().c_str())->value;
		Value* valueMember = FindValue(property);
		if (valueMember)
		{
			std::vector<std::unique_ptr<CReflectedBase>>* vectorOfObject =
				reinterpret_cast<std::vector<std::unique_ptr<CReflectedBase>>*>(property.GetAddress(obj));

			vectorOfObject->clear(); // Clear existing content

			Value* arrayMember = &member->FindMember(JTAG_ELELMENTS)->value;
			if (arrayMember->IsArray())
			{
				vectorOfObject->reserve(arrayMember->GetArray().Size());

				for (auto& element : arrayMember->GetArray())
					{
						Push(&element);
						ScopedPop elementGuard(*this);
						Value* valueMember = &element.FindMember(JTAG_VALUE)->value;
						auto subObj = CreateObjectAsUniquePtr(valueMember->GetString());
						if (subObj) {
							subObj->ReadMembers(*this);
							vectorOfObject->push_back(std::move(subObj));
						}
					}
			}
		}
	}
	void WriteFloat(const CPropertyBase& property, CReflectedBase* obj)
	{
		BeginObject();
		float* value = reinterpret_cast<float*>(property.GetAddress(obj));
		Value name;
		name.SetString(JTAG_VALUE, m_allocator);
		m_currentObject->AddMember(name, *value, m_allocator);
		WriteProperties(property);
		EndObject(property.GetName().c_str());
	}
	void WriteInt(const CPropertyBase& property, CReflectedBase* obj)
	{
		BeginObject();
		int* value = reinterpret_cast<int*>(property.GetAddress(obj));
		Value name;
		name.SetString(JTAG_VALUE, m_allocator);
		m_currentObject->AddMember(name, *value, m_allocator);
		WriteProperties(property);
		EndObject(property.GetName().c_str());
	}
	void WriteString(const CPropertyBase& property, CReflectedBase* obj)
	{
		BeginObject();
		std::string* string = reinterpret_cast<std::string*>(property.GetAddress(obj));
		Value name;
		name.SetString(JTAG_VALUE, m_allocator);
		Value item;
		item.SetString(string->c_str(), m_allocator);
		m_currentObject->AddMember(name, item, m_allocator);
		WriteProperties(property);
		EndObject(property.GetName().c_str());
	}
	void WriteMatrix4(const CPropertyBase& property, CReflectedBase* obj)
	{
		BeginObject();
		Matrix4f* matrix4 = reinterpret_cast<Matrix4f*>(property.GetAddress(obj));
		Value array;
		array.SetArray();
		for (auto& element : matrix4->GetData())
		{
			array.PushBack(Value(element).Move(), m_allocator);
		}
		Value name;
		name.SetString(JTAG_VALUE, m_allocator);
		m_currentObject->AddMember(name, array, m_allocator);
		WriteProperties(property);
		EndObject(property.GetName().c_str());
	}
	void WriteVector4(const CPropertyBase& property, CReflectedBase* obj)
	{
		BeginObject();
		Vector4f* vector4 = reinterpret_cast<Vector4f*>(property.GetAddress(obj));
		Value array;
		array.SetArray();

		array.PushBack(vector4->getX(), m_allocator);
		array.PushBack(vector4->getY(), m_allocator);
		array.PushBack(vector4->getZ(), m_allocator);
		array.PushBack(vector4->getW(), m_allocator);
		Value name;
		name.SetString(JTAG_VALUE, m_allocator);
		m_currentObject->AddMember(name, array, m_allocator);
		WriteProperties(property);
		EndObject(property.GetName().c_str());
	}
	void WriteVector3(const CPropertyBase& property, CReflectedBase* obj)
	{
		BeginObject();
		Vector3f* vector3 = reinterpret_cast<Vector3f*>(property.GetAddress(obj));
		Value array;
		array.SetArray();

		array.PushBack(vector3->getX(), m_allocator);
		array.PushBack(vector3->getY(), m_allocator);
		array.PushBack(vector3->getZ(), m_allocator);
		Value name;
		name.SetString(JTAG_VALUE, m_allocator);
		m_currentObject->AddMember(name, array, m_allocator);
		WriteProperties(property);
		EndObject(property.GetName().c_str());
	}
	void WriteObject(const CPropertyBase& property, CReflectedBase* obj)
	{
		CReflectedBase* subObj = reinterpret_cast<CReflectedBase*>(property.GetAddress(obj));
		BeginObject(subObj->GetRflClassName());
		subObj->WriteMembers(*this);
		WriteProperties(property);
		EndObject(property.GetName().c_str());
	}

	// Refactored to use unique_ptr
	void WriteObjectPtr(const CPropertyBase& property, CReflectedBase* obj)
	{
		std::unique_ptr<CReflectedBase>* subObjPtr =
			reinterpret_cast<std::unique_ptr<CReflectedBase>*>(property.GetAddress(obj));

		if (*subObjPtr) {
			CReflectedBase* subObj = subObjPtr->get();
			BeginObject(subObj->GetRflClassName());
			subObj->WriteMembers(*this);
			WriteProperties(property);
			EndObject(property.GetName().c_str());
		}
		else {
			// Handle null pointer case
			BeginObject("");
			WriteProperties(property);
			EndObject(property.GetName().c_str());
		}
	}

	// Refactored to use vector of unique_ptr
	void WriteObjectPtrArray(const CPropertyBase& property, CReflectedBase* obj)
	{
		std::vector<std::unique_ptr<CReflectedBase>>* objVector =
			reinterpret_cast<std::vector<std::unique_ptr<CReflectedBase>>*>(property.GetAddress(obj));

		BeginObject(obj->GetRflClassName());
		BeginArray();
		for (const auto& element : *objVector)
		{
			if (element) {
				BeginObject(element->GetRflClassName());
				element->WriteMembers(*this);
				EndObject(property.GetName().c_str());
			}
			else {
				// Handle null pointer case in array
				BeginObject("");
				EndObject(property.GetName().c_str());
			}
		}
		EndArray();
		WriteProperties(property);
		EndObject(property.GetName().c_str());
	}

	// --- Primitive vector support ---
	void ReadIntVector(const CPropertyBase& property, CReflectedBase* obj)
	{
		Value* valueMember = FindValue(property);
		if (valueMember && valueMember->IsArray())
		{
			std::vector<int>* vec = reinterpret_cast<std::vector<int>*>(property.GetAddress(obj));
			vec->clear();
			vec->reserve(valueMember->GetArray().Size());
			for (auto& element : valueMember->GetArray())
			{
				vec->push_back(element.GetInt());
			}
		}
	}
	void WriteIntVector(const CPropertyBase& property, CReflectedBase* obj)
	{
		BeginObject();
		std::vector<int>* vec = reinterpret_cast<std::vector<int>*>(property.GetAddress(obj));
		Value array;
		array.SetArray();
		for (auto& element : *vec)
		{
			array.PushBack(element, m_allocator);
		}
		Value name;
		name.SetString(JTAG_VALUE, m_allocator);
		m_currentObject->AddMember(name, array, m_allocator);
		WriteProperties(property);
		EndObject(property.GetName().c_str());
	}
	void ReadFloatVector(const CPropertyBase& property, CReflectedBase* obj)
	{
		Value* valueMember = FindValue(property);
		if (valueMember && valueMember->IsArray())
		{
			std::vector<float>* vec = reinterpret_cast<std::vector<float>*>(property.GetAddress(obj));
			vec->clear();
			vec->reserve(valueMember->GetArray().Size());
			for (auto& element : valueMember->GetArray())
			{
				vec->push_back(element.GetFloat());
			}
		}
	}
	void WriteFloatVector(const CPropertyBase& property, CReflectedBase* obj)
	{
		BeginObject();
		std::vector<float>* vec = reinterpret_cast<std::vector<float>*>(property.GetAddress(obj));
		Value array;
		array.SetArray();
		for (auto& element : *vec)
		{
			array.PushBack(element, m_allocator);
		}
		Value name;
		name.SetString(JTAG_VALUE, m_allocator);
		m_currentObject->AddMember(name, array, m_allocator);
		WriteProperties(property);
		EndObject(property.GetName().c_str());
	}
	void ReadBoolVector(const CPropertyBase& property, CReflectedBase* obj)
	{
		Value* valueMember = FindValue(property);
		if (valueMember && valueMember->IsArray())
		{
			std::vector<bool>* vec = reinterpret_cast<std::vector<bool>*>(property.GetAddress(obj));
			vec->clear();
			vec->reserve(valueMember->GetArray().Size());
			for (auto& element : valueMember->GetArray())
			{
				vec->push_back(element.GetBool());
			}
		}
	}
	void WriteBoolVector(const CPropertyBase& property, CReflectedBase* obj)
	{
		BeginObject();
		std::vector<bool>* vec = reinterpret_cast<std::vector<bool>*>(property.GetAddress(obj));
		Value array;
		array.SetArray();
		for (size_t i = 0; i < vec->size(); i++)
		{
			array.PushBack((*vec)[i] ? true : false, m_allocator);
		}
		Value name;
		name.SetString(JTAG_VALUE, m_allocator);
		m_currentObject->AddMember(name, array, m_allocator);
		WriteProperties(property);
		EndObject(property.GetName().c_str());
	}
	void ReadStringVector(const CPropertyBase& property, CReflectedBase* obj)
	{
		Value* valueMember = FindValue(property);
		if (valueMember && valueMember->IsArray())
		{
			std::vector<std::string>* vec = reinterpret_cast<std::vector<std::string>*>(property.GetAddress(obj));
			vec->clear();
			vec->reserve(valueMember->GetArray().Size());
			for (auto& element : valueMember->GetArray())
			{
				vec->push_back(element.GetString());
			}
		}
	}
	void WriteStringVector(const CPropertyBase& property, CReflectedBase* obj)
	{
		BeginObject();
		std::vector<std::string>* vec = reinterpret_cast<std::vector<std::string>*>(property.GetAddress(obj));
		Value array;
		array.SetArray();
		for (auto& element : *vec)
		{
			Value item;
			item.SetString(element.c_str(), m_allocator);
			array.PushBack(item, m_allocator);
		}
		Value name;
		name.SetString(JTAG_VALUE, m_allocator);
		m_currentObject->AddMember(name, array, m_allocator);
		WriteProperties(property);
		EndObject(property.GetName().c_str());
	}

	// Component-specific methods - handle components using ComponentManager
	void ReadComponent(const CPropertyBase& property, CReflectedBase* obj)
	{
		Value* valueMember = FindValue(property);
		if (valueMember)
		{
			Value* member = &m_currentObject->FindMember(property.GetName().c_str())->value;
			Push(member);
			ScopedPop guard(*this);
			ComponentSystem::Component* subObj = reinterpret_cast<ComponentSystem::Component*>(property.GetAddress(obj));
			subObj->ReadMembers(*this);
		}
	}

	void ReadComponentPtr(const CPropertyBase& property, CReflectedBase* obj)
	{
		Value* valueMember = FindValue(property);
		if (valueMember)
		{
			Value* member = &m_currentObject->FindMember(property.GetName().c_str())->value;
			Push(member);
			ScopedPop guard(*this);
			std::unique_ptr<ComponentSystem::Component>* subObjPtr =
				reinterpret_cast<std::unique_ptr<ComponentSystem::Component>*>(property.GetAddress(obj));

			auto subObj = CreateComponentAsUniquePtr(valueMember->GetString());
			if (subObj) {
				subObj->ReadMembers(*this);
				*subObjPtr = std::move(subObj);
			}
		}
	}

	void ReadComponentPtrArray(const CPropertyBase& property, CReflectedBase* obj)
	{
		Value* member = &m_currentObject->FindMember(property.GetName().c_str())->value;

		Value* valueMember = FindValue(property);
		if (valueMember)
		{
			std::vector<std::unique_ptr<ComponentSystem::Component>>* vectorOfComponent =
				reinterpret_cast<std::vector<std::unique_ptr<ComponentSystem::Component>>*>(property.GetAddress(obj));

			vectorOfComponent->clear(); // Clear existing content

			Value* arrayMember = &member->FindMember(JTAG_ELELMENTS)->value;
			if (arrayMember->IsArray())
			{
				vectorOfComponent->reserve(arrayMember->GetArray().Size());

				for (auto& element : arrayMember->GetArray())
					{
						Push(&element);
						ScopedPop elementGuard(*this);
						Value* valueMember = &element.FindMember(JTAG_VALUE)->value;
						auto subObj = CreateComponentAsUniquePtr(valueMember->GetString());
						if (subObj) {
							subObj->ReadMembers(*this);
							vectorOfComponent->push_back(std::move(subObj));
						}
					}
			}
		}
	}

	void WriteComponent(const CPropertyBase& property, CReflectedBase* obj)
	{
		ComponentSystem::Component* subObj = reinterpret_cast<ComponentSystem::Component*>(property.GetAddress(obj));
		BeginObject(subObj->GetRflClassName());
		subObj->WriteMembers(*this);
		WriteProperties(property);
		EndObject(property.GetName().c_str());
	}

	void WriteComponentPtr(const CPropertyBase& property, CReflectedBase* obj)
	{
		std::unique_ptr<ComponentSystem::Component>* subObjPtr =
			reinterpret_cast<std::unique_ptr<ComponentSystem::Component>*>(property.GetAddress(obj));

		if (*subObjPtr) {
			ComponentSystem::Component* subObj = subObjPtr->get();
			BeginObject(subObj->GetRflClassName());
			subObj->WriteMembers(*this);
			WriteProperties(property);
			EndObject(property.GetName().c_str());
		}
		else {
			// Handle null pointer case
			BeginObject("");
			WriteProperties(property);
			EndObject(property.GetName().c_str());
		}
	}

	void WriteComponentPtrArray(const CPropertyBase& property, CReflectedBase* obj)
	{
		std::vector<std::unique_ptr<ComponentSystem::Component>>* componentVector =
			reinterpret_cast<std::vector<std::unique_ptr<ComponentSystem::Component>>*>(property.GetAddress(obj));

		BeginObject(obj->GetRflClassName());
		BeginArray();
		for (const auto& element : *componentVector)
		{
			if (element) {
				BeginObject(element->GetRflClassName());
				element->WriteMembers(*this);
				EndObject(property.GetName().c_str());
			}
			else {
				// Handle null pointer case in array
				BeginObject("");
				EndObject(property.GetName().c_str());
			}
		}
		EndArray();
		WriteProperties(property);
		EndObject(property.GetName().c_str());
	}

	void ReadComponentRawPtrArray(const CPropertyBase& property, CReflectedBase* obj)
	{
		Value* member = &m_currentObject->FindMember(property.GetName().c_str())->value;

		Value* valueMember = FindValue(property);
		if (valueMember)
		{
			std::vector<ComponentSystem::Component*>* vectorOfComponent =
				reinterpret_cast<std::vector<ComponentSystem::Component*>*>(property.GetAddress(obj));

			vectorOfComponent->clear(); // Clear existing content

			Value* arrayMember = &member->FindMember(JTAG_ELELMENTS)->value;
			if (arrayMember->IsArray())
			{
				ReflectionDebug.print("ReadComponentRawPtrArray: Found array with " + std::to_string(arrayMember->GetArray().Size()) + " elements\n");
				vectorOfComponent->reserve(arrayMember->GetArray().Size());

				ComponentSystem::Component* parentComponent = dynamic_cast<ComponentSystem::Component*>(obj);
				ReflectionDebug.print("ReadComponentRawPtrArray: Parent component is " + std::string(parentComponent ? "valid" : "null") + "\n");

				for (auto& element : arrayMember->GetArray())
				{
					Push(&element);
					ScopedPop elementGuard(*this);
					Value* valueMember = &element.FindMember(JTAG_VALUE)->value;

					std::string className = valueMember->GetString();
					ReflectionDebug.print("ReadComponentRawPtrArray: Creating component of type: " + className + "\n");

					// Create component through ClassFactory
					CReflectedBase* newObject = ClassFactory::CreateObject(className.c_str());
					if (newObject) {
						// Ensure it's actually a Component
						ComponentSystem::Component* childComponent = dynamic_cast<ComponentSystem::Component*>(newObject);
						if (childComponent) {
							ReflectionDebug.print("ReadComponentRawPtrArray: Component created successfully\n");
							childComponent->ReadMembers(*this);

							// If the parent is a Component, use AddChild to properly set up the relationship
							if (parentComponent) {
								parentComponent->AddChild(childComponent);
								ReflectionDebug.print("ReadComponentRawPtrArray: Added child to parent via AddChild\n");
							}
							else {
								// Fallback: just add to the vector if not a Component parent
								vectorOfComponent->push_back(childComponent);
								ReflectionDebug.print("ReadComponentRawPtrArray: Added component directly to vector\n");
							}
						}
						else {
							ReflectionDebug.print("ReadComponentRawPtrArray: Created object is not a Component! Deleting.\n");
							delete newObject;
						}
					}
					else {
							ReflectionDebug.print("ReadComponentRawPtrArray: FAILED to create component of type: " + className + "\n");
							}
						}
			}
			else {
				ReflectionDebug.print("ReadComponentRawPtrArray: arrayMember is not an array!\n");
			}
		}
		else {
			ReflectionDebug.print("ReadComponentRawPtrArray: valueMember is NULL for property: " + property.GetName() + "\n");
		}
	}

	void WriteComponentRawPtrArray(const CPropertyBase& property, CReflectedBase* obj)
	{
		std::vector<ComponentSystem::Component*>* componentVector =
			reinterpret_cast<std::vector<ComponentSystem::Component*>*>(property.GetAddress(obj));

		BeginObject(obj->GetRflClassName());
		BeginArray();
		for (const auto& element : *componentVector)
		{
			if (element) {
				BeginObject(element->GetRflClassName());
				element->WriteMembers(*this);
				EndObject(property.GetName().c_str());
			}
			else {
				// Handle null pointer case in array
				BeginObject("");
				EndObject(property.GetName().c_str());
			}
		}
		EndArray();
		WriteProperties(property);
		EndObject(property.GetName().c_str());
	}

	void ReadBool(const CPropertyBase& property, CReflectedBase* obj)
	{
		Value* valueMember = FindValue(property);
		if (valueMember && valueMember->IsBool())
		{
			bool* value = reinterpret_cast<bool*>(property.GetAddress(obj));
			*value = valueMember->GetBool();
		}
	}

	void WriteBool(const CPropertyBase& property, CReflectedBase* obj)
	{
		BeginObject();
		bool* value = reinterpret_cast<bool*>(property.GetAddress(obj));
		Value name;
		name.SetString(JTAG_VALUE, m_allocator);
		m_currentObject->AddMember(name, *value, m_allocator);
		WriteProperties(property);
		EndObject(property.GetName().c_str());
	}

	bool WriteProperties(const CPropertyBase& property)
	{
		Value name;
		Value memberType;
		memberType.SetInt((int)property.GetType());
		m_currentObject->AddMember(JTAG_TYPE, memberType, m_allocator);
		Value memberSize;
		memberSize.SetInt((int)property.GetSize());
		m_currentObject->AddMember(JTAG_SIZE, memberSize, m_allocator);
		return true;
	}

	void Push(Value* value)
	{
		m_objectStack.push_back(m_currentObject);
		m_currentObject = value;
	}
	void Pop()
	{
		m_currentObject = m_objectStack.back();
		m_objectStack.pop_back();
	}

	/// RAII guard that calls Pop() on destruction, ensuring Push/Pop balance even
	/// when an exception is thrown during a nested ReadMembers call.
	struct ScopedPop
	{
		CRFL_JSONParser& parser;
		explicit ScopedPop(CRFL_JSONParser& p) : parser(p) {}
		~ScopedPop() { parser.Pop(); }
		ScopedPop(const ScopedPop&) = delete;
		ScopedPop& operator=(const ScopedPop&) = delete;
	};
	void BeginArray()
	{
		Value* newValue = new Value;
		newValue->SetArray();
		Push(newValue);
	}
	void EndArray()
	{
		Value name;
		name.SetString(JTAG_ELELMENTS, m_allocator);
		Value* child = m_currentObject;
		Pop();
		m_currentObject->AddMember(name, *child, m_allocator);
		delete child;
	}
	void BeginObject(const char* className = "")
	{
		Value* newValue = new Value;
		newValue->SetObject();
		Push(newValue);
		if (std::string(className).size())
		{
			Value name;
			name.SetString(className, m_allocator);
			m_currentObject->AddMember(JTAG_VALUE, name, m_allocator);
		}
	}
	void EndObject(const char* memberName)
	{
		Value name;
		name.SetString(memberName, m_allocator);
		Value* child = m_currentObject;
		Pop();
		if (m_currentObject->IsArray())
			m_currentObject->PushBack(*child, m_allocator);
		else
			m_currentObject->AddMember(name, *child, m_allocator);
		delete child;
	}
	bool BeginOutput(const std::string& fileName)
	{
		m_currentObject = &m_document.SetObject();
		BeginObject();

		// Use FileSystemManager instead of direct file operations
		auto* fileSystemManager = Core::CoreSystem::GetFileSystemManager();
		if (!fileSystemManager) {
			ReflectionDebug.print("FileSystemManager not available\n");
			return false;
		}

		auto fileResult = fileSystemManager->CreateFile(fileName);
		if (fileResult.HasError()) {
			ReflectionDebug.print("Failed to create file: " + fileResult.GetError() + "\n");
			return false;
		}

		m_outputFile = std::move(fileResult.GetValue());
		return m_outputFile && m_outputFile->IsOpen();
	}
	void EndOutput()
	{
		EndObject(JTAG_ROOT);
		StringBuffer buffer;
		PrettyWriter<StringBuffer> writer(buffer);
		m_document.Accept(writer);

		if (m_outputFile && m_outputFile->IsOpen()) {
			std::string jsonString = buffer.GetString();
			jsonString += "\n"; // Add newline
			auto result = m_outputFile->WriteAllText(jsonString);
			if (result.HasError()) {
				ReflectionDebug.print("Failed to write JSON: " + result.GetError() + "\n");
			}
			m_outputFile->Close();
		}
		m_outputFile.reset();
	}
	bool BeginInput(const std::string& fileName)
	{
		// Use FileSystemManager instead of direct file operations
		auto* fileSystemManager = Core::CoreSystem::GetFileSystemManager();
		if (!fileSystemManager) {
			ReflectionDebug.print("FileSystemManager not available\n");
			return false;
		}

		auto fileResult = fileSystemManager->OpenFile(fileName, FileSystem::FileMode::Read);
		if (fileResult.HasError()) {
			ReflectionDebug.print("Failed to open file: " + fileResult.GetError() + "\n");
			return false;
		}

		m_inputFile = std::move(fileResult.GetValue());
		if (!m_inputFile || !m_inputFile->IsOpen()) {
			return false;
		}

		auto textResult = m_inputFile->ReadAllText();
		if (textResult.HasError()) {
			ReflectionDebug.print("Failed to read file: " + textResult.GetError() + "\n");
			return false;
		}

		m_document.Parse(textResult.GetValue().c_str());
		if (!m_document.HasParseError())
		{
			auto it = m_document.FindMember(JTAG_ROOT);
			if (it == m_document.MemberEnd()) {
				ReflectionDebug.print("BeginInput: missing RootObject\n");
				return false;
			}
			m_currentObject = &it->value;
			return true;
		}

		ReflectionDebug.print("JSON parse error\n");
		return false;
	}
	void EndInput()
	{
		if (m_inputFile) {
			m_inputFile->Close();
			m_inputFile.reset();
		}
	}
	
	// safer ValidateProperties
	bool ValidateProperties(const CPropertyBase& property, Value* objectValue)
	{
		if (!objectValue || !objectValue->IsObject())
			return false;

		auto sizeIt = objectValue->FindMember(JTAG_SIZE);
		if (sizeIt == objectValue->MemberEnd()) return false;
		//if (property.GetSize() != sizeIt->value.GetInt()) return false;

		auto typeIt = objectValue->FindMember(JTAG_TYPE);
		if (typeIt == objectValue->MemberEnd()) return false;
		if (property.GetType() != typeIt->value.GetInt()) return false;

		return true;
	}

	Value* FindValue(const CPropertyBase& property)
	{
		if (!m_currentObject) return nullptr;

		auto memberIt = m_currentObject->FindMember(property.GetName().c_str());
		if (memberIt == m_currentObject->MemberEnd())
			return nullptr;

		Value* member = &memberIt->value;
		if (!ValidateProperties(property, member))
			return nullptr;

		auto valueIt = member->FindMember(JTAG_VALUE);
		if (valueIt == member->MemberEnd())
			return nullptr;

		return &valueIt->value;
	}

	void ReadStringBoolMap(const CPropertyBase& property, CReflectedBase* obj)
	{
		Value* valueMember = FindValue(property);
		if (valueMember && valueMember->IsObject())
		{
			std::unordered_map<std::string, bool>* map =
				reinterpret_cast<std::unordered_map<std::string, bool>*>(property.GetAddress(obj));
			map->clear();
			for (auto it = valueMember->MemberBegin(); it != valueMember->MemberEnd(); ++it)
			{
				map->emplace(it->name.GetString(), it->value.GetBool());
			}
		}
	}
	void WriteStringBoolMap(const CPropertyBase& property, CReflectedBase* obj)
	{
		BeginObject();
		std::unordered_map<std::string, bool>* map =
			reinterpret_cast<std::unordered_map<std::string, bool>*>(property.GetAddress(obj));
		Value mapObj;
		mapObj.SetObject();
		for (auto& [key, val] : *map)
		{
			Value keyName;
			keyName.SetString(key.c_str(), m_allocator);
			mapObj.AddMember(keyName, val, m_allocator);
		}
		Value name;
		name.SetString(JTAG_VALUE, m_allocator);
		m_currentObject->AddMember(name, mapObj, m_allocator);
		WriteProperties(property);
		EndObject(property.GetName().c_str());
	}
protected:
	Document m_document;
	Value* m_currentObject;
	Document::AllocatorType& m_allocator = m_document.GetAllocator();

private:
	std::unique_ptr<FileSystem::IFile> m_outputFile;
	std::unique_ptr<FileSystem::IFile> m_inputFile;
	std::vector<Value*> m_objectStack;
	Value& m_objectArray = m_document.SetArray();
	Writer<StringBuffer>* m_writer;
	StringBuffer m_stringBuffer;

	// Helper function to create components using ComponentManager
	static std::unique_ptr<ComponentSystem::Component> CreateComponentAsUniquePtr(const char* className)
	{
		auto* componentManager = Core::CoreSystem::GetComponentManager();
		if (!componentManager) {
			ReflectionDebug.print("ComponentManager not available\n");
			return nullptr;
		}
		ComponentSystem::Component* component = componentManager->CreateComponentByName(className);
		if (component) {
			return std::unique_ptr<ComponentSystem::Component>(component);
		}
		ReflectionDebug.print("Failed to create component: " + std::string(className) + "\n");
		return nullptr;
	}
};