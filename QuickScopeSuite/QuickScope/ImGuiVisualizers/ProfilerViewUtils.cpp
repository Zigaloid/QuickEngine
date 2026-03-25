#include "ProfilerViewUtils.h"
#include "Profiler/Profiler.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <functional>
#include <cmath>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <commdlg.h>
#endif

namespace Profiler::ViewUtils {

    // -----------------------------------------------------------------
    // CPU frequency (cached)
    // -----------------------------------------------------------------

    double GetCachedCPUFrequency()
    {
        // Delegates to the existing Profiler::GetCPUFrequency() which
        // already caches after the first call.
        return Profiler::GetCPUFrequency();
    }

    // -----------------------------------------------------------------
    // Formatting
    // -----------------------------------------------------------------

    std::string FormatCycles(uint64_t cycles, double cpuFrequency)
    {
        double seconds = static_cast<double>(cycles) / cpuFrequency;

        std::ostringstream oss;
        oss << std::fixed;

        if (seconds >= 1.0)
            oss << std::setprecision(3) << seconds << " s";
        else if (seconds >= 1e-3)
            oss << std::setprecision(3) << (seconds * 1e3) << " ms";
        else if (seconds >= 1e-6)
            oss << std::setprecision(3) << (seconds * 1e6) << " us";
        else
            oss << std::setprecision(0) << (seconds * 1e9) << " ns";

        return oss.str();
    }

    std::string FormatCycles(float cycles, double cpuFrequency)
    {
        return FormatCycles(static_cast<uint64_t>(cycles), cpuFrequency);
    }

    std::string FormatPercentage(uint64_t cycles, uint64_t total)
    {
        if (total == 0)
            return "--";

        double pct = static_cast<double>(cycles) / static_cast<double>(total) * 100.0;

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << pct << "%";
        return oss.str();
    }

    // -----------------------------------------------------------------
    // Coloring
    // -----------------------------------------------------------------

    ImU32 GetNodeColor(const std::string& functionName, float intensity)
    {
        std::hash<std::string> hasher;
        size_t hash = hasher(functionName);

        uint8_t r = static_cast<uint8_t>((hash >> 0) & 0xFF);
        uint8_t g = static_cast<uint8_t>((hash >> 8) & 0xFF);
        uint8_t b = static_cast<uint8_t>((hash >> 16) & 0xFF);

        float fr = r / 255.0f;
        float fg = g / 255.0f;
        float fb = b / 255.0f;

        float luminance = 0.299f * fr + 0.587f * fg + 0.114f * fb;

        if (luminance > 0.6f) {
            float darkenFactor = 0.4f / luminance;
            fr *= darkenFactor;
            fg *= darkenFactor;
            fb *= darkenFactor;
        }
        else {
            fr = fr * 0.8f + 0.2f;
            fg = fg * 0.8f + 0.2f;
            fb = fb * 0.8f + 0.2f;
        }

        fr = std::clamp(fr * intensity, 0.0f, 1.0f);
        fg = std::clamp(fg * intensity, 0.0f, 1.0f);
        fb = std::clamp(fb * intensity, 0.0f, 1.0f);

        return IM_COL32(static_cast<uint8_t>(fr * 255),
            static_cast<uint8_t>(fg * 255),
            static_cast<uint8_t>(fb * 255), 255);
    }

    // -----------------------------------------------------------------
    // Data gathering
    // -----------------------------------------------------------------

    std::vector<FrameMarker> GatherSortedGlobalFrameMarkers(
        TimelineFlameGraphData* data)
    {
        std::vector<FrameMarker> markers;
        if (!data) return markers;

        for (size_t i = 0; i < data->GetThreadCount(); ++i) {
            auto* thread = data->GetThread(i);
            if (thread && !thread->frameMarkers.empty()) {
                markers.insert(markers.end(),
                    thread->frameMarkers.begin(), thread->frameMarkers.end());
            }
        }
        std::sort(markers.begin(), markers.end(),
            [](const FrameMarker& a, const FrameMarker& b) {
                return a.clockCycles < b.clockCycles;
            });
        return markers;
    }

    std::vector<FrameFunctionCost> AccumulateFrameCosts(
        TimelineFlameGraphData* data, uint64_t frameNumber)
    {
        auto globalFrameMarkers = GatherSortedGlobalFrameMarkers(data);
        if (globalFrameMarkers.empty()) {
            return {};
        }

        uint64_t frameStart = 0;
        uint64_t frameEnd = 0;
        bool found = false;

        for (size_t i = 0; i < globalFrameMarkers.size(); ++i) {
            if (globalFrameMarkers[i].frameNumber == frameNumber) {
                frameStart = globalFrameMarkers[i].clockCycles;
                frameEnd = (i + 1 < globalFrameMarkers.size())
                    ? globalFrameMarkers[i + 1].clockCycles
                    : data->globalEndTime;
                found = true;
                break;
            }
        }

        if (!found) {
            return {};
        }

        std::map<std::string, FrameFunctionCost> costMap;

        for (size_t t = 0; t < data->GetThreadCount(); ++t) {
            auto* thread = data->GetThread(t);
            if (!thread) continue;

            std::vector<TimelineFlameNode*> frameEvents;
            for (const auto& event : thread->events) {
                if (event->duration == 0) continue;
                if (event->startTime >= frameStart && event->startTime < frameEnd) {
                    frameEvents.push_back(event.get());
                }
            }

            std::sort(frameEvents.begin(), frameEvents.end(),
                [](const TimelineFlameNode* a, const TimelineFlameNode* b) {
                    if (a->startTime != b->startTime)
                        return a->startTime < b->startTime;
                    return a->depth < b->depth;
                });

            for (size_t i = 0; i < frameEvents.size(); ++i) {
                auto* event = frameEvents[i];
                uint64_t inclusive = event->duration;
                uint64_t childTime = 0;

                for (size_t j = i + 1; j < frameEvents.size(); ++j) {
                    auto* candidate = frameEvents[j];
                    if (candidate->startTime >= event->endTime)
                        break;
                    if (candidate->depth == event->depth + 1 &&
                        candidate->startTime >= event->startTime &&
                        candidate->endTime <= event->endTime) {
                        childTime += candidate->duration;
                    }
                }

                uint64_t exclusive = (childTime <= inclusive) ? (inclusive - childTime) : 0;

                auto& cost = costMap[event->name];
                cost.name = event->name;
                cost.inclusiveCycles += inclusive;
                cost.exclusiveCycles += exclusive;
                cost.callCount++;
            }
        }

        std::vector<FrameFunctionCost> result;
        result.reserve(costMap.size());
        for (auto& [name, cost] : costMap) {
            result.push_back(std::move(cost));
        }
        std::sort(result.begin(), result.end(),
            [](const FrameFunctionCost& a, const FrameFunctionCost& b) {
                return a.inclusiveCycles > b.inclusiveCycles;
            });

        return result;
    }

    // -----------------------------------------------------------------
    // File dialogs (Win32)
    // -----------------------------------------------------------------

#ifdef _WIN32

    std::string ShowOpenFileDialog(const char* filter, const char* defaultExt)
    {
        char filePath[MAX_PATH] = { 0 };

        OPENFILENAMEA ofn = {};
        ofn.lStructSize  = sizeof(ofn);
        ofn.hwndOwner    = nullptr;
        ofn.lpstrFilter  = filter;
        ofn.lpstrFile    = filePath;
        ofn.nMaxFile     = MAX_PATH;
        ofn.lpstrDefExt  = defaultExt;
        ofn.Flags        = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

        if (GetOpenFileNameA(&ofn)) {
            return std::string(filePath);
        }
        return {};
    }

    std::string ShowSaveFileDialog(const char* filter, const char* defaultExt)
    {
        char filePath[MAX_PATH] = { 0 };

        OPENFILENAMEA ofn = {};
        ofn.lStructSize  = sizeof(ofn);
        ofn.hwndOwner    = nullptr;
        ofn.lpstrFilter  = filter;
        ofn.lpstrFile    = filePath;
        ofn.nMaxFile     = MAX_PATH;
        ofn.lpstrDefExt  = defaultExt;
        ofn.Flags        = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

        if (GetSaveFileNameA(&ofn)) {
            return std::string(filePath);
        }
        return {};
    }

#else

    std::string ShowOpenFileDialog(const char* /*filter*/, const char* /*defaultExt*/)
    {
        // Non-Windows stub — no native dialog available
        return {};
    }

    std::string ShowSaveFileDialog(const char* /*filter*/, const char* /*defaultExt*/)
    {
        return {};
    }

#endif

} // namespace Profiler::ViewUtils