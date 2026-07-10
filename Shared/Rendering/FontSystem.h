#pragma once

#include "../bgfx_common/font/font_manager.h"
#include "../bgfx_common/font/text_buffer_manager.h"

#include <memory>

/**
 * @brief Singleton that owns the shared FontManager and TextBufferManager
 *        instances used by all CFontResource data and CUITextComponent
 *        rendering.
 *
 * Both subsystems must be created after bgfx::init() and destroyed before
 * bgfx::shutdown() because the FontManager creates a GPU cube atlas.
 */
class FontSystem
{
public:
	static FontSystem& Instance();

	FontSystem(const FontSystem&) = delete;
	FontSystem& operator=(const FontSystem&) = delete;
	FontSystem(FontSystem&&) = delete;
	FontSystem& operator=(FontSystem&&) = delete;

	bool Initialize();
	void Shutdown();
	bool IsInitialized() const { return m_initialized; }

	FontManager&       GetFontManager()       { return *m_fontManager; }
	TextBufferManager& GetTextBufferManager() { return *m_textBufferManager; }

	/** @brief Allocate a new dynamic text buffer for a component. */
	TextBufferHandle CreateTextBuffer(uint32_t fontType = FONT_TYPE_DISTANCE);

	/** @brief Release a text buffer back to the pool. */
	void ReleaseTextBuffer(TextBufferHandle handle);

private:
	FontSystem() = default;
	~FontSystem() = default;

	std::unique_ptr<FontManager>        m_fontManager;
	std::unique_ptr<TextBufferManager>  m_textBufferManager;
	bool                                m_initialized = false;
};
