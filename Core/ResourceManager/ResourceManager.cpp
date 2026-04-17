#include "ResourceManager.h"
#include "CoreSystem\CoreSystem.h"


REFL_DEFINE_OBJECT(ResourceSystem::Resource)
REFL_DEFINE_END

// ── CResourceReference ────────────────────────────────────────────
REFL_DEFINE_OBJECT(CResourceReference)
	REFL_DEFINE_STRING_MEMBER(CResourceReference, m_resourceFileName),
REFL_DEFINE_END
