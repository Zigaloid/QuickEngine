#include "NexusLogFilterState.h"

REFL_DEFINE_OBJECT(NexusLogFilterState)
    REFL_DEFINE_STRING_BOOL_MAP_MEMBER(NexusLogFilterState, m_appVisibility),
    REFL_DEFINE_STRING_BOOL_MAP_MEMBER(NexusLogFilterState, m_pipeVisibility),
    REFL_DEFINE_STRING_BOOL_MAP_MEMBER(NexusLogFilterState, m_appCollectionEnabled),
    REFL_DEFINE_STRING_BOOL_MAP_MEMBER(NexusLogFilterState, m_pipeCollectionEnabled),
REFL_DEFINE_END