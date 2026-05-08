#pragma once

#include "ProfilerSession.h"
#include "IImguiVisualizer.h"
#include <memory>
#include <vector>
#include <string>

/**
 * @brief Manages multiple ProfilerSession documents in a tabbed interface.
 *
 * Owns the Nexus subscription for incoming profiler data and routes packets
 * to the currently live session. Start / Stop / Clear actions are forwarded
 * only to the active (selected) tab.
 */
class ProfilerSessionManager : public ImGuiVisualizers::IImGuiVisualizer {
public:
    ProfilerSessionManager();
    ~ProfilerSessionManager() = default;

    // ---- IImGuiVisualizer interface ----
    void Initialize() override;
    void Shutdown() override;
    void Update(float deltaTime) override { (void)deltaTime; }
    bool Render(bool* isOpen) override;
    const char* GetName() const override { return "ProfilerManager"; }
    ImGuiKey GetShortcut() const override { return ImGuiKey_None; }
    const char* GetMenuCategory() const override { return "Show"; }

    // ---- Session management ----

    /// Create a new empty session and make it the active tab.
    ProfilerSession* NewSession(const std::string& displayName = "Untitled");

    /// Open a session from a file and make it the active tab.
    ProfilerSession* OpenSession(const std::string& filePath);

    /// Close a session by index. Returns true if closed.
    bool CloseSession(size_t index);

    /// The currently selected (active) tab session, or nullptr.
    ProfilerSession* GetActiveSession();
    const ProfilerSession* GetActiveSession() const;

    /// Number of open sessions.
    size_t GetSessionCount() const { return m_sessions.size(); }

    // ---- Profiler control (delegates to active session) ----
    void StartProfiling();
    void StopProfiling();
    void ClearActive();

private:
    // Nexus message handlers — routes data to the live session
    void handleProfilerPacket(const std::string& messageBody);
    void handleProfilerBinaryPacket(const std::vector<uint8_t>& messageBody);

    // Find the session marked as the live capture target
    ProfilerSession* findLiveSession();

    void registerManagerActions();

    // ---- Data ----
    std::vector<std::unique_ptr<ProfilerSession>> m_sessions;
    size_t m_activeIndex = 0;
    bool m_initialized = false;
    int m_untitledCounter = 0;

    // Shared action manager for manager-level actions (New, Open, etc.)
    UI::UnifiedActionManager m_actionManager;
};