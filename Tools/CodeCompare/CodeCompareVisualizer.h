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

    // Comparison entry points
    void StartComparison();
    void DoComparison(std::string pathA, std::string pathB,
                      std::set<std::string> extensions);

    // Rendering helpers
    void RenderLegend()                               const;
    void RenderOverallStats(const DirCompareNode& node) const;
    void RenderDirNode(const DirCompareNode& node)    const;
    void RenderFileRow(const FileCompareResult& file) const;

    // Filter helpers
    static std::set<std::string> ParseExtensions(const char* filterStr);
    static bool                  PassesFilter(const std::filesystem::path& p,
                                              const std::set<std::string>& exts);

#ifdef _WIN32
    bool BrowseForFolder(char* outPath, std::size_t outSize);
#endif
    static float ComputeDiffPercent(const std::string& pathA, const std::string& pathB);
};