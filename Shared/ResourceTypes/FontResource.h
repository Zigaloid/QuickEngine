#pragma once

#include "Reflection/ReflectionBase.h"
#include "ResourceManager/ResourceManager.h"
#include "TTFResource.h"
#include "../bgfx_common/font/font_manager.h"

#include <string>
#include <vector>
#include <cstdint>

/**
 * @brief Resource that loads a .font.json definition, reads the referenced
 *        TTF resource, and registers a FontHandle in the shared FontManager.
 *
 * The .font.json format:
 * @code
 * {
 *     "ttfResource":  { ... CTTFResourceReference },
 *     "pixelSize":  24.0,
 *     "fontType":   1024,
 *     "sdfPadding": 4.0
 * }
 * @endcode
 *
 * Usage:
 * @code
 *   auto font = resourceMgr.RequestResource<CFontResource>("fonts/roboto-24.font.json");
 *   // ... after UpdateFinalization() ...
 *   if (font->IsFinalized()) { FontHandle h = font->GetFontHandle(); }
 * @endcode
 */
class CFontResource : public ResourceSystem::Resource
{
public:
	REFL_DECLARE_OBJECT(CFontResource, ResourceSystem::Resource);

	static std::vector<std::string_view> GetSupportedExtensions()
	{
		return { ".font.json", ".font.obj.json", ".ttf" };
	}

	explicit CFontResource(const std::string& path);
	CFontResource() = default;
	~CFontResource() override;

	// ── Two-phase loading ────────────────────────────────────────────────

	bool Update(FileSystem::FileSystemManager& fileSystem) override;
	void Finalize() override;

	// ── Accessors ────────────────────────────────────────────────────────

	FontHandle GetFontHandle() const { return m_fontHandle; }
	uint32_t   GetFontType()   const { return m_fontType; }
	float      GetPixelSize()  const { return m_pixelSize; }

private:
	// ── Reflected (deserialized from .font.json) ─────────────────────────
	CTTFResourceReference m_ttfResource;
	float                 m_pixelSize   = 24.0f;
	uint32_t              m_fontType    = FONT_TYPE_DISTANCE;
	float                 m_sdfPadding  = 4.0f;

	// ── Non-reflected runtime state ──────────────────────────────────────
	std::vector<uint8_t> m_ttfData;
	TrueTypeHandle       m_trueTypeHandle = { UINT16_MAX };
	FontHandle           m_fontHandle     = { UINT16_MAX };
};

// ── Resource Reference ───────────────────────────────────────────────────────

class CFontResourceReference : public CTypedResourceReference<CFontResource>
{
public:
	REFL_DECLARE_OBJECT(CFontResourceReference, CResourceReference);
};
