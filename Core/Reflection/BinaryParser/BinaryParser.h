#pragma once

#include <iostream>
#include <vector>
#include <memory>
#include "Reflection/Reflection.h"
#include "CoreSystem/CoreSystem.h"

class CRFL_BinaryParser : public IRFL_Parser
{
public:

	void ReadFloat(const CPropertyBase& property, CReflectedBase* obj)
	{
		if (ValidateProperties(property))
			ReadRaw(property, obj);
	}
	void ReadInt(const CPropertyBase& property, CReflectedBase* obj)
	{
		if (ValidateProperties(property))
			ReadRaw(property, obj);
	}
	void ReadString(const CPropertyBase& property, CReflectedBase* obj)
	{
		if (ValidateProperties(property))
		{
			std::string* string = reinterpret_cast<std::string*>(property.GetAddress(obj));
			ReadString(*string);
		}
	}
	void ReadMatrix4(const CPropertyBase& property, CReflectedBase* obj)
	{
		if (ValidateProperties(property))
			ReadRaw(property, obj);
	}
	void ReadVector4(const CPropertyBase& property, CReflectedBase* obj)
	{
		if (ValidateProperties(property))
			ReadRaw(property, obj);
	}
	void ReadVector3(const CPropertyBase& property, CReflectedBase* obj)
	{
		if (ValidateProperties(property))
			ReadRaw(property, obj);
	}
	void ReadObject(const CPropertyBase& property, CReflectedBase* obj)
	{
		if (ValidateProperties(property))
		{
			CReflectedBase* subObj = reinterpret_cast<CReflectedBase*>(property.GetAddress(obj));
			subObj->ReadMembers(*this);
		}
	}
	void ReadObjectPtr(const CPropertyBase& property, CReflectedBase* obj)
	{
		if (ValidateProperties(property))
		{
			std::string className;
			ReadString(className);
			std::unique_ptr<CReflectedBase>* subObjPtr =
				reinterpret_cast<std::unique_ptr<CReflectedBase>*>(property.GetAddress(obj));
			auto subObj = CreateObjectAsUniquePtr(className.c_str());
			if (subObj) {
				subObj->ReadMembers(*this);
				*subObjPtr = std::move(subObj);
			}
		}
	}
	void ReadObjectPtrArray(const CPropertyBase& property, CReflectedBase* obj)
	{
		if (ValidateProperties(property))
		{
			int arraySize;
			ReadInt(arraySize);
			std::vector<std::unique_ptr<CReflectedBase>>* objVector =
				reinterpret_cast<std::vector<std::unique_ptr<CReflectedBase>>*>(property.GetAddress(obj));
			objVector->clear();
			objVector->reserve(arraySize);
			for (int i = 0; i < arraySize; i++)
			{
				std::string className;
				ReadString(className);
				auto subObj = CreateObjectAsUniquePtr(className.c_str());
				if (subObj) {
					subObj->ReadMembers(*this);
					objVector->push_back(std::move(subObj));
				}
			}
		}
	}
	void WriteFloat(const CPropertyBase& property, CReflectedBase* obj)
	{
		WriteProperties(property);
		WriteRaw(property, obj);
	}
	void WriteInt(const CPropertyBase& property, CReflectedBase* obj)
	{
		WriteProperties(property);
		WriteRaw(property, obj);
	}
	void WriteString(const CPropertyBase& property, CReflectedBase* obj)
	{
		WriteProperties(property);
		std::string* string = reinterpret_cast<std::string*>(property.GetAddress(obj));
		WriteString(*string);
	}
	void WriteMatrix4(const CPropertyBase& property, CReflectedBase* obj)
	{
		WriteProperties(property);
		WriteRaw(property, obj);
	}
	void WriteVector4(const CPropertyBase& property, CReflectedBase* obj)
	{
		WriteProperties(property);
		WriteRaw(property, obj);
	}
	void WriteVector3(const CPropertyBase& property, CReflectedBase* obj)
	{
		WriteProperties(property);
		WriteRaw(property, obj);
	}
	void WriteObject(const CPropertyBase& property, CReflectedBase* obj)
	{
		WriteProperties(property);
		CReflectedBase* subObj = reinterpret_cast<CReflectedBase*>(property.GetAddress(obj));
		subObj->WriteMembers(*this);
	}
	void WriteObjectPtr(const CPropertyBase& property, CReflectedBase* obj)
	{
		WriteProperties(property);
		std::unique_ptr<CReflectedBase>* subObjPtr =
			reinterpret_cast<std::unique_ptr<CReflectedBase>*>(property.GetAddress(obj));
		if (*subObjPtr) {
			CReflectedBase* subObj = subObjPtr->get();
			std::string className = subObj->GetRflClassName();
			WriteString(className);
			subObj->WriteMembers(*this);
		}
		else {
			WriteString(std::string(""));
		}
	}
	void WriteObjectPtrArray(const CPropertyBase& property, CReflectedBase* obj)
	{
		WriteProperties(property);
		std::vector<std::unique_ptr<CReflectedBase>>* objVector =
			reinterpret_cast<std::vector<std::unique_ptr<CReflectedBase>>*>(property.GetAddress(obj));
		WriteInt(static_cast<int>(objVector->size()));
		for (const auto& element : *objVector)
		{
			if (element) {
				std::string className = element->GetRflClassName();
				WriteString(className);
				element->WriteMembers(*this);
			}
			else {
				WriteString(std::string(""));
			}
		}
	}

	bool BeginOutput(const std::string& fileName)
	{
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
		m_writePosition = 0;
		return m_outputFile && m_outputFile->IsOpen();
	}
	void EndOutput()
	{
		if (m_outputFile) {
			m_outputFile->Flush();
			m_outputFile->Close();
			m_outputFile.reset();
		}
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
		m_readPosition = 0;
		return m_inputFile && m_inputFile->IsOpen();
	}
	void EndInput()
	{
		if (m_inputFile) {
			m_inputFile->Close();
			m_inputFile.reset();
		}
	}
	void BeginObject(const char* className = "") {}
	void EndObject(const char* memberName) {}
	void BeginArray() {}
	void EndArray() {}

	bool ReadString(std::string& theString)
	{
		size_t length;
		if (!Read(&length, sizeof(size_t))) {
			return false;
		}

		if (length > 0 && length < 65536) // Reasonable size limit
		{
			std::vector<char> tempString(length);
			if (Read(&tempString[0], length)) {
				theString.assign(tempString.data(), length - 1); // Exclude null terminator
				return true;
			}
		}
		return false;
	}

	virtual bool Write(const void* data, size_t size)
	{
		if (!m_outputFile || !m_outputFile->IsOpen()) {
			return false;
		}

		auto result = m_outputFile->Write(data, size);
		if (result.IsSuccess()) {
			m_writePosition += size;
			return true;
		}

		ReflectionDebug.print("Write failed: " + result.GetError() + "\n");
		return false;
	}

	virtual bool Read(void* data, size_t size)
	{
		if (!m_inputFile || !m_inputFile->IsOpen()) {
			return false;
		}

		auto result = m_inputFile->Read(data, size);
		if (result.IsSuccess() && result.GetValue() == size) {
			m_readPosition += size;
			return true;
		}

		if (result.HasError()) {
			ReflectionDebug.print("Read failed: " + result.GetError() + "\n");
		}
		else {
			ReflectionDebug.print("Read incomplete: expected " + std::to_string(size) +
				" bytes, got " + std::to_string(result.GetValue()) + "\n");
		}
		return false;
	}

	bool WriteProperties(const CPropertyBase& property)
	{
		size_t size = property.GetSize();
		if (!Write(&size, sizeof(size_t))) return false;
		RFL_Type type = property.GetType();
		if (!Write(&type, sizeof(RFL_Type))) return false;
		return true;
	}

	bool ValidateProperties(const CPropertyBase& property)
	{
		size_t size;
		if (!Read(&size, sizeof(size_t))) return false;
		RFL_Type type;
		if (!Read(&type, sizeof(RFL_Type))) return false;

		if (size != property.GetSize())
		{
			ReflectionDebug.print("Bad Type Size\n");
			return false;
		}
		if (type != property.GetType())
		{
			ReflectionDebug.print("Bad Type\n");
			return false;
		}
		return true;
	}

	bool WriteRaw(const CPropertyBase& property, CReflectedBase* obj)
	{
		char* address = reinterpret_cast<char*>(property.GetAddress(obj));
		return Write(address, property.GetSize());
	}

	bool ReadRaw(const CPropertyBase& property, CReflectedBase* obj)
	{
		char* address = reinterpret_cast<char*>(property.GetAddress(obj));
		return Read(address, property.GetSize());
	}

	bool WriteString(const std::string theString)
	{
		size_t length = theString.length() + 1; // Include null terminator
		if (!Write(&length, sizeof(size_t))) return false;
		return Write(theString.c_str(), length);
	}

	bool WriteInt(const int& value)
	{
		return Write(&value, sizeof(int));
	}
	bool ReadInt(int& value)
	{
		return Read(&value, sizeof(int));
	}

	// Component-specific methods - handle components using ComponentManager
	void ReadComponent(const CPropertyBase& property, CReflectedBase* obj)
	{
		if (ValidateProperties(property))
		{
			ComponentSystem::Component* subObj = reinterpret_cast<ComponentSystem::Component*>(property.GetAddress(obj));
			subObj->ReadMembers(*this);
		}
	}

	void ReadComponentPtr(const CPropertyBase& property, CReflectedBase* obj)
	{
		if (ValidateProperties(property))
		{
			std::string className;
			ReadString(className);
			std::unique_ptr<ComponentSystem::Component>* subObjPtr =
				reinterpret_cast<std::unique_ptr<ComponentSystem::Component>*>(property.GetAddress(obj));

			auto subObj = CreateComponentAsUniquePtr(className.c_str());
			if (subObj) {
				subObj->ReadMembers(*this);
				*subObjPtr = std::move(subObj);
			}
		}
	}

	void ReadComponentPtrArray(const CPropertyBase& property, CReflectedBase* obj)
	{
		if (ValidateProperties(property))
		{
			int arraySize;
			ReadInt(arraySize);
			std::vector<std::unique_ptr<ComponentSystem::Component>>* componentVector =
				reinterpret_cast<std::vector<std::unique_ptr<ComponentSystem::Component>>*>(property.GetAddress(obj));

			componentVector->clear(); // Clear existing content
			componentVector->reserve(arraySize); // Reserve space for efficiency

			for (int i = 0; i < arraySize; i++)
			{
				std::string className;
				ReadString(className);
				auto subObj = CreateComponentAsUniquePtr(className.c_str());
				if (subObj) {
					subObj->ReadMembers(*this);
					componentVector->push_back(std::move(subObj));
				}
			}
		}
	}

	void WriteComponent(const CPropertyBase& property, CReflectedBase* obj)
	{
		WriteProperties(property);
		ComponentSystem::Component* subObj = reinterpret_cast<ComponentSystem::Component*>(property.GetAddress(obj));
		subObj->WriteMembers(*this);
	}

	void WriteComponentPtr(const CPropertyBase& property, CReflectedBase* obj)
	{
		WriteProperties(property);
		std::unique_ptr<ComponentSystem::Component>* subObjPtr =
			reinterpret_cast<std::unique_ptr<ComponentSystem::Component>*>(property.GetAddress(obj));

		if (*subObjPtr) {
			ComponentSystem::Component* subObj = subObjPtr->get();
			std::string className = subObj->GetRflClassName();
			WriteString(className);
			subObj->WriteMembers(*this);
		}
		else {
			// Handle null pointer case
			WriteString(std::string(""));
		}
	}

	void WriteComponentPtrArray(const CPropertyBase& property, CReflectedBase* obj)
	{
		WriteProperties(property);
		std::vector<std::unique_ptr<ComponentSystem::Component>>* componentVector =
			reinterpret_cast<std::vector<std::unique_ptr<ComponentSystem::Component>>*>(property.GetAddress(obj));

		WriteInt(static_cast<int>(componentVector->size()));
		for (const auto& element : *componentVector)
		{
			if (element) {
				std::string className = element->GetRflClassName();
				WriteString(className);
				element->WriteMembers(*this);
			}
			else {
				// Handle null pointer case in array
				WriteString(std::string(""));
			}
		}
	}

	void ReadComponentRawPtrArray(const CPropertyBase& property, CReflectedBase* obj)
	{
		if (ValidateProperties(property))
		{
			int arraySize;
			ReadInt(arraySize);
			std::vector<ComponentSystem::Component*>* componentVector =
				reinterpret_cast<std::vector<ComponentSystem::Component*>*>(property.GetAddress(obj));

			componentVector->clear();

			ComponentSystem::Component* parentComponent = dynamic_cast<ComponentSystem::Component*>(obj);

			for (int i = 0; i < arraySize; i++)
			{
				std::string className;
				ReadString(className);

				// Create component through ClassFactory
				CReflectedBase* newObject = ClassFactory::createObject(className.c_str());
				if (newObject) {
					// Ensure it's actually a Component
					ComponentSystem::Component* childComponent = dynamic_cast<ComponentSystem::Component*>(newObject);
					if (childComponent) {
						childComponent->ReadMembers(*this);

						if (parentComponent) {
							parentComponent->AddChild(childComponent);
						}
						else {
							componentVector->push_back(childComponent);
						}
					}
					else {
						delete newObject;
					}
				}
			}
		}
	}

	void WriteComponentRawPtrArray(const CPropertyBase& property, CReflectedBase* obj)
	{
		WriteProperties(property);
		std::vector<ComponentSystem::Component*>* componentVector =
			reinterpret_cast<std::vector<ComponentSystem::Component*>*>(property.GetAddress(obj));

		WriteInt(static_cast<int>(componentVector->size()));
		for (const auto& element : *componentVector)
		{
			if (element) {
				std::string className = element->GetRflClassName();
				WriteString(className);
				element->WriteMembers(*this);
			}
			else {
				WriteString(std::string(""));
			}
		}
	}

	void ReadBool(const CPropertyBase& property, CReflectedBase* obj)
	{
		if (ValidateProperties(property))
		{
			bool* value = reinterpret_cast<bool*>(property.GetAddress(obj));
			char raw;
			if (Read(&raw, sizeof(char)))
				*value = (raw != 0);
		}
	}

	void WriteBool(const CPropertyBase& property, CReflectedBase* obj)
	{
		WriteProperties(property);
		bool* value = reinterpret_cast<bool*>(property.GetAddress(obj));
		char raw = *value ? 1 : 0;
		Write(&raw, sizeof(char));
	}

	// --- Primitive vector support ---
	void ReadIntVector(const CPropertyBase& property, CReflectedBase* obj)
	{
		if (ValidateProperties(property))
		{
			int arraySize;
			ReadInt(arraySize);
			std::vector<int>* vec = reinterpret_cast<std::vector<int>*>(property.GetAddress(obj));
			vec->clear();
			vec->resize(arraySize);
			if (arraySize > 0)
				Read(vec->data(), arraySize * sizeof(int));
		}
	}
	void WriteIntVector(const CPropertyBase& property, CReflectedBase* obj)
	{
		WriteProperties(property);
		std::vector<int>* vec = reinterpret_cast<std::vector<int>*>(property.GetAddress(obj));
		WriteInt(static_cast<int>(vec->size()));
		if (!vec->empty())
			Write(vec->data(), vec->size() * sizeof(int));
	}
	void ReadFloatVector(const CPropertyBase& property, CReflectedBase* obj)
	{
		if (ValidateProperties(property))
		{
			int arraySize;
			ReadInt(arraySize);
			std::vector<float>* vec = reinterpret_cast<std::vector<float>*>(property.GetAddress(obj));
			vec->clear();
			vec->resize(arraySize);
			if (arraySize > 0)
				Read(vec->data(), arraySize * sizeof(float));
		}
	}
	void WriteFloatVector(const CPropertyBase& property, CReflectedBase* obj)
	{
		WriteProperties(property);
		std::vector<float>* vec = reinterpret_cast<std::vector<float>*>(property.GetAddress(obj));
		WriteInt(static_cast<int>(vec->size()));
		if (!vec->empty())
			Write(vec->data(), vec->size() * sizeof(float));
	}
	void ReadBoolVector(const CPropertyBase& property, CReflectedBase* obj)
	{
		if (ValidateProperties(property))
		{
			int arraySize;
			ReadInt(arraySize);
			std::vector<bool>* vec = reinterpret_cast<std::vector<bool>*>(property.GetAddress(obj));
			vec->clear();
			vec->reserve(arraySize);
			for (int i = 0; i < arraySize; i++)
			{
				char raw;
				Read(&raw, sizeof(char));
				vec->push_back(raw != 0);
			}
		}
	}
	void WriteBoolVector(const CPropertyBase& property, CReflectedBase* obj)
	{
		WriteProperties(property);
		std::vector<bool>* vec = reinterpret_cast<std::vector<bool>*>(property.GetAddress(obj));
		WriteInt(static_cast<int>(vec->size()));
		for (size_t i = 0; i < vec->size(); i++)
		{
			char raw = (*vec)[i] ? 1 : 0;
			Write(&raw, sizeof(char));
		}
	}
	void ReadStringVector(const CPropertyBase& property, CReflectedBase* obj)
	{
		if (ValidateProperties(property))
		{
			int arraySize;
			ReadInt(arraySize);
			std::vector<std::string>* vec = reinterpret_cast<std::vector<std::string>*>(property.GetAddress(obj));
			vec->clear();
			vec->reserve(arraySize);
			for (int i = 0; i < arraySize; i++)
			{
				std::string str;
				ReadString(str);
				vec->push_back(std::move(str));
			}
		}
	}
	void WriteStringVector(const CPropertyBase& property, CReflectedBase* obj)
	{
		WriteProperties(property);
		std::vector<std::string>* vec = reinterpret_cast<std::vector<std::string>*>(property.GetAddress(obj));
		WriteInt(static_cast<int>(vec->size()));
		for (auto& element : *vec)
		{
			WriteString(element);
		}
	}
	void ReadStringBoolMap(const CPropertyBase& property, CReflectedBase* obj)
	{
		if (ValidateProperties(property))
		{
			int mapSize;
			ReadInt(mapSize);
			std::unordered_map<std::string, bool>* map =
				reinterpret_cast<std::unordered_map<std::string, bool>*>(property.GetAddress(obj));
			map->clear();
			map->reserve(mapSize);
			for (int i = 0; i < mapSize; i++)
			{
				std::string key;
				ReadString(key);
				char raw;
				Read(&raw, sizeof(char));
				map->emplace(std::move(key), raw != 0);
			}
		}
	}
	void WriteStringBoolMap(const CPropertyBase& property, CReflectedBase* obj)
	{
		WriteProperties(property);
		std::unordered_map<std::string, bool>* map =
			reinterpret_cast<std::unordered_map<std::string, bool>*>(property.GetAddress(obj));
		WriteInt(static_cast<int>(map->size()));
		for (auto& [key, val] : *map)
		{
			WriteString(key);
			char raw = val ? 1 : 0;
			Write(&raw, sizeof(char));
		}
	}
private:
	std::unique_ptr<FileSystem::IFile> m_outputFile;
	std::unique_ptr<FileSystem::IFile> m_inputFile;
	uint64_t m_writePosition = 0;
	uint64_t m_readPosition = 0;

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