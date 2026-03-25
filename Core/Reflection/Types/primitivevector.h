#ifndef REFLECTION_TYPES_PRIMITIVEVECTOR_H
#define REFLECTION_TYPES_PRIMITIVEVECTOR_H

class CIntVectorProperty : public CPropertyBase
{
public:
	CIntVectorProperty(RFL_Type type, size_t size, unsigned int memberOffset, const char* name) : CPropertyBase(type, size, memberOffset, name)
	{
	}
	virtual void Read(IRFL_Parser* doc, CReflectedBase* obj)
	{
		doc->ReadIntVector(*this, obj);
	}
	virtual void Write(IRFL_Parser* doc, CReflectedBase* obj)
	{
		doc->WriteIntVector(*this, obj);
	}
	virtual const char* GetTypeAsString() { return "vector_int"; }
};

class CFloatVectorProperty : public CPropertyBase
{
public:
	CFloatVectorProperty(RFL_Type type, size_t size, unsigned int memberOffset, const char* name) : CPropertyBase(type, size, memberOffset, name)
	{
	}
	virtual void Read(IRFL_Parser* doc, CReflectedBase* obj)
	{
		doc->ReadFloatVector(*this, obj);
	}
	virtual void Write(IRFL_Parser* doc, CReflectedBase* obj)
	{
		doc->WriteFloatVector(*this, obj);
	}
	virtual const char* GetTypeAsString() { return "vector_float"; }
};

class CBoolVectorProperty : public CPropertyBase
{
public:
	CBoolVectorProperty(RFL_Type type, size_t size, unsigned int memberOffset, const char* name) : CPropertyBase(type, size, memberOffset, name)
	{
	}
	virtual void Read(IRFL_Parser* doc, CReflectedBase* obj)
	{
		doc->ReadBoolVector(*this, obj);
	}
	virtual void Write(IRFL_Parser* doc, CReflectedBase* obj)
	{
		doc->WriteBoolVector(*this, obj);
	}
	virtual const char* GetTypeAsString() { return "vector_bool"; }
};

class CStringVectorProperty : public CPropertyBase
{
public:
	CStringVectorProperty(RFL_Type type, size_t size, unsigned int memberOffset, const char* name) : CPropertyBase(type, size, memberOffset, name)
	{
	}
	virtual void Read(IRFL_Parser* doc, CReflectedBase* obj)
	{
		doc->ReadStringVector(*this, obj);
	}
	virtual void Write(IRFL_Parser* doc, CReflectedBase* obj)
	{
		doc->WriteStringVector(*this, obj);
	}
	virtual const char* GetTypeAsString() { return "vector_string"; }
};

#endif