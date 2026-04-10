#pragma once


#include "ReflectionMap.h"
#include "ClassFactory/ClassFactory.h"
#include "RFL_Pareser.h"
#include "DebugChannel\DebugChannel.h"
#include "ReflectionError.h"
#include "Types/bool.h"

#include <vector>

#define REFL_DECLARE_OBJECT(_O_,_P_)												\
    std::vector<CReflectionMapEntry> &GetReflectionMap() { return _O_::s_ReflectionMap; }\
    virtual const char *GetRflClassName() const override { return #_O_; }				\
	static const char *ClassName() { return #_O_; }									\
    static std::vector<CReflectionMapEntry> s_ReflectionMap;								\
    static CReflectedBase *create() { return new _O_; }								\
																					\
	virtual void WriteMembers(IRFL_Parser& doc)										\
	{																				\
		try {																		\
			InternalWriteMembers( GetReflectionMap(), doc);							\
			_P_::WriteMembers(doc);													\
		} catch (const Reflection::ReflectionException& e) {						\
			REFL_ERROR(Reflection::ErrorCategory::Validation, 					\
				"Failed to write members", std::string(GetRflClassName()) + ": " + e.what());\
			throw;																	\
		}																			\
	}																				\
	virtual void CollectHierarchyReflectionMaps(std::vector<std::pair<const char*, std::vector<CReflectionMapEntry>*>>& hierarchyMaps) const override \
	{																				\
		hierarchyMaps.emplace_back(#_O_, &_O_::s_ReflectionMap);					\
		_P_::CollectHierarchyReflectionMaps(hierarchyMaps);						\
	}																				\
	virtual void ReadMembers( IRFL_Parser& doc )									\
	{																				\
		try {																		\
			InternalReadMembers( GetReflectionMap(), doc);							\
			_P_::ReadMembers(doc);													\
		} catch (const Reflection::ReflectionException& e) {						\
			REFL_ERROR(Reflection::ErrorCategory::Validation, 					\
				"Failed to read members", std::string(GetRflClassName()) + ": " + e.what());\
			throw;																	\
		}																			\
	}																				\
    static ClassFactory s_factoryRegistration; 

#define REFL_DEFINE_OBJECT(_O_)										\
    ClassFactory _O_::s_factoryRegistration(#_O_,_O_::create);		\
    std::vector<CReflectionMapEntry> _O_::s_ReflectionMap = { 

#define REFL_DEFINE_END };

#define REFL_DEFINE_FLOAT_MEMBER(_O_,_M_)				CReflectionMapEntry(new CFloatProperty( RT_Float, sizeof(float), offsetof(_O_, _M_), #_M_) )
#define REFL_DEFINE_STRING_MEMBER(_O_,_M_)				CReflectionMapEntry(new CStringProperty( RT_String, sizeof(std::string), offsetof(_O_, _M_), #_M_) )
#define REFL_DEFINE_INT_MEMBER(_O_,_M_)					CReflectionMapEntry(new CIntProperty( RT_Int, sizeof(int), offsetof(_O_, _M_), #_M_ ) )
#define REFL_DEFINE_OBJECT_MEMBER(_O_,_M_)              CReflectionMapEntry(new CSubObjectProperty( RT_Object, sizeof((( _O_*)0)->_M_), offsetof(_O_, _M_), #_M_ ) )
#define REFL_DEFINE_OBJECT_PTR_MEMBER(_O_,_M_)			CReflectionMapEntry(new CSubObjectPtrProperty(RT_ObjectPtr, sizeof(void *), offsetof(_O_, _M_), #_M_ ) )
#define REFL_DEFINE_OBJECT_PTR_VECTOR_MEMBER(_O_,_M_)	CReflectionMapEntry(new CObjPtrVectorProperty( RT_ObjectPtrVec, sizeof(std::vector<CPropertyBase*>), offsetof(_O_, _M_), #_M_ ) )
#define REFL_DEFINE_VECTOR4_MEMBER(_O_,_M_)				CReflectionMapEntry(new CVector4Property( RT_Vector4, sizeof(Vector4f),offsetof(_O_, _M_), #_M_) )
#define REFL_DEFINE_VECTOR3_MEMBER(_O_,_M_)				CReflectionMapEntry(new CVector3Property( RT_Vector3, sizeof(Vector3f),offsetof(_O_, _M_), #_M_) )
#define REFL_DEFINE_MATRIX4_MEMBER(_O_,_M_)				CReflectionMapEntry(new CMatrix4Property( RT_Matrix4, sizeof(Matrix4f),offsetof(_O_, _M_), #_M_) )
#define REFL_DEFINE_COMPONENT_PTR_VECTOR_MEMBER(_O_,_M_) CReflectionMapEntry(new CComponentPtrVectorProperty( RT_ComponentPtrVec, sizeof(std::vector<std::unique_ptr<Component>>), offsetof(_O_, _M_), #_M_ ))
#define REFL_DEFINE_COMPONENT_RAW_PTR_VECTOR_MEMBER(_O_,_M_) CReflectionMapEntry(new CComponentRawPtrVectorProperty( RT_ComponentRawPtrVec, sizeof(std::vector<Component*>), offsetof(_O_, _M_), #_M_ ))
#define REFL_DEFINE_BOOL_MEMBER(_O_,_M_) CReflectionMapEntry(new CBoolProperty(RT_Bool, sizeof(bool), offsetof(_O_, _M_), #_M_))
#define REFL_DEFINE_INT_VECTOR_MEMBER(_O_,_M_)			CReflectionMapEntry(new CIntVectorProperty( RT_IntVec, sizeof(std::vector<int>), offsetof(_O_, _M_), #_M_) )
#define REFL_DEFINE_FLOAT_VECTOR_MEMBER(_O_,_M_)		CReflectionMapEntry(new CFloatVectorProperty( RT_FloatVec, sizeof(std::vector<float>), offsetof(_O_, _M_), #_M_) )
#define REFL_DEFINE_BOOL_VECTOR_MEMBER(_O_,_M_)			CReflectionMapEntry(new CBoolVectorProperty( RT_BoolVec, sizeof(std::vector<bool>), offsetof(_O_, _M_), #_M_) )
#define REFL_DEFINE_STRING_VECTOR_MEMBER(_O_,_M_)		CReflectionMapEntry(new CStringVectorProperty( RT_StringVec, sizeof(std::vector<std::string>), offsetof(_O_, _M_), #_M_) )
#define REFL_DEFINE_STRING_BOOL_MAP_MEMBER(_O_,_M_)		CReflectionMapEntry(new CStringBoolMapProperty( RT_StringBoolMap, sizeof(std::unordered_map<std::string, bool>), offsetof(_O_, _M_), #_M_) )

class CReflectedBase
{
public:
	CReflectedBase() {}
	virtual ~CReflectedBase() {}

	// Enhanced Read/Write methods with better error handling
	bool Read(const char* fileName);
	bool Write(const char* fileName);

	// New safe read/write methods that use Result<T>
	Reflection::Result<bool> SafeRead(const std::string& fileName);
	Reflection::Result<bool> SafeWrite(const std::string& fileName);

	// In-memory JSON string buffer read/write
	bool ReadFromJsonString(const std::string& jsonString);
	Reflection::Result<std::string> WriteToJsonString();	

	// In-memory binary buffer read/write
	bool ReadFromBinaryBuffer(const std::vector<uint8_t>& binaryData);
	Reflection::Result<std::vector<uint8_t>> WriteToBinaryBuffer();

	virtual void ReadMembers(IRFL_Parser& doc) {}
	virtual void WriteMembers(IRFL_Parser& doc) {}
	virtual const char* GetRflClassName() const { return nullptr; }
	virtual void CollectHierarchyReflectionMaps(std::vector<std::pair<const char*, std::vector<CReflectionMapEntry>*>>& hierarchyMaps) const {}

protected:
	void InternalReadMembers(std::vector<CReflectionMapEntry>& reflectionMap, IRFL_Parser& doc);
	void InternalWriteMembers(std::vector<CReflectionMapEntry>& reflectionMap, IRFL_Parser& doc);

	// Helper methods for validation
	bool ValidateReflectionMap(const std::vector<CReflectionMapEntry>& reflectionMap) const;
	bool ValidateParser(IRFL_Parser& doc) const;
	
};