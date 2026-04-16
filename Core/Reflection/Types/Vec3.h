#ifndef REFLECTION_TYPES_VEC3_H
#define REFLECTION_TYPES_VEC3_H

/** @brief Reflected property descriptor for Vector3f members. */
class CVector3Property : public CPropertyBase
{
public:
    CVector3Property(RFL_Type type, size_t size, unsigned int memberOffset, const char* name)
        : CPropertyBase(type, size, memberOffset, name)
    {
    }

    void Read(IRFL_Parser* doc, CReflectedBase* obj) override
    {
        doc->ReadVector3(*this, obj);
    }

    void Write(IRFL_Parser* doc, CReflectedBase* obj) override
    {
        doc->WriteVector3(*this, obj);
    }

    const char* GetTypeAsString() override { return "vector3"; }
};

#endif
