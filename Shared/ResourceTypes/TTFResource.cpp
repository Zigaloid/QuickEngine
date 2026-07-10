#include "TTFResource.h"
#include "CoreSystem/CoreSystem.h"
#include "../Rendering/FontSystem.h"

REFL_DEFINE_OBJECT(CTTFResourceReference)
REFL_DEFINE_END

void CTTFResource::Finalize()
{
	if (GetLoadedSize() == 0 || GetData().empty())
	{
		m_isFinalized = false;
		return;
	}

	if (!FontSystem::Instance().IsInitialized())
	{
		m_isFinalized = false;
		return;
	}

	auto& fontMgr = FontSystem::Instance().GetFontManager();

	m_trueTypeHandle = fontMgr.createTtf(GetData().data(), static_cast<uint32_t>(GetLoadedSize()));
	if (m_trueTypeHandle.idx == UINT16_MAX)
	{
		m_isFinalized = false;
		return;
	}

	m_isFinalized = true;
}
