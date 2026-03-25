#ifndef REFLECTION_TYPES_VEC4_H
#define REFLECTION_TYPES_VEC4_H

class CVector4Property : public CPropertyBase
{
public:
	CVector4Property(RFL_Type type, size_t size, unsigned int memberOffset, const char *name ) : CPropertyBase(type, size,memberOffset, name )
	{
	}
	virtual void Read(IRFL_Parser*doc, CReflectedBase *obj)
	{		
		doc->ReadVector4(*this, obj);	
	}
	virtual void Write(IRFL_Parser* doc, CReflectedBase *obj)
	{
		doc->WriteVector4(*this, obj);		
	}
	virtual const char *GetTypeAsString() { return "vector4"; }	
};

#endif
