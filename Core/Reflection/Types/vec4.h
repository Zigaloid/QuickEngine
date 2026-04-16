#ifndef REFLECTION_TYPES_VEC4_H
#define REFLECTION_TYPES_VEC4_H

/** @brief Reflected property descriptor for Vector4f members. */
class CVector4Property : public CPropertyBase
{
public:
    CVector4Property(RFL_Type type, size_t size, unsigned int memberOffset, const char* name)
        : CPropertyBase(type, size, memberOffset, name)
    {
    }

    void Read(IRFL_Parser* doc, CReflectedBase* obj) override
    {
        doc->ReadVector4(*this, obj);
    }

    void Write(IRFL_Parser* doc, CReflectedBase* obj) override
    {
        doc->WriteVector4(*this, obj);
    }

    const char* GetTypeAsString() override { return "vector4"; }
};

#endif
