#include "FontSystem.h"

FontSystem& FontSystem::Instance()
{
	static FontSystem instance;
	return instance;
}

bool FontSystem::Initialize()
{
	if (m_initialized)
		return true;

	m_fontManager = std::make_unique<FontManager>(512);
	m_textBufferManager = std::make_unique<TextBufferManager>(m_fontManager.get());
	m_initialized = true;
	return true;
}

void FontSystem::Shutdown()
{
	m_textBufferManager.reset();
	m_fontManager.reset();
	m_initialized = false;
}

TextBufferHandle FontSystem::CreateTextBuffer(uint32_t fontType)
{
	if (!m_initialized)
		return { UINT16_MAX };
	return m_textBufferManager->createTextBuffer(fontType, BufferType::Dynamic);
}

void FontSystem::ReleaseTextBuffer(TextBufferHandle handle)
{
	if (m_initialized)
		m_textBufferManager->destroyTextBuffer(handle);
}
