#include "FontResource.h"
#include "CoreSystem/CoreSystem.h"
#include "CoreSystem/AppConfig.h"
#include "../Rendering/FontSystem.h"

#include <iostream>

// ── Reflection registration ──────────────────────────────────────────────────

REFL_DEFINE_OBJECT(CFontResource)
	REFL_DEFINE_OBJECT_MEMBER(CFontResource, m_ttfResource),
	REFL_DEFINE_FLOAT_MEMBER(CFontResource, m_pixelSize),
	REFL_DEFINE_INT_MEMBER(CFontResource, m_fontType),
	REFL_DEFINE_FLOAT_MEMBER(CFontResource, m_sdfPadding),
REFL_DEFINE_END

REFL_DEFINE_OBJECT(CFontResourceReference)
REFL_DEFINE_END

// ── CFontResource ────────────────────────────────────────────────────────────

CFontResource::CFontResource(const std::string& path)
	: Resource(path)
{
}

CFontResource::~CFontResource()
{
	// Handles are indices into the FontManager's internal arrays.
	// The FontManager owns the actual resources; we just release our claim
	// when the FontSystem is shut down. No explicit destroy needed here
	// because all font data lives in the FontManager's shared cube atlas.
}

bool CFontResource::Update(FileSystem::FileSystemManager& fileSystem)
{
	if (m_isLoaded)
		return true;

	const std::string& path = GetPath();
	const bool isJson = path.find(".font.obj.json") != std::string::npos;

	if (isJson)
	{
		// Deserialize .font.json config — populates m_ttfResource, m_pixelSize,
		// m_fontType, m_sdfPadding from reflected members. The TTF resource
		// is requested asynchronously via CTypedResourceReference::OnLoaded().
		auto readResult = SafeRead(path);
		if (readResult.IsError())
		{
			std::cerr << "CFontResource: Failed to read " << path
				<< " - " << readResult.GetError().message << std::endl;
			return false;
		}
	}
	else
	{
		// Direct .ttf load — use default config values.
		auto ttfResult = fileSystem.ReadAllBytes(path);
		if (!ttfResult.IsSuccess())
		{
			std::cerr << "CFontResource: Failed to read TTF " << path << std::endl;
			return false;
		}

		m_ttfData = ttfResult.GetValue();
		// m_pixelSize, m_fontType, m_sdfPadding remain at their defaults.
	}

	m_isLoaded = true;
	return true;
}

void CFontResource::Finalize()
{
	if (!m_isLoaded)
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
	TrueTypeHandle ttfHandle = { UINT16_MAX };

	// Attempt to get the TrueTypeHandle from the referenced TTF resource.
	auto ttfRes = m_ttfResource.GetResourceAs<CTTFResource>();
	if (ttfRes && ttfRes->IsFinalized())
	{
		ttfHandle = ttfRes->GetTrueTypeHandle();
	}
	else if (!m_ttfData.empty())
	{
		// Fallback: direct .ttf loading (legacy path).
		ttfHandle = fontMgr.createTtf(m_ttfData.data(), static_cast<uint32_t>(m_ttfData.size()));
	}

	if (ttfHandle.idx == UINT16_MAX)
	{
		m_isFinalized = false;
		return;
	}

	m_trueTypeHandle = ttfHandle;

	m_fontHandle = fontMgr.createFontByPixelSize(
		m_trueTypeHandle,
		0,                                        // typefaceIndex
		static_cast<uint32_t>(m_pixelSize),
		m_fontType,
		static_cast<uint16_t>(m_sdfPadding),       // glyphWidthPadding
		static_cast<uint16_t>(m_sdfPadding));       // glyphHeightPadding

	if (m_fontHandle.idx == UINT16_MAX)
	{
		m_isFinalized = false;
		return;
	}

	// Preload common ASCII printable glyphs so they are already in the atlas
	// when the first text component tries to append text.
	static const wchar_t kAscii[] =
		L" !\"#$%&'()*+,-./0123456789:;<=>?"
		L"@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_"
		L"`abcdefghijklmnopqrstuvwxyz{|}~";
	fontMgr.preloadGlyph(m_fontHandle, kAscii);

	// Release the raw TTF data — glyphs are baked into the atlas now.
	m_ttfData.clear();
	m_ttfData.shrink_to_fit();

	m_isFinalized = true;
}
