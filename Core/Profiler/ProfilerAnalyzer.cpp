#include "ProfilerAnalyzer.h"
#include "Profiler.h"  // Include for ProfileEvent access

#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <functional>
#include <chrono>
#include <execution>  // For std::execution::par

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <windows.h>
#endif

#include "CoreSystem/CoreSystem.h"  // Add for JobSystem access

namespace Profiler 
{
	// Helper function to format timing values in human-readable units
	static std::string FormatTiming(uint64_t cycles, double cpuFrequency) {
		if (cycles == 0) return "0";

		double seconds = static_cast<double>(cycles) / cpuFrequency;

		std::ostringstream oss;
		oss << std::fixed;

		if (seconds >= 1.0) {
			oss << std::setprecision(3) << seconds << " s";
		}
		else if (seconds >= 1e-3) {
			oss << std::setprecision(3) << (seconds * 1e3) << " ms";
		}
		else if (seconds >= 1e-6) {
			oss << std::setprecision(3) << (seconds * 1e6) << " us";
		}
		else {
			oss << std::setprecision(0) << (seconds * 1e9) << " ns";
		}

		return oss.str();
	}

	// ===== FlameNode Implementation =====
	void FlameNode::AddChild(std::unique_ptr<FlameNode> child) {
		if (child) {
			child->parent = this;
			children.push_back(std::move(child));
		}
	}

	FlameNode* FlameNode::FindChild(const std::string& functionName) {
		for (auto& child : children) {
			if (child->name == functionName) {
				return child.get();
			}
		}
		return nullptr;
	}

	// ===== TimelineThreadData Implementation =====
	void TimelineThreadData::CalculateTimelineLayout() {
		if (threadDuration == 0 || events.empty()) return;

		// Calculate layout positions based on timeline
		for (auto& event : events) {
			// Calculate x position (0.0 to 1.0) based on start time relative to thread timeline
			event->x = static_cast<float>(event->startTime - threadStartTime) / threadDuration;

			// Calculate width (0.0 to 1.0) based on duration relative to thread timeline
			event->width = static_cast<float>(event->duration) / threadDuration;

			// Clamp values to valid range
			event->x = std::max(0.0f, std::min(1.0f, event->x));
			event->width = std::max(0.0f, std::min(1.0f - event->x, event->width));
		}
	}

	std::vector<TimelineFlameNode*> TimelineThreadData::GetEventsAtDepth(int targetDepth) const {
		std::vector<TimelineFlameNode*> result;
		for (const auto& event : events) {
			if (event->depth == targetDepth) {
				result.push_back(event.get());
			}
		}
		return result;
	}

	std::vector<TimelineFlameNode*> TimelineThreadData::GetEventsInTimeRange(uint64_t startTime, uint64_t endTime) const {
		std::vector<TimelineFlameNode*> result;
		for (const auto& event : events) {
			// Check if event overlaps with the time range
			if (event->endTime >= startTime && event->startTime <= endTime) {
				result.push_back(event.get());
			}
		}
		return result;
	}

	std::vector<const FrameMarker*> TimelineThreadData::GetFrameMarkersInTimeRange(uint64_t startTime, uint64_t endTime) const {
		std::vector<const FrameMarker*> result;
		for (const auto& marker : frameMarkers) {
			if (marker.clockCycles >= startTime && marker.clockCycles <= endTime) {
				result.push_back(&marker);
			}
		}
		return result;
	}

	// ===== TimelineFlameGraphData Implementation =====
	TimelineThreadData* TimelineFlameGraphData::FindThread(size_t threadHash) {
		for (auto& thread : threads) {
			if (thread->threadHash == threadHash) {
				return thread.get();
			}
		}
		return nullptr;
	}

	TimelineThreadData* TimelineFlameGraphData::GetThread(size_t index) {
		if (index < threads.size()) {
			return threads[index].get();
		}
		return nullptr;
	}

	void TimelineFlameGraphData::CalculateGlobalTimeline() {
		globalStartTime = UINT64_MAX;
		globalEndTime = 0;

		// Calculate global timeline bounds across all threads
		for (const auto& thread : threads) {
			if (thread->threadStartTime < globalStartTime) {
				globalStartTime = thread->threadStartTime;
			}
			if (thread->threadEndTime > globalEndTime) {
				globalEndTime = thread->threadEndTime;
			}

			// Calculate layout for each thread
			thread->CalculateTimelineLayout();
		}

		globalDuration = globalEndTime - globalStartTime;
	}

	std::vector<TimelineFlameNode*> TimelineFlameGraphData::GetAllEventsInTimeRange(uint64_t startTime, uint64_t endTime) const {
		std::vector<TimelineFlameNode*> result;
		for (const auto& thread : threads) {
			auto threadEvents = thread->GetEventsInTimeRange(startTime, endTime);
			result.insert(result.end(), threadEvents.begin(), threadEvents.end());
		}
		return result;
	}

	std::vector<const FrameMarker*> TimelineFlameGraphData::GetAllFrameMarkersInTimeRange(uint64_t startTime, uint64_t endTime) const {
		std::vector<const FrameMarker*> result;
		for (const auto& thread : threads) {
			auto threadMarkers = thread->GetFrameMarkersInTimeRange(startTime, endTime);
			result.insert(result.end(), threadMarkers.begin(), threadMarkers.end());
		}
		return result;
	}

	// ===== ThreadFlameData Implementation =====
	void ThreadFlameData::CalculateLayout() {
		if (!rootNode || rootNode->children.empty()) return;

		uint64_t timelineStart = threadStartTime;
		uint64_t timelineEnd = threadEndTime;

		if (timelineStart == timelineEnd) {
			// Handle edge case where timeline has no duration
			timelineEnd = timelineStart + 1;
		}

		// Calculate layout for all nodes starting from root's children
		float currentX = 0.0f;
		for (auto& child : rootNode->children) {
			float nodeWidth = static_cast<float>(child->totalTime) / (timelineEnd - timelineStart);
			CalculateNodeLayout(child.get(), currentX, nodeWidth, timelineStart, timelineEnd);
			currentX += nodeWidth;
		}
	}

	void ThreadFlameData::CalculateNodeLayout(FlameNode* node, float startX, float width,
		uint64_t timelineStart, uint64_t timelineEnd) {
		if (!node) return;

		node->x = startX;
		node->width = width;

		// Calculate layout for children
		if (!node->children.empty()) {
			float childStartX = startX;
			for (auto& child : node->children) {
				float childWidth = width * (static_cast<float>(child->totalTime) / node->totalTime);
				CalculateNodeLayout(child.get(), childStartX, childWidth, timelineStart, timelineEnd);
				childStartX += childWidth;
			}
		}
	}

	// ===== FlameGraphData Implementation =====
	void FlameGraphData::CalculateGlobalTimeline() {
		// Calculate layout for each thread
		for (auto& thread : threads) {
			thread->CalculateLayout();
		}
	}
	
	void ProfilerAnalyzer::LoadFromProfileEvents(const std::deque<ProfileEvent>& events, bool clearExisting) {
		if (clearExisting) {
			Clear();
		}

		std::vector<RawEvent> rawEvents;
		std::vector<FrameMarker> frameMarkers;

		ConvertProfileEventsToRawEvents(events, rawEvents, frameMarkers);

		// Append frame markers
		m_frameMarkers.insert(m_frameMarkers.end(), frameMarkers.begin(), frameMarkers.end());
		
		MatchStartEndPairs(rawEvents);
	}

	void ProfilerAnalyzer::AppendFromProfileEvents(const std::deque<ProfileEvent>& events) {
		LoadFromProfileEvents(events, false);
	}

	void ProfilerAnalyzer::Clear() {
		m_analyzedEvents.clear();
		m_frameMarkers.clear();
		m_callStacks.clear();
		m_threadHashToString.clear();
	}

	size_t ProfilerAnalyzer::GetThreadCount() const {
		return m_threadHashToString.size();
	}

	void ProfilerAnalyzer::ConvertProfileEventsToRawEvents(const std::deque<ProfileEvent>& events,
		std::vector<RawEvent>& outRawEvents,
		std::vector<FrameMarker>& outFrameMarkers) {
		outRawEvents.clear();
		outFrameMarkers.clear();
		outRawEvents.reserve(events.size());

		for (const auto& event : events) {
			// Use threadIdStr hash for consistent representation
			std::hash<std::string> hasher;
			size_t threadHash = hasher(event.threadIdStr);

			if (event.GetEventType() == EventType::Frame) {
				// Handle frame events
				outFrameMarkers.emplace_back(threadHash, event.GetClockCycles(), event.GetFrameNumber());
			}
			else {
				// Handle scoped events
				RawEvent rawEvent;
				rawEvent.threadHash = threadHash;
				rawEvent.name = event.name;
				rawEvent.clockCycles = event.GetClockCycles();
				rawEvent.isStart = event.isStart;
				outRawEvents.push_back(rawEvent);
			}

			// Store thread hash to display name mapping
			// Prefer the human-readable thread name if available, otherwise fall back to ID string
			if (!event.threadNameStr.empty()) {
				m_threadHashToString[threadHash] = event.threadNameStr;
			} else {
				m_threadHashToString[threadHash] = event.threadIdStr;
			}
		}
	}

	void ProfilerAnalyzer::LoadFromFile(const std::string& filename) {
		std::ifstream file(filename);
		if (!file.is_open()) {
			std::cerr << "Failed to open profile file: " << filename << std::endl;
			return;
		}

		std::string line;
		// Skip header
		std::getline(file, line);

		std::vector<RawEvent> rawEvents;
		m_frameMarkers.clear(); // Clear existing frame markers

		while (std::getline(file, line)) {
			std::istringstream ss(line);
			std::string threadStr, name, cyclesStr, eventType, frameNumberStr;

			// Parse the new 5-column CSV format: ThreadID,Name,ClockCycles,EventType,FrameNumber
			if (std::getline(ss, threadStr, ',') &&
				std::getline(ss, name, ',') &&
				std::getline(ss, cyclesStr, ',') &&
				std::getline(ss, eventType, ',') &&
				std::getline(ss, frameNumberStr)) {

				// Parse thread ID string as hash value
				size_t threadHash = 0;
				try {
					threadHash = std::stoull(threadStr);
				}
				catch (const std::exception& e) {
					std::cerr << "Failed to parse thread ID: " << threadStr << " - " << e.what() << std::endl;
					continue;
				}

				// Handle Frame events separately - STORE THEM instead of discarding!
				if (eventType == "FRAME") {
					try {
						uint64_t frameNumber = std::stoull(frameNumberStr);
						uint64_t clockCycles = std::stoull(cyclesStr);
						m_frameMarkers.emplace_back(threadHash, clockCycles, frameNumber);
					}
					catch (const std::exception& e) {
						std::cerr << "Failed to parse frame event: " << e.what() << std::endl;
					}
					continue; // Continue to next line after processing frame event
				}

				// Handle scoped events
				RawEvent event;
				event.threadHash = threadHash;
				event.name = name;
				event.clockCycles = std::stoull(cyclesStr);
				event.isStart = (eventType == "START");

				rawEvents.push_back(event);
			}
			else {
				// Try parsing the old 4-column format for backward compatibility
				std::istringstream ss2(line);
				if (std::getline(ss2, threadStr, ',') &&
					std::getline(ss2, name, ',') &&
					std::getline(ss2, cyclesStr, ',') &&
					std::getline(ss2, eventType)) {

					// Parse thread ID string as hash value
					size_t threadHash = 0;
					try {
						threadHash = std::stoull(threadStr);
					}
					catch (const std::exception& e) {
						std::cerr << "Failed to parse thread ID: " << threadStr << " - " << e.what() << std::endl;
						continue;
					}

					RawEvent event;
					event.threadHash = threadHash;
					event.name = name;
					event.clockCycles = std::stoull(cyclesStr);
					event.isStart = (eventType == "START");

					rawEvents.push_back(event);
				}
			}
		}

		std::cout << "Loaded " << m_frameMarkers.size() << " frame markers and "
			<< rawEvents.size() << " scoped events" << std::endl; // Debug output

		MatchStartEndPairs(rawEvents);
	}

	// ORIGINAL: Keep for compatibility and small datasets
	void ProfilerAnalyzer::MatchStartEndPairs(const std::vector<RawEvent>& rawEvents) {
		// Stack to track nested function calls per thread
		std::map<size_t, std::vector<RawEvent>> threadStacks;

		m_analyzedEvents.clear();

		for (const auto& event : rawEvents) {
			auto& stack = threadStacks[event.threadHash];

			if (event.isStart) {
				// Push start event onto stack
				stack.push_back(event);
			}
			else {
				// Find matching start event
				if (!stack.empty()) {
					// Look for matching function name from the top of stack
					for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
						if (it->name == event.name) {
							// Found matching start event
							AnalyzedEvent analyzed;
							analyzed.threadHash = event.threadHash;  // Fixed: Use actual thread hash
							analyzed.name = event.name;
							analyzed.startCycles = it->clockCycles;
							analyzed.endCycles = event.clockCycles;
							analyzed.duration = event.clockCycles - it->clockCycles;
							analyzed.depth = static_cast<int>(std::distance(it, stack.rend()) - 1);

							m_analyzedEvents.push_back(analyzed);

							// Remove the matched start event and all nested calls
							stack.erase(std::next(it).base(), stack.end());
							break;
						}
					}
				}
			}
		}

		// Store thread hash to string mapping for display (only if not already set
		// by ConvertProfileEventsToRawEvents with a human-readable name)
		for (const auto& [threadHash, stack] : threadStacks) {
			if (m_threadHashToString.find(threadHash) == m_threadHashToString.end()) {
				m_threadHashToString[threadHash] = std::to_string(threadHash);
			}
		}
	}

	void ProfilerAnalyzer::ReconstructCallStack() {
		// Sort events by start time within each thread
		std::map<size_t, std::vector<const AnalyzedEvent*>> threadEvents;

		for (const auto& event : m_analyzedEvents) {
			threadEvents[event.threadHash].push_back(&event);  // Fixed: Use actual thread hash
		}

		// Sort by start time for each thread
		for (auto& [threadHash, events] : threadEvents) {
			std::sort(events.begin(), events.end(),
				[](const AnalyzedEvent* a, const AnalyzedEvent* b) {
					return a->startCycles < b->startCycles;
				});

			// Build call stack representation
			std::vector<std::string> callStack;
			for (const auto* event : events) {
				// Add indentation based on depth
				std::string indented = std::string(event->depth * 2, ' ') + event->name;
				callStack.push_back(indented);
			}

			m_callStacks[threadHash] = callStack;  // Fixed: Use actual thread hash as key
		}
	}

	void ProfilerAnalyzer::AnalyzeCallStack() {
		ReconstructCallStack();

		// Additional analysis can be added here:
		// - Calculate average function times
		// - Find hotspots
		// - Detect performance bottlenecks
	}

	void ProfilerAnalyzer::PrintReport() const 
	{
		static double cpuFrequency = GetCPUFrequency();

		std::cout << "\n=== PROFILER REPORT ===" << std::endl;
		std::cout << "CPU Frequency (estimated): " << std::fixed << std::setprecision(2)
			<< (cpuFrequency / 1e9) << " GHz" << std::endl;
		std::cout << std::left << std::setw(30) << "Function Name"
			<< std::setw(15) << "Duration"
			<< std::setw(15) << "Duration (cycles)"
			<< std::setw(10) << "Depth"
			<< std::setw(15) << "Thread Hash" << std::endl;
		std::cout << std::string(85, '-') << std::endl;

		for (const auto& event : m_analyzedEvents) {
			std::string formattedTime = FormatTiming(event.duration, cpuFrequency);

			std::cout << std::left << std::setw(30) << event.name
				<< std::setw(15) << formattedTime
				<< std::setw(15) << event.duration
				<< std::setw(10) << event.depth
				<< std::setw(15) << event.threadHash << std::endl;  // Fixed: Use actual thread hash
		}

		std::cout << "\nTotal Events: " << m_analyzedEvents.size() << std::endl;
	}

	void ProfilerAnalyzer::PrintCallStack() const 
	{
		static double cpuFrequency = GetCPUFrequency();

		std::cout << "\n=== CALL STACK HIERARCHY ===" << std::endl;
		std::cout << "CPU Frequency (estimated): " << std::fixed << std::setprecision(2)
			<< (cpuFrequency / 1e9) << " GHz" << std::endl;

		// Structure to hold function statistics
		struct FunctionStats {
			uint64_t totalCycles = 0;
			uint32_t callCount = 0;
			uint64_t minCycles = UINT64_MAX;
			uint64_t maxCycles = 0;
			std::map<std::string, FunctionStats> children; // Child functions
		};

		// Group events by thread hash
		std::map<size_t, std::vector<const AnalyzedEvent*>> threadGroups;
		for (const auto& event : m_analyzedEvents) {
			threadGroups[event.threadHash].push_back(&event);  // Fixed: Use actual thread hash
		}

		// Process each thread separately
		for (const auto& [threadHash, events] : threadGroups) {
			std::cout << "\n" << std::string(80, '=') << std::endl;
			std::cout << "Thread " << threadHash << std::endl;
			std::cout << std::string(80, '=') << std::endl;

			// Sort events by start time to maintain chronological order
			auto sortedEvents = events;
			std::sort(sortedEvents.begin(), sortedEvents.end(),
				[](const AnalyzedEvent* a, const AnalyzedEvent* b) {
					return a->startCycles < b->startCycles;
				});

			// Build hierarchical tree structure
			FunctionStats rootStats;
			std::vector<std::string> callStack;

			for (const auto* event : sortedEvents) {
				// Navigate to the correct position in the hierarchy
				FunctionStats* currentStats = &rootStats;

				// Resize call stack to match current depth
				if (static_cast<size_t>(event->depth + 1) > callStack.size()) {
					callStack.resize(event->depth + 1);
				}
				else {
					callStack.resize(event->depth + 1);
				}

				callStack[event->depth] = event->name;

				// Navigate through the hierarchy to find the correct parent
				for (int i = 0; i <= event->depth; ++i) {
					if (i == event->depth) {
						// This is the function we're recording stats for
						auto& stats = currentStats->children[callStack[i]];
						stats.totalCycles += event->duration;
						stats.callCount++;
						stats.minCycles = std::min(stats.minCycles, event->duration);
						stats.maxCycles = std::max(stats.maxCycles, event->duration);
					}
					else {
						// Navigate to the parent function
						currentStats = &currentStats->children[callStack[i]];
					}
				}
			}

			// Recursive function to print the hierarchy
			std::function<void(const std::map<std::string, FunctionStats>&, int)> printHierarchy;
			printHierarchy = [&](const std::map<std::string, FunctionStats>& functions, int depth) {
				for (const auto& [functionName, stats] : functions) {
					// Print indentation based on depth
					std::string indent(depth * 2, ' ');

					// Calculate average cycles
					uint64_t avgCycles = stats.callCount > 0 ? stats.totalCycles / stats.callCount : 0;

					// Format timing values
					std::string totalTime = FormatTiming(stats.totalCycles, cpuFrequency);
					std::string avgTime = FormatTiming(avgCycles, cpuFrequency);

					// Print function with statistics
					std::cout << indent << functionName;
					if (stats.callCount > 0) {
						std::cout << " [Total: " << totalTime
							<< ", Calls: " << stats.callCount
							<< ", Avg: " << avgTime;

						if (stats.callCount > 1) {
							std::string minTime = FormatTiming(stats.minCycles, cpuFrequency);
							std::string maxTime = FormatTiming(stats.maxCycles, cpuFrequency);
							std::cout << ", Min: " << minTime
								<< ", Max: " << maxTime;
						}
						std::cout << "]";
					}
					std::cout << std::endl;

					// Recursively print children
					if (!stats.children.empty()) {
						printHierarchy(stats.children, depth + 1);
					}
				}
				};

			// Print the hierarchy for this thread
			if (!rootStats.children.empty()) {
				printHierarchy(rootStats.children, 0);
			}
			else {
				std::cout << "No function calls recorded for this thread." << std::endl;
			}

			// Print thread summary
			uint64_t threadTotalCycles = 0;
			uint32_t threadTotalCalls = 0;

			std::function<void(const std::map<std::string, FunctionStats>&)> calculateTotals;
			calculateTotals = [&](const std::map<std::string, FunctionStats>& functions) {
				for (const auto& [name, stats] : functions) {
					threadTotalCycles += stats.totalCycles;
					threadTotalCalls += stats.callCount;
					calculateTotals(stats.children);
				}
				};

			calculateTotals(rootStats.children);

			std::string threadTotalTime = FormatTiming(threadTotalCycles, cpuFrequency);
			uint64_t threadAvgCycles = threadTotalCalls > 0 ? threadTotalCycles / threadTotalCalls : 0;
			std::string threadAvgTime = FormatTiming(threadAvgCycles, cpuFrequency);

			std::cout << std::string(80, '-') << std::endl;
			std::cout << "Thread Summary - Total: " << threadTotalTime
				<< ", Total Calls: " << threadTotalCalls;
			if (threadTotalCalls > 0) {
				std::cout << ", Avg per Call: " << threadAvgTime;
			}
			std::cout << std::endl;
		}

		// Print overall summary
		std::cout << "\n" << std::string(80, '=') << std::endl;
		std::cout << "=== OVERALL SUMMARY ===" << std::endl;

		uint64_t grandTotalCycles = 0;
		uint32_t grandTotalCalls = 0;

		for (const auto& event : m_analyzedEvents) {
			grandTotalCycles += event.duration;
			grandTotalCalls++;
		}

		std::string grandTotalTime = FormatTiming(grandTotalCycles, cpuFrequency);
		uint64_t grandAvgCycles = grandTotalCalls > 0 ? grandTotalCycles / grandTotalCalls : 0;
		std::string grandAvgTime = FormatTiming(grandAvgCycles, cpuFrequency);

		std::cout << "Total threads: " << m_threadHashToString.size() << std::endl;
		std::cout << "Grand total: " << grandTotalTime << std::endl;
		std::cout << "Grand total calls: " << grandTotalCalls << std::endl;

		if (grandTotalCalls > 0) {
			std::cout << "Average per call: " << grandAvgTime << std::endl;
		}
	}

	// NEW: Timeline-based flamegraph generation
	std::unique_ptr<TimelineFlameGraphData> ProfilerAnalyzer::GenerateTimelineFlameGraphData() const {
		auto timelineData = std::make_unique<TimelineFlameGraphData>();

		// Group events by thread
		std::map<size_t, std::vector<const AnalyzedEvent*>> threadEvents;
		for (const auto& event : m_analyzedEvents) {
			threadEvents[event.threadHash].push_back(&event);
		}

		// Group frame markers by thread - THIS WAS MISSING!
		std::map<size_t, std::vector<FrameMarker>> threadFrameMarkers;
		for (const auto& marker : m_frameMarkers) {
			threadFrameMarkers[marker.threadHash].push_back(marker);
		}

		// Process each thread
		for (const auto& [threadHash, events] : threadEvents) {
			// Use human-readable thread name if available, otherwise fall back to hash
			std::string threadName;
			auto nameIt = m_threadHashToString.find(threadHash);
			if (nameIt != m_threadHashToString.end() && !nameIt->second.empty()) {
				threadName = nameIt->second;
			}
			else {
				threadName = "Thread " + std::to_string(threadHash);
			}
			auto threadData = std::make_unique<TimelineThreadData>(threadHash, threadName);

			// Sort events by start time to maintain chronological order
			auto sortedEvents = events;
			std::sort(sortedEvents.begin(), sortedEvents.end(),
				[](const AnalyzedEvent* a, const AnalyzedEvent* b) {
					return a->startCycles < b->startCycles;
				});

			// Add frame markers for this thread if they exist - THIS WAS MISSING!
			if (threadFrameMarkers.find(threadHash) != threadFrameMarkers.end()) {
				auto& markers = threadFrameMarkers[threadHash];
				// Sort frame markers by time
				std::sort(markers.begin(), markers.end(),
					[](const FrameMarker& a, const FrameMarker& b) {
						return a.clockCycles < b.clockCycles;
					});
				threadData->frameMarkers = std::move(markers);
				std::cout << "Added " << threadData->frameMarkers.size()
					<< " frame markers to thread " << threadHash << std::endl; // Debug
			}

			// Calculate thread timeline bounds
			if (!sortedEvents.empty()) {
				threadData->threadStartTime = sortedEvents.front()->startCycles;
				threadData->threadEndTime = sortedEvents.front()->endCycles;

				// Find actual timeline bounds
				for (const auto* event : sortedEvents) {
					if (event->startCycles < threadData->threadStartTime) {
						threadData->threadStartTime = event->startCycles;
					}
					if (event->endCycles > threadData->threadEndTime) {
						threadData->threadEndTime = event->endCycles;
					}
					if (event->depth > threadData->maxDepth) {
						threadData->maxDepth = event->depth;
					}
				}

				// Also consider frame markers for timeline bounds - THIS WAS MISSING!
				for (const auto& marker : threadData->frameMarkers) {
					if (marker.clockCycles < threadData->threadStartTime) {
						threadData->threadStartTime = marker.clockCycles;
					}
					if (marker.clockCycles > threadData->threadEndTime) {
						threadData->threadEndTime = marker.clockCycles;
					}
				}

				threadData->threadDuration = threadData->threadEndTime - threadData->threadStartTime;
			}

			// Create timeline flame nodes for each event
			uint32_t eventIndex = 0;
			for (const auto* event : sortedEvents) {
				auto timelineNode = std::make_unique<TimelineFlameNode>(event);
				timelineNode->eventIndex = eventIndex++;
				threadData->events.push_back(std::move(timelineNode));
			}

			timelineData->threads.push_back(std::move(threadData));
		}

		// Calculate global timeline and layout
		timelineData->CalculateGlobalTimeline();

		return timelineData;
	}

	// LEGACY: Keep original accumulated flamegraph generation for compatibility
	std::unique_ptr<FlameGraphData> ProfilerAnalyzer::GenerateFlameGraphData() const {
		auto flameData = std::make_unique<FlameGraphData>();

		// Group events by thread
		std::map<size_t, std::vector<const AnalyzedEvent*>> threadEvents;
		for (const auto& event : m_analyzedEvents) {
			threadEvents[event.threadHash].push_back(&event);
		}

		// Process each thread
		for (const auto& [threadHash, events] : threadEvents) {
			// Create thread data
			std::string threadName = "Thread " + std::to_string(threadHash);
			auto threadData = std::make_unique<ThreadFlameData>(threadHash, threadName);

			// Sort events by start time to maintain chronological order
			auto sortedEvents = events;
			std::sort(sortedEvents.begin(), sortedEvents.end(),
				[](const AnalyzedEvent* a, const AnalyzedEvent* b) {
					return a->startCycles < b->startCycles;
				});

			// Track call stack for building hierarchy
			std::vector<FlameNode*> callStack;
			callStack.push_back(threadData->rootNode.get()); // Start with root

			// Calculate thread timeline
			if (!sortedEvents.empty()) {
				threadData->threadStartTime = sortedEvents.front()->startCycles;
				threadData->threadEndTime = sortedEvents.back()->endCycles;

				// Find actual end time (latest end among all events)
				for (const auto* event : sortedEvents) {
					if (event->endCycles > threadData->threadEndTime) {
						threadData->threadEndTime = event->endCycles;
					}
				}
			}

			// Build flamegraph tree structure
			for (const auto* event : sortedEvents) {
				// Adjust call stack to match current event's depth
				int targetDepth = event->depth + 1; // +1 because root is at depth 0

				// Pop stack if we've returned from deeper calls
				while (static_cast<int>(callStack.size()) > targetDepth + 1) {
					callStack.pop_back();
				}

				// Get or create the node at current depth
				FlameNode* parentNode = callStack.back();
				FlameNode* currentNode = parentNode->FindChild(event->name);

				if (!currentNode) {
					// Create new node
					auto newNode = std::make_unique<FlameNode>();
					newNode->name = event->name;
					newNode->level = targetDepth;
					newNode->startTime = event->startCycles;
					newNode->endTime = event->endCycles;
					newNode->totalTime = event->duration;
					newNode->selfTime = event->duration;
					newNode->callCount = 1;

					currentNode = newNode.get();
					parentNode->AddChild(std::move(newNode));
				}
				else {
					// Update existing node
					currentNode->totalTime += event->duration;
					currentNode->selfTime += event->duration;
					currentNode->callCount++;

					// Update timing bounds
					if (event->startCycles < currentNode->startTime) {
						currentNode->startTime = event->startCycles;
					}
					if (event->endCycles > currentNode->endTime) {
						currentNode->endTime = event->endCycles;
					}
				}

				// Update max depth
				if (targetDepth > threadData->maxDepth) {
					threadData->maxDepth = targetDepth;
				}

				// Add to call stack for potential children
				if (static_cast<int>(callStack.size()) == targetDepth + 1) {
					callStack.push_back(currentNode);
				}
			}

			// Calculate self times (subtract children time from total time)
			CalculateSelfTimes(threadData->rootNode.get());

			// Calculate total thread time from root's children
			threadData->totalThreadTime = 0;
			for (const auto& child : threadData->rootNode->children) {
				threadData->totalThreadTime += child->totalTime;
			}

			flameData->threads.push_back(std::move(threadData));
		}

		// Calculate global timeline and layout
		flameData->CalculateGlobalTimeline();

		return flameData;
	}

	void ProfilerAnalyzer::CalculateSelfTimes(FlameNode* node) const {
		if (!node) return;

		// Recursively calculate for children first
		for (auto& child : node->children) {
			CalculateSelfTimes(child.get());
		}

		// Calculate self time by subtracting children time from total time
		uint64_t childrenTime = 0;
		for (const auto& child : node->children) {
			childrenTime += child->totalTime;
		}

		// Self time is the time spent in this function excluding children
		if (node->totalTime >= childrenTime) {
			node->selfTime = node->totalTime - childrenTime;
		}
		else {
			node->selfTime = 0; // Shouldn't happen, but guard against underflow
		}
	}

} // namespace Profiler