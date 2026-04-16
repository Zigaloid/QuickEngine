#include "ProfilerAnalyzer.h"
#include "Profiler.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <functional>
#include <chrono>
#include <execution>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <windows.h>
#endif

#include "CoreSystem/CoreSystem.h"

namespace Profiler
{

// ── Helpers ───────────────────────────────────────────────────────────────────

static std::string FormatTiming(uint64_t cycles, double cpuFrequency)
{
    if (cycles == 0) return "0";

    double seconds = static_cast<double>(cycles) / cpuFrequency;

    std::ostringstream oss;
    oss << std::fixed;

    if (seconds >= 1.0)
    {
        oss << std::setprecision(3) << seconds << " s";
    }
    else if (seconds >= 1e-3)
    {
        oss << std::setprecision(3) << (seconds * 1e3) << " ms";
    }
    else if (seconds >= 1e-6)
    {
        oss << std::setprecision(3) << (seconds * 1e6) << " us";
    }
    else
    {
        oss << std::setprecision(0) << (seconds * 1e9) << " ns";
    }

    return oss.str();
}

// ── FlameNode ─────────────────────────────────────────────────────────────────

void FlameNode::AddChild(std::unique_ptr<FlameNode> child)
{
    if (child)
    {
        child->parent = this;
        children.push_back(std::move(child));
    }
}

FlameNode* FlameNode::FindChild(const std::string& functionName)
{
    for (auto& child : children)
    {
        if (child->name == functionName)
        {
            return child.get();
        }
    }
    return nullptr;
}

// ── TimelineThreadData ────────────────────────────────────────────────────────

void TimelineThreadData::CalculateTimelineLayout()
{
    if (threadDuration == 0 || events.empty()) return;

    for (auto& event : events)
    {
        event->x     = static_cast<float>(event->startTime - threadStartTime) / static_cast<float>(threadDuration);
        event->width = static_cast<float>(event->duration) / static_cast<float>(threadDuration);

        event->x     = std::max(0.0f, std::min(1.0f, event->x));
        event->width = std::max(0.0f, std::min(1.0f - event->x, event->width));
    }
}

std::vector<TimelineFlameNode*> TimelineThreadData::GetEventsAtDepth(int targetDepth) const
{
    std::vector<TimelineFlameNode*> result;
    for (const auto& event : events)
    {
        if (event->depth == targetDepth)
        {
            result.push_back(event.get());
        }
    }
    return result;
}

std::vector<TimelineFlameNode*> TimelineThreadData::GetEventsInTimeRange(uint64_t startTime, uint64_t endTime) const
{
    std::vector<TimelineFlameNode*> result;
    for (const auto& event : events)
    {
        if (event->endTime >= startTime && event->startTime <= endTime)
        {
            result.push_back(event.get());
        }
    }
    return result;
}

std::vector<const FrameMarker*> TimelineThreadData::GetFrameMarkersInTimeRange(uint64_t startTime, uint64_t endTime) const
{
    std::vector<const FrameMarker*> result;
    for (const auto& marker : frameMarkers)
    {
        if (marker.clockCycles >= startTime && marker.clockCycles <= endTime)
        {
            result.push_back(&marker);
        }
    }
    return result;
}

// ── TimelineFlameGraphData ────────────────────────────────────────────────────

TimelineThreadData* TimelineFlameGraphData::FindThread(size_t threadHash)
{
    for (auto& thread : threads)
    {
        if (thread->threadHash == threadHash)
        {
            return thread.get();
        }
    }
    return nullptr;
}

TimelineThreadData* TimelineFlameGraphData::GetThread(size_t index)
{
    if (index < threads.size())
    {
        return threads[index].get();
    }
    return nullptr;
}

void TimelineFlameGraphData::CalculateGlobalTimeline()
{
    globalStartTime = UINT64_MAX;
    globalEndTime   = 0;

    for (const auto& thread : threads)
    {
        if (thread->threadStartTime < globalStartTime)
        {
            globalStartTime = thread->threadStartTime;
        }
        if (thread->threadEndTime > globalEndTime)
        {
            globalEndTime = thread->threadEndTime;
        }

        thread->CalculateTimelineLayout();
    }

    globalDuration = globalEndTime - globalStartTime;
}

std::vector<TimelineFlameNode*> TimelineFlameGraphData::GetAllEventsInTimeRange(uint64_t startTime, uint64_t endTime) const
{
    std::vector<TimelineFlameNode*> result;
    for (const auto& thread : threads)
    {
        auto threadEvents = thread->GetEventsInTimeRange(startTime, endTime);
        result.insert(result.end(), threadEvents.begin(), threadEvents.end());
    }
    return result;
}

std::vector<const FrameMarker*> TimelineFlameGraphData::GetAllFrameMarkersInTimeRange(uint64_t startTime, uint64_t endTime) const
{
    std::vector<const FrameMarker*> result;
    for (const auto& thread : threads)
    {
        auto threadMarkers = thread->GetFrameMarkersInTimeRange(startTime, endTime);
        result.insert(result.end(), threadMarkers.begin(), threadMarkers.end());
    }
    return result;
}

// ── ThreadFlameData ───────────────────────────────────────────────────────────

void ThreadFlameData::CalculateLayout()
{
    if (!rootNode || rootNode->children.empty()) return;

    uint64_t timelineStart = threadStartTime;
    uint64_t timelineEnd   = threadEndTime;

    if (timelineStart == timelineEnd)
    {
        timelineEnd = timelineStart + 1;
    }

    float currentX = 0.0f;
    for (auto& child : rootNode->children)
    {
        float nodeWidth = static_cast<float>(child->totalTime) / static_cast<float>(timelineEnd - timelineStart);
        CalculateNodeLayout(child.get(), currentX, nodeWidth, timelineStart, timelineEnd);
        currentX += nodeWidth;
    }
}

void ThreadFlameData::CalculateNodeLayout(FlameNode* node, float startX, float width,
    uint64_t timelineStart, uint64_t timelineEnd)
{
    if (!node) return;

    node->x     = startX;
    node->width = width;

    if (!node->children.empty())
    {
        float childStartX = startX;
        for (auto& child : node->children)
        {
            float childWidth = width * (static_cast<float>(child->totalTime) / static_cast<float>(node->totalTime));
            CalculateNodeLayout(child.get(), childStartX, childWidth, timelineStart, timelineEnd);
            childStartX += childWidth;
        }
    }
}

// ── FlameGraphData ────────────────────────────────────────────────────────────

void FlameGraphData::CalculateGlobalTimeline()
{
    for (auto& thread : threads)
    {
        thread->CalculateLayout();
    }
}

// ── ProfilerAnalyzer ──────────────────────────────────────────────────────────

void ProfilerAnalyzer::LoadFromProfileEvents(const std::deque<ProfileEvent>& events, bool clearExisting)
{
    if (clearExisting)
    {
        Clear();
    }

    std::vector<RawEvent> rawEvents;
    std::vector<FrameMarker> frameMarkers;

    ConvertProfileEventsToRawEvents(events, rawEvents, frameMarkers);

    // Append frame markers
    m_frameMarkers.insert(m_frameMarkers.end(), frameMarkers.begin(), frameMarkers.end());

    MatchStartEndPairs(rawEvents);
}

void ProfilerAnalyzer::AppendFromProfileEvents(const std::deque<ProfileEvent>& events)
{
    LoadFromProfileEvents(events, false);
}

void ProfilerAnalyzer::Clear()
{
    m_analyzedEvents.clear();
    m_frameMarkers.clear();
    m_callStacks.clear();
    m_threadHashToString.clear();
}

size_t ProfilerAnalyzer::GetThreadCount() const
{
    return m_threadHashToString.size();
}

void ProfilerAnalyzer::ConvertProfileEventsToRawEvents(const std::deque<ProfileEvent>& events,
    std::vector<RawEvent>& outRawEvents,
    std::vector<FrameMarker>& outFrameMarkers)
{
    outRawEvents.clear();
    outFrameMarkers.clear();
    outRawEvents.reserve(events.size());

    for (const auto& event : events)
    {
        std::hash<std::string> hasher;
        size_t threadHash = hasher(event.threadIdStr);

        if (event.GetEventType() == EventType::Frame)
        {
            outFrameMarkers.emplace_back(threadHash, event.GetClockCycles(), event.GetFrameNumber());
        }
        else
        {
            RawEvent rawEvent;
            rawEvent.threadHash  = threadHash;
            rawEvent.name        = event.name;
            rawEvent.clockCycles = event.GetClockCycles();
            rawEvent.isStart     = event.isStart;
            outRawEvents.push_back(rawEvent);
        }

        // Prefer the human-readable thread name if available, otherwise fall back to ID string
        if (!event.threadNameStr.empty())
        {
            m_threadHashToString[threadHash] = event.threadNameStr;
        }
        else
        {
            m_threadHashToString[threadHash] = event.threadIdStr;
        }
    }
}

void ProfilerAnalyzer::LoadFromFile(const std::string& filename)
{
    std::ifstream file(filename);
    if (!file.is_open())
    {
        std::cerr << "Failed to open profile file: " << filename << std::endl;
        return;
    }

    std::string line;
    // Skip header
    std::getline(file, line);

    std::vector<RawEvent> rawEvents;
    m_frameMarkers.clear();

    while (std::getline(file, line))
    {
        std::istringstream ss(line);
        std::string threadStr, name, cyclesStr, eventType, frameNumberStr;

        // Parse the new 5-column CSV format: ThreadID,Name,ClockCycles,EventType,FrameNumber
        if (std::getline(ss, threadStr, ',') &&
            std::getline(ss, name, ',') &&
            std::getline(ss, cyclesStr, ',') &&
            std::getline(ss, eventType, ',') &&
            std::getline(ss, frameNumberStr))
        {
            size_t threadHash = 0;
            try
            {
                threadHash = std::stoull(threadStr);
            }
            catch (const std::exception& e)
            {
                std::cerr << "Failed to parse thread ID: " << threadStr << " - " << e.what() << std::endl;
                continue;
            }

            if (eventType == "FRAME")
            {
                try
                {
                    uint64_t frameNumber = std::stoull(frameNumberStr);
                    uint64_t clockCycles = std::stoull(cyclesStr);
                    m_frameMarkers.emplace_back(threadHash, clockCycles, frameNumber);
                }
                catch (const std::exception& e)
                {
                    std::cerr << "Failed to parse frame event: " << e.what() << std::endl;
                }
                continue;
            }

            RawEvent event;
            event.threadHash  = threadHash;
            event.name        = name;
            event.clockCycles = std::stoull(cyclesStr);
            event.isStart     = (eventType == "START");

            rawEvents.push_back(event);
        }
        else
        {
            // Try parsing the old 4-column format for backward compatibility
            std::istringstream ss2(line);
            if (std::getline(ss2, threadStr, ',') &&
                std::getline(ss2, name, ',') &&
                std::getline(ss2, cyclesStr, ',') &&
                std::getline(ss2, eventType))
            {
                size_t threadHash = 0;
                try
                {
                    threadHash = std::stoull(threadStr);
                }
                catch (const std::exception& e)
                {
                    std::cerr << "Failed to parse thread ID: " << threadStr << " - " << e.what() << std::endl;
                    continue;
                }

                RawEvent event;
                event.threadHash  = threadHash;
                event.name        = name;
                event.clockCycles = std::stoull(cyclesStr);
                event.isStart     = (eventType == "START");

                rawEvents.push_back(event);
            }
        }
    }

    std::cout << "Loaded " << m_frameMarkers.size() << " frame markers and "
              << rawEvents.size() << " scoped events" << std::endl;

    MatchStartEndPairs(rawEvents);
}

void ProfilerAnalyzer::MatchStartEndPairs(const std::vector<RawEvent>& rawEvents)
{
    std::map<size_t, std::vector<RawEvent>> threadStacks;

    m_analyzedEvents.clear();

    for (const auto& event : rawEvents)
    {
        auto& stack = threadStacks[event.threadHash];

        if (event.isStart)
        {
            stack.push_back(event);
        }
        else
        {
            if (!stack.empty())
            {
                for (auto it = stack.rbegin(); it != stack.rend(); ++it)
                {
                    if (it->name == event.name)
                    {
                        AnalyzedEvent analyzed;
                        analyzed.threadHash  = event.threadHash;
                        analyzed.name        = event.name;
                        analyzed.startCycles = it->clockCycles;
                        analyzed.endCycles   = event.clockCycles;
                        analyzed.duration    = event.clockCycles - it->clockCycles;
                        analyzed.depth       = static_cast<int>(std::distance(it, stack.rend()) - 1);

                        m_analyzedEvents.push_back(analyzed);
                        stack.erase(std::next(it).base(), stack.end());
                        break;
                    }
                }
            }
        }
    }

    for (const auto& [threadHash, stack] : threadStacks)
    {
        if (m_threadHashToString.find(threadHash) == m_threadHashToString.end())
        {
            m_threadHashToString[threadHash] = std::to_string(threadHash);
        }
    }
}

void ProfilerAnalyzer::ReconstructCallStack()
{
    std::map<size_t, std::vector<const AnalyzedEvent*>> threadEvents;

    for (const auto& event : m_analyzedEvents)
    {
        threadEvents[event.threadHash].push_back(&event);
    }

    for (auto& [threadHash, events] : threadEvents)
    {
        std::sort(events.begin(), events.end(),
            [](const AnalyzedEvent* a, const AnalyzedEvent* b)
            {
                return a->startCycles < b->startCycles;
            });

        std::vector<std::string> callStack;
        for (const auto* event : events)
        {
            std::string indented = std::string(event->depth * 2, ' ') + event->name;
            callStack.push_back(indented);
        }

        m_callStacks[threadHash] = callStack;
    }
}

void ProfilerAnalyzer::AnalyzeCallStack()
{
    ReconstructCallStack();
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

    for (const auto& event : m_analyzedEvents)
    {
        std::string formattedTime = FormatTiming(event.duration, cpuFrequency);

        std::cout << std::left << std::setw(30) << event.name
                  << std::setw(15) << formattedTime
                  << std::setw(15) << event.duration
                  << std::setw(10) << event.depth
                  << std::setw(15) << event.threadHash << std::endl;
    }

    std::cout << "\nTotal Events: " << m_analyzedEvents.size() << std::endl;
}

void ProfilerAnalyzer::PrintCallStack() const
{
    static double cpuFrequency = GetCPUFrequency();

    std::cout << "\n=== CALL STACK HIERARCHY ===" << std::endl;
    std::cout << "CPU Frequency (estimated): " << std::fixed << std::setprecision(2)
              << (cpuFrequency / 1e9) << " GHz" << std::endl;

    struct FunctionStats
    {
        uint64_t totalCycles = 0;
        uint32_t callCount   = 0;
        uint64_t minCycles   = UINT64_MAX;
        uint64_t maxCycles   = 0;
        std::map<std::string, FunctionStats> children;
    };

    std::map<size_t, std::vector<const AnalyzedEvent*>> threadGroups;
    for (const auto& event : m_analyzedEvents)
    {
        threadGroups[event.threadHash].push_back(&event);
    }

    for (const auto& [threadHash, events] : threadGroups)
    {
        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "Thread " << threadHash << std::endl;
        std::cout << std::string(80, '=') << std::endl;

        auto sortedEvents = events;
        std::sort(sortedEvents.begin(), sortedEvents.end(),
            [](const AnalyzedEvent* a, const AnalyzedEvent* b)
            {
                return a->startCycles < b->startCycles;
            });

        FunctionStats rootStats;
        std::vector<std::string> callStack;

        for (const auto* event : sortedEvents)
        {
            FunctionStats* currentStats = &rootStats;

            callStack.resize(event->depth + 1);
            callStack[event->depth] = event->name;

            for (int i = 0; i <= event->depth; ++i)
            {
                if (i == event->depth)
                {
                    auto& stats = currentStats->children[callStack[i]];
                    stats.totalCycles += event->duration;
                    stats.callCount++;
                    stats.minCycles = std::min(stats.minCycles, event->duration);
                    stats.maxCycles = std::max(stats.maxCycles, event->duration);
                }
                else
                {
                    currentStats = &currentStats->children[callStack[i]];
                }
            }
        }

        std::function<void(const std::map<std::string, FunctionStats>&, int)> printHierarchy;
        printHierarchy = [&](const std::map<std::string, FunctionStats>& functions, int depth)
        {
            for (const auto& [functionName, stats] : functions)
            {
                std::string indent(depth * 2, ' ');

                uint64_t avgCycles = stats.callCount > 0 ? stats.totalCycles / stats.callCount : 0;

                std::string totalTime = FormatTiming(stats.totalCycles, cpuFrequency);
                std::string avgTime   = FormatTiming(avgCycles, cpuFrequency);

                std::cout << indent << functionName;
                if (stats.callCount > 0)
                {
                    std::cout << " [Total: " << totalTime
                              << ", Calls: " << stats.callCount
                              << ", Avg: " << avgTime;

                    if (stats.callCount > 1)
                    {
                        std::string minTime = FormatTiming(stats.minCycles, cpuFrequency);
                        std::string maxTime = FormatTiming(stats.maxCycles, cpuFrequency);
                        std::cout << ", Min: " << minTime
                                  << ", Max: " << maxTime;
                    }
                    std::cout << "]";
                }
                std::cout << std::endl;

                if (!stats.children.empty())
                {
                    printHierarchy(stats.children, depth + 1);
                }
            }
        };

        if (!rootStats.children.empty())
        {
            printHierarchy(rootStats.children, 0);
        }
        else
        {
            std::cout << "No function calls recorded for this thread." << std::endl;
        }

        uint64_t threadTotalCycles = 0;
        uint32_t threadTotalCalls  = 0;

        std::function<void(const std::map<std::string, FunctionStats>&)> calculateTotals;
        calculateTotals = [&](const std::map<std::string, FunctionStats>& functions)
        {
            for (const auto& [name, stats] : functions)
            {
                threadTotalCycles += stats.totalCycles;
                threadTotalCalls  += stats.callCount;
                calculateTotals(stats.children);
            }
        };

        calculateTotals(rootStats.children);

        std::string threadTotalTime = FormatTiming(threadTotalCycles, cpuFrequency);
        uint64_t threadAvgCycles    = threadTotalCalls > 0 ? threadTotalCycles / threadTotalCalls : 0;
        std::string threadAvgTime   = FormatTiming(threadAvgCycles, cpuFrequency);

        std::cout << std::string(80, '-') << std::endl;
        std::cout << "Thread Summary - Total: " << threadTotalTime
                  << ", Total Calls: " << threadTotalCalls;
        if (threadTotalCalls > 0)
        {
            std::cout << ", Avg per Call: " << threadAvgTime;
        }
        std::cout << std::endl;
    }

    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "=== OVERALL SUMMARY ===" << std::endl;

    uint64_t grandTotalCycles = 0;
    uint32_t grandTotalCalls  = 0;

    for (const auto& event : m_analyzedEvents)
    {
        grandTotalCycles += event.duration;
        grandTotalCalls++;
    }

    std::string grandTotalTime = FormatTiming(grandTotalCycles, cpuFrequency);
    uint64_t grandAvgCycles    = grandTotalCalls > 0 ? grandTotalCycles / grandTotalCalls : 0;
    std::string grandAvgTime   = FormatTiming(grandAvgCycles, cpuFrequency);

    std::cout << "Total threads: " << m_threadHashToString.size() << std::endl;
    std::cout << "Grand total: " << grandTotalTime << std::endl;
    std::cout << "Grand total calls: " << grandTotalCalls << std::endl;

    if (grandTotalCalls > 0)
    {
        std::cout << "Average per call: " << grandAvgTime << std::endl;
    }
}

std::unique_ptr<TimelineFlameGraphData> ProfilerAnalyzer::GenerateTimelineFlameGraphData() const
{
    auto timelineData = std::make_unique<TimelineFlameGraphData>();

    std::map<size_t, std::vector<const AnalyzedEvent*>> threadEvents;
    for (const auto& event : m_analyzedEvents)
    {
        threadEvents[event.threadHash].push_back(&event);
    }

    std::map<size_t, std::vector<FrameMarker>> threadFrameMarkers;
    for (const auto& marker : m_frameMarkers)
    {
        threadFrameMarkers[marker.threadHash].push_back(marker);
    }

    for (const auto& [threadHash, events] : threadEvents)
    {
        std::string threadName;
        auto nameIt = m_threadHashToString.find(threadHash);
        if (nameIt != m_threadHashToString.end() && !nameIt->second.empty())
        {
            threadName = nameIt->second;
        }
        else
        {
            threadName = "Thread " + std::to_string(threadHash);
        }
        auto threadData = std::make_unique<TimelineThreadData>(threadHash, threadName);

        auto sortedEvents = events;
        std::sort(sortedEvents.begin(), sortedEvents.end(),
            [](const AnalyzedEvent* a, const AnalyzedEvent* b)
            {
                return a->startCycles < b->startCycles;
            });

        if (threadFrameMarkers.find(threadHash) != threadFrameMarkers.end())
        {
            auto& markers = threadFrameMarkers[threadHash];
            std::sort(markers.begin(), markers.end(),
                [](const FrameMarker& a, const FrameMarker& b)
                {
                    return a.clockCycles < b.clockCycles;
                });
            threadData->frameMarkers = std::move(markers);
            std::cout << "Added " << threadData->frameMarkers.size()
                      << " frame markers to thread " << threadHash << std::endl;
        }

        if (!sortedEvents.empty())
        {
            threadData->threadStartTime = sortedEvents.front()->startCycles;
            threadData->threadEndTime   = sortedEvents.front()->endCycles;

            for (const auto* event : sortedEvents)
            {
                if (event->startCycles < threadData->threadStartTime)
                {
                    threadData->threadStartTime = event->startCycles;
                }
                if (event->endCycles > threadData->threadEndTime)
                {
                    threadData->threadEndTime = event->endCycles;
                }
                if (event->depth > threadData->maxDepth)
                {
                    threadData->maxDepth = event->depth;
                }
            }

            for (const auto& marker : threadData->frameMarkers)
            {
                if (marker.clockCycles < threadData->threadStartTime)
                {
                    threadData->threadStartTime = marker.clockCycles;
                }
                if (marker.clockCycles > threadData->threadEndTime)
                {
                    threadData->threadEndTime = marker.clockCycles;
                }
            }

            threadData->threadDuration = threadData->threadEndTime - threadData->threadStartTime;
        }

        uint32_t eventIndex = 0;
        for (const auto* event : sortedEvents)
        {
            auto timelineNode = std::make_unique<TimelineFlameNode>(event);
            timelineNode->eventIndex = eventIndex++;
            threadData->events.push_back(std::move(timelineNode));
        }

        timelineData->threads.push_back(std::move(threadData));
    }

    timelineData->CalculateGlobalTimeline();

    return timelineData;
}

std::unique_ptr<FlameGraphData> ProfilerAnalyzer::GenerateFlameGraphData() const
{
    auto flameData = std::make_unique<FlameGraphData>();

    std::map<size_t, std::vector<const AnalyzedEvent*>> threadEvents;
    for (const auto& event : m_analyzedEvents)
    {
        threadEvents[event.threadHash].push_back(&event);
    }

    for (const auto& [threadHash, events] : threadEvents)
    {
        std::string threadName = "Thread " + std::to_string(threadHash);
        auto threadData = std::make_unique<ThreadFlameData>(threadHash, threadName);

        auto sortedEvents = events;
        std::sort(sortedEvents.begin(), sortedEvents.end(),
            [](const AnalyzedEvent* a, const AnalyzedEvent* b)
            {
                return a->startCycles < b->startCycles;
            });

        std::vector<FlameNode*> callStack;
        callStack.push_back(threadData->rootNode.get());

        if (!sortedEvents.empty())
        {
            threadData->threadStartTime = sortedEvents.front()->startCycles;
            threadData->threadEndTime   = sortedEvents.back()->endCycles;

            for (const auto* event : sortedEvents)
            {
                if (event->endCycles > threadData->threadEndTime)
                {
                    threadData->threadEndTime = event->endCycles;
                }
            }
        }

        for (const auto* event : sortedEvents)
        {
            int targetDepth = event->depth + 1;

            while (static_cast<int>(callStack.size()) > targetDepth + 1)
            {
                callStack.pop_back();
            }

            FlameNode* parentNode  = callStack.back();
            FlameNode* currentNode = parentNode->FindChild(event->name);

            if (!currentNode)
            {
                auto newNode        = std::make_unique<FlameNode>();
                newNode->name       = event->name;
                newNode->level      = targetDepth;
                newNode->startTime  = event->startCycles;
                newNode->endTime    = event->endCycles;
                newNode->totalTime  = event->duration;
                newNode->selfTime   = event->duration;
                newNode->callCount  = 1;

                currentNode = newNode.get();
                parentNode->AddChild(std::move(newNode));
            }
            else
            {
                currentNode->totalTime += event->duration;
                currentNode->selfTime  += event->duration;
                currentNode->callCount++;

                if (event->startCycles < currentNode->startTime)
                {
                    currentNode->startTime = event->startCycles;
                }
                if (event->endCycles > currentNode->endTime)
                {
                    currentNode->endTime = event->endCycles;
                }
            }

            if (targetDepth > threadData->maxDepth)
            {
                threadData->maxDepth = targetDepth;
            }

            if (static_cast<int>(callStack.size()) == targetDepth + 1)
            {
                callStack.push_back(currentNode);
            }
        }

        CalculateSelfTimes(threadData->rootNode.get());

        threadData->totalThreadTime = 0;
        for (const auto& child : threadData->rootNode->children)
        {
            threadData->totalThreadTime += child->totalTime;
        }

        flameData->threads.push_back(std::move(threadData));
    }

    flameData->CalculateGlobalTimeline();

    return flameData;
}

void ProfilerAnalyzer::CalculateSelfTimes(FlameNode* node) const
{
    if (!node) return;

    for (auto& child : node->children)
    {
        CalculateSelfTimes(child.get());
    }

    uint64_t childrenTime = 0;
    for (const auto& child : node->children)
    {
        childrenTime += child->totalTime;
    }

    if (node->totalTime >= childrenTime)
    {
        node->selfTime = node->totalTime - childrenTime;
    }
    else
    {
        node->selfTime = 0;
    }
}

} // namespace Profiler
