#ifndef REFLECTION_TYPES_MAT4_H
#define REFLECTION_TYPES_MAT4_H

/** @brief Reflected property descriptor for Matrix4f members. */
class CMatrix4Property : public CPropertyBase
{
public:
    CMatrix4Property(RFL_Type type, size_t size, unsigned int memberOffset, const char* name)
        : CPropertyBase(type, size, memberOffset, name)
    {
    }

    void Read(IRFL_Parser* doc, CReflectedBase* obj) override
    {
        doc->ReadMatrix4(*this, obj);
    }

    void Write(IRFL_Parser* doc, CReflectedBase* obj) override
    {
        doc->WriteMatrix4(*this, obj);
    }

    const char* GetTypeAsString() override { return "CMatrix4"; }
};

#endif
