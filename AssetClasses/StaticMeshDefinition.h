#pragma once
#include "Reflection/ReflectionBase.h"

class CStaticMeshDefinition : public CReflectedBase
{
public:
	REFL_DECLARE_OBJECT(CStaticMeshDefinition, CReflectedBase);
private:
	std::string m_name = "undfined";
};