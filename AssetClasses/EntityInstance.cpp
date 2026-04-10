#include "EntityInstance.h"
#include "Reflection/Reflection.h"
#include "CoreSystem/CoreSystem.h"

#include <bx/math.h>
#include <iostream>

// ── CEntityInstance ────────────────────────────────────────────────
REGISTER_COMPONENT(CEntityInstance, "Entity", "Game Entity");

REFL_DEFINE_OBJECT(CEntityInstance)
	REFL_DEFINE_STRING_MEMBER(CEntityInstance, m_entityDefinition),
	REFL_DEFINE_STRING_MEMBER(CEntityInstance, m_name),
	REFL_DEFINE_VECTOR4_MEMBER(CEntityInstance, m_color)
REFL_DEFINE_END

