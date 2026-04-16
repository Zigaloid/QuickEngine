#pragma once

#include "imgui.h"
#include "Net/NexusProtocol.h"
#include "IImGuiVisualizer.h"

#include <chrono>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

// Forward declarations
class CNexusClient;

/**
 * @brief ImGui-based structured log visualizer for Nexus IPC messages.
 *
 * Subscribes to messages from a CNexusClient and displays them in a
 * scrollable, filterable log grouped by appName and pipeName.
 */
class NexusLogVisualizer : public ImGuiVisualizers::IImGuiVisualizer
{
public:

	/**
 * @brief Called once after the visualizer is registered with the manager.
 * Use this for deferred setup that requires external systems to be ready.
 */
	void Initialize() override
	{
		InitializePipes();
	}

	void Shutdown() override;
	void Update(float deltaTime) override { (void)deltaTime; }

	bool Render(bool* isOpen) override
	{
		RenderWindow(GetName(), isOpen);
		return true;
	}

	const char* GetName() const override { return "Log"; }
	const char* GetShortcut() const override { return nullptr; }
	const char* GetMenuCategory() const override { return "Show"; }

	/**
	 * @brief A single log entry received from the Nexus network.
	 */
	struct LogEntry
	{
		std::string senderApp;
		std::string pipeName;
		std::string body;
		std::chrono::system_clock::time_point timestamp;

		LogEntry(const std::string& app, const std::string& pipe,
			const std::string& msg, std::chrono::system_clock::time_point ts)
			: senderApp(app), pipeName(pipe), body(msg), timestamp(ts) {
		}
	};

	/**
	 * @brief Constructor
	 * @param maxEntries Maximum number of log entries to retain (default: 5000)
	 */
	explicit NexusLogVisualizer(int maxEntries = 5000);
	~NexusLogVisualizer();

	// Non-copyable
	NexusLogVisualizer(const NexusLogVisualizer&) = delete;
	NexusLogVisualizer& operator=(const NexusLogVisualizer&) = delete;

	/**
	 * @brief Initialize the visualizer by subscribing to NexusClient messages.
	 * Subscribes to all pipes/apps ("ANY","ANY") by default.
	 * @param pipeName Pipe to subscribe to, or "ANY" for all pipes.
	 * @param appName App to subscribe to, or "ANY" for all apps.
	 * @return true if NexusClient was found and subscription succeeded.
	 */
	bool InitializePipes(const std::string& pipeName = "ANY", const std::string& appName = "ANY");


	/**
	 * @brief Manually push a log entry (thread-safe).
	 */
	void AddEntry(const std::string& senderApp, const std::string& pipeName, const std::string& body);

	/**
	 * @brief Clear all log entries.
	 */
	void ClearEntries();

	/**
	 * @brief Render the log visualizer ImGui window.
	 * @param windowTitle Title for the ImGui window.
	 * @param isOpen Pointer to bool controlling window visibility.
	 * @return true if window is open and rendering.
	 */
	bool RenderWindow(const char* windowTitle = "Nexus Log", bool* isOpen = nullptr);

	/**
	 * @brief Save the current filter and collection states to a JSON file.
	 * @param filePath Path to the output JSON file.
	 * @return true if the save succeeded.
	 */
	bool SaveFilterState(const std::string& filePath) const;

	/**
	 * @brief Load filter and collection states from a JSON file.
	 * Loaded keys that match known apps/pipes update their state;
	 * keys not yet known are stored and applied when the app/pipe appears.
	 * @param filePath Path to the input JSON file.
	 * @return true if the load succeeded.
	 */
	bool LoadFilterState(const std::string& filePath);

	// Accessors
	bool IsEnabled() const { return m_isEnabled; }
	void SetEnabled(bool val) { m_isEnabled = val; }
	int  GetEntryCount() const;

private:
	// Rendering helpers
	void RenderToolbar();
	void RenderLogTable();
	void RenderStatusBar();

	// Filtering helpers
	bool PassesFilter(const LogEntry& entry) const;
	bool IsCollectionDisabled(const std::string& senderApp, const std::string& pipeName) const;
	std::string FormatTimestamp(std::chrono::system_clock::time_point tp) const;

	// NexusClient message callback
	void OnNexusMessage(const SNexusMessage& msg);

	// Log storage (guarded by m_mutex)
	mutable std::mutex m_mutex;
	std::vector<LogEntry> m_entries;
	int m_maxEntries;

	// Known senders and pipes for filter combo boxes
	std::set<std::string> m_knownApps;
	std::set<std::string> m_knownPipes;

	// Filter state � multi-select visibility (true = shown in log view)
	std::unordered_map<std::string, bool> m_appVisibility;
	std::unordered_map<std::string, bool> m_pipeVisibility;

	// Collection state � when false, incoming entries are discarded
	std::unordered_map<std::string, bool> m_appCollectionEnabled;
	std::unordered_map<std::string, bool> m_pipeCollectionEnabled;

	char m_textFilter[256];
	bool m_autoScroll;
	bool m_scrollToBottom;

	// Subscription info
	std::string m_subscribedPipe;
	std::string m_subscribedApp;
	CNexusClient* m_nexusClient;

	// State
	bool m_isEnabled = false;
	bool m_wasWindowOpen = false;
};