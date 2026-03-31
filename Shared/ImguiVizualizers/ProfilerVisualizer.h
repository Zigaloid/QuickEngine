#pragma once

#include "imgui.h"
#include "Profiler/ProfilerAnalyzer.h"
#include "ProfilerViewUtils.h"
#include "UnifiedActionManager.h"
#include <memory>
#include <mutex>
#include <vector>
#include <string>
#include <map>
#include <set>

namespace Profiler {

	class ProfilerVisualizer
	{
	public:
		ProfilerVisualizer();
		~ProfilerVisualizer();

		// Main rendering methods
		bool RenderFlameGraphWindow(const char* windowTitle, bool* isOpen = nullptr);
		void SetFlameGraphData(std::unique_ptr<TimelineFlameGraphData> data);

		// Render only the flame graph content (no window/menu/buttons).
		// Must be called between ImGui::Begin / ImGui::End.
		void RenderFlameGraphContent();

		// Apply any staged pending data (call before rendering)
		void ApplyPendingData();

		// Data management
		void ClearData();

		// Data access — allows other views to read the timeline data
		TimelineFlameGraphData* GetTimelineData() { return m_TimelineFlameGraphData.get(); }

		// Configuration
		void SetShowCallCounts(bool show) { m_ShowCallCounts = show; }
		void SetShowFrameMarkers(bool show) { m_ShowFrameMarkers = show; }
		void SetShowTimeTicks(bool show) { m_ShowTimeTicks = show; }
		void SetMinDisplayTime(float minTime) { m_MinDisplayTime = minTime; }
		void SetAutoRefresh(bool autoRefresh) { m_AutoRefreshFlameGraph = autoRefresh; }

		// Getters
		bool GetShowCallCounts() const { return m_ShowCallCounts; }
		bool GetShowFrameMarkers() const { return m_ShowFrameMarkers; }
		bool GetShowTimeTicks() const { return m_ShowTimeTicks; }
		float GetMinDisplayTime() const { return m_MinDisplayTime; }
		bool GetAutoRefresh() const { return m_AutoRefreshFlameGraph; }

		// Frame selection
		const std::set<uint64_t>& GetSelectedFrames() const { return m_SelectedFrames; }
		std::set<uint64_t>& GetSelectedFrames() { return m_SelectedFrames; }
		bool IsFrameSelected(uint64_t frameNumber) const { return m_SelectedFrames.count(frameNumber) > 0; }
		void SelectFrame(uint64_t frameNumber) { m_SelectedFrames.insert(frameNumber); }
		void DeselectFrame(uint64_t frameNumber) { m_SelectedFrames.erase(frameNumber); }
		void ToggleFrameSelection(uint64_t frameNumber);
		void ClearFrameSelection() { m_SelectedFrames.clear(); }

		// Outlier detection
		float GetOutlierThresholdPct() const { return m_OutlierThresholdPct; }
		void SetOutlierThresholdPct(float pct) { m_OutlierThresholdPct = pct; }
		void SelectOutlierFrames();

		// View control
		void ResetView();
		void ZoomToNode(TimelineFlameNode* node);

		// Capture the mouse wheel for Ctrl+zoom (call after ImGui::Begin)
		void CaptureMouseWheel();

		// Thread filter: when non-empty, only threads whose name is in this set are rendered.
		// An empty set means "show all threads".
		void SetThreadFilter(const std::vector<std::string>& threadNames) { m_ThreadFilter.assign(threadNames.begin(), threadNames.end()); }
		void ClearThreadFilter() { m_ThreadFilter.clear(); }

	private:
		// Rendering methods
		void RenderFlameGraph();
		bool InitializeFlameGraphRendering();
		void InitializeFlameGraphView();

		// Returns true if the given thread should be rendered based on the current filter
		bool IsThreadInFilter(const TimelineThreadData* thread) const;

		// Mouse and keyboard interaction
		void HandleMouseInteraction(ImVec2 canvasPos, ImVec2 canvasSize);
		void HandleMouseWheelZoom(ImVec2 canvasPos, ImVec2 canvasSize);
		void HandleMouseDrag(ImVec2 canvasSize);
		void HandleKeyboardZoom(ImVec2 canvasPos, ImVec2 canvasSize);
		void HandleFrameSelectionClick(ImVec2 canvasPos, ImVec2 canvasSize);

		// Event rendering
		void RenderEvents(TimelineThreadData* thread, ImVec2 canvasPos, ImVec2 canvasSize, ImDrawList* drawList);
		void RenderFlameGraphNode(TimelineFlameNode* node, TimelineThreadData* thread,
			float containerWidth, float nodeHeight, uint64_t globalTotalTime,
			ImVec2 canvasPos, ImDrawList* drawList, float startY);

		// UI rendering
		void RenderGlobalTimeTicks(ImVec2 canvasPos, ImVec2 canvasSize, ImDrawList* drawList, float tickHeight);
		void RenderGlobalFrameMarkers(ImVec2 canvasPos, ImVec2 canvasSize, ImDrawList* drawList, float startY);
		void RenderFrameBackdrops(ImVec2 canvasPos, ImVec2 canvasSize, ImDrawList* drawList);
		void RenderThreadHeader(size_t threadIndex, TimelineThreadData* thread, ImVec2 headerPos, ImVec2 headerSize, ImDrawList* drawList);
		void RenderNodeInteractionUI(TimelineThreadData* selectedThread);

		// Utility helpers
		struct VisibleTimeRange {
			float startTime;
			float endTime;
			float duration;
		};
		VisibleTimeRange GetVisibleTimeRange() const;
		float TimeToScreenX(float time, float canvasX, float canvasWidth, const VisibleTimeRange& vtr) const;
		int CalculateActualMaxDepth(TimelineThreadData* thread);

		// Formatting helper for millisecond values (not covered by ViewUtils)
		std::string FormatMillisecondsWithUnit(double milliseconds);

		// Timeline flamegraph data (only accessed on render thread)
		std::unique_ptr<TimelineFlameGraphData> m_TimelineFlameGraphData;

		// Thread-safe pending data staging
		std::mutex m_PendingDataMutex;
		std::unique_ptr<TimelineFlameGraphData> m_PendingFlameGraphData;
		bool m_HasPendingData = false;
		bool m_PendingClear = false;

		// View state
		float m_FlameGraphZoom = 1.0f;
		float m_FlameGraphCenterTime = 0.0f;
		bool m_ViewInitialized = false;

		// Interaction state
		TimelineFlameNode* m_HoveredNode = nullptr;
		TimelineFlameNode* m_SelectedNode = nullptr;
		TimelineThreadData* m_HoveredThread = nullptr;
		TimelineThreadData* m_SelectedThread = nullptr;

		// Frame selection state
		std::set<uint64_t> m_SelectedFrames;

		// Display options
		bool m_ShowCallCounts = false;
		bool m_ShowFrameMarkers = true;
		bool m_ShowTimeTicks = true;
		float m_MinDisplayTime = 0.1f;
		bool m_AutoRefreshFlameGraph = false;
		float m_actualRenderedHeight = 100.0f;
		std::vector<bool> m_ThreadVisibility;

		// Thread name filter (empty = show all)
		std::vector<std::string> m_ThreadFilter;

		// CPU frequency for accurate timing calculations
		double m_CPUFrequency = 3.0e9;

		// Saved mouse wheel value for zoom
		float m_SavedMouseWheel = 0.0f;
		bool m_IsCapturing = false;

		// Outlier detection threshold (percentage above average)
		float m_OutlierThresholdPct = 20.0f;
	};

} // namespace Profiler