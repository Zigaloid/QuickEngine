#ifndef REFLECTION_TYPES_FLOAT_H
#define REFLECTION_TYPES_FLOAT_H

class CFloatProperty : public CPropertyBase
{
public:
	CFloatProperty(RFL_Type type, size_t size, unsigned int memberOffset, const char *name) : CPropertyBase(type, size, memberOffset, name)
	{
	}
	virtual void Read(IRFL_Parser*doc, CReflectedBase *obj)
	{				
		doc->ReadFloat(*this,obj);
	}
	virtual void Write(IRFL_Parser*doc, CReflectedBase *obj)
	{		
		doc->WriteFloat(*this, obj);
	}
	virtual const char *GetTypeAsString() { return "float"; }
};

#endif
