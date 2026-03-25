#ifndef REFLECTION_TYPES_BOOL_H
#define REFLECTION_TYPES_BOOL_H

#include "../ReflectionMap.h"

class CBoolProperty : public CPropertyBase
{
public:
    CBoolProperty(RFL_Type type, size_t size, unsigned int memberOffset, const char *name)
        : CPropertyBase(type, size, memberOffset, name) {}

    virtual void Read(IRFL_Parser* doc, CReflectedBase *obj) override
    {
        doc->ReadBool(*this, obj);
    }

    virtual void Write(IRFL_Parser* doc, CReflectedBase *obj) override
    {
        doc->WriteBool(*this, obj);
    }

    virtual const char* GetTypeAsString() override { return "bool"; }
};

#endif