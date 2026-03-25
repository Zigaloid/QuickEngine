#ifndef REFLECTION_TYPES_MAT4_H
#define REFLECTION_TYPES_MAT4_H

class CMatrix4Property : public CPropertyBase
{
public:
	CMatrix4Property(RFL_Type type, size_t size, unsigned int memberOffset, const char *name ) : CPropertyBase(type, size, memberOffset, name )
	{
	}
    virtual void Read(IRFL_Parser* doc, CReflectedBase* obj)
    {
        doc->ReadMatrix4(*this, obj);
    }
    virtual void Write(IRFL_Parser* doc, CReflectedBase* obj)
    {
        doc->WriteMatrix4(*this, obj);
    }    		
	virtual const char * GetTypeAsString() { return "CMatrix4"; }	
};

#endif
