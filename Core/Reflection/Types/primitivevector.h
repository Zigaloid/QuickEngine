#ifndef REFLECTION_TYPES_PRIMITIVEVECTOR_H
#define REFLECTION_TYPES_PRIMITIVEVECTOR_H

/** @brief Reflected property descriptor for std::vector<int> members. */
class CIntVectorProperty : public CPropertyBase
{
public:
    CIntVectorProperty(RFL_Type type, size_t size, unsigned int memberOffset, const char* name)
        : CPropertyBase(type, size, memberOffset, name)
    {
    }

    void Read(IRFL_Parser* doc, CReflectedBase* obj) override
    {
        doc->ReadIntVector(*this, obj);
    }

    void Write(IRFL_Parser* doc, CReflectedBase* obj) override
    {
        doc->WriteIntVector(*this, obj);
    }

    const char* GetTypeAsString() override { return "vector_int"; }
};

/** @brief Reflected property descriptor for std::vector<float> members. */
class CFloatVectorProperty : public CPropertyBase
{
public:
    CFloatVectorProperty(RFL_Type type, size_t size, unsigned int memberOffset, const char* name)
        : CPropertyBase(type, size, memberOffset, name)
    {
    }

    void Read(IRFL_Parser* doc, CReflectedBase* obj) override
    {
        doc->ReadFloatVector(*this, obj);
    }

    void Write(IRFL_Parser* doc, CReflectedBase* obj) override
    {
        doc->WriteFloatVector(*this, obj);
    }

    const char* GetTypeAsString() override { return "vector_float"; }
};

/** @brief Reflected property descriptor for std::vector<bool> members. */
class CBoolVectorProperty : public CPropertyBase
{
public:
    CBoolVectorProperty(RFL_Type type, size_t size, unsigned int memberOffset, const char* name)
        : CPropertyBase(type, size, memberOffset, name)
    {
    }

    void Read(IRFL_Parser* doc, CReflectedBase* obj) override
    {
        doc->ReadBoolVector(*this, obj);
    }

    void Write(IRFL_Parser* doc, CReflectedBase* obj) override
    {
        doc->WriteBoolVector(*this, obj);
    }

    const char* GetTypeAsString() override { return "vector_bool"; }
};

/** @brief Reflected property descriptor for std::vector<std::string> members. */
class CStringVectorProperty : public CPropertyBase
{
public:
    CStringVectorProperty(RFL_Type type, size_t size, unsigned int memberOffset, const char* name)
        : CPropertyBase(type, size, memberOffset, name)
    {
    }

    void Read(IRFL_Parser* doc, CReflectedBase* obj) override
    {
        doc->ReadStringVector(*this, obj);
    }

    void Write(IRFL_Parser* doc, CReflectedBase* obj) override
    {
        doc->WriteStringVector(*this, obj);
    }

    const char* GetTypeAsString() override { return "vector_string"; }
};

#endif
