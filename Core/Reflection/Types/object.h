#ifndef REFLECTION_TYPES_OBJECT_H
#define REFLECTION_TYPES_OBJECT_H

/** @brief Reflected property descriptor for embedded sub-object members. */
class CSubObjectProperty : public CPropertyBase
{
public:
    CSubObjectProperty(RFL_Type type, size_t size, unsigned int memberOffset, const char* name)
        : CPropertyBase(type, size, memberOffset, name)
    {
    }

    void Read(IRFL_Parser* doc, CReflectedBase* obj) override
    {
        doc->ReadObject(*this, obj);
    }

    void Write(IRFL_Parser* doc, CReflectedBase* obj) override
    {
        doc->WriteObject(*this, obj);
    }

    const char* GetTypeAsString() override { return "object"; }
};

/** @brief Reflected property descriptor for unique_ptr sub-object members. */
class CSubObjectPtrProperty : public CPropertyBase
{
public:
    CSubObjectPtrProperty(RFL_Type type, size_t size, unsigned int memberOffset, const char* name)
        : CPropertyBase(type, size, memberOffset, name)
    {
    }

    void Read(IRFL_Parser* doc, CReflectedBase* obj) override
    {
        doc->ReadObjectPtr(*this, obj);
    }

    void Write(IRFL_Parser* doc, CReflectedBase* obj) override
    {
        doc->WriteObjectPtr(*this, obj);
    }

    const char* GetTypeAsString() override { return "unique_ptr"; }
};

/** @brief Reflected property descriptor for vector-of-unique_ptr members. */
class CObjPtrVectorProperty : public CPropertyBase
{
public:
    CObjPtrVectorProperty(RFL_Type type, size_t size, unsigned int memberOffset, const char* name)
        : CPropertyBase(type, size, memberOffset, name)
    {
    }

    void Read(IRFL_Parser* doc, CReflectedBase* obj) override
    {
        doc->ReadObjectPtrArray(*this, obj);
    }

    void Write(IRFL_Parser* doc, CReflectedBase* obj) override
    {
        doc->WriteObjectPtrArray(*this, obj);
    }

    const char* GetTypeAsString() override { return "vector_unique_ptr"; }
};

#endif
