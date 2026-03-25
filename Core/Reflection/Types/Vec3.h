#ifndef REFLECTION_TYPES_VEC3_H
#define REFLECTION_TYPES_VEC3_H

class CVector3Property : public CPropertyBase
{
public:
	CVector3Property(RFL_Type type, size_t size, unsigned int memberOffset, const char* name) : CPropertyBase(type, size, memberOffset, name)
	{
	}
	virtual void Read(IRFL_Parser* doc, CReflectedBase* obj)
	{
		doc->ReadVector3(*this, obj);
	}
	virtual void Write(IRFL_Parser* doc, CReflectedBase* obj)
	{
		doc->WriteVector3(*this, obj);
	}
	virtual const char* GetTypeAsString() { return "vector3"; }
};

#endif