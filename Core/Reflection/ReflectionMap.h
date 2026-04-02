#pragma once

#include <vector>
#include <string>

enum RFL_Type
{
	RT_Int = 1,
	RT_Float = 2,
	RT_String = 3,
	RT_Object = 4,
	RT_ObjectPtr = 5,
	RT_ObjectPtrVec = 6,
	RT_Vector4 = 7,
	RT_Matrix4 = 8,
	RT_Component = 9,
	RT_ComponentPtr = 10,
	RT_ComponentPtrVec = 11,
	RT_Bool = 12,
	RT_Vector3 = 13,
	RT_IntVec = 14,
	RT_FloatVec = 15,
	RT_BoolVec = 16,
	RT_StringVec = 17,
	RT_StringBoolMap = 18,
	RT_ComponentRawPtrVec = 19,
};

class CReflectedBase;
class IRFL_Parser;
class CPropertyBase
{
public:
	CPropertyBase(RFL_Type type, size_t size, size_t memberOffset, const char* name) : m_type(type), m_size(size), m_memberOffset(memberOffset), m_name(name) {};
	void* GetAddress(CReflectedBase* obj) const { return (void*)((char*)(obj)+GetOffset()); }
	size_t GetOffset() const { return m_memberOffset; }
	size_t GetSize() const { return m_size; }
	const std::string & GetName() const { return m_name; }
	void SetName(const std::string & name) { m_name = name; }

	RFL_Type GetType() const { return m_type; }

	virtual void Read(IRFL_Parser* doc, CReflectedBase* obj) = 0;
	virtual void Write(IRFL_Parser* doc, CReflectedBase* obj) = 0;
	virtual const char* GetTypeAsString() = 0;
private:
	size_t m_memberOffset;
	size_t m_size;
	std::string m_name;
	RFL_Type m_type;

};

class CReflectionMapEntry
{
public:
	CReflectionMapEntry(CPropertyBase* prop) : m_property(prop) {}
	~CReflectionMapEntry() {}
	CPropertyBase* GetProperty() const { return m_property; }
private:
	CPropertyBase* m_property;
};