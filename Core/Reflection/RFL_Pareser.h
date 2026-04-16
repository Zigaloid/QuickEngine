#pragma once
#include <memory>
#include <string>
#include <vector>
#include "ReflectionMap.h"

// Forward declarations
namespace ComponentSystem {
	class Component;
	class ComponentManager;
}

namespace Core {
	class CoreSystem;
}

class CReflectedBase;

/** @brief Abstract parser interface used by the reflection system for reading and writing objects. */
class IRFL_Parser
{
public:
	virtual void WriteFloat(const CPropertyBase& property, CReflectedBase* obj) = 0;
	virtual void ReadFloat(const CPropertyBase& property, CReflectedBase* obj) = 0;
	virtual void WriteInt(const CPropertyBase& property, CReflectedBase* obj) = 0;
	virtual void ReadInt(const CPropertyBase& property, CReflectedBase* obj) = 0;
	virtual void ReadString(const CPropertyBase& property, CReflectedBase* obj) = 0;
	virtual void WriteString(const CPropertyBase& property, CReflectedBase* obj) = 0;
	virtual void WriteMatrix4(const CPropertyBase& property, CReflectedBase* obj) = 0;
	virtual void ReadMatrix4(const CPropertyBase& property, CReflectedBase* obj) = 0;
	virtual void WriteVector4(const CPropertyBase& property, CReflectedBase* obj) = 0;
	virtual void ReadVector4(const CPropertyBase& property, CReflectedBase* obj) = 0;
	virtual void WriteVector3(const CPropertyBase& property, CReflectedBase* obj) = 0;
	virtual void ReadVector3(const CPropertyBase& property, CReflectedBase* obj) = 0;
	virtual void WriteObject(const CPropertyBase& property, CReflectedBase* obj) = 0;
	virtual void ReadObject(const CPropertyBase& property, CReflectedBase* obj) = 0;

	// Updated to use unique_ptr
	virtual void WriteObjectPtr(const CPropertyBase& property, CReflectedBase* obj) = 0;
	virtual void ReadObjectPtr(const CPropertyBase& property, CReflectedBase* obj) = 0;
	virtual void WriteObjectPtrArray(const CPropertyBase& property, CReflectedBase* obj) = 0;
	virtual void ReadObjectPtrArray(const CPropertyBase& property, CReflectedBase* obj) = 0;

	// New component-specific methods that use ComponentManager
	virtual void WriteComponent(const CPropertyBase& property, CReflectedBase* obj) = 0;
	virtual void ReadComponent(const CPropertyBase& property, CReflectedBase* obj) = 0;
	virtual void WriteComponentPtr(const CPropertyBase& property, CReflectedBase* obj) = 0;
	virtual void ReadComponentPtr(const CPropertyBase& property, CReflectedBase* obj) = 0;
	virtual void WriteComponentPtrArray(const CPropertyBase& property, CReflectedBase* obj) = 0;
	virtual void ReadComponentPtrArray(const CPropertyBase& property, CReflectedBase* obj) = 0;
	virtual void WriteComponentRawPtrArray(const CPropertyBase& property, CReflectedBase* obj) = 0;
	virtual void ReadComponentRawPtrArray(const CPropertyBase& property, CReflectedBase* obj) = 0;

	// --- Add these for bool support ---
	virtual void WriteBool(const CPropertyBase& property, CReflectedBase* obj) = 0;
	virtual void ReadBool(const CPropertyBase& property, CReflectedBase* obj) = 0;
	// ----------------------------------

	// --- Primitive vector support ---
	virtual void WriteIntVector(const CPropertyBase& property, CReflectedBase* obj) = 0;
	virtual void ReadIntVector(const CPropertyBase& property, CReflectedBase* obj) = 0;
	virtual void WriteFloatVector(const CPropertyBase& property, CReflectedBase* obj) = 0;
	virtual void ReadFloatVector(const CPropertyBase& property, CReflectedBase* obj) = 0;
	virtual void WriteBoolVector(const CPropertyBase& property, CReflectedBase* obj) = 0;
	virtual void ReadBoolVector(const CPropertyBase& property, CReflectedBase* obj) = 0;
	virtual void WriteStringVector(const CPropertyBase& property, CReflectedBase* obj) = 0;
	virtual void ReadStringVector(const CPropertyBase& property, CReflectedBase* obj) = 0;
	// --------------------------------

	// --- String-bool map support ---
	virtual void WriteStringBoolMap(const CPropertyBase& property, CReflectedBase* obj) = 0;
	virtual void ReadStringBoolMap(const CPropertyBase& property, CReflectedBase* obj) = 0;
	// -------------------------------

	virtual bool BeginOutput(const std::string& fileName) = 0;
	virtual void EndOutput() = 0;
	virtual bool BeginInput(const std::string& fileName) = 0;
	virtual void EndInput() = 0;
	virtual void BeginObject(const char* className = "") = 0;
	virtual void EndObject(const char* memberName) = 0;
	virtual bool WriteProperties(const CPropertyBase& property) = 0;
	virtual void BeginArray() = 0;
	virtual void EndArray() = 0;

protected:
	static std::unique_ptr<CReflectedBase> CreateObjectAsUniquePtr(const char* className)
	{
		CReflectedBase* rawPtr = ClassFactory::CreateObject(className);
		return std::unique_ptr<CReflectedBase>(rawPtr);
	}

	static ComponentSystem::Component* CreateComponentFromManager(const char* className);
};