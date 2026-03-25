#ifndef REFLECTION_TYPES_INT_H
#define REFLECTION_TYPES_INT_H

class CIntProperty : public CPropertyBase
{
public:
    CIntProperty(  RFL_Type type, size_t size, unsigned int memberOffset, const char *name  ) : CPropertyBase(type, size, memberOffset, name)
    {
    }
    virtual void Read(IRFL_Parser*doc, CReflectedBase* obj)
    {                
        doc->ReadInt(*this,obj);
    }
    virtual void Write(IRFL_Parser* doc, CReflectedBase *obj )
    {
        doc->WriteInt(*this, obj);
    }
	virtual const char * GetTypeAsString() { return "int"; }
};

#endif


