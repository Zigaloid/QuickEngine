#include "UIWindowManager.h"
#include "ProfilerController.h"
#include "ImGuiVisualizers/FPSTracker.h"
#include "ImGuiVisualizers/CommandConsole.h"

#include "ImGuiVisualizers/ImGuiHeatMapVisualizer.h"
#include "Analysis/HeatMapContainer.h"

#include "ImGuiVisualizers/ProfilerViewUtils.h"
#include "CoreSystem/CoreSystem.h"
#include "CoreSystem/FunctionCallManager.h"
#include "Net/NexusClient.h"
#include "DebugChannel/DebugChannel.h"
#include "..\SharedNexusDefines.h"

#include <random>
#include <string>
#include <algorithm>
#include <cstring>
#include <charconv>

// ImGui includes
#include "imgui.h"

// GLFW for window close
#include <GLFW/glfw3.h>

using namespace Core;

extern DebugChannels::CDebugChannel AppDebug;

 
UIWindowManager::UIWindowManager()
{
}

UIWindowManager::~UIWindowManager()
{
}

// Simple test function that populates a HeatMapContainer with synthetic data.
// Creates series: "Temperature", "Density", "Humidity" with varying spatial patterns.
static void CreateTestHeatMap(HeatMapContainer& map)
{
	const std::size_t cols = map.GetColumns();
	const std::size_t rows = map.GetRows();

	std::mt19937 rng(12345);
	std::normal_distribution<double> noise(0.0, 0.2);

	for (std::size_t r = 0; r < rows; ++r) {
		for (std::size_t c = 0; c < cols; ++c) {
			// Center of the cell in meters
			float x = map.GetOriginX() + (static_cast<float>(c) + 0.5f) * map.GetCellSizeMeters();
			float y = map.GetOriginY() + (static_cast<float>(r) + 0.5f) * map.GetCellSizeMeters();

			// Spatial patterns
			double temp = 20.0 + 5.0 * std::sin(0.4 * x) * std::cos(0.3 * y) + noise(rng);
			double dens = 1.0 + 0.5 * std::cos(0.25 * x) + 0.3 * std::sin(0.35 * y) + noise(rng);
			double hum = 50.0 + 20.0 * std::exp(-((x - 5.0) * (x - 5.0) + (y - 4.0) * (y - 4.0)) / 10.0) + noise(rng);

			// Multiple samples per cell to exercise history/averaging
			map.AddValueByIndex(c, r, "Temperature", temp);
			map.AddValueByIndex(c, r, "Temperature", temp + noise(rng));

			map.AddValueByIndex(c, r, "Density", dens);
			map.AddValueByIndex(c, r, "Density", dens + noise(rng));

			map.AddValueByIndex(c, r, "Humidity", hum);
			map.AddValueByIndex(c, r, "Humidity", hum + noise(rng));
		}
	}

	return;
}

void UIWindowManager::Initialize(
	FPSTracker* fpsTracker,
	CommandConsole* console
)
{
	m_fpsTracker = fpsTracker;
	m_console = console;

	registerWindowActions();
	registerProfilerActions();

	if (m_console) {
		m_actionManager.SyncToConsole(*m_console);
		m_profilerActionManager.SyncToConsole(*m_console);
	}
}

void UIWindowManager::InitializeProfilerNetworking()
{
	if (m_networkInitialized) return;

	NEXUS_SUBSCRIBE_CALLBACK(PROFILER_PACKET_PIPE, "ANY", handleProfilerPacket);
	NEXUS_SUBSCRIBE_BINARY_CALLBACK(PROFILER_PACKET_PIPE, "ANY", handleProfilerBinaryPacket);

	m_networkInitialized = true;
	AppDebug.printf("Profiler networking initialized\n");
}

void UIWindowManager::InitializeHeatMapNetworking()
{
	if (m_heatmapNetworkInitialized) return;

	// Create the default heatmap container if none has been set externally.
	// Default: 100x100 meter area, 1 meter cells, origin at (0,0).
	if (!m_defaultHeatMap) {
		m_defaultHeatMap = std::make_unique<HeatMapContainer>(
			100.0f,  // widthMeters
			100.0f,  // heightMeters
			1.0f,    // cellSizeMeters
			0.0f,    // originX
			0.0f     // originY
		);
		m_heatmapVis.SetContainer(m_defaultHeatMap.get());
		m_heatmapVis.GetConfig().title = "Live Heat Map";
		AppDebug.printf("Default heatmap container created (100x100m, 1m cells)\n");
	}

	// Subscribe to the HeatMap text pipe for live data.
	// We subscribe directly so we can receive both messageType (series name) and body.
	CoreSystem::GetNexusClient()->Subscribe(TELEMETRY_PIPE, "ANY",
		[this](const SNexusMessage& msg) {
			handleHeatMapPacket(msg.messageType, msg.body);
		});

	m_heatmapNetworkInitialized = true;
	m_showHeatMap = true;
	AppDebug.printf("HeatMap networking initialized — listening on pipe '%s'\n", TELEMETRY_PIPE.c_str());
}

// ---------------------------------------------------------------------------
// Text heatmap packet format:
//
//   messageType  = series name  (e.g. "Temperature")
//   body         = "pos=x,y,z value=v"
//
// The x,z coordinates are in meters and used to look up the grid cell.
// The y coordinate is accepted but unused by the 2D heatmap (vertical axis).
// Example body: "pos=12.5,7.3,4.0 value=23.4"
// ---------------------------------------------------------------------------
void UIWindowManager::handleHeatMapPacket(const std::string& messageType, const std::string& body)
{
	if (!m_defaultHeatMap) return;
	if (messageType.empty() || body.empty()) return;

	// Locate "pos=" and "value=" tokens
	const auto posIdx = body.find("pos=");
	const auto valIdx = body.find("value=");
	if (posIdx == std::string::npos || valIdx == std::string::npos) {
		AppDebug.printf("HeatMap packet: malformed body (missing pos= or value=): %s\n", body.c_str());
		return;
	}

	// ---- Parse pos=x,y,z ----
	const char* posStart = body.c_str() + posIdx + 4; // skip "pos="
	const char* bodyEnd = body.c_str() + body.size();

	float x = 0.0f, y = 0.0f, z = 0.0f;
	// Parse x
	auto [ptrX, ecX] = std::from_chars(posStart, bodyEnd, x);
	if (ecX != std::errc{} || *ptrX != ',') {
		AppDebug.printf("HeatMap packet: failed to parse pos x: %s\n", body.c_str());
		return;
	}
	// Parse y (accepted but unused — vertical axis)
	auto [ptrY, ecY] = std::from_chars(ptrX + 1, bodyEnd, y);
	if (ecY != std::errc{}) {
		AppDebug.printf("HeatMap packet: failed to parse pos y: %s\n", body.c_str());
		return;
	}
	// Parse z (used as the second heatmap axis)
	if (*ptrY == ',') {
		auto [ptrZ, ecZ] = std::from_chars(ptrY + 1, bodyEnd, z);
		if (ecZ != std::errc{}) {
			AppDebug.printf("HeatMap packet: failed to parse pos z: %s\n", body.c_str());
			return;
		}
	}

	// ---- Parse value=v ----
	const char* valStart = body.c_str() + valIdx + 6; // skip "value="
	double value = 0.0;
	auto [ptrV, ecV] = std::from_chars(valStart, bodyEnd, value);
	if (ecV != std::errc{}) {
		AppDebug.printf("HeatMap packet: failed to parse value: %s\n", body.c_str());
		return;
	}

	if (value > 200) value = 200;
	// Add the value using world-space x,z coordinates; the container resolves the cell
	if (m_defaultHeatMap->AddValue(x, z, messageType, value)) {
		m_heatmapVis.RefreshSeriesList();
	}
}

void UIWindowManager::ShutdownProfiler()
{
	m_profilerSessions.clear();
	m_activeSessionIndex = -1;
	m_liveSessionIndex = -1;
	AppDebug.printf("All profiler sessions closed\n");
}

// ---- Network packet handlers — route to live session ----

void UIWindowManager::handleProfilerPacket(const std::string& messageBody)
{
	auto* live = GetOrCreateLiveSession();
	if (live) {
		live->HandleProfilerPacket(messageBody);
	}
}

void UIWindowManager::handleProfilerBinaryPacket(const std::vector<uint8_t>& messageBody)
{
	auto* live = GetOrCreateLiveSession();
	if (live) {
		live->HandleProfilerBinaryPacket(messageBody);
	}
}

// ---- Multi-document session management ----

ProfilerController* UIWindowManager::CreateSession(const std::string& displayName)
{
	auto session = std::make_unique<ProfilerController>();
	session->SetDisplayName(displayName);

	// Register per-session file/profiler actions
	session->Initialize();

	// Sync per-session actions to console if available
	if (m_console) {
		session->GetActionManager().SyncToConsole(*m_console);
	}

	m_profilerSessions.push_back(std::move(session));
	m_activeSessionIndex = static_cast<int>(m_profilerSessions.size()) - 1;

	AppDebug.printf("Created profiler session: %s (total: %d)\n",
		displayName.c_str(), static_cast<int>(m_profilerSessions.size()));

	return m_profilerSessions.back().get();
}

ProfilerController* UIWindowManager::GetOrCreateLiveSession()
{
	// Return existing live session if valid
	if (m_liveSessionIndex >= 0 && m_liveSessionIndex < static_cast<int>(m_profilerSessions.size())) {
		return m_profilerSessions[m_liveSessionIndex].get();
	}

	// Create a new live session
	auto* session = CreateSession("Live Capture");
	session->SetLiveSession(true);
	m_liveSessionIndex = static_cast<int>(m_profilerSessions.size()) - 1;
	return session;
}

void UIWindowManager::LoadSessionIntoNewTab(const std::string& filePath)
{
	// Extract filename for display
	std::string displayName = filePath;
	auto pos = filePath.find_last_of("\\/");
	if (pos != std::string::npos) {
		displayName = filePath.substr(pos + 1);
	}

	auto* session = CreateSession(displayName);
	if (!session->LoadSession(filePath)) {
		// Load failed — remove the session we just created
		m_profilerSessions.pop_back();
		if (m_activeSessionIndex >= static_cast<int>(m_profilerSessions.size())) {
			m_activeSessionIndex = static_cast<int>(m_profilerSessions.size()) - 1;
		}
		AppDebug.printf("Failed to load session into new tab: %s\n", filePath.c_str());
	}
}

void UIWindowManager::LoadSessionDialog()
{
	std::string path = Profiler::ViewUtils::ShowOpenFileDialog();
	if (!path.empty()) {
		LoadSessionIntoNewTab(path);
	}
}

bool UIWindowManager::CloseSession(int index)
{
	if (index < 0 || index >= static_cast<int>(m_profilerSessions.size()))
		return false;

	// Adjust live session index
	if (m_liveSessionIndex == index) {
		m_liveSessionIndex = -1;
	}
	else if (m_liveSessionIndex > index) {
		--m_liveSessionIndex;
	}

	m_profilerSessions.erase(m_profilerSessions.begin() + index);

	// Adjust active session index
	if (m_profilerSessions.empty()) {
		m_activeSessionIndex = -1;
	}
	else if (m_activeSessionIndex >= static_cast<int>(m_profilerSessions.size())) {
		m_activeSessionIndex = static_cast<int>(m_profilerSessions.size()) - 1;
	}

	return true;
}

ProfilerController* UIWindowManager::GetActiveSession()
{
	if (m_activeSessionIndex >= 0 && m_activeSessionIndex < static_cast<int>(m_profilerSessions.size()))
		return m_profilerSessions[m_activeSessionIndex].get();
	return nullptr;
}

const ProfilerController* UIWindowManager::GetActiveSession() const
{
	if (m_activeSessionIndex >= 0 && m_activeSessionIndex < static_cast<int>(m_profilerSessions.size()))
		return m_profilerSessions[m_activeSessionIndex].get();
	return nullptr;
}

ProfilerController* UIWindowManager::GetLiveSession()
{
	if (m_liveSessionIndex >= 0 && m_liveSessionIndex < static_cast<int>(m_profilerSessions.size()))
		return m_profilerSessions[m_liveSessionIndex].get();
	return nullptr;
}

void UIWindowManager::StartProfiling()
{
	auto* live = GetOrCreateLiveSession();
	if (live) {
		live->StartProfiling();
	}
}

void UIWindowManager::StopProfiling()
{
	auto* live = GetLiveSession();
	if (live) {
		live->StopProfiling();
	}
}

bool UIWindowManager::IsProfilingEnabled() const
{
	if (m_liveSessionIndex >= 0 && m_liveSessionIndex < static_cast<int>(m_profilerSessions.size()))
		return m_profilerSessions[m_liveSessionIndex]->IsProfilingEnabled();
	return false;
}

void UIWindowManager::processPendingCloses()
{
	if (m_pendingCloseIndices.empty()) return;

	// Sort descending so removal doesn't invalidate earlier indices
	std::sort(m_pendingCloseIndices.begin(), m_pendingCloseIndices.end(), std::greater<int>());
	for (int idx : m_pendingCloseIndices) {
		CloseSession(idx);
	}
	m_pendingCloseIndices.clear();
}

void UIWindowManager::registerWindowActions()
{
	// Windows.Profiler
	m_actionManager.RegisterAction({
		.path = "Windows.Profiler",
		.description = "Show or hide the profiler window",
		.targets = UI::ActionTarget::Menu | UI::ActionTarget::Console,
		.callback = [this]() { m_showProfiler = !m_showProfiler; },
		.isChecked = [this]() { return m_showProfiler; },
		.sortPriority = 5
		});

	// Windows.Frame Time Analysis
	m_actionManager.RegisterAction({
		.path = "Windows.Frame Time Analysis",
		.description = "Show or hide the frame time analysis window",
		.shortcutHint = "Ctrl+F",
		.targets = UI::ActionTarget::Menu | UI::ActionTarget::Console,
		.callback = [this]() { m_showFrameTimeAnalysis = !m_showFrameTimeAnalysis; },
		.isChecked = [this]() { return m_showFrameTimeAnalysis; },
		.sortPriority = 10
		});

	// Windows.Console
	m_actionManager.RegisterAction({
		.path = "Windows.Console",
		.description = "Show or hide the console window",
		.shortcutHint = "Ctrl+C",
		.targets = UI::ActionTarget::Menu | UI::ActionTarget::Console,
		.callback = [this]() { m_showConsole = !m_showConsole; },
		.isChecked = [this]() { return m_showConsole; },
		.sortPriority = 20
		});

	// Windows.Heat Map
	m_actionManager.RegisterAction({
		.path = "Windows.Heat Map",
		.description = "Show or hide the heat map viewer",
		.targets = UI::ActionTarget::Menu | UI::ActionTarget::Console,
		.callback = [this]() { m_showHeatMap = !m_showHeatMap; },
		.isChecked = [this]() { return m_showHeatMap; },
		.sortPriority = 30
		});

	// Windows.Session Comparison
	m_actionManager.RegisterAction({
		.path = "Windows.Session Comparison",
		.description = "Show or hide the cross-session comparison window",
		.targets = UI::ActionTarget::Menu | UI::ActionTarget::Console,
		.callback = [this]() { m_showSessionComparison = !m_showSessionComparison; },
		.isChecked = [this]() { return m_showSessionComparison; },
		.sortPriority = 35
		});
}

void UIWindowManager::registerProfilerActions()
{
	// Profiler-window actions — rendered only inside the Profiler window's menu bar,
	// not in the main application menu bar.

	m_profilerActionManager.RegisterAction({
		.path = "Profiler.Start",
		.description = "Start profiling (live capture)",
		.targets = UI::ActionTarget::Menu | UI::ActionTarget::Console,
		.callback = [this]() { StartProfiling(); },
		.isEnabled = [this]() { return !IsProfilingEnabled(); },
		.isChecked = [this]() { return IsProfilingEnabled(); },
		.sortPriority = 10
		});

	m_profilerActionManager.RegisterAction({
		.path = "Profiler.Stop",
		.description = "Stop profiling (live capture)",
		.targets = UI::ActionTarget::Menu | UI::ActionTarget::Console,
		.callback = [this]() { StopProfiling(); },
		.isEnabled = [this]() { return IsProfilingEnabled(); },
		.isChecked = [this]() { return !IsProfilingEnabled(); },
		.sortPriority = 20
		});

	m_profilerActionManager.RegisterAction({
		.path = "File.Load Session",
		.description = "Load a profiler session from a file into a new tab",
		.shortcutHint = "Ctrl+O",
		.targets = UI::ActionTarget::Menu | UI::ActionTarget::Console,
		.callback = [this]() { LoadSessionDialog(); },
		.sortPriority = 5
		});

	m_profilerActionManager.RegisterAction({
		.path = "File.New Session",
		.description = "Create a new empty profiler session tab",
		.targets = UI::ActionTarget::Menu | UI::ActionTarget::Console,
		.callback = [this]() { CreateSession("Untitled"); },
		.sortPriority = 1
		});

	m_profilerActionManager.RegisterAction({
		.path = "File.Save Active",
		.description = "Save the active profiler session",
		.shortcutHint = "Ctrl+S",
		.targets = UI::ActionTarget::Menu | UI::ActionTarget::Console,
		.callback = [this]() {
			auto* s = GetActiveSession();
			if (s) s->SaveSessionCurrent();
		},
		.isEnabled = [this]() {
			auto* s = GetActiveSession();
			return s && s->HasSessionData();
		},
		.sortPriority = 10
		});

	m_profilerActionManager.RegisterAction({
		.path = "File.Save Active As",
		.description = "Save the active profiler session to a new file",
		.shortcutHint = "Ctrl+Shift+S",
		.targets = UI::ActionTarget::Menu | UI::ActionTarget::Console,
		.callback = [this]() {
			auto* s = GetActiveSession();
			if (s) s->SaveSessionAs();
		},
		.isEnabled = [this]() {
			auto* s = GetActiveSession();
			return s && s->HasSessionData();
		},
		.sortPriority = 15
		});

	m_profilerActionManager.RegisterAction({
		.path = "File.Close Session",
		.description = "Close the active profiler session tab",
		.targets = UI::ActionTarget::Menu | UI::ActionTarget::Console,
		.callback = [this]() {
			if (m_activeSessionIndex >= 0) {
				CloseSession(m_activeSessionIndex);
			}
		},
		.isEnabled = [this]() { return m_activeSessionIndex >= 0; },
		.sortPriority = 40
		});

	m_profilerActionManager.RegisterAction({
		.path = "Profiler.Compare Sessions",
		.description = "Open the cross-session comparison view",
		.targets = UI::ActionTarget::Menu | UI::ActionTarget::Console | UI::ActionTarget::Toolbar,
		.callback = [this]() { m_showSessionComparison = !m_showSessionComparison; },
		.isEnabled = [this]() { return m_profilerSessions.size() >= 2; },
		.isChecked = [this]() { return m_showSessionComparison; },
		.sortPriority = 70
		});
}

void UIWindowManager::SetupHeatMap(HeatMapContainer* container, const std::string& backgroundTexturePath)
{
	if (!backgroundTexturePath.empty()) {
		m_heatmapVis.LoadBackgroundTextureFromResource(backgroundTexturePath);
	}
	m_heatmapVis.SetContainer(container);
	m_showHeatMap = true;
}

void UIWindowManager::RenderAllWindows()
{
	// Render menu bar first
	RenderMenuBar();
	// Render all windows
	renderProfilerWindow();
	renderFrameTimeAnalysisWindow();
	renderConsoleWindow();
	renderHeatMapWindow();
	renderSessionComparisonWindow();

	// Process any deferred session closes
	processPendingCloses();
}

void UIWindowManager::RenderHeatMap()
{

}

void UIWindowManager::RenderMenuBar()
{
	if (ImGui::BeginMenuBar()) {
		// Render only app-level menus (Windows)
		m_actionManager.RenderMenuBar();

		// Add spacing between menus and toolbar buttons
		ImGui::Separator();

		// Render app-level toolbar buttons inline in the menu bar
		m_actionManager.RenderToolbar();

		ImGui::EndMenuBar();
	}
}

void UIWindowManager::RenderToolbar()
{
	// Render a horizontal toolbar strip below the menu bar
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.0f, 4.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 4.0f));
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyleColorVec4(ImGuiCol_MenuBarBg));

	float toolbarHeight = ImGui::GetFrameHeight() + ImGui::GetStyle().WindowPadding.y * 2.0f;
	if (ImGui::BeginChild("##Toolbar", ImVec2(0, toolbarHeight), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
		m_actionManager.RenderToolbar();
	}
	ImGui::EndChild();

	ImGui::PopStyleColor();
	ImGui::PopStyleVar(2);

	ImGui::Separator();
}

void UIWindowManager::renderProfilerWindow()
{
	if (!m_showProfiler) return;
	if (m_profilerSessions.empty()) return;

	ImGuiWindowFlags wflags = ImGuiWindowFlags_MenuBar;

	if (ImGui::Begin("Profiler", &m_showProfiler, wflags))
	{
		// Profiler-specific menu bar (File, Profiler actions — not in the main app menu)
		if (ImGui::BeginMenuBar())
		{
			m_profilerActionManager.RenderMenuBar();
			ImGui::Separator();
			m_profilerActionManager.RenderToolbar();
			ImGui::EndMenuBar();
		}

		// Tab bar — one tab per open session
		ImGuiTabBarFlags tabFlags = ImGuiTabBarFlags_Reorderable
			| ImGuiTabBarFlags_AutoSelectNewTabs
			| ImGuiTabBarFlags_FittingPolicyScroll
			| ImGuiTabBarFlags_TabListPopupButton;

		if (ImGui::BeginTabBar("##ProfilerSessionTabs", tabFlags))
		{
			for (int i = 0; i < static_cast<int>(m_profilerSessions.size()); ++i)
			{
				auto& session = m_profilerSessions[i];

				// Build tab label: "DisplayName" or "DisplayName *" if dirty
				std::string label = session->GetDisplayName();
				if (session->IsDirty()) {
					label += " *";
				}
				if (session->IsLiveSession()) {
					label = "[Live] " + label;
				}

				bool tabOpen = true;
				ImGui::PushID(i);

				if (ImGui::BeginTabItem(label.c_str(), &tabOpen))
				{
					m_activeSessionIndex = i;

					// Render this session's content
					session->RenderSessionContent();

					ImGui::EndTabItem();
				}

				ImGui::PopID();

				// User closed the tab via the X button
				if (!tabOpen) {
					m_pendingCloseIndices.push_back(i);
				}
			}
			ImGui::EndTabBar();
		}
	}
	ImGui::End();
}

void UIWindowManager::renderFrameTimeAnalysisWindow()
{
	if (m_showFrameTimeAnalysis && m_fpsTracker) {
		m_fpsTracker->RenderFPSAnalysisWindow("Frame Time Analysis", &m_showFrameTimeAnalysis);
	}
}

void UIWindowManager::renderConsoleWindow()
{
	if (m_showConsole && m_console) {
		m_console->RenderConsoleWindow("Console", &m_showConsole);
	}
}

void UIWindowManager::renderHeatMapWindow()
{
	if (m_showHeatMap) {
		m_heatmapVis.GetConfig().title = "Heat Map Test";
		m_heatmapVis.RenderWindow("HeatMap Viewer", &m_showHeatMap);
	}
}

void UIWindowManager::renderSessionComparisonWindow()
{
	if (!m_showSessionComparison) return;

	// Build the session info list from all open profiler sessions
	std::vector<Profiler::SessionInfo> sessionInfos;
	sessionInfos.reserve(m_profilerSessions.size());

	for (const auto& session : m_profilerSessions) {
		Profiler::SessionInfo info;
		info.displayName = session->GetDisplayName();
		info.data = session->GetTimelineData();
		info.selected = false; // Selection state is managed internally by the view
		sessionInfos.push_back(info);
	}

	m_sessionComparisonView.SetSessions(sessionInfos);
	m_sessionComparisonView.Render("Session Comparison", &m_showSessionComparison);
}
