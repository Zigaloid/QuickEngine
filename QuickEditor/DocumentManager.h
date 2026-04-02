#pragma once

#include "ImGuiVisualizerManager.h"
#include "AssetBrowser.h"

#include <unordered_set>
#include <string>
#include <vector>

/**
 * @brief Manages document registration (asset types and launch options)
 *        and the lifecycle of dynamically-created editor instances.
 *
 * Extracted from QuickEditApp so the app class stays focused on its
 * top-level lifecycle while document concerns live here.
 */
class DocumentManager
{
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
};
