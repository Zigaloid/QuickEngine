#ifndef REFLECTION_TYPES_FLOAT_H
#define REFLECTION_TYPES_FLOAT_H

/** @brief Reflected property descriptor for float members. */
class CFloatProperty : public CPropertyBase
{
public:
    CFloatProperty(RFL_Type type, size_t size, unsigned int memberOffset, const char* name)
        : CPropertyBase(type, size, memberOffset, name)
    {
    }

    void Read(IRFL_Parser* doc, CReflectedBase* obj) override
    {
        doc->ReadFloat(*this, obj);
    }

    void Write(IRFL_Parser* doc, CReflectedBase* obj) override
    {
        doc->WriteFloat(*this, obj);
    }

    const char* GetTypeAsString() override { return "float"; }
};

#endif
