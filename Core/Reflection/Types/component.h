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

// Custom property type for Component-derived objects that uses ComponentManager for creation
class CComponentProperty : public CPropertyBase
{
public:
    CComponentProperty(RFL_Type type, size_t size, unsigned int memberOffset, const char *name) 
        : CPropertyBase(type, size, memberOffset, name)
    {
    }

    virtual void Read(IRFL_Parser* doc, CReflectedBase *obj) override
    {
        doc->ReadComponent(*this, obj);
    }

    virtual void Write(IRFL_Parser* doc, CReflectedBase *obj) override
    {
        doc->WriteComponent(*this, obj);
    }

    virtual const char* GetTypeAsString() override { return "component"; }
};

// Custom property type for Component pointer (managed by ComponentManager)
class CComponentPtrProperty : public CPropertyBase
{
public:
    CComponentPtrProperty(RFL_Type type, size_t size, unsigned int memberOffset, const char *name) 
        : CPropertyBase(type, size, memberOffset, name)
    {
    }

    virtual void Read(IRFL_Parser* doc, CReflectedBase *obj) override
    {
        doc->ReadComponentPtr(*this, obj);
    }

    virtual void Write(IRFL_Parser* doc, CReflectedBase *obj) override
    {
        doc->WriteComponentPtr(*this, obj);
    }

    virtual const char* GetTypeAsString() override { return "component_ptr"; }
};

// Custom property type for vector of Component unique_ptrs (managed by ComponentManager)
class CComponentPtrVectorProperty : public CPropertyBase
{
public:
    CComponentPtrVectorProperty(RFL_Type type, size_t size, unsigned int memberOffset, const char *name) 
        : CPropertyBase(type, size, memberOffset, name)
    {
    }

    virtual void Read(IRFL_Parser* doc, CReflectedBase *obj) override
    {
        doc->ReadComponentPtrArray(*this, obj);
    }

    virtual void Write(IRFL_Parser* doc, CReflectedBase* obj) override
    {
        doc->WriteComponentPtrArray(*this, obj);
    }

    virtual const char* GetTypeAsString() override { return "vector_component_ptr"; }
};

// Custom property type for vector of raw Component pointers (managed by ComponentManager)
class CComponentRawPtrVectorProperty : public CPropertyBase
{
public:
    CComponentRawPtrVectorProperty(RFL_Type type, size_t size, unsigned int memberOffset, const char *name) 
        : CPropertyBase(type, size, memberOffset, name)
    {
    }

    virtual void Read(IRFL_Parser* doc, CReflectedBase *obj) override
    {
        doc->ReadComponentRawPtrArray(*this, obj);
    }

    virtual void Write(IRFL_Parser* doc, CReflectedBase* obj) override
    {
        doc->WriteComponentRawPtrArray(*this, obj);
    }

    virtual const char* GetTypeAsString() override { return "vector_component_raw_ptr"; }
};

#endif