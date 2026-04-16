#ifndef REFLECTION_TYPES_BOOL_H
#define REFLECTION_TYPES_BOOL_H

#include "../ReflectionMap.h"

/** @brief Reflected property descriptor for bool members. */
class CBoolProperty : public CPropertyBase
{
public:
    CBoolProperty(RFL_Type type, size_t size, unsigned int memberOffset, const char* name)
        : CPropertyBase(type, size, memberOffset, name)
    {
    }

    void Read(IRFL_Parser* doc, CReflectedBase* obj) override
    {
        doc->ReadBool(*this, obj);
    }

    void Write(IRFL_Parser* doc, CReflectedBase* obj) override
    {
        doc->WriteBool(*this, obj);
    }

    const char* GetTypeAsString() override { return "bool"; }
};

#endif
