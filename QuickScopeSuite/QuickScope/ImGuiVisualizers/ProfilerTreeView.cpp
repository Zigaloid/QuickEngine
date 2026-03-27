#include "ProfilerTreeView.h"
#include "Profiler/Profiler.h"
#include <algorithm>
#include <cstring>

namespace Profiler {

    // -------------------------------------------------------------------------
    // Recursive helper to compute exclusive times for the call tree
    // -------------------------------------------------------------------------
    static void CalculateExclusiveTimes(CallTreeNode* node)
    {
        uint64_t childSum = 0;
        for (auto& child : node->children) {
            CalculateExclusiveTimes(child.get());
            childSum += child->inclusiveCycles;
        }
        node->exclusiveCycles = (node->inclusiveCycles > childSum)
            ? node->inclusiveCycles - childSum
            : 0;
    }

    // -------------------------------------------------------------------------
    // CallTreeNode
    // -------------------------------------------------------------------------

    CallTreeNode* CallTreeNode::GetOrCreateChild(const std::string& childName)
    {
        for (auto& child : children) {
            if (child->name == childName)
                return child.get();
        }
        auto newChild = std::make_unique<CallTreeNode>();
        newChild->name = childName;
        newChild->depth = depth + 1;
        auto* ptr = newChild.get();
        children.push_back(std::move(newChild));
        return ptr;
    }

    // -------------------------------------------------------------------------
    // ProfilerTreeView
    // -------------------------------------------------------------------------

    ProfilerTreeView::ProfilerTreeView()
    {
        m_CPUFrequency = ViewUtils::GetCachedCPUFrequency();
        m_FilterBuffer[0] = '\0';
    }

    // -------------------------------------------------------------------------
    // Main render entry (self-contained window)
    // -------------------------------------------------------------------------

    void ProfilerTreeView::Render(const char* windowTitle, TimelineFlameGraphData* data, bool* isOpen)
    {
        if (ImGui::Begin(windowTitle, isOpen)) {
            RenderTreeContent(data);
        }
        ImGui::End();
    }

    // -------------------------------------------------------------------------
    // Content-only rendering (called by the controller inside its window)
    // -------------------------------------------------------------------------

    void ProfilerTreeView::RenderContent(TimelineFlameGraphData* data)
    {
        RenderTreeContent(data);
    }

    // -------------------------------------------------------------------------
    // Shared content implementation
    // -------------------------------------------------------------------------

    void ProfilerTreeView::RenderTreeContent(TimelineFlameGraphData* data)
    {
        if (!data || data->GetThreadCount() == 0) {
            ImGui::TextDisabled("No profiler data available.");
            return;
        }

        RebuildTree(data);
        RenderToolbar();

        ImGui::Separator();

        // Table with columns: Name | Inclusive | Exclusive | Calls | Inc % | Exc %
        constexpr ImGuiTableFlags tableFlags =
            ImGuiTableFlags_BordersV |
            ImGuiTableFlags_BordersOuterH |
            ImGuiTableFlags_RowBg |
            ImGuiTableFlags_Resizable |
            ImGuiTableFlags_ScrollY |
            ImGuiTableFlags_SizingStretchProp;

        if (ImGui::BeginTable("CallTreeTable", m_ShowCallCounts ? 6 : 5, tableFlags)) {
            ImGui::TableSetupScrollFreeze(0, 1); // Freeze header row

            ImGui::TableSetupColumn("Function", ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_WidthStretch, 3.0f);
            ImGui::TableSetupColumn("Inclusive",  ImGuiTableColumnFlags_WidthFixed, 100.0f);
            ImGui::TableSetupColumn("Exclusive",  ImGuiTableColumnFlags_WidthFixed, 100.0f);
            if (m_ShowCallCounts)
                ImGui::TableSetupColumn("Calls", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableSetupColumn("Inc %", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableSetupColumn("Exc %", ImGuiTableColumnFlags_WidthFixed, 60.0f);

            ImGui::TableHeadersRow();

            for (size_t i = 0; i < m_ThreadRoots.size(); ++i) {
                if (!IsThreadInFilter(m_ThreadRoots[i]->name))
                    continue;
                if (SubtreePassesFilter(*m_ThreadRoots[i])) {
                    RenderThreadTree(*m_ThreadRoots[i], i);
                }
            }

            ImGui::EndTable();
        }
    }

    // -------------------------------------------------------------------------
    // Toolbar — filter, options
    // -------------------------------------------------------------------------

    void ProfilerTreeView::RenderToolbar()
    {
        ImGui::SetNextItemWidth(250.0f);
        ImGui::InputTextWithHint("##Filter", "Filter functions...", m_FilterBuffer, sizeof(m_FilterBuffer));
        ImGui::SameLine();
        ImGui::Checkbox("Show Calls", &m_ShowCallCounts);
        ImGui::SameLine();
        ImGui::Checkbox("Sort Exclusive", &m_SortByExclusive);
        ImGui::SameLine();
        ImGui::TextDisabled("CPU: %.2f GHz", m_CPUFrequency / 1e9);
    }

    // -------------------------------------------------------------------------
    // Tree building
    // -------------------------------------------------------------------------

    void ProfilerTreeView::RebuildTree(TimelineFlameGraphData* data)
    {
        // Quick fingerprint to avoid rebuilding every frame
        size_t fingerprint = 0;
        for (size_t t = 0; t < data->GetThreadCount(); ++t) {
            auto* thread = data->GetThread(t);
            if (thread)
                fingerprint += thread->events.size() * (t + 1);
        }

        if (m_LastBuiltData == data && m_LastEventFingerprint == fingerprint)
            return;

        m_LastBuiltData = data;
        m_LastEventFingerprint = fingerprint;
        m_ThreadRoots.clear();

        for (size_t t = 0; t < data->GetThreadCount(); ++t) {
            auto* thread = data->GetThread(t);
            if (!thread)
                continue;

            // Create a root node representing the thread
            auto threadRoot = std::make_unique<CallTreeNode>();
            threadRoot->name = thread->threadName;
            threadRoot->depth = -1; // Sentinel — thread level

            // Sort events by start time for correct stack reconstruction
            std::vector<const TimelineFlameNode*> sorted;
            sorted.reserve(thread->events.size());
            for (const auto& e : thread->events) {
                if (e->duration > 0)
                    sorted.push_back(e.get());
            }
            std::sort(sorted.begin(), sorted.end(),
                [](const TimelineFlameNode* a, const TimelineFlameNode* b) {
                    return a->startTime < b->startTime;
                });

            // Walk events and build the tree using the depth information.
            // We maintain a stack of CallTreeNode* that mirrors the current
            // call-stack depth so we can find the correct parent.
            std::vector<CallTreeNode*> stack;
            stack.push_back(threadRoot.get());

            for (const auto* event : sorted) {
                int targetDepth = event->depth; // 0-based

                // Pop the stack back to the parent depth.
                // stack[0] is the thread root (depth -1), so the node at
                // stack[d+1] corresponds to depth d.
                while (static_cast<int>(stack.size()) > targetDepth + 1)
                    stack.pop_back();

                CallTreeNode* parent = stack.back();
                CallTreeNode* node = parent->GetOrCreateChild(event->name);
                node->inclusiveCycles += event->duration;
                node->callCount++;

                // Push so that deeper events become children of this node
                stack.push_back(node);
            }

            // Calculate exclusive times for all children
            for (auto& child : threadRoot->children)
                CalculateExclusiveTimes(child.get());

            // Thread-root inclusive = thread duration
            threadRoot->inclusiveCycles = thread->threadDuration;
            uint64_t childSum = 0;
            for (auto& c : threadRoot->children)
                childSum += c->inclusiveCycles;
            threadRoot->exclusiveCycles = (threadRoot->inclusiveCycles > childSum)
                ? threadRoot->inclusiveCycles - childSum : 0;

            m_ThreadRoots.push_back(std::move(threadRoot));
        }
    }

    // -------------------------------------------------------------------------
    // Rendering a thread subtree
    // -------------------------------------------------------------------------

    void ProfilerTreeView::RenderThreadTree(const CallTreeNode& threadRoot, size_t threadIndex)
    {
        ImGui::PushID(static_cast<int>(threadIndex));

        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        // Thread header row — always a tree node
        bool open = ImGui::TreeNodeEx(threadRoot.name.c_str(),
            ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth);

        // Inclusive
        ImGui::TableNextColumn();
        ImGui::TextDisabled("%s", ViewUtils::FormatCycles(threadRoot.inclusiveCycles, m_CPUFrequency).c_str());
        // Exclusive
        ImGui::TableNextColumn();
        ImGui::TextDisabled("%s", ViewUtils::FormatCycles(threadRoot.exclusiveCycles, m_CPUFrequency).c_str());
        // Calls
        if (m_ShowCallCounts) {
            ImGui::TableNextColumn();
            ImGui::TextDisabled("--");
        }
        // Inc %
        ImGui::TableNextColumn();
        ImGui::TextDisabled("100%%");
        // Exc %
        ImGui::TableNextColumn();
        ImGui::TextDisabled("%s", ViewUtils::FormatPercentage(threadRoot.exclusiveCycles, threadRoot.inclusiveCycles).c_str());

        if (open) {
            // Optionally sort children
            std::vector<const CallTreeNode*> sortedChildren;
            sortedChildren.reserve(threadRoot.children.size());
            for (auto& c : threadRoot.children)
                sortedChildren.push_back(c.get());

            std::sort(sortedChildren.begin(), sortedChildren.end(),
                [this](const CallTreeNode* a, const CallTreeNode* b) {
                    return m_SortByExclusive
                        ? a->exclusiveCycles > b->exclusiveCycles
                        : a->inclusiveCycles > b->inclusiveCycles;
                });

            for (const auto* child : sortedChildren) {
                if (SubtreePassesFilter(*child))
                    RenderNodeRow(*child, threadRoot.inclusiveCycles);
            }

            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    // -------------------------------------------------------------------------
    // Rendering a single node row (recursive)
    // -------------------------------------------------------------------------

    void ProfilerTreeView::RenderNodeRow(const CallTreeNode& node, uint64_t threadTotalCycles)
    {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth;
        if (node.children.empty())
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

        bool open = ImGui::TreeNodeEx(node.name.c_str(), flags);

        // Inclusive
        ImGui::TableNextColumn();
        ImGui::Text("%s", ViewUtils::FormatCycles(node.inclusiveCycles, m_CPUFrequency).c_str());

        // Exclusive
        ImGui::TableNextColumn();
        if (node.exclusiveCycles > 0)
            ImGui::Text("%s", ViewUtils::FormatCycles(node.exclusiveCycles, m_CPUFrequency).c_str());
        else
            ImGui::TextDisabled("--");

        // Calls
        if (m_ShowCallCounts) {
            ImGui::TableNextColumn();
            ImGui::Text("%u", node.callCount);
        }

        // Inc %
        ImGui::TableNextColumn();
        ImGui::Text("%s", ViewUtils::FormatPercentage(node.inclusiveCycles, threadTotalCycles).c_str());

        // Exc %
        ImGui::TableNextColumn();
        if (node.exclusiveCycles > 0)
            ImGui::Text("%s", ViewUtils::FormatPercentage(node.exclusiveCycles, threadTotalCycles).c_str());
        else
            ImGui::TextDisabled("--");

        if (open && !node.children.empty()) {
            std::vector<const CallTreeNode*> sortedChildren;
            sortedChildren.reserve(node.children.size());
            for (auto& c : node.children)
                sortedChildren.push_back(c.get());

            std::sort(sortedChildren.begin(), sortedChildren.end(),
                [this](const CallTreeNode* a, const CallTreeNode* b) {
                    return m_SortByExclusive
                        ? a->exclusiveCycles > b->exclusiveCycles
                        : a->inclusiveCycles > b->inclusiveCycles;
                });

            for (const auto* child : sortedChildren) {
                if (SubtreePassesFilter(*child))
                    RenderNodeRow(*child, threadTotalCycles);
            }

            ImGui::TreePop();
        }
    }

    // -------------------------------------------------------------------------
    // Filter helpers
    // -------------------------------------------------------------------------

    bool ProfilerTreeView::PassesFilter(const CallTreeNode& node) const
    {
        if (m_FilterBuffer[0] == '\0')
            return true;

        // Case-insensitive substring search
        std::string lower = node.name;
        std::string filter = m_FilterBuffer;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);
        return lower.find(filter) != std::string::npos;
    }

    bool ProfilerTreeView::SubtreePassesFilter(const CallTreeNode& node) const
    {
        if (m_FilterBuffer[0] == '\0')
            return true;

        if (PassesFilter(node))
            return true;

        for (auto& child : node.children) {
            if (SubtreePassesFilter(*child))
                return true;
        }
        return false;
    }

    bool ProfilerTreeView::IsThreadInFilter(const std::string& threadName) const
    {
        if (m_ThreadFilter.empty())
            return true; // No filter = show all

        return std::find(m_ThreadFilter.begin(), m_ThreadFilter.end(), threadName) != m_ThreadFilter.end();
    }

} // namespace Profiler