#include "ProfilerSessionManager.h"
#include "ProfilerViewUtils.h"
#include "CoreSystem/CoreSystem.h"
#include "CoreSystem/FunctionCallManager.h"
#include "Net/NexusClient.h"
#include "DebugChannel/DebugChannel.h"
#include "FileSystem/StandardFileSystem.h"
#include "SharedNexusDefines.h"

#include "imgui.h"
#include <algorithm>
#include <sstream>

using namespace Core;

#pragma optimize("", off)

extern DebugChannels::CDebugChannel ProfilerDebug;

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

ProfilerSessionManager::ProfilerSessionManager()
{
}

// ---------------------------------------------------------------------------
// IImGuiVisualizer lifecycle
// ---------------------------------------------------------------------------

void ProfilerSessionManager::Initialize()
{
    DECLARE_FUNC_VLOW();
    if (m_initialized) return;

    // Subscribe to Nexus once — the manager owns the pipe, not individual sessions
    NEXUS_SUBSCRIBE_CALLBACK(PROFILER_PACKET_PIPE, "ANY", handleProfilerPacket);
    NEXUS_SUBSCRIBE_BINARY_CALLBACK(PROFILER_PACKET_PIPE, "ANY", handleProfilerBinaryPacket);

    registerManagerActions();

    // Create an initial live session so there's always a tab open
    auto* session = NewSession("Live Capture");
    session->SetLiveSession(true);

    m_initialized = true;
    ProfilerDebug.printf("ProfilerSessionManager initialized\n");
}

void ProfilerSessionManager::Shutdown()
{
    DECLARE_FUNC_VLOW();
    if (!m_initialized) return;

    m_sessions.clear();
    m_initialized = false;
    ProfilerDebug.printf("ProfilerSessionManager shutdown\n");
}

// ---------------------------------------------------------------------------
// Render — tabbed document interface
// ---------------------------------------------------------------------------

bool ProfilerSessionManager::Render(bool* isOpen)
{
    ImGuiWindowFlags wflags = ImGuiWindowFlags_MenuBar;

    // Suppress mouse-wheel scrolling on the parent window when Ctrl is held,
    // so the flame graph child can use it for zooming.
    if (ImGui::IsKeyDown(ImGuiMod_Ctrl))
        wflags |= ImGuiWindowFlags_NoScrollWithMouse;

    if (!ImGui::Begin(GetName(), isOpen, wflags))
    {
        ImGui::End();
        return false;
    }

    // Shared menu bar (manager-level actions + active session actions)
    if (ImGui::BeginMenuBar())
    {
        m_actionManager.RenderMenuBar();

        // Also render the active session's menu bar entries
        if (auto* active = GetActiveSession())
        {
            active->GetActionManager().RenderMenuBar();
        }
        ImGui::EndMenuBar();
    }

    // ---- Tab bar ----
    ImGuiTabBarFlags tabFlags = ImGuiTabBarFlags_Reorderable
                              | ImGuiTabBarFlags_AutoSelectNewTabs
                              | ImGuiTabBarFlags_FittingPolicyScroll;

    if (ImGui::BeginTabBar("##SessionTabs", tabFlags))
    {
        for (size_t i = 0; i < m_sessions.size(); /* increment in loop */)
        {
            auto& session = m_sessions[i];

            // Build tab label: "DisplayName" or "DisplayName*" if dirty
            std::string label = session->GetDisplayName();
            if (session->IsDirty())
                label += "*";
            label += "###Session" + std::to_string(i);

            bool open = true;
            ImGuiTabItemFlags itemFlags = 0;

            if (ImGui::BeginTabItem(label.c_str(), &open, itemFlags))
            {
                // This tab is now the active one
                m_activeIndex = i;

                // Render the session content inside the tab
                session->RenderSessionContent();

                ImGui::EndTabItem();
            }

            if (!open)
            {
                // Tab close button was clicked
                CloseSession(i);
                // Don't increment — the vector shifted
                if (m_activeIndex >= m_sessions.size() && !m_sessions.empty())
                    m_activeIndex = m_sessions.size() - 1;
            }
            else
            {
                ++i;
            }
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
    return true;
}

// ---------------------------------------------------------------------------
// Session management
// ---------------------------------------------------------------------------

ProfilerSession* ProfilerSessionManager::NewSession(const std::string& displayName)
{
    auto session = std::make_unique<ProfilerSession>();

    std::string name = displayName;
    if (name == "Untitled")
    {
        ++m_untitledCounter;
        if (m_untitledCounter > 1)
            name = "Untitled " + std::to_string(m_untitledCounter);
    }

    session->SetDisplayName(name);
    // Register per-session actions (file save, view modes, etc.)
    // We call the session's own registration which sets up its action manager.
    session->Initialize();

    m_sessions.push_back(std::move(session));
    m_activeIndex = m_sessions.size() - 1;
    return m_sessions.back().get();
}

ProfilerSession* ProfilerSessionManager::OpenSession(const std::string& filePath)
{
    auto* session = NewSession("Loading...");
    if (!session->LoadSession(filePath))
    {
        // Load failed — remove the session we just created
        m_sessions.pop_back();
        if (m_activeIndex >= m_sessions.size() && !m_sessions.empty())
            m_activeIndex = m_sessions.size() - 1;
        return nullptr;
    }
    return session;
}

bool ProfilerSessionManager::CloseSession(size_t index)
{
    if (index >= m_sessions.size()) return false;

    m_sessions.erase(m_sessions.begin() + static_cast<ptrdiff_t>(index));

    if (m_activeIndex >= m_sessions.size() && !m_sessions.empty())
        m_activeIndex = m_sessions.size() - 1;

    return true;
}

ProfilerSession* ProfilerSessionManager::GetActiveSession()
{
    if (m_sessions.empty()) return nullptr;
    if (m_activeIndex >= m_sessions.size()) m_activeIndex = 0;
    return m_sessions[m_activeIndex].get();
}

const ProfilerSession* ProfilerSessionManager::GetActiveSession() const
{
    if (m_sessions.empty()) return nullptr;
    if (m_activeIndex >= m_sessions.size()) return m_sessions[0].get();
    return m_sessions[m_activeIndex].get();
}

// ---------------------------------------------------------------------------
// Profiler control — delegates to the active session
// ---------------------------------------------------------------------------

void ProfilerSessionManager::StartProfiling()
{
    if (auto* session = GetActiveSession())
    {
        session->StartProfiling();
    }
}

void ProfilerSessionManager::StopProfiling()
{
    if (auto* session = GetActiveSession())
    {
        session->StopProfiling();
    }
}

void ProfilerSessionManager::ClearActive()
{
    if (auto* session = GetActiveSession())
    {
        session->Clear();
    }
}

// ---------------------------------------------------------------------------
// Nexus handlers — route to the live session
// ---------------------------------------------------------------------------

void ProfilerSessionManager::handleProfilerPacket(const std::string& messageBody)
{
    if (auto* live = findLiveSession())
    {
        live->HandleProfilerPacket(messageBody);
    }
}

void ProfilerSessionManager::handleProfilerBinaryPacket(const std::vector<uint8_t>& messageBody)
{
    if (auto* live = findLiveSession())
    {
        live->HandleProfilerBinaryPacket(messageBody);
    }
}

ProfilerSession* ProfilerSessionManager::findLiveSession()
{
    for (auto& session : m_sessions)
    {
        if (session->IsLiveSession())
            return session.get();
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Manager-level actions
// ---------------------------------------------------------------------------

void ProfilerSessionManager::registerManagerActions()
{
    m_actionManager.RegisterAction({
        .path = "File.New Session",
        .description = "Create a new empty profiler session tab",
        .shortcutHint = "Ctrl+N",
        .targets = UI::ActionTarget::Menu | UI::ActionTarget::Console,
        .callback = [this]() { NewSession(); },
        .sortPriority = 5
    });

    m_actionManager.RegisterAction({
        .path = "File.Open Session",
        .description = "Open a profiler session from a file",
        .shortcutHint = "Ctrl+O",
        .targets = UI::ActionTarget::Menu | UI::ActionTarget::Console,
        .callback = [this]() {
            std::string path = Profiler::ViewUtils::ShowOpenFileDialog();
            if (!path.empty())
                OpenSession(path);
        },
        .sortPriority = 10
    });

    m_actionManager.RegisterAction({
        .path = "File.Close Session",
        .description = "Close the currently active session tab",
        .shortcutHint = "Ctrl+W",
        .targets = UI::ActionTarget::Menu | UI::ActionTarget::Console,
        .callback = [this]() {
            if (!m_sessions.empty())
                CloseSession(m_activeIndex);
        },
        .isEnabled = [this]() { return !m_sessions.empty(); },
        .sortPriority = 40
    });
}
