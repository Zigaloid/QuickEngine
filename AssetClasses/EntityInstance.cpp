#include "EntityInstance.h"
#include "Reflection/Reflection.h"

REGISTER_COMPONENT(CEntityInstance, "Entity", "Game Entity");

REFL_DEFINE_OBJECT(CEntityInstance)
	REFL_DEFINE_STRING_MEMBER(CEntityInstance, m_entityDefinition),
	REFL_DEFINE_STRING_MEMBER(CEntityInstance, m_name),
	REFL_DEFINE_VECTOR4_MEMBER(CEntityInstance, m_color)
REFL_DEFINE_END


REGISTER_COMPONENT(CMeshComponent, "Mesh", "Graphics");

REFL_DEFINE_OBJECT(CMeshComponent)
	REFL_DEFINE_STRING_MEMBER(CMeshComponent, m_meshResource),
	REFL_DEFINE_STRING_MEMBER(CMeshComponent, m_materialResource),
	REFL_DEFINE_VECTOR4_MEMBER(CMeshComponent, m_color)
REFL_DEFINE_END