#include "AIAssistantVisualizer.h"

#include "imgui.h"

#include <sstream>

// ─── Construction ─────────────────────────────────────────────────────────────

AIAssistantVisualizer::AIAssistantVisualizer(AIAssistantService& service)
    : m_service(service)
{
}

// ─── Render ───────────────────────────────────────────────────────────────────

bool AIAssistantVisualizer::Render(bool* isOpen)
{
    ImGui::SetNextWindowSize(ImVec2(600, 500), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(GetName(), isOpen))
    {
        ImGui::End();
        return false;
    }

    const bool querying = m_service.IsQuerying();

    // ── Prompt input ─────────────────────────────────────────────────────────
    ImGui::TextUnformatted("Prompt:");
    ImGui::SameLine();
    ImGui::TextDisabled("(ANTHROPIC_API_KEY env var required)");

    ImGui::PushItemWidth(-1.0f);
    ImGui::InputTextMultiline("##prompt", m_promptBuf, sizeof(m_promptBuf),
        ImVec2(-1.0f, 120.0f));
    ImGui::PopItemWidth();

    // ── Buttons ───────────────────────────────────────────────────────────────
    if (querying) ImGui::BeginDisabled();

    if (ImGui::Button("Submit"))
        SubmitQuery();

    if (m_diffLines && !m_diffLines->empty())
    {
        ImGui::SameLine();
        if (ImGui::Button("Explain Diff"))
        {
            // Build a unified-diff style prompt from the current DiffLines
            std::ostringstream oss;
            oss << "Explain the following code diff. Focus on what changed and why it might matter:\n\n";
            int lineCount = 0;
            for (const auto& dl : *m_diffLines)
            {
                if (lineCount++ > 200) { oss << "... (truncated)\n"; break; }
                switch (dl.type)
                {
                case DiffLine::Type::Context:
                    oss << "  " << dl.lineA << '\n';
                    break;
                case DiffLine::Type::OnlyA:
                    oss << "- " << dl.lineA << '\n';
                    break;
                case DiffLine::Type::OnlyB:
                    oss << "+ " << dl.lineB << '\n';
                    break;
                }
            }
            const std::string prompt = oss.str();
            strncpy_s(m_promptBuf, prompt.c_str(), sizeof(m_promptBuf) - 1);
            SubmitQuery();
        }
    }

    if (ImGui::Button("Clear"))
    {
        m_promptBuf[0] = '\0';
        m_response.clear();
        m_hasError = false;
    }

    if (querying) ImGui::EndDisabled();

    if (querying)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("Querying...");
    }

    ImGui::Separator();

    // ── Response area ─────────────────────────────────────────────────────────
    ImGui::TextUnformatted("Response:");

    const float footerHeight = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild("##response", ImVec2(0, -footerHeight), true,
        ImGuiWindowFlags_HorizontalScrollbar);

    if (!m_response.empty())
        RenderResponseText(m_response);

    if (m_scrollToBottom)
    {
        ImGui::SetScrollHereY(1.0f);
        m_scrollToBottom = false;
    }

    ImGui::EndChild();

    ImGui::End();
    return true;
}

// ─── Helpers ──────────────────────────────────────────────────────────────────

void AIAssistantVisualizer::SubmitQuery()
{
    if (m_promptBuf[0] == '\0' || m_service.IsQuerying())
        return;

    m_response.clear();
    m_hasError = false;

    m_service.Query(m_promptBuf,
        [this](const std::string& response, bool isError)
        {
            m_response       = response;
            m_hasError       = isError;
            m_scrollToBottom = true;
        });
}

void AIAssistantVisualizer::RenderResponseText(const std::string& text)
{
    if (m_hasError)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
        ImGui::TextUnformatted(text.c_str());
        ImGui::PopStyleColor();
        return;
    }

    // Render line-by-line so ImGui wraps correctly
    const char* begin = text.c_str();
    const char* end   = begin + text.size();
    const char* lineStart = begin;

    while (lineStart < end)
    {
        const char* lineEnd = lineStart;
        while (lineEnd < end && *lineEnd != '\n')
            ++lineEnd;

        ImGui::TextUnformatted(lineStart, lineEnd);

        lineStart = (lineEnd < end) ? lineEnd + 1 : end;
    }
}
