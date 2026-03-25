#ifndef REFLECTION_TYPES_STRINGBOOLMAP_H
#define REFLECTION_TYPES_STRINGBOOLMAP_H

class CStringBoolMapProperty : public CPropertyBase
{
public:
	CStringBoolMapProperty(RFL_Type type, size_t size, unsigned int memberOffset, const char* name) : CPropertyBase(type, size, memberOffset, name)
	{
	}
	virtual void Read(IRFL_Parser* doc, CReflectedBase* obj)
	{
		doc->ReadStringBoolMap(*this, obj);
	}
	virtual void Write(IRFL_Parser* doc, CReflectedBase* obj)
	{
		doc->WriteStringBoolMap(*this, obj);
	}
	virtual const char* GetTypeAsString() { return "map_string_bool"; }
};

#endif