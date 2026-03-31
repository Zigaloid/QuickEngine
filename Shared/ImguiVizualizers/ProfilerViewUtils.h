#pragma once

#include "imgui.h"
#include "Profiler/ProfilerAnalyzer.h"
#include <string>
#include <vector>
#include <map>
#include <cstdint>

namespace Profiler {

    // -------------------------------------------------------------------------
    // Shared types used across multiple profiler views
    // -------------------------------------------------------------------------

    // Cost display mode for frame comparison / outlier views
    enum class CostMode {
        Inclusive,   // Total time including children
        Exclusive    // Self time only (excluding children)
    };

    // Accumulated function cost data for a single frame
    struct FrameFunctionCost {
        std::string name;
        uint64_t inclusiveCycles = 0;   // Accumulated duration including children
        uint64_t exclusiveCycles = 0;   // Accumulated self-time excluding children
        uint32_t callCount = 0;         // Number of times the function was called in the frame
    };

    // -------------------------------------------------------------------------
    // ProfilerViewUtils — shared formatting, coloring, and data-gathering
    // helpers used by ProfilerVisualizer, ProfilerTreeView,
    // ProfilerOutlierView, and ProfilerFrameComparisonView.
    // -------------------------------------------------------------------------
    namespace ViewUtils {

        // Cached CPU frequency (initialized once on first call)
        double GetCachedCPUFrequency();

        // -----------------------------------------------------------------
        // Formatting
        // -----------------------------------------------------------------

        /// Convert clock cycles to a human-readable duration string
        /// using the given CPU frequency (e.g. "1.234 ms", "56.7 us").
        std::string FormatCycles(uint64_t cycles, double cpuFrequency);

        /// Overload accepting float cycles (for butterfly-chart values).
        std::string FormatCycles(float cycles, double cpuFrequency);

        /// Format a percentage, returning "--" when total is zero.
        std::string FormatPercentage(uint64_t cycles, uint64_t total);

        // -----------------------------------------------------------------
        // Coloring
        // -----------------------------------------------------------------

        /// Deterministic hash-based color for a function name.
        /// Produces visually distinct, medium-luminance colors suitable
        /// for dark-background flame graphs and bar charts.
        ImU32 GetNodeColor(const std::string& functionName, float intensity = 1.0f);

        // -----------------------------------------------------------------
        // Data gathering
        // -----------------------------------------------------------------

        /// Collect all frame markers across every thread, sorted by clock
        /// cycles ascending.
        std::vector<FrameMarker> GatherSortedGlobalFrameMarkers(
            TimelineFlameGraphData* data);

        /// Accumulate per-function inclusive/exclusive costs for a single
        /// frame, returned sorted by inclusive cycles descending.
        std::vector<FrameFunctionCost> AccumulateFrameCosts(
            TimelineFlameGraphData* data, uint64_t frameNumber);

        /// Accumulate per-function inclusive/exclusive costs for a single
        /// frame, considering only threads whose names are in the filter.
        /// An empty filter means "all threads".
        std::vector<FrameFunctionCost> AccumulateFrameCosts(
            TimelineFlameGraphData* data, uint64_t frameNumber,
            const std::vector<std::string>& threadFilter);

        // -----------------------------------------------------------------
        // File dialogs (Win32 common dialogs)
        // -----------------------------------------------------------------

        /// Default file filter for profiler session files.
        constexpr const char* kSessionFileFilter = "Profiler Sessions (*.profsession)\0*.profsession\0All Files (*.*)\0*.*\0";
        constexpr const char* kSessionDefaultExt = "profsession";

        /// Show an Open File dialog. Returns the chosen path, or empty
        /// string if the user cancelled.
        std::string ShowOpenFileDialog(const char* filter = kSessionFileFilter,
                                       const char* defaultExt = kSessionDefaultExt);

        /// Show a Save File dialog. Returns the chosen path, or empty
        /// string if the user cancelled.
        std::string ShowSaveFileDialog(const char* filter = kSessionFileFilter,
                                       const char* defaultExt = kSessionDefaultExt);

    } // namespace ViewUtils
} // namespace Profiler