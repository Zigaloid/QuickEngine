#ifndef REFLECTION_TYPES_STRINGBOOLMAP_H
#define REFLECTION_TYPES_STRINGBOOLMAP_H

/** @brief Reflected property descriptor for std::unordered_map<std::string, bool> members. */
class CStringBoolMapProperty : public CPropertyBase
{
public:
    CStringBoolMapProperty(RFL_Type type, size_t size, unsigned int memberOffset, const char* name)
        : CPropertyBase(type, size, memberOffset, name)
    {
    }

    void Read(IRFL_Parser* doc, CReflectedBase* obj) override
    {
        doc->ReadStringBoolMap(*this, obj);
    }

    void Write(IRFL_Parser* doc, CReflectedBase* obj) override
    {
        doc->WriteStringBoolMap(*this, obj);
    }

    const char* GetTypeAsString() override { return "map_string_bool"; }
};

#endif
