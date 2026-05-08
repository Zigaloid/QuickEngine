#pragma once

#include "IImGuiVisualizer.h"
#include "AIAssistantService.h"
#include "CodeCompareVisualizer.h"

#include <string>
#include <vector>

/**
 * @brief ImGui panel for querying the Claude AI assistant.
 *
 * Provides a prompt input, response display, and an "Explain Diff" button
 * that automatically builds context from a set of DiffLines.
 */
class AIAssistantVisualizer : public ImGuiVisualizers::IImGuiVisualizer
{
public:
    explicit AIAssistantVisualizer(AIAssistantService& service);

    const char* GetName()         const override { return "AI Assistant"; }
    const char* GetMenuCategory() const override { return "Tools"; }

    void Initialize() override {}
    void Shutdown()   override {}
    void Update(float deltaTime) override { (void)deltaTime; }

    bool Render(bool* isOpen) override;

    /**
     * @brief Feed diff lines so the "Explain Diff" button can build context automatically.
     * @param lines Pointer to the current diff line set (may be nullptr).
     */
    void SetDiffContext(const std::vector<DiffLine>* lines) { m_diffLines = lines; }

private:
    void SubmitQuery();
    void RenderResponseText(const std::string& text);

    AIAssistantService&        m_service;
    const std::vector<DiffLine>* m_diffLines = nullptr;

    char        m_promptBuf[4096] = {};
    std::string m_response;
    bool        m_hasError = false;
    bool        m_scrollToBottom = false;
};
