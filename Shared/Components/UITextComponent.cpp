#include "UITextComponent.h"
#include "CoreSystem/CoreSystem.h"
#include "../Rendering/FontSystem.h"
#include "../Rendering/BgfxUIView.h"

#include <iostream>

// ── Component registration ───────────────────────────────────────────────────

REGISTER_COMPONENT(CUITextComponent, "UIText", "UI");

REFL_DEFINE_OBJECT(CUITextComponent)
	REFL_DEFINE_STRING_MEMBER(CUITextComponent, m_text),
	REFL_DEFINE_VECTOR4_MEMBER(CUITextComponent, m_textColor),
	REFL_DEFINE_VECTOR4_MEMBER(CUITextComponent, m_bgColor),
	REFL_DEFINE_OBJECT_MEMBER(CUITextComponent, m_fontResource),
REFL_DEFINE_END

// ── CUITextComponent ─────────────────────────────────────────────────────────

bool CUITextComponent::OnInitialize()
{
	if (!CUIElementComponent::OnInitialize())
		return false;

	m_textBuffer = FontSystem::Instance().CreateTextBuffer(FONT_TYPE_DISTANCE);
	if (m_textBuffer.idx == UINT16_MAX)
	{
		std::cerr << "CUITextComponent: Failed to create text buffer" << std::endl;
		return false;
	}

	m_textDirty = true;
	return true;
}

void CUITextComponent::OnUpdate(double deltaTime)
{
	CUIElementComponent::OnUpdate(deltaTime);

	auto* renderFunctionQueue = Core::CoreSystem::GetRenderFunctionQueue();
	if (renderFunctionQueue)
	{
		renderFunctionQueue->AddFunction([this]()
			{
				Render(Rendering::BgfxUIView::GetTextViewID());
			}, "CUITextComponent::Render");
	}
}

void CUITextComponent::OnShutdown()
{
	if (m_textBuffer.idx != UINT16_MAX)
	{
		FontSystem::Instance().ReleaseTextBuffer(m_textBuffer);
		m_textBuffer = { UINT16_MAX };
	}

	CUIElementComponent::OnShutdown();
}

void CUITextComponent::Render(bgfx::ViewId /*viewId*/)
{
	if (!FontSystem::Instance().IsInitialized())
		return;

	auto fontResPtr = m_fontResource.GetResourceAs<CFontResource>();
	if (!fontResPtr || !fontResPtr->IsFinalized())
		return;

	FontHandle fontHandle = fontResPtr->GetFontHandle();
	if (fontHandle.idx == UINT16_MAX)
		return;

	auto& tbMgr = FontSystem::Instance().GetTextBufferManager();

	if (m_currentFont.idx != fontHandle.idx)
	{
		m_currentFont = fontHandle;
		m_textDirty = true;
	}

	auto modelMatrix = GetModelMatrix();
	if (!modelMatrix)
		return;

	const float* m = modelMatrix->GetData().data();
	const float penX = m[12];
	const float penY = m[13];

	if (m_textDirty)
		RebuildTextBuffer();

	tbMgr.setPenPosition(m_textBuffer, penX, penY);
	tbMgr.submitTextBuffer(m_textBuffer, Rendering::BgfxUIView::GetTextViewID());
}

void CUITextComponent::RebuildTextBuffer()
{
	auto& tbMgr = FontSystem::Instance().GetTextBufferManager();

	tbMgr.clearTextBuffer(m_textBuffer);
	tbMgr.setPenPosition(m_textBuffer, 0.0f, 0.0f);
	tbMgr.setTextColor(m_textBuffer, PackColorRGBA(m_textColor));

	if (m_bgColor.w > 0.0f)
	{
		tbMgr.setBackgroundColor(m_textBuffer, PackColorRGBA(m_bgColor));
		tbMgr.setStyle(m_textBuffer, STYLE_BACKGROUND);
	}
	else
	{
		tbMgr.setStyle(m_textBuffer, STYLE_NORMAL);
	}

	if (!m_text.empty())
		tbMgr.appendText(m_textBuffer, m_currentFont, m_text.c_str());

	m_textDirty = false;
}

uint32_t CUITextComponent::PackColorRGBA(const Vector4f& c)
{
	const auto clamp = [](float v) -> uint32_t {
		if (v <= 0.0f) return 0;
		if (v >= 1.0f) return 255;
		return static_cast<uint32_t>(v * 255.0f);
	};

	return (clamp(c.x) << 24)
		 | (clamp(c.y) << 16)
		 | (clamp(c.z) << 8)
		 |  clamp(c.w);
}
