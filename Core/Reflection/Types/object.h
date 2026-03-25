#ifndef REFLECTION_TYPES_OBJECT_H
#define REFLECTION_TYPES_OBJECT_H

class CSubObjectProperty : public CPropertyBase
{
public:
	CSubObjectProperty(RFL_Type type, size_t size, unsigned int memberOffset, const char *name) : CPropertyBase(type, size, memberOffset, name)
	{
	}
	virtual void Read(IRFL_Parser* doc, CReflectedBase *obj)
	{		
		doc->ReadObject(*this, obj);
	}
	virtual void Write(IRFL_Parser* doc, CReflectedBase *obj)
	{		
		doc->WriteObject(*this, obj);
	}
	virtual const char * GetTypeAsString() { return "object"; }
};

class CSubObjectPtrProperty : public CPropertyBase
{
public:
	CSubObjectPtrProperty(RFL_Type type, size_t size, unsigned int memberOffset, const char *name) : CPropertyBase(type, size, memberOffset, name)
	{
	}
	virtual void Read(IRFL_Parser* doc, CReflectedBase *obj)
	{
		doc->ReadObjectPtr(*this, obj);
	}

	virtual void Write(IRFL_Parser* doc, CReflectedBase *obj)
	{	
		doc->WriteObjectPtr(*this, obj);		
	}
	virtual const char * GetTypeAsString() { return "unique_ptr"; }
};

class CObjPtrVectorProperty : public CPropertyBase
{
public:
	CObjPtrVectorProperty(RFL_Type type, size_t size, unsigned int memberOffset, const char *name) : CPropertyBase(type, size, memberOffset, name)
	{
	}
	virtual void Read(IRFL_Parser* doc, CReflectedBase *obj)
	{
		doc->ReadObjectPtrArray(*this, obj);
	}
	virtual void Write(IRFL_Parser* doc, CReflectedBase* obj)
	{
		doc->WriteObjectPtrArray(*this, obj);
	}
	virtual const char * GetTypeAsString() { return "vector_unique_ptr"; }
};

#endif
