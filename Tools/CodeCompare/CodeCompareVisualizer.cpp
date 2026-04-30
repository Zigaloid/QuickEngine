#include "CodeCompareVisualizer.h"

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
                result.status = FileCompareResult::Status::OnlyInA;
            }
            else if (!inA && inB)
            {
                result.status = FileCompareResult::Status::OnlyInB;
            }
            else
            {
                result.diffPercent = ComputeDiffPercent(filesA.at(rel).string(),
                                                        filesB.at(rel).string());
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

    auto entry = [](ImU32 col, const char* label)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, col);
        ImGui::Bullet();
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 4.f);
        ImGui::TextUnformatted(label);
    };

    entry(IM_COL32( 60, 190,  60, 255), "Identical");
    ImGui::SameLine(0, gap);
    entry(IM_COL32(220, 185,  40, 255), "Different");
    ImGui::SameLine(0, gap);
    entry(IM_COL32( 70, 140, 220, 255), "Only in A");
    ImGui::SameLine(0, gap);
    entry(IM_COL32(220,  75,  75, 255), "Only in B");
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

void CodeCompareVisualizer::RenderFileRow(const FileCompareResult& file) const
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

    // Column 0 – filename
    ImGui::TableSetColumnIndex(0);
    ImGui::PushStyleColor(ImGuiCol_Text, textCol);
    ImGui::TextUnformatted(file.name.c_str());
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

void CodeCompareVisualizer::RenderDirNode(const DirCompareNode& node) const
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
                RenderFileRow(f);

            ImGui::EndTable();
        }
    }

    ImGui::TreePop();
}

bool CodeCompareVisualizer::Render(bool* isOpen)
{
    ImGui::SetNextWindowSize({ 1100.f, 750.f }, ImGuiCond_FirstUseEver);

    if (!ImGui::Begin(GetName(), isOpen, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End();
        return false;
    }

    // ── Path inputs ───────────────────────────────────────────────────────
    ImGui::SeparatorText("Directories to Compare");

    const float labelW  = ImGui::CalcTextSize("Path A:").x + 8.f;
    const float inputW  = -1.f; // fill remaining

    ImGui::Text("Path A:"); ImGui::SameLine(labelW);
    ImGui::SetNextItemWidth(-90.f);
    ImGui::InputText("##pathA", m_pathA, sizeof(m_pathA));
    ImGui::SameLine();
    if (ImGui::Button("Browse##A", { 80.f, 0.f }))
    {
#ifdef _WIN32
        BrowseForFolder(m_pathA, sizeof(m_pathA));
#endif
    }

    ImGui::Text("Path B:"); ImGui::SameLine(labelW);
    ImGui::SetNextItemWidth(-90.f);
    ImGui::InputText("##pathB", m_pathB, sizeof(m_pathB));
    ImGui::SameLine();
    if (ImGui::Button("Browse##B", { 80.f, 0.f }))
    {
#ifdef _WIN32
        BrowseForFolder(m_pathB, sizeof(m_pathB));
#endif
    }

    // ── Extension filter ──────────────────────────────────────────────────
    ImGui::Text("Filter: "); ImGui::SameLine(labelW);
    ImGui::SetNextItemWidth(-90.f);
    ImGui::InputText("##filter", m_filterStr, sizeof(m_filterStr));
    ImGui::SameLine();
    if (ImGui::Button("Clear##filter", { 80.f, 0.f }))
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

    // ── Results ───────────────────────────────────────────────────────────
    const bool hasResult  = m_hasResult.load();
    const bool isWorking  = m_isComparing.load();

    if (!hasResult || isWorking)
    {
        ImGui::Spacing();
        ImGui::TextDisabled("Enter two directory paths above and press Compare.");
        ImGui::End();
        return true;
    }

    // Safe to read m_rootNode — worker has finished
    RenderOverallStats(m_rootNode);

    ImGui::BeginChild("##tree", { 0.f, 0.f }, false,
        ImGuiWindowFlags_HorizontalScrollbar);
    RenderDirNode(m_rootNode);
    ImGui::EndChild();

    ImGui::End();
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