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
    void EnqueueEditor(const std::string& key,
                       const std::string& filePath,
                       const std::string& className);

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
    virtual void Launch(const std::string& assetPath) = 0;
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

private:
    DocumentManager& m_manager;
    std::string m_suffix;
    std::string m_className;
};

/**
 * @brief No-op launcher for TODO/unimplemented features
 */
class NoOpLauncher : public IAssetLauncher
{
public:
    void Launch(const std::string& assetPath) override;
};
