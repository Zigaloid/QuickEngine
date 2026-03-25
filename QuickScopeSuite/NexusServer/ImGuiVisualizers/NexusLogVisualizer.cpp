#include "NexusLogVisualizer.h"
#include "NexusLogFilterState.h"
#include "Profiler/Profiler.h"
#include "CoreSystem/CoreSystem.h"
#include "Net/NexusClient.h"

#include <algorithm>
#include <ctime>
#include <iomanip>
#include <sstream>

// ---------------------------------------------------------------------------
// Construction / Destruction
// ---------------------------------------------------------------------------

NexusLogVisualizer::NexusLogVisualizer(int maxEntries)
    : m_maxEntries(maxEntries)
    , m_autoScroll(true)
    , m_scrollToBottom(false)
    , m_nexusClient(nullptr)
    , m_wasWindowOpen(false)
{
    DECLARE_FUNC_VLOW();
    memset(m_textFilter, 0, sizeof(m_textFilter));
}

NexusLogVisualizer::~NexusLogVisualizer()
{
    Shutdown();
}

// ---------------------------------------------------------------------------
// Initialization
// ---------------------------------------------------------------------------

bool NexusLogVisualizer::Initialize(const std::string& pipeName, const std::string& appName)
{
    DECLARE_FUNC_VLOW();
    m_isEnabled = true;
    LoadFilterState("NexusLogFilters.json");
    return true;
}

void NexusLogVisualizer::Shutdown()
{
    DECLARE_FUNC_VLOW();

    if (m_nexusClient && !m_subscribedPipe.empty())
    {
        m_nexusClient->RemoveCallbacks(m_subscribedPipe);
    }
    m_nexusClient = nullptr;
    m_isEnabled = false;
}

// ---------------------------------------------------------------------------
// Entry management
// ---------------------------------------------------------------------------

void NexusLogVisualizer::AddEntry(const std::string& senderApp,
                                  const std::string& pipeName,
                                  const std::string& body)
{
    DECLARE_FUNC_VLOW();

    std::lock_guard<std::mutex> lock(m_mutex);

    // Track known apps / pipes and default new ones to enabled
    if (m_knownApps.insert(senderApp).second)
    {
        m_appVisibility[senderApp] = true;
        m_appCollectionEnabled[senderApp] = true;
    }
    if (m_knownPipes.insert(pipeName).second)
    {
        m_pipeVisibility[pipeName] = true;
        m_pipeCollectionEnabled[pipeName] = true;
    }

    // If collection is disabled for this app or pipe, discard the entry
    if (IsCollectionDisabled(senderApp, pipeName))
        return;

    m_entries.emplace_back(senderApp, pipeName, body, std::chrono::system_clock::now());

    // Limit history size
    if (static_cast<int>(m_entries.size()) > m_maxEntries)
    {
        m_entries.erase(m_entries.begin(),
                        m_entries.begin() + (static_cast<int>(m_entries.size()) - m_maxEntries));
    }

    m_scrollToBottom = true;
}

void NexusLogVisualizer::ClearEntries()
{
    DECLARE_FUNC_VLOW();
    std::lock_guard<std::mutex> lock(m_mutex);
    m_entries.clear();
}

int NexusLogVisualizer::GetEntryCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return static_cast<int>(m_entries.size());
}

// ---------------------------------------------------------------------------
// NexusClient callback
// ---------------------------------------------------------------------------

void NexusLogVisualizer::OnNexusMessage(const SNexusMessage& msg)
{
    AddEntry(msg.senderApp, msg.pipeName, msg.body);
}

// ---------------------------------------------------------------------------
// Save / Load filter state via Reflection System
// ---------------------------------------------------------------------------

bool NexusLogVisualizer::SaveFilterState(const std::string& filePath) const
{
    DECLARE_FUNC_VLOW();

    NexusLogFilterState state;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        state.m_appVisibility        = m_appVisibility;
        state.m_pipeVisibility       = m_pipeVisibility;
        state.m_appCollectionEnabled = m_appCollectionEnabled;
        state.m_pipeCollectionEnabled = m_pipeCollectionEnabled;
    }

    auto result = state.SafeWrite(filePath);
    return result.IsSuccess() && result.GetValue();
}

bool NexusLogVisualizer::LoadFilterState(const std::string& filePath)
{
    DECLARE_FUNC_VLOW();

    NexusLogFilterState state;
    auto result = state.SafeRead(filePath);
    if (!result.IsSuccess() || !result.GetValue())
        return false;

    std::lock_guard<std::mutex> lock(m_mutex);

    // Merge loaded state — overwrite existing keys, insert new ones
    for (auto& [key, val] : state.m_appVisibility)
    {
        m_appVisibility[key] = val;
        m_knownApps.insert(key);
    }
    for (auto& [key, val] : state.m_pipeVisibility)
    {
        m_pipeVisibility[key] = val;
        m_knownPipes.insert(key);
    }
    for (auto& [key, val] : state.m_appCollectionEnabled)
    {
        m_appCollectionEnabled[key] = val;
        m_knownApps.insert(key);
    }
    for (auto& [key, val] : state.m_pipeCollectionEnabled)
    {
        m_pipeCollectionEnabled[key] = val;
        m_knownPipes.insert(key);
    }

    return true;
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

bool NexusLogVisualizer::RenderWindow(const char* windowTitle, bool* isOpen)
{
    DECLARE_FUNC_LOW();

    bool windowShouldBeOpen = (isOpen == nullptr) || *isOpen;
    if (!windowShouldBeOpen)
    {
        m_wasWindowOpen = false;
        return false;
    }
    m_wasWindowOpen = true;

    if (!ImGui::Begin(windowTitle, isOpen, ImGuiWindowFlags_MenuBar))
    {
        ImGui::End();
        return false;
    }

    RenderToolbar();
    RenderLogTable();
    RenderStatusBar();

    ImGui::End();
    return true;
}

// ---------------------------------------------------------------------------
// Toolbar: multi-select filter checkboxes, text filter, clear button
// ---------------------------------------------------------------------------

void NexusLogVisualizer::RenderToolbar()
{
    DECLARE_FUNC_LOW();

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("Options"))
        {
            ImGui::MenuItem("Auto-scroll", nullptr, &m_autoScroll);
            if (ImGui::MenuItem("Clear"))
            {
                ClearEntries();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Save Filters..."))
            {
                SaveFilterState("NexusLogFilters.json");
            }
            if (ImGui::MenuItem("Load Filters..."))
            {
                LoadFilterState("NexusLogFilters.json");
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    // --- App filter multi-select ---
    {
        // Build preview string showing selected count
        int visibleAppCount = 0;
        for (const auto& [app, visible] : m_appVisibility)
        {
            if (visible) ++visibleAppCount;
        }
        const int totalApps = static_cast<int>(m_knownApps.size());
        char appPreview[64];
        if (visibleAppCount == totalApps)
            snprintf(appPreview, sizeof(appPreview), "All Apps (%d)", totalApps);
        else
            snprintf(appPreview, sizeof(appPreview), "Apps (%d/%d)", visibleAppCount, totalApps);

        ImGui::SetNextItemWidth(180.0f);
        if (ImGui::BeginCombo("##AppFilter", appPreview))
        {
            // Select All / Deselect All helpers
            if (ImGui::Button("All##Apps"))
            {
                for (auto& [app, visible] : m_appVisibility)
                    visible = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("None##Apps"))
            {
                for (auto& [app, visible] : m_appVisibility)
                    visible = false;
            }
            ImGui::Separator();

            for (const auto& app : m_knownApps)
            {
                bool visible = m_appVisibility[app];
                bool collecting = m_appCollectionEnabled[app];

                ImGui::PushID(app.c_str());

                // Visibility checkbox
                if (ImGui::Checkbox("##vis", &visible))
                    m_appVisibility[app] = visible;

                ImGui::SameLine();

                // Collection enabled checkbox (green when on, red when off)
                ImVec4 collectColor = collecting ? ImVec4(0.2f, 0.8f, 0.2f, 1.0f)
                                                 : ImVec4(0.8f, 0.2f, 0.2f, 1.0f);
                ImGui::PushStyleColor(ImGuiCol_CheckMark, collectColor);
                if (ImGui::Checkbox("##col", &collecting))
                    m_appCollectionEnabled[app] = collecting;
                ImGui::PopStyleColor();
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Collection %s for '%s'", collecting ? "enabled" : "disabled", app.c_str());

                ImGui::SameLine();
                ImGui::TextUnformatted(app.c_str());

                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Left checkbox: show/hide in view\nRight checkbox: enable/disable collection");
    }

    ImGui::SameLine();

    // --- Pipe filter multi-select ---
    {
        int visiblePipeCount = 0;
        for (const auto& [pipe, visible] : m_pipeVisibility)
        {
            if (visible) ++visiblePipeCount;
        }
        const int totalPipes = static_cast<int>(m_knownPipes.size());
        char pipePreview[64];
        if (visiblePipeCount == totalPipes)
            snprintf(pipePreview, sizeof(pipePreview), "All Pipes (%d)", totalPipes);
        else
            snprintf(pipePreview, sizeof(pipePreview), "Pipes (%d/%d)", visiblePipeCount, totalPipes);

        ImGui::SetNextItemWidth(180.0f);
        if (ImGui::BeginCombo("##PipeFilter", pipePreview))
        {
            if (ImGui::Button("All##Pipes"))
            {
                for (auto& [pipe, visible] : m_pipeVisibility)
                    visible = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("None##Pipes"))
            {
                for (auto& [pipe, visible] : m_pipeVisibility)
                    visible = false;
            }
            ImGui::Separator();

            for (const auto& pipe : m_knownPipes)
            {
                bool visible = m_pipeVisibility[pipe];
                bool collecting = m_pipeCollectionEnabled[pipe];

                ImGui::PushID(pipe.c_str());

                // Visibility checkbox
                if (ImGui::Checkbox("##vis", &visible))
                    m_pipeVisibility[pipe] = visible;

                ImGui::SameLine();

                // Collection enabled checkbox
                ImVec4 collectColor = collecting ? ImVec4(0.2f, 0.8f, 0.2f, 1.0f)
                                                 : ImVec4(0.8f, 0.2f, 0.2f, 1.0f);
                ImGui::PushStyleColor(ImGuiCol_CheckMark, collectColor);
                if (ImGui::Checkbox("##col", &collecting))
                    m_pipeCollectionEnabled[pipe] = collecting;
                ImGui::PopStyleColor();
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Collection %s for '%s'", collecting ? "enabled" : "disabled", pipe.c_str());

                ImGui::SameLine();
                ImGui::TextUnformatted(pipe.c_str());

                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Left checkbox: show/hide in view\nRight checkbox: enable/disable collection");
    }

    ImGui::SameLine();

    // --- Text filter ---
    ImGui::SetNextItemWidth(200.0f);
    ImGui::InputTextWithHint("##TextFilter", "Filter body text...", m_textFilter, sizeof(m_textFilter));

    ImGui::SameLine();

    // --- Clear button ---
    if (ImGui::Button("Clear"))
    {
        ClearEntries();
    }

    ImGui::Separator();
}

// ---------------------------------------------------------------------------
// Log table
// ---------------------------------------------------------------------------

void NexusLogVisualizer::RenderLogTable()
{
    DECLARE_FUNC_LOW();

    std::lock_guard<std::mutex> lock(m_mutex);

    const float footerReserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();

    if (!ImGui::BeginChild("LogScrollRegion", ImVec2(0, -footerReserve), false,
                           ImGuiWindowFlags_HorizontalScrollbar))
    {
        ImGui::EndChild();
        return;
    }

    const ImGuiTableFlags tableFlags = ImGuiTableFlags_Resizable
                                     | ImGuiTableFlags_RowBg
                                     | ImGuiTableFlags_Borders
                                     | ImGuiTableFlags_ScrollY;

    if (ImGui::BeginTable("NexusLogTable", 4, tableFlags))
    {
        ImGui::TableSetupColumn("Time",    ImGuiTableColumnFlags_WidthFixed,   90.0f);
        ImGui::TableSetupColumn("App",     ImGuiTableColumnFlags_WidthFixed,  120.0f);
        ImGui::TableSetupColumn("Pipe",    ImGuiTableColumnFlags_WidthFixed,  120.0f);
        ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (const auto& entry : m_entries)
        {
            if (!PassesFilter(entry))
                continue;

            ImGui::TableNextRow();

            // Time column
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(FormatTimestamp(entry.timestamp).c_str());

            // App column
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(entry.senderApp.c_str());

            // Pipe column
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(entry.pipeName.c_str());

            // Message body column
            ImGui::TableSetColumnIndex(3);
            ImGui::TextWrapped("%s", entry.body.c_str());
        }

        // Auto-scroll
        if (m_autoScroll && m_scrollToBottom)
        {
            ImGui::SetScrollHereY(1.0f);
            m_scrollToBottom = false;
        }

        ImGui::EndTable();
    }

    ImGui::EndChild();
}

// ---------------------------------------------------------------------------
// Status bar
// ---------------------------------------------------------------------------

void NexusLogVisualizer::RenderStatusBar()
{
    DECLARE_FUNC_LOW();

    std::lock_guard<std::mutex> lock(m_mutex);

    int visibleCount = 0;
    for (const auto& entry : m_entries)
    {
        if (PassesFilter(entry))
            ++visibleCount;
    }

    ImGui::Text("Showing %d / %d entries | Apps: %d | Pipes: %d",
                visibleCount, static_cast<int>(m_entries.size()),
                static_cast<int>(m_knownApps.size()),
                static_cast<int>(m_knownPipes.size()));
}

// ---------------------------------------------------------------------------
// Filtering
// ---------------------------------------------------------------------------

bool NexusLogVisualizer::PassesFilter(const LogEntry& entry) const
{
    // App visibility filter (multi-select)
    {
        auto it = m_appVisibility.find(entry.senderApp);
        if (it != m_appVisibility.end() && !it->second)
            return false;
    }

    // Pipe visibility filter (multi-select)
    {
        auto it = m_pipeVisibility.find(entry.pipeName);
        if (it != m_pipeVisibility.end() && !it->second)
            return false;
    }

    // Text filter (case-insensitive substring match on body)
    if (m_textFilter[0] != '\0')
    {
        std::string filterLower(m_textFilter);
        std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), ::tolower);

        std::string bodyLower = entry.body;
        std::transform(bodyLower.begin(), bodyLower.end(), bodyLower.begin(), ::tolower);

        if (bodyLower.find(filterLower) == std::string::npos)
            return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Collection gating
// ---------------------------------------------------------------------------

bool NexusLogVisualizer::IsCollectionDisabled(const std::string& senderApp, const std::string& pipeName) const
{
    {
        auto it = m_appCollectionEnabled.find(senderApp);
        if (it != m_appCollectionEnabled.end() && !it->second)
            return true;
    }
    {
        auto it = m_pipeCollectionEnabled.find(pipeName);
        if (it != m_pipeCollectionEnabled.end() && !it->second)
            return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

std::string NexusLogVisualizer::FormatTimestamp(std::chrono::system_clock::time_point tp) const
{
    auto time_t_val = std::chrono::system_clock::to_time_t(tp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  tp.time_since_epoch()) % 1000;

    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &time_t_val);
#else
    localtime_r(&time_t_val, &tm_buf);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}