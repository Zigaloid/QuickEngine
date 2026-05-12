#include "ComponentDependencyDefinition.h"
#include "Reflection/Reflection.h"

// Include primitives renderer for DebugRender implementation

// ── ComponentDependencyDefinition ───────────────────────────────────────


REFL_DEFINE_OBJECT(ComponentDependencyDefinition)
REFL_DEFINE_STRING_MEMBER(ComponentDependencyDefinition, m_dependent),
REFL_DEFINE_STRING_MEMBER(ComponentDependencyDefinition, m_dependsOn)
REFL_DEFINE_END

// ── ComponentDependencyDefinitionList ───────────────────────────────────
REFL_DEFINE_OBJECT(ComponentDependencyDefinitionList)
REFL_DEFINE_OBJECT_PTR_VECTOR_MEMBER(ComponentDependencyDefinitionList, m_dependencies, ComponentDependencyDefinition)
REFL_DEFINE_END
