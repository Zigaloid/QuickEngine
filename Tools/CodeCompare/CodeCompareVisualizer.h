#pragma once

#include <string>
#include <vector>
#include <set>
#include <atomic>
#include <mutex>
#include <filesystem>

#include "IImGuiVisualizer.h"

// ─── Data structures ─────────────────────────────────────────────────────────

struct FileCompareResult
{
    enum class Status { OnlyInA, OnlyInB, Identical, Different };

    std::string name;
    Status      status      = Status::Identical;
    float       diffPercent = 0.f; // 0 = identical … 100 = completely different
    std::string absPathA;   // absolute path in tree A (empty if OnlyInB)
    std::string absPathB;   // absolute path in tree B (empty if OnlyInA)
};

// One line in a side-by-side diff view
struct DiffLine
{
    enum class Type { Context, OnlyA, OnlyB };
    Type        type     = Type::Context;
    std::string lineA;
    std::string lineB;
    int         lineNumA = 0;
    int         lineNumB = 0;
};

struct DirCompareNode
{
    std::string                    name;
    std::vector<FileCompareResult> files;
    std::vector<DirCompareNode>    children;

    // Aggregated totals (this dir + all subdirs)
    int totalFiles     = 0;
    int countOnlyA     = 0;
    int countOnlyB     = 0;
    int countIdentical = 0;
    int countDifferent = 0;

    void ComputeAggregates();
};

// ─── Visualizer ──────────────────────────────────────────────────────────────

class CodeCompareVisualizer : public ImGuiVisualizers::IImGuiVisualizer
{
public:
    const char* GetName()         const override { return "Code Comparison"; }
    const char* GetMenuCategory() const override { return "Tools"; }

    bool Render(bool* isOpen) override;
    void Update(float deltaTime) override { (void)deltaTime; }

private:
    // Path inputs
    char m_pathA[1024]   = {};
    char m_pathB[1024]   = {};

    // Extension filter  –  e.g. "*.cpp;*.h;*.inl"   (empty = accept all)
    char m_filterStr[256] = "*.cpp;*.h;*.inl";

    // Parsed from m_filterStr before each comparison run
    std::set<std::string> m_activeExtensions; // lowercase, with leading dot e.g. ".cpp"

    // Worker state
    std::atomic<bool> m_isComparing{ false };
    std::atomic<bool> m_hasResult  { false };

    std::string    m_statusMessage;
    std::mutex     m_statusMutex;
    DirCompareNode m_rootNode;

    // File diff panel
    std::string                m_selectedName;
    std::string                m_selectedAbsPathA;
    std::string                m_selectedAbsPathB;
    FileCompareResult::Status  m_selectedStatus = FileCompareResult::Status::Identical;
    std::vector<DiffLine>      m_diffLines;
    bool                       m_hasDiff    = false;
    float                      m_treeHeight = 350.f;

    // Visibility toggles (legend checkboxes)
    bool m_showIdentical = true;
    bool m_showDifferent = true;
    bool m_showOnlyA     = true;
    bool m_showOnlyB     = true;

    // Comparison entry points
    void StartComparison();
    void DoComparison(std::string pathA, std::string pathB,
                      std::set<std::string> extensions);

    // Rendering helpers
    void RenderLegend()                               const;
    void RenderOverallStats(const DirCompareNode& node) const;
    void RenderDirNode(const DirCompareNode& node);
    void RenderFileRow(const FileCompareResult& file);
    void RenderDiffPanel();
    void LoadFileDiff(const std::string& absPathA, const std::string& absPathB,
                      FileCompareResult::Status status);
    static std::vector<DiffLine> ComputeDiff(const std::vector<std::string>& linesA,
                                             const std::vector<std::string>& linesB);

    // Filter helpers
    static std::set<std::string> ParseExtensions(const char* filterStr);
    static bool                  PassesFilter(const std::filesystem::path& p,
                                              const std::set<std::string>& exts);
    bool                         PassesVisibilityFilter(FileCompareResult::Status s) const;

#ifdef _WIN32
    bool BrowseForFolder(char* outPath, std::size_t outSize);
#endif
    static float ComputeDiffPercent(const std::string& pathA, const std::string& pathB);
};