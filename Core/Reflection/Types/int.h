#ifndef REFLECTION_TYPES_INT_H
#define REFLECTION_TYPES_INT_H

/** @brief Reflected property descriptor for int members. */
class CIntProperty : public CPropertyBase
{
public:
    CIntProperty(RFL_Type type, size_t size, unsigned int memberOffset, const char* name)
        : CPropertyBase(type, size, memberOffset, name)
    {
    }

    void Read(IRFL_Parser* doc, CReflectedBase* obj) override
    {
        doc->ReadInt(*this, obj);
    }

    void Write(IRFL_Parser* doc, CReflectedBase* obj) override
    {
        doc->WriteInt(*this, obj);
    }

    const char* GetTypeAsString() override { return "int"; }
};

#endif
