#include "EntityDefinition.h"
#include "Reflection/Reflection.h"

REFL_DEFINE_OBJECT(CEntityDefinition)
	REFL_DEFINE_STRING_MEMBER(CEntityDefinition, m_name),
	REFL_DEFINE_VECTOR4_MEMBER(CEntityDefinition, m_color)
REFL_DEFINE_END