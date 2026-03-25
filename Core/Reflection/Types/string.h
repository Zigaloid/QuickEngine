#ifndef REFLECTION_TYPES_STRING_H
#define REFLECTION_TYPES_STRING_H

class CStringProperty : public CPropertyBase
{
public:
	CStringProperty(RFL_Type type, size_t size, unsigned int memberOffset, const char *name) : CPropertyBase(type, size, memberOffset, name)
	{
	}
	virtual void Read(IRFL_Parser* doc, CReflectedBase *obj)
	{		
		doc->ReadString(*this,obj);

	}
	virtual void Write(IRFL_Parser* doc, CReflectedBase *obj)
	{
		doc->WriteString(*this, obj);
	}
	virtual const char * GetTypeAsString() { return "string"; }
};

#endif
