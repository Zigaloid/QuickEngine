#pragma once

#include "IImGuiVisualizer.h"
#include "imgui.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <algorithm>

// Forward declarations
namespace FileSystem { class FileSystemManager; }

namespace ImGuiVisualizers {

// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Describes how an asset can be opened (internally or externally).
 */
enum class AssetLaunchMode
{
    Internal,   ///< Open within the application (callback-driven)
    External    ///< Open with an external program (OS shell or custom path)
};

/**
 * @brief A single launch option for an asset type.
 *
 * Each option has a display label, a launch mode, and either an internal
 * callback or an external program path.
 */
struct AssetLaunchOption
{
    /// Display name shown in the context menu, e.g. "Scene Editor", "Open in VS Code"
    std::string label;

    /// Whether this option opens internally or externally
    AssetLaunchMode mode = AssetLaunchMode::Internal;

    /// Callback invoked for Internal launch. Receives the full asset path.
    std::function<void(const std::string& assetPath)> internalHandler;

    /// For External launch: path to the executable. Empty = OS default association.
    std::string externalProgram;
};

// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Defines how a registered asset type should be displayed in the browser.
 */
struct AssetTypeInfo
{
    /// Multi-part extension including the leading dots, e.g. ".scene.json"
    std::string extension;

    /// Human-readable label shown in the UI, e.g. "Scene"
    std::string displayName;

    /// Color used when rendering the asset label in the list
    ImU32 color = IM_COL32(255, 255, 255, 255);

    /// Optional icon character (e.g. a FontAwesome glyph or a simple letter)
    std::string icon;

    /// Whether assets of this type are shown by default when the filter is active
    bool visibleByDefault = true;

    /// Reflected class name used by ClassFactory to instantiate the object.
    /// When non-empty, the asset browser can create an instance via
    /// ClassFactory::createObject() and load the file with CReflectedBase::Read().
    std::string reflectedClassName;

    /// Primary launch option (triggered by double-click). May be empty.
    AssetLaunchOption primaryLaunch;

    /// Additional launch options shown in the right-click "Open With" submenu.
    std::vector<AssetLaunchOption> secondaryLaunches;
};

/**
 * @brief Registry that maps multi-part file extensions to asset type metadata.
 *
 * Extensions are stored and compared in lowercase. Multi-part extensions
 * (e.g. ".anim.bin", ".level.json") are matched longest-first so that
 * "foo.level.json" resolves to ".level.json" rather than ".json".
 */
class AssetTypeRegistry
{
public:
    /**
     * @brief Register an asset type by its (possibly multi-part) extension.
     * @param extension  Extension including dots, e.g. ".scene.json"
     * @param displayName Human-readable name, e.g. "Scene"
     * @param color      Display color for the asset label
     * @param icon       Optional icon string
     */
    void Register(const std::string& extension,
                  const std::string& displayName,
                  ImU32 color = IM_COL32(255, 255, 255, 255),
                  const std::string& icon = "");

    /**
     * @brief Register an asset type from a pre-built info struct.
     */
    void Register(const AssetTypeInfo& info);

    /**
     * @brief Unregister an asset type.
     */
    void Unregister(const std::string& extension);

    /**
     * @brief Clear all registered asset types.
     */
    void Clear();

    /**
     * @brief Look up the best-matching asset type for a filename.
     *
     * Performs longest-suffix matching so multi-part extensions win over
     * shorter ones (e.g. ".scene.json" is preferred over ".json").
     *
     * @param filename  The file name (or full path) to match.
     * @return Pointer to the matching AssetTypeInfo, or nullptr if none.
     */
    const AssetTypeInfo* Find(const std::string& filename) const;

    /**
     * @brief Get a mutable pointer to a registered type by extension.
     *
     * Useful for adding launch options after initial registration.
     * @return Pointer to the AssetTypeInfo, or nullptr if not found.
     */
    AssetTypeInfo* FindMutable(const std::string& extension);

    /**
     * @brief Check whether any asset type is registered for a filename.
     */
    bool IsRegisteredAsset(const std::string& filename) const;

    /**
     * @brief Get all registered asset types.
     */
    const std::vector<AssetTypeInfo>& GetAll() const { return m_types; }

    /**
     * @brief Get the number of registered types.
     */
    std::size_t GetCount() const { return m_types.size(); }
    

private:
    

    std::string ToLower(const std::string& str) const;
    bool EndsWith(const std::string& str, const std::string& suffix) const;

    /// All registered types, kept sorted longest-extension-first for matching.
    std::vector<AssetTypeInfo> m_types;
};

// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Folder node used to represent the directory tree.
 */
struct FolderNode
{
    std::string name;
    std::string fullPath;
    std::vector<FolderNode> children;
    bool expanded = false;
};

/**
 * @brief Cached information about a single asset file in the current folder.
 */
struct AssetEntry
{
    std::string fileName;
    std::string fullPath;
    uint64_t    size = 0;
    const AssetTypeInfo* typeInfo = nullptr; // may be nullptr for unregistered files
};

// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief ImGui visualizer that displays a folder tree on the left and the
 *        assets contained in the selected folder on the right.
 *
 * Only files whose extensions are registered in the AssetTypeRegistry are
 * shown (unless "Show All Files" is enabled).
 */
class AssetBrowser : public IImGuiVisualizer
{
public:
    explicit AssetBrowser(const std::string& rootPath = "");
    ~AssetBrowser() override = default;

    // ── IImGuiVisualizer interface ──────────────────────────────────────

    void Initialize() override;
    void Shutdown() override;
    void Update(float deltaTime) override;
    bool Render(bool* isOpen) override;
    const char* GetName() const override;
    const char* GetShortcut() const override;
    const char* GetMenuCategory() const override;

    // ── Configuration ───────────────────────────────────────────────────

    /**
     * @brief Get the asset type registry for external registration.
     */
    AssetTypeRegistry& GetRegistry() { return m_registry; }
    const AssetTypeRegistry& GetRegistry() const { return m_registry; }

    /**
     * @brief Set the root path that the folder tree starts from.
     */
    void SetRootPath(const std::string& path);

    /**
     * @brief Get the currently selected folder path.
     */
    const std::string& GetSelectedFolder() const { return m_selectedFolder; }

    /**
     * @brief Toggle whether non-registered files are shown in the list.
     */
    void SetShowAllFiles(bool show) { m_showAllFiles = show; }
    bool GetShowAllFiles() const { return m_showAllFiles; }

private:
    // Tree construction
    void RebuildFolderTree();
    void BuildFolderTreeRecursive(FolderNode& node);

    // Asset list
    void RefreshAssetList();

    // Rendering helpers
    void RenderFolderTree();
    void RenderFolderNode(FolderNode& node);
    void RenderAssetList();
    void RenderFilterBar();
    void RenderAssetContextMenu(const AssetEntry& asset);
    void RenderFolderContextMenu(FolderNode& node);

    // Launch helpers
    void LaunchAsset(const AssetEntry& asset, const AssetLaunchOption& option);
    void LaunchExternal(const std::string& assetPath, const std::string& program);

    /// Get the effective launch option (primary if defined, otherwise first secondary)
    const AssetLaunchOption* GetEffectiveLaunchOption(const AssetEntry& asset) const;

    // Asset creation
    void OpenCreateAssetDialog(const std::string& folderPath);
    void RenderCreateAssetDialog();
    bool CreateAssetFromTemplate(const std::string& folderPath, const std::string& assetName, const AssetTypeInfo* typeInfo);
    std::string FindTemplateFile(const std::string& extension) const;

    // Asset operations
    void OpenRenameAssetDialog(const AssetEntry& asset);
    void RenderRenameAssetDialog();
    bool RenameAsset(const std::string& oldPath, const std::string& newName);
    bool DeleteAsset(const std::string& assetPath);
    bool MoveAsset(const std::string& assetPath, const std::string& targetFolder);

    // Utility
    std::string FormatFileSize(uint64_t bytes) const;

    // ── State ───────────────────────────────────────────────────────────

    AssetTypeRegistry m_registry;
    FileSystem::FileSystemManager* m_fileSystem = nullptr;

    std::string m_rootPath;
    std::string m_selectedFolder;
    FolderNode  m_rootNode;

    std::vector<AssetEntry> m_assets;

    // UI state
    bool m_showAllFiles     = false;
    bool m_needsRefresh     = true;
    bool m_treeNeedsRebuild = true;
    char m_filterBuffer[256] = {};
    float m_treePanelWidth  = 250.0f;

    // Filter visibility per registered type (keyed by lowercase extension)
    std::unordered_map<std::string, bool> m_typeFilterVisible;

    // Asset creation dialog state
    bool m_showCreateAssetDialog = false;
    std::string m_createAssetFolder;
    char m_createAssetNameBuffer[256] = {};
    int m_selectedAssetTypeIndex = 0;
    std::string m_templateAssetsPath = "./TemplateAssets";

    // Asset rename dialog state
    bool m_showRenameAssetDialog = false;
    std::string m_renameAssetPath;
    std::string m_renameAssetOldName;
    char m_renameAssetNameBuffer[256] = {};
};

} // namespace ImGuiVisualizers