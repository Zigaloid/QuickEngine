#include "ProfilerSession.h"
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

ProfilerSession::ProfilerSession()
	: m_showFlameGraph(true)
	, m_showFrameComparison(false)
	, m_initialized(false)
	, m_viewMode(ViewMode::FlameGraph)
{
}

void ProfilerSession::HandleProfilerBinaryPacket(const std::vector<uint8_t>& messageBody)
{
	DECLARE_FUNC_VLOW();

	Profiler::CEventArray events;
	if (events.ReadFromBinaryBuffer(messageBody))
	{
		// Retain the raw binary data for session save/load
		m_sessionPackets.push_back(messageBody);

		Clear();
		// Convert from vector<ProfileEvent*> to deque<ProfileEvent> for the analyzer
		std::deque<Profiler::ProfileEvent> eventDeque;
		for (const auto* event : events.m_events)
		{
			if (event)
			{
				eventDeque.push_back(*event);
			}
		}

		// Append events to the analyzer (accumulates across packets)
		m_analyzer.AppendFromProfileEvents(eventDeque);

		// Regenerate flame graph data and update the visualizer
		auto flameGraphData = m_analyzer.GenerateTimelineFlameGraphData();
		m_visualizer.SetFlameGraphData(std::move(flameGraphData));

		m_dirty = true;

		// ---- New code to update thread IDs ----
		std::set<std::string> threadSet;
		for (const auto* event : events.m_events)
		{
			if (event)
			{
				if (event->GetTheadName() != "")
					threadSet.insert(event->GetTheadName());
				else
					threadSet.insert(event->GetThreadIdStr());
			}
		}
		m_threadNames.assign(threadSet.begin(), threadSet.end());
		if (m_threadNames.size() != 0)
		{
			m_selectedThreadId = m_threadNames.front();
		}
		// -----------------------------------------
		for (const auto* event : events.m_events)
			delete event; // Clean up dynamically allocated ProfileEvent objects
	}
}

void ProfilerSession::HandleProfilerPacket(const std::string& messageBody)
{
	DECLARE_FUNC_VLOW();

	Profiler::CEventArray events;
	if (events.ReadFromJsonString(messageBody))
	{
		Clear();
		// Convert from vector<ProfileEvent*> to deque<ProfileEvent> for the analyzer
		std::deque<Profiler::ProfileEvent> eventDeque;
		for (const auto* event : events.m_events)
		{
			if (event)
			{
				eventDeque.push_back(*event);
			}
		}

		// Append events to the analyzer (accumulates across packets)
		m_analyzer.AppendFromProfileEvents(eventDeque);

		// Regenerate flame graph data and update the visualizer
		auto flameGraphData = m_analyzer.GenerateTimelineFlameGraphData();
		m_visualizer.SetFlameGraphData(std::move(flameGraphData));

		m_dirty = true;

		for (const auto* event : events.m_events)
			delete event; // Clean up dynamically allocated ProfileEvent objects
	}
}

bool ProfilerSession::SaveSession(const std::string& filePath)
{
	DECLARE_FUNC_LOW();

	if (m_sessionPackets.empty())
	{
		ProfilerDebug.printf("No profiler session data to save\n");
		return false;
	}

	// File format: [packetCount (uint32)] then for each packet: [size (uint32)][data bytes]
	std::vector<uint8_t> fileBuffer;

	uint32_t packetCount = static_cast<uint32_t>(m_sessionPackets.size());
	fileBuffer.insert(fileBuffer.end(),
		reinterpret_cast<const uint8_t*>(&packetCount),
		reinterpret_cast<const uint8_t*>(&packetCount) + sizeof(packetCount));

	for (const auto& packet : m_sessionPackets)
	{
		uint32_t packetSize = static_cast<uint32_t>(packet.size());
		fileBuffer.insert(fileBuffer.end(),
			reinterpret_cast<const uint8_t*>(&packetSize),
			reinterpret_cast<const uint8_t*>(&packetSize) + sizeof(packetSize));
		fileBuffer.insert(fileBuffer.end(), packet.begin(), packet.end());
	}

	FileSystem::StandardFileSystem fs;
	auto result = fs.WriteAllBytes(filePath, fileBuffer);
	if (result.HasError())
	{
		ProfilerDebug.printf("Failed to save profiler session: %s\n", result.GetError().c_str());
		return false;
	}

	m_currentSessionPath = filePath;
	m_dirty = false;

	// Update display name from file path
	auto pos = filePath.find_last_of("\\/");
	m_displayName = (pos != std::string::npos) ? filePath.substr(pos + 1) : filePath;

	ProfilerDebug.printf("Profiler session saved to %s (%u packets)\n", filePath.c_str(), packetCount);
	return true;
}

bool ProfilerSession::LoadSession(const std::string& filePath)
{
	DECLARE_FUNC_LOW();

	FileSystem::StandardFileSystem fs;
	auto readResult = fs.ReadAllBytes(filePath);
	if (readResult.HasError())
	{
		ProfilerDebug.printf("Failed to load profiler session: %s\n", readResult.GetError().c_str());
		return false;
	}

	const auto& fileBuffer = readResult.GetValue();
	size_t offset = 0;

	if (fileBuffer.size() < sizeof(uint32_t))
	{
		ProfilerDebug.printf("Invalid profiler session file (too small)\n");
		return false;
	}

	uint32_t packetCount = 0;
	std::memcpy(&packetCount, fileBuffer.data() + offset, sizeof(packetCount));
	offset += sizeof(packetCount);

	// Clear existing data before loading
	Clear();
	m_sessionPackets.clear();

	for (uint32_t i = 0; i < packetCount; ++i)
	{
		if (offset + sizeof(uint32_t) > fileBuffer.size())
		{
			ProfilerDebug.printf("Invalid profiler session file (truncated at packet %u)\n", i);
			return false;
		}

		uint32_t packetSize = 0;
		std::memcpy(&packetSize, fileBuffer.data() + offset, sizeof(packetSize));
		offset += sizeof(packetSize);

		if (offset + packetSize > fileBuffer.size())
		{
			ProfilerDebug.printf("Invalid profiler session file (truncated data at packet %u)\n", i);
			return false;
		}

		std::vector<uint8_t> packet(fileBuffer.begin() + offset,
			fileBuffer.begin() + offset + packetSize);
		offset += packetSize;

		// Replay the packet through the normal handler (which also re-retains it)
		HandleProfilerBinaryPacket(packet);
	}

	m_currentSessionPath = filePath;
	m_dirty = false;

	// Update display name from file path
	auto pos = filePath.find_last_of("\\/");
	m_displayName = (pos != std::string::npos) ? filePath.substr(pos + 1) : filePath;

	ProfilerDebug.printf("Profiler session loaded from %s (%u packets)\n", filePath.c_str(), packetCount);
	return true;
}

void ProfilerSession::SaveSessionAs()
{
	std::string path = Profiler::ViewUtils::ShowSaveFileDialog();
	if (!path.empty())
	{
		SaveSession(path);
	}
}

void ProfilerSession::SaveSessionCurrent()
{
	if (m_currentSessionPath.empty())
	{
		SaveSessionAs();
	}
	else
	{
		SaveSession(m_currentSessionPath);
	}
}

void ProfilerSession::LoadSessionDialog()
{
	std::string path = Profiler::ViewUtils::ShowOpenFileDialog();
	if (!path.empty())
	{
		LoadSession(path);
	}
}

void ProfilerSession::Initialize()
{
	DECLARE_FUNC_VLOW();

	if (m_initialized) return;

	registerFileActions();
	registerProfilerActions();

	m_initialized = true;
	ProfilerDebug.printf("ProfilerSession initialized\n");
}

void ProfilerSession::Shutdown()
{
	DECLARE_FUNC_VLOW();

	if (!m_initialized) return;

	m_initialized = false;
	ProfilerDebug.printf("ProfilerSession shutdown\n");
}

void ProfilerSession::StartProfiling()
{
	DECLARE_FUNC_LOW();
	NEXUS_SEND_MESSAGE(PROFILER_PIPE, MSG_TYPE_PROFILE_CONTROL, "Start");
	m_enabled = true;
}

void ProfilerSession::StopProfiling()
{
	DECLARE_FUNC_LOW();
	NEXUS_SEND_MESSAGE(PROFILER_PIPE, MSG_TYPE_PROFILE_CONTROL, "Stop");
	m_enabled = false;
}

void ProfilerSession::Clear()
{
	DECLARE_FUNC_LOW();

	m_analyzer.Clear();
	m_visualizer.ClearData();
	m_sessionPackets.clear();
	m_currentSessionPath.clear();
	m_dirty = false;
	ProfilerDebug.printf("Profiler data cleared\n");
}


void ProfilerSession::ResetView()
{
	DECLARE_FUNC_VLOW();

	m_visualizer.ResetView();
	m_showFlameGraph = true;
}

void ProfilerSession::registerFileActions()
{
	// File.Save
	m_actionManager.RegisterAction({
		.path = "File.Save",
		.description = "Save the current profiler session",
		.shortcutHint = "Ctrl+S",
		.targets = UI::ActionTarget::Menu | UI::ActionTarget::Console,
		.callback = [this]() { SaveSessionCurrent(); },
		.isEnabled = [this]() { return !m_sessionPackets.empty(); },
		.sortPriority = 20
		});

	// File.Save As
	m_actionManager.RegisterAction({
		.path = "File.Save As",
		.description = "Save the current profiler session to a new file",
		.shortcutHint = "Ctrl+Shift+S",
		.targets = UI::ActionTarget::Menu | UI::ActionTarget::Console,
		.callback = [this]() { SaveSessionAs(); },
		.isEnabled = [this]() { return !m_sessionPackets.empty(); },
		.sortPriority = 30
		});
}

void ProfilerSession::registerProfilerActions()
{
	// Profiler.Start
	m_actionManager.RegisterAction({
		.path = "Profiler.Start",
		.description = "Start profiling",
		.targets = UI::ActionTarget::Menu | UI::ActionTarget::Console | UI::ActionTarget::Toolbar,
		.callback = [this]() { StartProfiling(); },
		.isEnabled = [this]() { return m_enabled == false; },
		.isChecked = [this]() { return m_enabled == true; },
		.sortPriority = 10
		});

	// Profiler.Stop
	m_actionManager.RegisterAction({
		.path = "Profiler.Stop",
		.description = "Stop profiling",
		.targets = UI::ActionTarget::Menu | UI::ActionTarget::Console | UI::ActionTarget::Toolbar,
		.callback = [this]() { StopProfiling(); },
		.isEnabled = [this]() { return m_enabled == true; },
		.isChecked = [this]() { return m_enabled == false; },
		.sortPriority = 20
		});

	// Profiler.Clear
	m_actionManager.RegisterAction({
		.path = "Profiler.Clear",
		.description = "Clear profiler data",
		.targets = UI::ActionTarget::Menu | UI::ActionTarget::Console | UI::ActionTarget::Toolbar,
		.callback = [this]() { Clear(); },
		.sortPriority = 30
		});

	// Profiler.CompareFrames
	m_actionManager.RegisterAction({
		.path = "Profiler.CompareFrames",
		.description = "Toggle frame comparison butterfly chart",
		.targets = UI::ActionTarget::Menu | UI::ActionTarget::Console | UI::ActionTarget::Toolbar,
		.callback = [this]() { m_showFrameComparison = !m_showFrameComparison; },
		.isEnabled = [this]() { return m_visualizer.GetSelectedFrames().size() >= 2; },
		.isChecked = [this]() { return m_showFrameComparison; },
		.sortPriority = 40
		});

	// Profiler.SelectOutliers
	m_actionManager.RegisterAction({
		.path = "Profiler.SelectOutliers",
		.description = "Select frames exceeding the average duration by the outlier threshold",
		.targets = UI::ActionTarget::Menu | UI::ActionTarget::Console | UI::ActionTarget::Toolbar,
		.callback = [this]() { m_visualizer.SelectOutlierFrames(); },
		.sortPriority = 50
		});

	m_actionManager.RegisterAction({
		.path = "Profiler.Analize Spikes",
		.description = "Finds slow frames and allows comparison to average frames",
		.targets = UI::ActionTarget::Menu | UI::ActionTarget::Console | UI::ActionTarget::Toolbar,
		.callback = [this]() {  m_viewMode = ViewMode::OutlierAnalysis; },
		.isEnabled = [this]() { return m_viewMode != ViewMode::OutlierAnalysis; },
		.isChecked = [this]() { return m_viewMode == ViewMode::OutlierAnalysis; },
		.sortPriority = 60
		});

	m_actionManager.RegisterAction({
		.path = "Profiler.Flame Graph",
		.description = "Switches to the flame graph view of the data",
		.targets = UI::ActionTarget::Menu | UI::ActionTarget::Console | UI::ActionTarget::Toolbar,
		.callback = [this]() {  m_viewMode = ViewMode::FlameGraph; },
		.isEnabled = [this]() { return m_viewMode != ViewMode::FlameGraph; },
		.isChecked = [this]() { return m_viewMode == ViewMode::FlameGraph; },
		.sortPriority = 55
		});

	m_actionManager.RegisterAction({
		.path = "Profiler.Tree View",
		.description = "Switches to the a tree view of the data",
		.targets = UI::ActionTarget::Menu | UI::ActionTarget::Console | UI::ActionTarget::Toolbar,
		.callback = [this]() {  m_viewMode = ViewMode::TreeView; },
		.isEnabled = [this]() { return m_viewMode != ViewMode::TreeView; },
		.isChecked = [this]() { return m_viewMode == ViewMode::TreeView; },
		.sortPriority = 55
		});

}

void ProfilerSession::RenderSessionContent()
{
	// Apply pending data before rendering
	m_visualizer.ApplyPendingData();

	// Render per-session toolbar only (menu bar is handled by the parent window)
	m_actionManager.RenderToolbar();

	float pct = m_visualizer.GetOutlierThresholdPct();
	ImGui::SetNextItemWidth(80.0f);
	if (ImGui::SliderFloat("##OutlierPct", &pct, 1.0f, 200.0f, "%.0f%%"))
	{
		m_visualizer.SetOutlierThresholdPct(pct);
	}

	RenderThreadFilter();

	// Push thread filter to all views before rendering
	if (m_selectedThreadIds.empty()) {
		m_visualizer.ClearThreadFilter();
		m_treeView.ClearThreadFilter();
		m_frameComparisonView.ClearThreadFilter();
		m_outlierView.ClearThreadFilter();
	}
	else {
		m_visualizer.SetThreadFilter(m_selectedThreadIds);
		m_treeView.SetThreadFilter(m_selectedThreadIds);
		m_frameComparisonView.SetThreadFilter(m_selectedThreadIds);
		m_outlierView.SetThreadFilter(m_selectedThreadIds);
	}

	// ---- Dispatch to the active view ----
	switch (m_viewMode)
	{
	case ViewMode::FlameGraph:
		m_visualizer.RenderFlameGraphContent();
		break;

	case ViewMode::TreeView:
		m_treeView.RenderContent(m_visualizer.GetTimelineData());
		break;

	case ViewMode::OutlierAnalysis:
		m_outlierView.RenderContent(m_visualizer.GetTimelineData(),
			m_visualizer.GetOutlierThresholdPct());
		break;
	}

	// Render the frame comparison as a separate window (independent of view mode)
	if (m_showFrameComparison) {
		std::string compTitle = "Frame Comparison - " + m_displayName;
		m_frameComparisonView.Render(compTitle.c_str(),
			m_visualizer.GetTimelineData(),
			m_visualizer.GetSelectedFrames(),
			&m_showFrameComparison);
	}
}

void ProfilerSession::RenderThreadFilter()
{
	ImGui::Text("Thread:");
	ImGui::SameLine();

	// Display selected threads as comma-separated, or "All"
	std::string selectedLabel;
	if (m_selectedThreadIds.empty())
		selectedLabel = "All";
	else
		for (size_t i = 0; i < m_selectedThreadIds.size(); ++i) {
			selectedLabel += m_selectedThreadIds[i];
			if (i + 1 < m_selectedThreadIds.size())
				selectedLabel += ", ";
		}

	ImGui::PushID("ThreadFilter");
	if (ImGui::BeginCombo("##Combo", selectedLabel.c_str())) {
		// Text filter box
		char filterBuf[256] = {};
		strncpy_s(filterBuf, sizeof(filterBuf), m_threadFilterText.c_str(), _TRUNCATE);
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (ImGui::InputTextWithHint("##FilterText", "Filter...", filterBuf, sizeof(filterBuf))) {
			m_threadFilterText = filterBuf;
		}

		// Build list of visible (filtered, non-empty) thread names
		std::vector<std::string> visibleThreads;
		for (const std::string& tid : m_threadNames) {
			if (tid.empty())
				continue;
			if (!m_threadFilterText.empty() &&
				tid.find(m_threadFilterText) == std::string::npos)
				continue;
			visibleThreads.push_back(tid);
		}

		// Select All / Clear All buttons
		if (ImGui::Button("Select All")) {
			for (const std::string& tid : visibleThreads) {
				if (std::find(m_selectedThreadIds.begin(), m_selectedThreadIds.end(), tid) == m_selectedThreadIds.end()) {
					m_selectedThreadIds.push_back(tid);
				}
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear All")) {
			for (const std::string& tid : visibleThreads) {
				m_selectedThreadIds.erase(
					std::remove(m_selectedThreadIds.begin(), m_selectedThreadIds.end(), tid),
					m_selectedThreadIds.end());
			}
		}

		ImGui::Separator();

		// "All" option (resets to no filter)
		bool allSelected = m_selectedThreadIds.empty();
		if (ImGui::Selectable("All", allSelected)) {
			m_selectedThreadIds.clear();
		}

		// Filter and show thread names
		for (const std::string& tid : visibleThreads)
		{
			bool isSelected = std::find(m_selectedThreadIds.begin(), m_selectedThreadIds.end(), tid) != m_selectedThreadIds.end();
			if (ImGui::Checkbox(tid.c_str(), &isSelected)) {
				if (isSelected) {
					m_selectedThreadIds.push_back(tid);
				}
				else {
					m_selectedThreadIds.erase(std::remove(m_selectedThreadIds.begin(), m_selectedThreadIds.end(), tid), m_selectedThreadIds.end());
				}
			}
		}
		ImGui::EndCombo();
	}
	ImGui::PopID();
}

void ProfilerSession::RenderFlameGraphWindow(const std::string& windowTitle, bool* showWindow)
{
	if (!m_showFlameGraph)
		return;

	// Apply pending data before rendering
	m_visualizer.ApplyPendingData();

	ImGuiWindowFlags wflags = ImGuiWindowFlags_MenuBar;

	if (ImGui::Begin(windowTitle.c_str(), showWindow, wflags))
	{
		// Capture mouse wheel early, before child windows consume it
		m_visualizer.CaptureMouseWheel();

		// Render profiler-specific menu bar
		if (ImGui::BeginMenuBar())
		{
			m_actionManager.RenderMenuBar();
			ImGui::EndMenuBar();
		}

		m_actionManager.RenderToolbar();

		float pct = m_visualizer.GetOutlierThresholdPct();
		ImGui::SetNextItemWidth(80.0f);
		if (ImGui::SliderFloat("##OutlierPct", &pct, 1.0f, 200.0f, "%.0f%%"))
		{
			m_visualizer.SetOutlierThresholdPct(pct);
		}

		// ---- Dispatch to the active view ----
		switch (m_viewMode)
		{
		case ViewMode::FlameGraph:
			m_visualizer.RenderFlameGraphContent();
			break;

		case ViewMode::TreeView:
			m_treeView.RenderContent(m_visualizer.GetTimelineData());
			break;

		case ViewMode::OutlierAnalysis:
			m_outlierView.RenderContent(m_visualizer.GetTimelineData(),
				m_visualizer.GetOutlierThresholdPct());
			break;
		}
	}
	ImGui::End();

	// Render the frame comparison as a separate window (independent of view mode)
	if (m_showFrameComparison) {
		m_frameComparisonView.Render("Frame Comparison - Butterfly Chart",
			m_visualizer.GetTimelineData(),
			m_visualizer.GetSelectedFrames(),
			&m_showFrameComparison);
	}
}
