#pragma once
#include "Reflection/ReflectionBase.h"
#include "math\Vector4f.h"

class CEntityDefinition : public CReflectedBase
{
public:
	REFL_DECLARE_OBJECT(CEntityDefinition, CReflectedBase);
private:
	std::string m_name = "undfined";	
};

