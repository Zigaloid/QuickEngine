#ifndef REFLECTION_TYPES_COMPONENT_H
#define REFLECTION_TYPES_COMPONENT_H

#include "../ReflectionMap.h"

// Forward declarations to avoid circular includes
namespace Core {
    class CoreSystem;
}

namespace ComponentSystem {
    class Component;
    class ComponentManager;
}

/** @brief Reflected property descriptor for embedded Component members. */
class CComponentProperty : public CPropertyBase
{
public:
    CComponentProperty(RFL_Type type, size_t size, unsigned int memberOffset, const char* name)
        : CPropertyBase(type, size, memberOffset, name)
    {
    }

    void Read(IRFL_Parser* doc, CReflectedBase* obj) override
    {
        doc->ReadComponent(*this, obj);
    }

    void Write(IRFL_Parser* doc, CReflectedBase* obj) override
    {
        doc->WriteComponent(*this, obj);
    }

    const char* GetTypeAsString() override { return "component"; }
};

/** @brief Reflected property descriptor for unique_ptr<Component> members. */
class CComponentPtrProperty : public CPropertyBase
{
public:
    CComponentPtrProperty(RFL_Type type, size_t size, unsigned int memberOffset, const char* name)
        : CPropertyBase(type, size, memberOffset, name)
    {
    }

    void Read(IRFL_Parser* doc, CReflectedBase* obj) override
    {
        doc->ReadComponentPtr(*this, obj);
    }

    void Write(IRFL_Parser* doc, CReflectedBase* obj) override
    {
        doc->WriteComponentPtr(*this, obj);
    }

    const char* GetTypeAsString() override { return "component_ptr"; }
};

/** @brief Reflected property descriptor for vector<unique_ptr<Component>> members. */
class CComponentPtrVectorProperty : public CPropertyBase
{
public:
    CComponentPtrVectorProperty(RFL_Type type, size_t size, unsigned int memberOffset, const char* name)
        : CPropertyBase(type, size, memberOffset, name)
    {
    }

    void Read(IRFL_Parser* doc, CReflectedBase* obj) override
    {
        doc->ReadComponentPtrArray(*this, obj);
    }

    void Write(IRFL_Parser* doc, CReflectedBase* obj) override
    {
        doc->WriteComponentPtrArray(*this, obj);
    }

    const char* GetTypeAsString() override { return "vector_component_ptr"; }
};

/** @brief Reflected property descriptor for vector<Component*> (raw-pointer) members. */
class CComponentRawPtrVectorProperty : public CPropertyBase
{
public:
    CComponentRawPtrVectorProperty(RFL_Type type, size_t size, unsigned int memberOffset, const char* name)
        : CPropertyBase(type, size, memberOffset, name)
    {
    }

    void Read(IRFL_Parser* doc, CReflectedBase* obj) override
    {
        doc->ReadComponentRawPtrArray(*this, obj);
    }

    void Write(IRFL_Parser* doc, CReflectedBase* obj) override
    {
        doc->WriteComponentRawPtrArray(*this, obj);
    }

    const char* GetTypeAsString() override { return "vector_component_raw_ptr"; }
};

#endif
