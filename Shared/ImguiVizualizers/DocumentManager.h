#pragma once

#include "ImGuiVisualizerManager.h"
#include "AssetBrowser.h"

#include <unordered_set>
#include <unordered_map>
#include <string>
#include <vector>
#include <memory>

// Forward declarations
class IAssetLauncher;
namespace ImGuiVisualizers { class IImGuiVisualizer; }

/**
 * @brief Manages document registration (asset types and launch options)
 *        and the lifecycle of dynamically-created editor instances.
 *
 * Extracted from QuickEditApp so the app class stays focused on its
 * top-level lifecycle while document concerns live here.
 */
class DocumentManager
{
    // Allow launchers to access private members
    friend class ObjJsonLauncher;
    friend class MeshComponentLauncher;
    friend class LevelComponentLauncher;
    friend class EntityComponentLauncher;
    friend class WidgetEditorLauncher;    
    friend class ComponentDependencyGraphLauncher;
    friend class HeightFieldMeshLauncher;
public:
    explicit DocumentManager(ImGuiVisualizers::ImGuiVisualizerManager& visualizerManager);

    /// Register all known asset types and their launch options with the
    /// given AssetBrowser, then add the browser to the visualizer manager.
    void RegisterAssetTypes();

    /// Process deferred editor registrations (safe outside RenderAll).
    void ProcessPendingEditors();

    /// Remove editor instances whose windows have been closed.
    void CleanupClosedEditors();

private:
    /// Queue an editor to be opened on the next ProcessPendingEditors() call.
    // Note: store a non-owning pointer to the launcher so ProcessPendingEditors
    // can call its Create(...) factory method.
    void EnqueueEditor(const std::string& key,
        const std::string& filePath,
        const std::string& className,
        IAssetLauncher* launcher);

    // ── Asset Launcher Registry ─────────────────────────────────────────

    /// Initialize all asset launchers
    void InitializeLaunchers();

    /// Get a launcher by name
    IAssetLauncher* GetLauncher(const char* name);

    // ── Asset type configuration table ──────────────────────────────────

    /// Configuration for a single asset type
    struct AssetTypeConfig {
        const char* extension;
        const char* displayName;
        ImU32 color;
        const char* icon;
        const char* primaryLauncherName;
        bool hasExternalJsonFallback;
        bool hasExternalEditorFallback;
    };

    /// Get the configuration table for all asset types
    std::vector<AssetTypeConfig> GetAssetTypeConfigs();

    ImGuiVisualizers::ImGuiVisualizerManager& m_visualizerManager;

    /// Manager keys of dynamically-created ObjJsonEditor instances.
    std::unordered_set<std::string> m_openEditorKeys;

    /// Deferred registrations to avoid mutating m_entries during RenderAll.
    struct PendingEditor {
        std::string key;
        std::string filePath;
        std::string className;
        IAssetLauncher* launcher = nullptr; // non-owning
    };
    std::vector<PendingEditor> m_pendingEditors;

    /// Registry of asset launchers by name
    std::unordered_map<std::string, std::unique_ptr<IAssetLauncher>> m_launchers;
};

// ─────────────────────────────────────────────────────────────────────────────
// Asset Launcher Interface and Implementations
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Base interface for asset launchers
 */
class IAssetLauncher
{
public:
    virtual ~IAssetLauncher() = default;

    // Launch is the callback used by the AssetBrowser (keeps behavior).
    virtual void Launch(const std::string& assetPath) = 0;

    // Factory method: create and (optionally) initialize a visualizer for the
    // given asset. Return nullptr if the launcher has no internal visualizer.
    virtual std::unique_ptr<ImGuiVisualizers::IImGuiVisualizer>
        Create(const std::string& assetPath, const std::string& className) = 0;
};

/**
 * @brief Launcher for ObjJson editors
 */
class ObjJsonLauncher : public IAssetLauncher
{
public:
    ObjJsonLauncher(DocumentManager& manager,
        const std::string& suffix,
        const std::string& className);

    void Launch(const std::string& assetPath) override;

    std::unique_ptr<ImGuiVisualizers::IImGuiVisualizer>
        Create(const std::string& assetPath, const std::string& className) override;

private:
    DocumentManager& m_manager;
    std::string m_suffix;
    std::string m_className;
};


class MeshComponentLauncher : public IAssetLauncher
{
public:
    MeshComponentLauncher(DocumentManager& manager,
        const std::string& suffix,
        const std::string& className);

    void Launch(const std::string& assetPath) override;

    std::unique_ptr<ImGuiVisualizers::IImGuiVisualizer>
        Create(const std::string& assetPath, const std::string& className) override;

private:
    DocumentManager& m_manager;
    std::string m_suffix;
    std::string m_className;
};

class LevelComponentLauncher : public IAssetLauncher
{
public:
    LevelComponentLauncher(DocumentManager& manager,
        const std::string& suffix,
        const std::string& className);

    void Launch(const std::string& assetPath) override;

    std::unique_ptr<ImGuiVisualizers::IImGuiVisualizer>
        Create(const std::string& assetPath, const std::string& className) override;

private:
    DocumentManager& m_manager;
    std::string m_suffix;
    std::string m_className;
};

class EntityComponentLauncher : public IAssetLauncher
{
public:
    EntityComponentLauncher(DocumentManager& manager,
        const std::string& suffix,
        const std::string& className);

    void Launch(const std::string& assetPath) override;

    std::unique_ptr<ImGuiVisualizers::IImGuiVisualizer>
        Create(const std::string& assetPath, const std::string& className) override;

private:
    DocumentManager& m_manager;
    std::string m_suffix;
    std::string m_className;
};


class WidgetEditorLauncher : public IAssetLauncher
{
public:
    WidgetEditorLauncher(DocumentManager& manager,
        const std::string& suffix,
        const std::string& className);

    void Launch(const std::string& assetPath) override;

    std::unique_ptr<ImGuiVisualizers::IImGuiVisualizer>
        Create(const std::string& assetPath, const std::string& className) override;

private:
    DocumentManager& m_manager;
    std::string m_suffix;
    std::string m_className;
};

class HeightFieldMeshLauncher : public IAssetLauncher
{
public:
    HeightFieldMeshLauncher(DocumentManager& manager,
        const std::string& suffix,
        const std::string& className);

    void Launch(const std::string& assetPath) override;

    std::unique_ptr<ImGuiVisualizers::IImGuiVisualizer>
        Create(const std::string& assetPath, const std::string& className) override;

private:
    DocumentManager& m_manager;
    std::string m_suffix;
    std::string m_className;
};



/**
 * @brief Launcher for ComponentDependencyGraphVisualizer (.cdep.obj.json files).
 *
 * Opens the file directly in a ComponentDependencyGraphVisualizer window,
 * showing an auto-laid-out directed graph of the component dependency
 * definitions stored in the JSON asset.
 */
class ComponentDependencyGraphLauncher : public IAssetLauncher
{
public:
    ComponentDependencyGraphLauncher(DocumentManager& manager,
        const std::string& suffix);

    void Launch(const std::string& assetPath) override;

    std::unique_ptr<ImGuiVisualizers::IImGuiVisualizer>
        Create(const std::string& assetPath, const std::string& className) override;

private:
    DocumentManager& m_manager;
    std::string       m_suffix;
};

/**
 * @brief No-op launcher for TODO/unimplemented features
 */
class NoOpLauncher : public IAssetLauncher
{
public:
    void Launch(const std::string& assetPath) override;
    std::unique_ptr<ImGuiVisualizers::IImGuiVisualizer>
        Create(const std::string& assetPath, const std::string& className) override;
};