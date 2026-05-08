#include "CodeCompareVisualizer.h"
#include "AIAssistantService.h"

#include "imgui.h"

#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <thread>
#include <algorithm>
#include <cstdio>

#ifdef _WIN32
#   define WIN32_LEAN_AND_MEAN
#   define NOMINMAX
#   include <windows.h>
#   include <shobjidl.h>     // IFileOpenDialog
#   pragma comment(lib, "ole32.lib")
#endif

namespace fs = std::filesystem;

// ─── DirCompareNode ──────────────────────────────────────────────────────────

void DirCompareNode::ComputeAggregates()
{
    countOnlyA = countOnlyB = countIdentical = countDifferent = 0;

    for (const auto& f : files)
    {
        switch (f.status)
        {
        case FileCompareResult::Status::OnlyInA:    ++countOnlyA;     break;
        case FileCompareResult::Status::OnlyInB:    ++countOnlyB;     break;
        case FileCompareResult::Status::Identical:  ++countIdentical; break;
        case FileCompareResult::Status::Different:  ++countDifferent; break;
        }
    }

    for (auto& child : children)
    {
        child.ComputeAggregates();
        countOnlyA     += child.countOnlyA;
        countOnlyB     += child.countOnlyB;
        countIdentical += child.countIdentical;
        countDifferent += child.countDifferent;
    }

    totalFiles = countOnlyA + countOnlyB + countIdentical + countDifferent;
}

// ─── Internal helpers ────────────────────────────────────────────────────────

static DirCompareNode& GetOrCreateChild(DirCompareNode& node, const std::string& name)
{
    for (auto& child : node.children)
        if (child.name == name)
            return child;

    DirCompareNode& child = node.children.emplace_back();
    child.name = name;
    return child;
}

static void InsertFile(DirCompareNode& root,
                       const fs::path& relPath,
                       FileCompareResult result)
{
    DirCompareNode* cur = &root;

    auto it  = relPath.begin();
    auto end = relPath.end();

    while (it != end)
    {
        const std::string component = it->string();
        ++it;

        if (it == end) // leaf = file name
        {
            result.name = component;
            cur->files.push_back(std::move(result));
        }
        else           // intermediate = directory
        {
            cur = &GetOrCreateChild(*cur, component);
        }
    }
}

static std::vector<std::string> ReadLines(const std::string& path)
{
    std::vector<std::string> lines;
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open())
        return lines;

    std::string line;
    while (std::getline(f, line))
    {
        // Strip trailing \r for CRLF files
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        lines.push_back(std::move(line));
    }
    return lines;
}

// ─── Diff ────────────────────────────────────────────────────────────────────

/*static*/ float CodeCompareVisualizer::ComputeDiffPercent(const std::string& pathA,
                                                           const std::string& pathB)
{
    const auto linesA = ReadLines(pathA);
    const auto linesB = ReadLines(pathB);

    if (linesA.empty() && linesB.empty()) return 0.f;
    if (linesA.empty() || linesB.empty()) return 100.f;

    // Sørensen-Dice coefficient:  similarity = 2|A∩B| / (|A|+|B|)
    std::map<std::string, int> countA, countB;
    for (const auto& l : linesA) ++countA[l];
    for (const auto& l : linesB) ++countB[l];

    int intersection = 0;
    for (const auto& [line, cnt] : countA)
    {
        auto it = countB.find(line);
        if (it != countB.end())
            intersection += std::min(cnt, it->second);
    }

    const float similarity = 2.f * static_cast<float>(intersection) /
                             static_cast<float>(linesA.size() + linesB.size());
    return (1.f - similarity) * 100.f;
}

// ─── Comparison worker ───────────────────────────────────────────────────────

void CodeCompareVisualizer::StartComparison()
{
    if (m_isComparing.load()) return;

    // Parse extensions on the main thread before the worker starts
    m_activeExtensions = ParseExtensions(m_filterStr);

    m_isComparing = true;
    m_hasResult   = false;

    {
        std::lock_guard<std::mutex> lk(m_statusMutex);
        m_statusMessage = "Starting...";
    }

    std::thread([this,
                 a    = std::string(m_pathA),
                 b    = std::string(m_pathB),
                 exts = m_activeExtensions]()   // copy the parsed set into the thread
    {
        DoComparison(a, b, exts);
    }).detach();
}

void CodeCompareVisualizer::DoComparison(std::string pathA, std::string pathB,
                                         std::set<std::string> extensions)
{
    auto setStatus = [&](std::string msg)
    {
        std::lock_guard<std::mutex> lk(m_statusMutex);
        m_statusMessage = std::move(msg);
    };

    try
    {
        if (!fs::exists(pathA) || !fs::is_directory(pathA))
            { setStatus("Error: Path A is not a valid directory."); m_isComparing = false; return; }
        if (!fs::exists(pathB) || !fs::is_directory(pathB))
            { setStatus("Error: Path B is not a valid directory."); m_isComparing = false; return; }

        setStatus("Scanning directories...");

        std::map<std::string, fs::path> filesA, filesB;
        constexpr auto scanOpts = fs::directory_options::skip_permission_denied;

        for (const auto& entry : fs::recursive_directory_iterator(pathA, scanOpts))
            if (entry.is_regular_file() && PassesFilter(entry.path(), extensions))
                filesA[fs::relative(entry.path(), pathA).string()] = entry.path();

        for (const auto& entry : fs::recursive_directory_iterator(pathB, scanOpts))
            if (entry.is_regular_file() && PassesFilter(entry.path(), extensions))
                filesB[fs::relative(entry.path(), pathB).string()] = entry.path();

        // Union of all relative paths
        std::set<std::string> allRel;
        for (const auto& [k, v] : filesA) allRel.insert(k);
        for (const auto& [k, v] : filesB) allRel.insert(k);

        const int total     = static_cast<int>(allRel.size());
        int       processed = 0;

        DirCompareNode root;
        root.name = "/";

        for (const auto& rel : allRel)
        {
            ++processed;
            if (processed % 100 == 0)
            {
                char buf[128];
                std::snprintf(buf, sizeof(buf),
                    "Comparing file %d / %d…", processed, total);
                setStatus(buf);
            }

            const bool inA = filesA.count(rel) > 0;
            const bool inB = filesB.count(rel) > 0;

            FileCompareResult result;

            if (inA && !inB)
            {
                result.status   = FileCompareResult::Status::OnlyInA;
                result.absPathA = filesA.at(rel).string();
            }
            else if (!inA && inB)
            {
                result.status   = FileCompareResult::Status::OnlyInB;
                result.absPathB = filesB.at(rel).string();
            }
            else
            {
                result.absPathA    = filesA.at(rel).string();
                result.absPathB    = filesB.at(rel).string();
                result.diffPercent = ComputeDiffPercent(result.absPathA, result.absPathB);
                result.status = result.diffPercent < 0.01f
                    ? FileCompareResult::Status::Identical
                    : FileCompareResult::Status::Different;
            }

            InsertFile(root, fs::path(rel), result);
        }

        root.ComputeAggregates();

        {
            std::lock_guard<std::mutex> lk(m_statusMutex);
            m_rootNode = std::move(root);
        }

        char buf[128];
        std::snprintf(buf, sizeof(buf), "Done — %d files compared.", total);
        setStatus(buf);

        m_hasResult   = true;
        m_isComparing = false;
    }
    catch (const std::exception& ex)
    {
        setStatus(std::string("Error: ") + ex.what());
        m_isComparing = false;
    }
}

// ─── ImGui rendering ─────────────────────────────────────────────────────────

static void DrawStatBar(const DirCompareNode& node, float height = 14.f)
{
    if (node.totalFiles == 0) return;

    const ImVec2 size  = { ImGui::GetContentRegionAvail().x, height };
    const ImVec2 p0    = ImGui::GetCursorScreenPos();
    ImDrawList*  dl    = ImGui::GetWindowDrawList();
    const float  scale = size.x / static_cast<float>(node.totalFiles);

    float x = p0.x;

    auto segment = [&](int count, ImU32 col)
    {
        const float w = count * scale;
        if (w > 0.f)
        {
            dl->AddRectFilled({ x, p0.y }, { x + w, p0.y + height }, col);
            x += w;
        }
    };

    segment(node.countIdentical, IM_COL32( 60, 190,  60, 210));
    segment(node.countDifferent, IM_COL32(220, 185,  40, 210));
    segment(node.countOnlyA,     IM_COL32( 70, 140, 220, 210));
    segment(node.countOnlyB,     IM_COL32(220,  75,  75, 210));

    // Border
    dl->AddRect(p0, { p0.x + size.x, p0.y + height }, IM_COL32(80, 80, 80, 180));
    ImGui::Dummy(size);
}

void CodeCompareVisualizer::RenderLegend() const
{
    constexpr float gap = 18.f;

    // Helper: checkbox + coloured label inline
    struct Entry { bool* flag; ImU32 col; const char* cbId; const char* label; };
    const Entry entries[] =
    {
        { &const_cast<CodeCompareVisualizer*>(this)->m_showIdentical, IM_COL32( 60, 190,  60, 255), "##chkIdentical", "Identical" },
        { &const_cast<CodeCompareVisualizer*>(this)->m_showDifferent, IM_COL32(220, 185,  40, 255), "##chkDifferent", "Different" },
        { &const_cast<CodeCompareVisualizer*>(this)->m_showOnlyA,     IM_COL32( 70, 140, 220, 255), "##chkOnlyA",     "Only in A" },
        { &const_cast<CodeCompareVisualizer*>(this)->m_showOnlyB,     IM_COL32(220,  75,  75, 255), "##chkOnlyB",     "Only in B" },
    };

    for (int i = 0; i < 4; ++i)
    {
        if (i > 0) ImGui::SameLine(0, gap);
        ImGui::Checkbox(entries[i].cbId, entries[i].flag);
        ImGui::SameLine(0, 4.f);
        ImGui::PushStyleColor(ImGuiCol_Text,
            *entries[i].flag ? entries[i].col : IM_COL32(90, 90, 90, 255));
        ImGui::TextUnformatted(entries[i].label);
        ImGui::PopStyleColor();
    }
}

void CodeCompareVisualizer::RenderOverallStats(const DirCompareNode& node) const
{
    const int   n    = node.totalFiles;
    const float invN = n > 0 ? 100.f / static_cast<float>(n) : 0.f;

    ImGui::TextUnformatted("Overall: ");
    ImGui::SameLine();
    ImGui::TextColored({ 0.35f, 0.85f, 0.35f, 1.f },
        "%d identical (%.1f%%)", node.countIdentical, node.countIdentical * invN);
    ImGui::SameLine(0, 20.f);
    ImGui::TextColored({ 0.90f, 0.80f, 0.25f, 1.f },
        "%d different (%.1f%%)", node.countDifferent, node.countDifferent * invN);
    ImGui::SameLine(0, 20.f);
    ImGui::TextColored({ 0.35f, 0.60f, 0.90f, 1.f },
        "%d only in A (%.1f%%)", node.countOnlyA, node.countOnlyA * invN);
    ImGui::SameLine(0, 20.f);
    ImGui::TextColored({ 0.90f, 0.38f, 0.38f, 1.f },
        "%d only in B (%.1f%%)", node.countOnlyB, node.countOnlyB * invN);
    ImGui::SameLine(0, 20.f);
    ImGui::TextDisabled("(%d total)", n);

    ImGui::Spacing();
    DrawStatBar(node, 16.f);
    ImGui::Spacing();
}

void CodeCompareVisualizer::RenderFileRow(const FileCompareResult& file)
{
    ImU32       textCol;
    const char* statusStr;

    switch (file.status)
    {
    case FileCompareResult::Status::Identical:
        textCol   = IM_COL32( 60, 190,  60, 255); statusStr = "Identical"; break;
    case FileCompareResult::Status::Different:
        textCol   = IM_COL32(220, 185,  40, 255); statusStr = "Different"; break;
    case FileCompareResult::Status::OnlyInA:
        textCol   = IM_COL32( 70, 140, 220, 255); statusStr = "Only in A"; break;
    case FileCompareResult::Status::OnlyInB:
        textCol   = IM_COL32(220,  75,  75, 255); statusStr = "Only in B"; break;
    default:
        textCol   = IM_COL32(255, 255, 255, 255); statusStr = ""; break;
    }

    ImGui::TableNextRow();

    // Column 0 – filename with selectable spanning all columns
    ImGui::TableSetColumnIndex(0);
    const bool isSelected = (m_selectedAbsPathA == file.absPathA &&
                             m_selectedAbsPathB == file.absPathB &&
                             m_selectedName     == file.name);

    ImGui::PushStyleColor(ImGuiCol_Text, textCol);
    if (ImGui::Selectable(file.name.c_str(), isSelected,
                          ImGuiSelectableFlags_SpanAllColumns |
                          ImGuiSelectableFlags_AllowOverlap,
                          { 0.f, 0.f }))
    {
        m_selectedName     = file.name;
        m_selectedAbsPathA = file.absPathA;
        m_selectedAbsPathB = file.absPathB;
        m_selectedStatus   = file.status;
        LoadFileDiff(file.absPathA, file.absPathB, file.status);
    }
    ImGui::PopStyleColor();

    // Column 1 – status
    ImGui::TableSetColumnIndex(1);
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(textCol), "%s", statusStr);

    // Column 2 – difference % with mini-bar
    ImGui::TableSetColumnIndex(2);
    if (file.status == FileCompareResult::Status::Different)
    {
        ImGui::Text("%.1f%%", file.diffPercent);

        const float availW = ImGui::GetContentRegionAvail().x;
        const float barW   = availW * (file.diffPercent / 100.f);
        const ImVec2 bp    = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(
            bp, { bp.x + barW, bp.y + 5.f },
            IM_COL32(220, 185, 40, 150));
        ImGui::Dummy({ availW, 5.f });
    }
}

void CodeCompareVisualizer::RenderDirNode(const DirCompareNode& node)
{
    if (node.totalFiles == 0) return;

    const int   n    = node.totalFiles;
    const float invN = n > 0 ? 100.f / static_cast<float>(n) : 0.f;

    // Pick label colour: green if all identical, yellow if any differ, white otherwise
    ImVec4 labelCol = { 1.f, 1.f, 1.f, 1.f };
    if (node.countIdentical == n)
        labelCol = { 0.40f, 0.88f, 0.40f, 1.f };
    else if (node.countDifferent > 0 || node.countOnlyA > 0 || node.countOnlyB > 0)
        labelCol = { 0.95f, 0.85f, 0.30f, 1.f };

    ImGui::PushStyleColor(ImGuiCol_Text, labelCol);
    const bool open = ImGui::TreeNode(node.name.c_str());
    ImGui::PopStyleColor();

    // Inline summary
    ImGui::SameLine();
    ImGui::TextDisabled(
        "  %d files  |  Same: %d (%.0f%%)  |  Diff: %d (%.0f%%)  |  Only A: %d (%.0f%%)  |  Only B: %d (%.0f%%)",
        n,
        node.countIdentical, node.countIdentical * invN,
        node.countDifferent, node.countDifferent * invN,
        node.countOnlyA,     node.countOnlyA     * invN,
        node.countOnlyB,     node.countOnlyB     * invN);

    // Stat bar always visible, even when collapsed
    DrawStatBar(node, 8.f);

    if (!open)
        return;

    // Subdirectories first
    for (const auto& child : node.children)
        RenderDirNode(child);

    // Files in this directory
    if (!node.files.empty())
    {
        constexpr ImGuiTableFlags tableFlags =
            ImGuiTableFlags_Borders    |
            ImGuiTableFlags_RowBg      |
            ImGuiTableFlags_ScrollY    |
            ImGuiTableFlags_SizingStretchProp;

        const float rowH    = ImGui::GetTextLineHeightWithSpacing();
        const float tableH  = std::min(static_cast<float>(node.files.size()) + 1.5f, 20.f) * rowH;

        if (ImGui::BeginTable("##files", 3, tableFlags, { 0.f, tableH }))
        {
            ImGui::TableSetupColumn("File",       ImGuiTableColumnFlags_WidthStretch, 0.55f);
            ImGui::TableSetupColumn("Status",     ImGuiTableColumnFlags_WidthStretch, 0.20f);
            ImGui::TableSetupColumn("Difference", ImGuiTableColumnFlags_WidthStretch, 0.25f);
            ImGui::TableHeadersRow();

            for (const auto& f : node.files)
            {
                if (PassesVisibilityFilter(f.status))
                    RenderFileRow(f);
            }

            ImGui::EndTable();
        }
    }

    ImGui::TreePop();
}

// ─── File diff ───────────────────────────────────────────────────────────────

/*static*/ std::vector<DiffLine> CodeCompareVisualizer::ComputeDiff(
    const std::vector<std::string>& A,
    const std::vector<std::string>& B)
{
    // LCS-based diff, capped to avoid O(N²) blowup on huge files
    const int m = static_cast<int>(std::min(A.size(), size_t(3000)));
    const int n = static_cast<int>(std::min(B.size(), size_t(3000)));

    // dp[i][j] = LCS length of A[0..i-1], B[0..j-1]
    // Use short to keep the table small (~18 MB for 3000×3000)
    std::vector<std::vector<short>> dp(m + 1, std::vector<short>(n + 1, 0));
    for (int i = 1; i <= m; ++i)
        for (int j = 1; j <= n; ++j)
            dp[i][j] = (A[i-1] == B[j-1])
                ? static_cast<short>(dp[i-1][j-1] + 1)
                : std::max(dp[i-1][j], dp[i][j-1]);

    // Backtrack to build the edit list
    std::vector<DiffLine> result;
    int lineNumA = 0, lineNumB = 0;

    // Build in reverse, track line numbers from the end
    int i = m, j = n;
    while (i > 0 || j > 0)
    {
        if (i > 0 && j > 0 && A[i-1] == B[j-1])
        {
            result.push_back({ DiffLine::Type::Context, A[i-1], B[j-1], i, j });
            --i; --j;
        }
        else if (j > 0 && (i == 0 || dp[i][j-1] >= dp[i-1][j]))
        {
            result.push_back({ DiffLine::Type::OnlyB, "", B[j-1], 0, j });
            --j;
        }
        else
        {
            result.push_back({ DiffLine::Type::OnlyA, A[i-1], "", i, 0 });
            --i;
        }
    }

    // Lines beyond the cap are appended as-is (context)
    for (int k = m; k < static_cast<int>(A.size()); ++k)
        result.push_back({ DiffLine::Type::OnlyA, A[k], "", k + 1, 0 });
    for (int k = n; k < static_cast<int>(B.size()); ++k)
        result.push_back({ DiffLine::Type::OnlyB, "", B[k], 0, k + 1 });

    std::reverse(result.begin(), result.end());
    return result;
}

void CodeCompareVisualizer::LoadFileDiff(const std::string& absPathA,
                                          const std::string& absPathB,
                                          FileCompareResult::Status status)
{
    const auto linesA = absPathA.empty() ? std::vector<std::string>{} : ReadLines(absPathA);
    const auto linesB = absPathB.empty() ? std::vector<std::string>{} : ReadLines(absPathB);
    m_diffLines = ComputeDiff(linesA, linesB);
    m_hasDiff   = true;

    // Compute a stable cache key for this file pair
    const std::string cacheKeyStr = absPathA + "|" + absPathB;
    m_aiCurrentKey = std::hash<std::string>{}(cacheKeyStr);

    // Restore a previously cached AI result if one exists
    const auto it = m_aiCache.find(m_aiCurrentKey);
    if (it != m_aiCache.end())
    {
        m_aiResponse     = it->second.text;
        m_aiIsError      = it->second.isError;
        m_aiHasResponse  = true;
        m_aiFromCache    = true;
        m_aiScrollBottom = false;
    }
    else
    {
        m_aiResponse     = {};
        m_aiIsError      = false;
        m_aiHasResponse  = false;
        m_aiFromCache    = false;
    }
}

void CodeCompareVisualizer::RenderDiffPanel()
{
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_HorizontalScrollbar;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(25, 25, 30, 255));

    if (!ImGui::Begin("File Diff##cc", &m_hasDiff, flags))
    {
        ImGui::PopStyleColor();
        ImGui::End();
        return;
    }

    ImGui::PopStyleColor();

    // Header row
    {
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(180, 180, 180, 255));
        ImGui::Text("Diff:  %s", m_selectedName.c_str());
        ImGui::PopStyleColor();

        if (m_aiService)
        {
            ImGui::SameLine();
            const bool aiQuerying = m_aiService->IsQuerying();
            if (aiQuerying) ImGui::BeginDisabled();
            if (ImGui::SmallButton(aiQuerying ? "Asking AI..." : "Ask AI"))
            {
                m_aiHasResponse = false;
                m_aiFromCache   = false;
                m_aiResponse.clear();
                m_aiCache.erase(m_aiCurrentKey); // discard stale cached result

                // Build a structured prompt from the current diff
                std::string prompt;
                prompt.reserve(4096);
                prompt += "You are a senior C++ engineer reviewing a code diff.\n";
                prompt += "File: " + m_selectedName + "\n";
                prompt += "Please:\n";
                prompt += "1. Summarise what changed and why it likely matters.\n";
                prompt += "2. Identify any public API changes (added/removed/renamed functions, changed signatures, new types).\n";
                prompt += "3. Flag any potential bugs or regressions introduced.\n\n";
                prompt += "Diff (- removed, + added, context lines unmarked):\n";
                prompt += "```\n";

                int lineCount = 0;
                for (const auto& dl : m_diffLines)
                {
                    if (++lineCount > 300) { prompt += "... (truncated)\n"; break; }
                    switch (dl.type)
                    {
                    case DiffLine::Type::Context: prompt += "  " + dl.lineA + "\n"; break;
                    case DiffLine::Type::OnlyA:   prompt += "- " + dl.lineA + "\n"; break;
                    case DiffLine::Type::OnlyB:   prompt += "+ " + dl.lineB + "\n"; break;
                    }
                }
                prompt += "```\n";

                m_aiService->Query(prompt,
                    [this](const std::string& response, bool isError)
                    {
                        m_aiResponse     = response;
                        m_aiIsError      = isError;
                        m_aiHasResponse  = true;
                        m_aiFromCache    = false;
                        m_aiScrollBottom = true;

                        // Persist result so switching files and back restores it
                        m_aiCache[m_aiCurrentKey] = { response, isError };
                    });
            }
            if (aiQuerying) ImGui::EndDisabled();
        }
        ImGui::Separator();
    }

    constexpr ImGuiTableFlags tflags =
        ImGuiTableFlags_ScrollY          |
        ImGuiTableFlags_ScrollX          |
        ImGuiTableFlags_RowBg            |
        ImGuiTableFlags_BordersInnerV    |
        ImGuiTableFlags_SizingFixedFit   |
        ImGuiTableFlags_NoHostExtendX;

    if (ImGui::BeginTable("##diff", 4, tflags))
    {
        const float lineNumW  = ImGui::CalcTextSize("99999").x + 8.f;
        const float codeW     = 600.f;

        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("##lnA",  ImGuiTableColumnFlags_WidthFixed, lineNumW);
        ImGui::TableSetupColumn("A",      ImGuiTableColumnFlags_WidthFixed, codeW);
        ImGui::TableSetupColumn("##lnB",  ImGuiTableColumnFlags_WidthFixed, lineNumW);
        ImGui::TableSetupColumn("B",      ImGuiTableColumnFlags_WidthFixed, codeW);
        ImGui::TableHeadersRow();

        // Background colours per row type
        constexpr ImU32 bgOnlyA  = IM_COL32( 80,  30,  30, 180);
        constexpr ImU32 bgOnlyB  = IM_COL32( 30,  70,  30, 180);
        constexpr ImU32 colNumA  = IM_COL32(180, 100, 100, 255);
        constexpr ImU32 colNumB  = IM_COL32(100, 180, 100, 255);
        constexpr ImU32 colCtx   = IM_COL32(120, 120, 120, 255);
        constexpr ImU32 colTextA = IM_COL32(230, 160, 160, 255);
        constexpr ImU32 colTextB = IM_COL32(140, 210, 140, 255);

        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(m_diffLines.size()));
        while (clipper.Step())
        {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
            {
                const DiffLine& dl = m_diffLines[row];

                ImGui::TableNextRow();

                if (dl.type == DiffLine::Type::OnlyA)
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, bgOnlyA);
                else if (dl.type == DiffLine::Type::OnlyB)
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, bgOnlyB);

                // Line number A
                ImGui::TableSetColumnIndex(0);
                if (dl.lineNumA > 0)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, colNumA);
                    ImGui::Text("%d", dl.lineNumA);
                    ImGui::PopStyleColor();
                }

                // Content A
                ImGui::TableSetColumnIndex(1);
                if (!dl.lineA.empty())
                {
                    const ImU32 tc = dl.type == DiffLine::Type::OnlyA ? colTextA : colCtx;
                    ImGui::PushStyleColor(ImGuiCol_Text, tc);
                    ImGui::TextUnformatted(dl.lineA.c_str());
                    ImGui::PopStyleColor();
                }

                // Line number B
                ImGui::TableSetColumnIndex(2);
                if (dl.lineNumB > 0)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, colNumB);
                    ImGui::Text("%d", dl.lineNumB);
                    ImGui::PopStyleColor();
                }

                // Content B
                ImGui::TableSetColumnIndex(3);
                if (!dl.lineB.empty())
                {
                    const ImU32 tc = dl.type == DiffLine::Type::OnlyB ? colTextB : colCtx;
                    ImGui::PushStyleColor(ImGuiCol_Text, tc);
                    ImGui::TextUnformatted(dl.lineB.c_str());
                    ImGui::PopStyleColor();
                }
            }
        }
        clipper.End();
        ImGui::EndTable();
    }

    ImGui::End();
}

void CodeCompareVisualizer::RenderAIPanel()
{
    if (!ImGui::Begin("AI Analysis##cc", &m_aiHasResponse))
    {
        ImGui::End();
        return;
    }

    // Header — show whether this is a cached or live result
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(180, 210, 255, 255));
    ImGui::TextUnformatted("AI Analysis:");
    ImGui::PopStyleColor();

    if (m_aiFromCache)
    {
        ImGui::SameLine(0, 8.f);
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(150, 150, 100, 255));
        ImGui::TextUnformatted("(cached — press \"Ask AI\" in the diff panel to refresh)");
        ImGui::PopStyleColor();
    }

    ImGui::Separator();

    if (m_aiIsError)
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 100, 100, 255));

    // Render line by line so ImGui wraps correctly
    const char* begin = m_aiResponse.c_str();
    const char* end   = begin + m_aiResponse.size();
    const char* ls    = begin;
    while (ls < end)
    {
        const char* le = ls;
        while (le < end && *le != '\n') ++le;
        ImGui::TextUnformatted(ls, le);
        ls = (le < end) ? le + 1 : end;
    }

    if (m_aiIsError)
        ImGui::PopStyleColor();

    if (m_aiScrollBottom)
    {
        ImGui::SetScrollHereY(1.0f);
        m_aiScrollBottom = false;
    }

    ImGui::End();
}

bool CodeCompareVisualizer::Render(bool* isOpen)
{
    ImGui::SetNextWindowSize({ 1200.f, 800.f }, ImGuiCond_FirstUseEver);

    const ImGuiWindowFlags mainFlags =
        ImGuiWindowFlags_NoCollapse;

    if (!ImGui::Begin(GetName(), isOpen, mainFlags))
    {
        ImGui::End();
        return false;
    }

    // ── Path inputs ───────────────────────────────────────────────────────
    ImGui::SeparatorText("Directories to Compare");

    const float labelW   = ImGui::CalcTextSize("Path A:").x + 8.f;
    const float browseW  = 80.f;
    const float spacing  = ImGui::GetStyle().ItemSpacing.x;

    ImGui::Text("Path A:"); ImGui::SameLine(labelW);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - browseW - spacing);
    ImGui::InputText("##pathA", m_pathA, sizeof(m_pathA));
    ImGui::SameLine();
    if (ImGui::Button("Browse##A", { browseW, 0.f }))
    {
#ifdef _WIN32
        BrowseForFolder(m_pathA, sizeof(m_pathA));
#endif
    }

    ImGui::Text("Path B:"); ImGui::SameLine(labelW);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - browseW - spacing);
    ImGui::InputText("##pathB", m_pathB, sizeof(m_pathB));
    ImGui::SameLine();
    if (ImGui::Button("Browse##B", { browseW, 0.f }))
    {
#ifdef _WIN32
        BrowseForFolder(m_pathB, sizeof(m_pathB));
#endif
    }

    // ── Extension filter ──────────────────────────────────────────────────
    ImGui::Text("Filter: "); ImGui::SameLine(labelW);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - browseW - spacing);
    ImGui::InputText("##filter", m_filterStr, sizeof(m_filterStr));
    ImGui::SameLine();
    if (ImGui::Button("Clear##filter", { browseW, 0.f }))
        m_filterStr[0] = '\0';
    ImGui::SameLine(0, 6.f);
    ImGui::TextDisabled("(e.g. *.cpp;*.h  |  empty = all files)");

    ImGui::Spacing();

    // ── Compare button ────────────────────────────────────────────────────
    const bool busy = m_isComparing.load();
    if (busy) ImGui::BeginDisabled();
    if (ImGui::Button("  Compare  ", { 140.f, 0.f }))
        StartComparison();
    if (busy) ImGui::EndDisabled();

    ImGui::SameLine(0, 16.f);
    if (busy)
    {
        const char* spinFrames[] = { "|", "/", "-", "\\" };
        const int   spinIdx      = static_cast<int>(ImGui::GetTime() * 8.0) % 4;
        ImGui::TextColored({ 1.f, 0.8f, 0.2f, 1.f }, "%s  Comparing...", spinFrames[spinIdx]);
    }

    {
        std::lock_guard<std::mutex> lk(m_statusMutex);
        if (!m_statusMessage.empty())
        {
            ImGui::SameLine(0, 12.f);
            ImGui::TextDisabled("%s", m_statusMessage.c_str());
        }
    }

    ImGui::Spacing();

    // ── Legend ────────────────────────────────────────────────────────────
    RenderLegend();

    ImGui::Separator();

    // ── DockSpace — fills the remaining area below the toolbar ────────────
    m_dockspaceId = ImGui::GetID("##ccdockspace");
    ImGui::DockSpace(m_dockspaceId, { 0.f, 0.f }, ImGuiDockNodeFlags_None);

    ImGui::End(); // end main "Code Comparison" window

    // ── Sub-windows docked into the DockSpace ─────────────────────────────

    // File Tree window (always shown)
    ImGui::SetNextWindowDockID(m_dockspaceId, ImGuiCond_FirstUseEver);
    if (ImGui::Begin("File Tree##cc"))
    {
        const bool hasResult = m_hasResult.load();
        const bool isWorking = m_isComparing.load();

        if (!hasResult || isWorking)
        {
            ImGui::Spacing();
            ImGui::TextDisabled("Enter two directory paths above and press Compare.");
        }
        else
        {
            RenderOverallStats(m_rootNode);
            RenderDirNode(m_rootNode);
        }
    }
    ImGui::End();

    // File Diff window (shown when a file is selected)
    if (m_hasDiff)
    {
        ImGui::SetNextWindowDockID(m_dockspaceId, ImGuiCond_FirstUseEver);
        RenderDiffPanel();
    }

    // AI Analysis window (shown when a response is available)
    if (m_aiHasResponse)
    {
        ImGui::SetNextWindowDockID(m_dockspaceId, ImGuiCond_FirstUseEver);
        RenderAIPanel();
    }

    return true;
}

// ─── Windows folder picker ───────────────────────────────────────────────────

#ifdef _WIN32
bool CodeCompareVisualizer::BrowseForFolder(char* outPath, std::size_t outSize)
{
    bool picked = false;

    IFileOpenDialog* pDlg = nullptr;
    if (FAILED(::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE)))
        return false;

    if (SUCCEEDED(::CoCreateInstance(CLSID_FileOpenDialog, nullptr,
                                     CLSCTX_INPROC_SERVER,
                                     IID_PPV_ARGS(&pDlg))))
    {
        // Configure as folder picker
        DWORD dwOptions = 0;
        pDlg->GetOptions(&dwOptions);
        pDlg->SetOptions(dwOptions | FOS_PICKFOLDERS | FOS_PATHMUSTEXIST | FOS_FORCEFILESYSTEM);

        // Pre-populate with whatever is already in the buffer
        if (outPath[0] != '\0')
        {
            wchar_t wide[1024] = {};
            ::MultiByteToWideChar(CP_UTF8, 0, outPath, -1, wide, 1024);

            IShellItem* pInitDir = nullptr;
            if (SUCCEEDED(::SHCreateItemFromParsingName(wide, nullptr,
                                                        IID_PPV_ARGS(&pInitDir))))
            {
                pDlg->SetFolder(pInitDir);
                pInitDir->Release();
            }
        }

        if (SUCCEEDED(pDlg->Show(nullptr)))
        {
            IShellItem* pItem = nullptr;
            if (SUCCEEDED(pDlg->GetResult(&pItem)))
            {
                PWSTR pszPath = nullptr;
                if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszPath)))
                {
                    ::WideCharToMultiByte(CP_UTF8, 0, pszPath, -1,
                                         outPath, static_cast<int>(outSize), nullptr, nullptr);
                    ::CoTaskMemFree(pszPath);
                    picked = true;
                }
                pItem->Release();
            }
        }
        pDlg->Release();
    }

    ::CoUninitialize();
    return picked;
}
#endif

// ─── Extension filter helpers ────────────────────────────────────────────────

bool CodeCompareVisualizer::PassesVisibilityFilter(FileCompareResult::Status s) const
{
    switch (s)
    {
    case FileCompareResult::Status::Identical: return m_showIdentical;
    case FileCompareResult::Status::Different: return m_showDifferent;
    case FileCompareResult::Status::OnlyInA:   return m_showOnlyA;
    case FileCompareResult::Status::OnlyInB:   return m_showOnlyB;
    default:                                   return true;
    }
}

/*static*/ std::set<std::string> CodeCompareVisualizer::ParseExtensions(const char* filterStr)
{
    std::set<std::string> result;
    if (!filterStr || filterStr[0] == '\0')
        return result; // empty = accept all

    std::string token;
    for (const char* p = filterStr; ; ++p)
    {
        const bool delim = (*p == ';' || *p == ',' || *p == ' ' || *p == '\0');
        if (delim)
        {
            // Strip leading "*" or "*." so "*.cpp", ".cpp" and "cpp" all normalise to ".cpp"
            if (!token.empty())
            {
                // Remove leading *
                if (token.front() == '*') token.erase(token.begin());
                // Ensure leading dot
                if (token.empty() || token.front() != '.') token.insert(token.begin(), '.');
                // Lowercase
                for (char& c : token) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                result.insert(token);
            }
            token.clear();
            if (*p == '\0') break;
        }
        else
        {
            token += *p;
        }
    }
    return result;
}

/*static*/ bool CodeCompareVisualizer::PassesFilter(const fs::path& p,
                                                     const std::set<std::string>& exts)
{
    if (exts.empty()) return true; // no filter = accept all

    std::string ext = p.extension().string();
    for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return exts.count(ext) > 0;
}