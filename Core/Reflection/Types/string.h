#ifndef REFLECTION_TYPES_STRING_H
#define REFLECTION_TYPES_STRING_H

/** @brief Reflected property descriptor for std::string members. */
class CStringProperty : public CPropertyBase
{
public:
    CStringProperty(RFL_Type type, size_t size, unsigned int memberOffset, const char* name)
        : CPropertyBase(type, size, memberOffset, name)
    {
    }

    void Read(IRFL_Parser* doc, CReflectedBase* obj) override
    {
        doc->ReadString(*this, obj);
    }

    void Write(IRFL_Parser* doc, CReflectedBase* obj) override
    {
        doc->WriteString(*this, obj);
    }

    const char* GetTypeAsString() override { return "string"; }
};

#endif
