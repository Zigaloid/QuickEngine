#pragma once

#include "ReflectionMap.h"
#include "ClassFactory/ClassFactory.h"
#include "RFL_Pareser.h"
#include "DebugChannel\DebugChannel.h"
#include "ReflectionError.h"
#include "Types/bool.h"
#include "Types/guid.h"
#include "Types/string.h"

#include <vector>
#include <string>
#include <array>

// Restore original behavior: do NOT inject base-class GUID into every derived class map.
#define REFL_DECLARE_OBJECT(_O_,_P_)                                                    \
    std::vector<CReflectionMapEntry> &GetReflectionMap() { return _O_::s_ReflectionMap; }\
    virtual const char *GetRflClassName() const override { return #_O_; }               \
    static const char *ClassName() { return #_O_; }                                     \
    static std::vector<CReflectionMapEntry> s_ReflectionMap;                            \
    static CReflectedBase *create() { return new _O_; }                                 \
                                                                                        \
    virtual void WriteMembers(IRFL_Parser& doc)                                         \
    {                                                                                   \
        try {                                                                           \
            InternalWriteMembers( GetReflectionMap(), doc);                             \
            _P_::WriteMembers(doc);                                                     \
        } catch (const Reflection::ReflectionException& e) {                           \
            REFL_ERROR(Reflection::ErrorCategory::Validation,                           \
                "Failed to write members", std::string(GetRflClassName()) + ": " + e.what());\
            throw;                                                                      \
        }                                                                               \
    }                                                                                   \
    virtual void CollectHierarchyReflectionMaps(std::vector<std::pair<const char*, std::vector<CReflectionMapEntry>*>>& hierarchyMaps) const override \
    {                                                                                   \
        hierarchyMaps.emplace_back(#_O_, &_O_::s_ReflectionMap);                       \
        _P_::CollectHierarchyReflectionMaps(hierarchyMaps);                            \
    }                                                                                   \
    virtual void ReadMembers( IRFL_Parser& doc )                                        \
    {                                                                                   \
        try {                                                                           \
            InternalReadMembers( GetReflectionMap(), doc);                              \
        } catch (const Reflection::ReflectionException& e) {                           \
            REFL_WARNING(Reflection::ErrorCategory::Validation,                         \
                "Skipping failed member read", std::string(GetRflClassName()) + ": " + e.what());\
        }                                                                               \
        try {                                                                           \
            _P_::ReadMembers(doc);                                                      \
        } catch (const Reflection::ReflectionException& e) {                           \
            REFL_WARNING(Reflection::ErrorCategory::Validation,                         \
                "Skipping failed base read", std::string(GetRflClassName()) + ": " + e.what());\
        }                                                                               \
    }                                                                                   \
    static ClassFactory s_factoryRegistration;

#define REFL_DEFINE_OBJECT(_O_)                             \
    ClassFactory _O_::s_factoryRegistration(#_O_,_O_::create);  \
    std::vector<CReflectionMapEntry> _O_::s_ReflectionMap = {

#define REFL_DEFINE_END };

#define REFL_DEFINE_FLOAT_MEMBER(_O_,_M_)               CReflectionMapEntry(new CFloatProperty( RT_Float, sizeof(float), offsetof(_O_, _M_), #_M_) )
#define REFL_DEFINE_STRING_MEMBER(_O_,_M_)              CReflectionMapEntry(new CStringProperty( RT_String, sizeof(std::string), offsetof(_O_, _M_), #_M_) )
#define REFL_DEFINE_INT_MEMBER(_O_,_M_)                 CReflectionMapEntry(new CIntProperty( RT_Int, sizeof(int), offsetof(_O_, _M_), #_M_ ) )
#define REFL_DEFINE_OBJECT_MEMBER(_O_,_M_)              CReflectionMapEntry(new CSubObjectProperty( RT_Object, sizeof((( _O_*)0)->_M_), offsetof(_O_, _M_), #_M_ ) )
#define REFL_DEFINE_OBJECT_PTR_MEMBER(_O_,_M_)          CReflectionMapEntry(new CSubObjectPtrProperty(RT_ObjectPtr, sizeof(void *), offsetof(_O_, _M_), #_M_ ) )
#define REFL_DEFINE_OBJECT_PTR_VECTOR_MEMBER(_O_,_M_)        CReflectionMapEntry(new CObjPtrVectorProperty( RT_ObjectPtrVec, sizeof(std::vector<CPropertyBase*>), offsetof(_O_, _M_), #_M_ ) )
#define REFL_DEFINE_SHARED_OBJECT_PTR_MEMBER(_O_,_M_)         CReflectionMapEntry(new CSubObjectSharedPtrProperty( RT_SharedObjectPtr, sizeof(void*), offsetof(_O_, _M_), #_M_ ) )
#define REFL_DEFINE_SHARED_OBJECT_PTR_VECTOR_MEMBER(_O_,_M_)  CReflectionMapEntry(new CObjSharedPtrVectorProperty( RT_SharedObjectPtrVec, sizeof(std::vector<std::shared_ptr<CReflectedBase>>), offsetof(_O_, _M_), #_M_ ) )
#define REFL_DEFINE_VECTOR4_MEMBER(_O_,_M_)             CReflectionMapEntry(new CVector4Property( RT_Vector4, sizeof(Vector4f),offsetof(_O_, _M_), #_M_) )
#define REFL_DEFINE_VECTOR3_MEMBER(_O_,_M_)             CReflectionMapEntry(new CVector3Property( RT_Vector3, sizeof(Vector3f),offsetof(_O_, _M_), #_M_) )
#define REFL_DEFINE_MATRIX4_MEMBER(_O_,_M_)             CReflectionMapEntry(new CMatrix4Property( RT_Matrix4, sizeof(Matrix4f),offsetof(_O_, _M_), #_M_) )
#define REFL_DEFINE_COMPONENT_PTR_VECTOR_MEMBER(_O_,_M_) CReflectionMapEntry(new CComponentPtrVectorProperty( RT_ComponentPtrVec, sizeof(std::vector<std::unique_ptr<Component>>), offsetof(_O_, _M_), #_M_ ))
#define REFL_DEFINE_COMPONENT_RAW_PTR_VECTOR_MEMBER(_O_,_M_) CReflectionMapEntry(new CComponentRawPtrVectorProperty( RT_ComponentRawPtrVec, sizeof(std::vector<Component*>), offsetof(_O_, _M_), #_M_ ))
#define REFL_DEFINE_COMPONENT_SHARED_PTR_VECTOR_MEMBER(_O_,_M_) CReflectionMapEntry(new CComponentSharedPtrVectorProperty( RT_ComponentSharedPtrVec, sizeof(std::vector<std::shared_ptr<Component>>), offsetof(_O_, _M_), #_M_ ))
#define REFL_DEFINE_BOOL_MEMBER(_O_,_M_)                CReflectionMapEntry(new CBoolProperty(RT_Bool, sizeof(bool), offsetof(_O_, _M_), #_M_))
#define REFL_DEFINE_INT_VECTOR_MEMBER(_O_,_M_)          CReflectionMapEntry(new CIntVectorProperty( RT_IntVec, sizeof(std::vector<int>), offsetof(_O_, _M_), #_M_) )
#define REFL_DEFINE_FLOAT_VECTOR_MEMBER(_O_,_M_)        CReflectionMapEntry(new CFloatVectorProperty( RT_FloatVec, sizeof(std::vector<float>), offsetof(_O_, _M_), #_M_) )
#define REFL_DEFINE_BOOL_VECTOR_MEMBER(_O_,_M_)         CReflectionMapEntry(new CBoolVectorProperty( RT_BoolVec, sizeof(std::vector<bool>), offsetof(_O_, _M_), #_M_) )
#define REFL_DEFINE_STRING_VECTOR_MEMBER(_O_,_M_)       CReflectionMapEntry(new CStringVectorProperty( RT_StringVec, sizeof(std::vector<std::string>), offsetof(_O_, _M_), #_M_) )
#define REFL_DEFINE_STRING_BOOL_MAP_MEMBER(_O_,_M_)     CReflectionMapEntry(new CStringBoolMapProperty( RT_StringBoolMap, sizeof(std::unordered_map<std::string, bool>), offsetof(_O_, _M_), #_M_) )

/** @brief Base class for all reflection-capable objects. */
class CReflectedBase
{
public:
    CReflectedBase()
        : m_guid(GUID128::Generate())
    {
        // Keep serializable string in sync with binary GUID
        m_rfl_guid = GUID128::ToString(m_guid);
    }

    virtual ~CReflectedBase() = default;

    /** @param fileName File to read from.
     *  @return true on success. */
    bool Read(const char* fileName);

    /** @param fileName File to write to.
     *  @return true on success. */
    bool Write(const char* fileName);

    /** @param fileName File to read from.
     *  @return Result<bool> indicating success or failure. */
    Reflection::Result<bool> SafeRead(const std::string& fileName);

    /** @param fileName File to write to.
     *  @return Result<bool> indicating success or failure. */
    Reflection::Result<bool> SafeWrite(const std::string& fileName);

    /** @param jsonString JSON string to parse.
     *  @return true on success. */
    bool ReadFromJsonString(const std::string& jsonString);

    /** @return Result containing the serialized JSON string, or an error. */
    Reflection::Result<std::string> WriteToJsonString();

    /** @param binaryData Binary buffer to parse.
     *  @return true on success. */
    bool ReadFromBinaryBuffer(const std::vector<uint8_t>& binaryData);

    /** @return Result containing the serialized binary buffer, or an error. */
    Reflection::Result<std::vector<uint8_t>> WriteToBinaryBuffer();

    // Base class reads/writes its own GUID string property so derived classes don't need to embed it in their maps.
    virtual void ReadMembers(IRFL_Parser& doc)
    {
        // Build a temporary string property that describes the base-class member and ask parser to read it.
        CStringProperty guidProp(RT_String, sizeof(std::string), offsetof(CReflectedBase, m_rfl_guid), "m_rfl_guid");
        guidProp.Read(&doc, this);

        // Try to parse the GUID string into binary form; if parsing fails, keep the previously generated GUID.
        GUID128 parsed;
        if (GUID128::FromString(m_rfl_guid, parsed))
        {
            m_guid = parsed;
        }
    }

    virtual void WriteMembers(IRFL_Parser& doc) 
    {
        CStringProperty guidProp(RT_String, sizeof(std::string), offsetof(CReflectedBase, m_rfl_guid), "m_rfl_guid");
        guidProp.Write(&doc, this);
    }

    virtual const char* GetRflClassName() const { return nullptr; }
    virtual void CollectHierarchyReflectionMaps(std::vector<std::pair<const char*, std::vector<CReflectionMapEntry>*>>& hierarchyMaps) const {}
	virtual void OnLoaded() {} // Called after successful read, can be overridden for post-processing

    // GUID accessors
    const std::string& GetGuidString() const { return m_rfl_guid; }
    const std::array<uint8_t, 16>& GetGuidBytes() const { return m_guid.bytes; }
    void SetGuidFromString(const std::string& s)
    {
        GUID128 parsed;
        if (GUID128::FromString(s, parsed)) {
            m_guid = parsed;
            m_rfl_guid = s;
        }
    }

protected:
    void InternalReadMembers(std::vector<CReflectionMapEntry>& reflectionMap, IRFL_Parser& doc);
    void InternalWriteMembers(std::vector<CReflectionMapEntry>& reflectionMap, IRFL_Parser& doc);

    bool ValidateReflectionMap(const std::vector<CReflectionMapEntry>& reflectionMap) const;
    bool ValidateParser(IRFL_Parser& doc) const;
    
    std::string m_rfl_guid;

private:
    // Binary GUID storage (16 bytes)
    GUID128 m_guid;
    // Serializable GUID string used by reflection system
    
};
