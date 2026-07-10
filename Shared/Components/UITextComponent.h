#pragma once

#include "UIElementComponent.h"
#include "../ResourceTypes/FontResource.h"
#include "../bgfx_common/font/text_buffer_manager.h"
#include "Math/Vector4f.h"

#include <string>

/**
 * @brief Renders a UTF-8 text string through the shared FontSystem using the
 *        bgfx font rendering pipeline (FontManager + TextBufferManager).
 *
 * The text is submitted into the dedicated text view (BgfxUIView::kTextViewID)
 * which uses an orthographic projection, so pen positions are in pixel
 * coordinates. The component's CTransformComponent sibling supplies the
 * on-screen pixel position (X,Y = pixels from top-left, Z = depth for ordering).
 *
 * The font is loaded asynchronously through a CFontResourceReference; no text
 * is drawn until the referenced .font.json asset is fully finalized.
 */
class CUITextComponent : public CUIElementComponent
{
public:
	REFL_DECLARE_OBJECT(CUITextComponent, CUIElementComponent);
	DECLARE_COMPONENT();

	CUITextComponent() = default;
	~CUITextComponent() override = default;

	// ── Lifecycle ────────────────────────────────────────────────────────

	bool OnInitialize() override;
	void OnUpdate(double deltaTime) override;
	void OnShutdown() override;
	void Render(bgfx::ViewId viewId) override;

	// ── Public API ──────────────────────────────────────────────────────

	CFontResourceReference&       GetFontResource()       { return m_fontResource; }
	const CFontResourceReference& GetFontResource() const { return m_fontResource; }

	const std::string& GetText()  const { return m_text; }
	void SetText(const std::string& text) { m_text = text; m_textDirty = true; }

	const Vector4f& GetTextColor() const { return m_textColor; }
	void SetTextColor(const Vector4f& c) { m_textColor = c; m_textDirty = true; }

	const Vector4f& GetBackgroundColor() const { return m_bgColor; }
	void SetBackgroundColor(const Vector4f& c) { m_bgColor = c; m_textDirty = true; }

	enum class Align { Left, Center, Right };
	Align GetHorizontalAlign() const { return m_horizontalAlign; }
	void  SetHorizontalAlign(Align a) { m_horizontalAlign = a; }

	enum class VAlign { Top, Middle, Bottom };
	VAlign GetVerticalAlign() const { return m_verticalAlign; }
	void   SetVerticalAlign(VAlign v) { m_verticalAlign = v; }

private:
	void RebuildTextBuffer();
	static uint32_t PackColorRGBA(const Vector4f& c);

	// ── Reflected members ───────────────────────────────────────────────
	std::string            m_text            = "Text";
	Vector4f               m_textColor       = { 1.0f, 1.0f, 1.0f, 1.0f };
	Vector4f               m_bgColor         = { 0.0f, 0.0f, 0.0f, 0.0f };
	Align                  m_horizontalAlign = Align::Left;
	VAlign                 m_verticalAlign   = VAlign::Top;
	CFontResourceReference m_fontResource;

	// ── Non-reflected runtime state ─────────────────────────────────────
	TextBufferHandle m_textBuffer   = { UINT16_MAX };
	bool             m_textDirty    = true;
	FontHandle       m_currentFont  = { UINT16_MAX };
};
