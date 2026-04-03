#include "DocumentManager.h"
#include "ObjJsonEditor.h"

#ifdef _WIN32
#include <Windows.h>
#include <shellapi.h>
#endif

// ─────────────────────────────────────────────────────────────────────────────
// Asset Launcher Implementations
// ─────────────────────────────────────────────────────────────────────────────

ObjJsonLauncher::ObjJsonLauncher(DocumentManager& manager, 
                                 const std::string& suffix, 
                                 const std::string& className)
    : m_manager(manager)
    , m_suffix(suffix)
    , m_className(className)
{
}

void ObjJsonLauncher::Launch(const std::string& assetPath)
{
    std::string key = ImGuiVisualizers::ObjJsonEditor::MakeDocumentKey(assetPath);

    // If already open, just bring it to front
    if (m_manager.m_openEditorKeys.contains(key)) {
        m_manager.m_visualizerManager.SetVisible(key, true);
        return;
    }

    // Defer registration to avoid mutating m_entries during RenderAll
    m_manager.EnqueueEditor(key, assetPath, m_className);
}

void NoOpLauncher::Launch(const std::string& assetPath)
{
#ifdef _WIN32
    ShellExecuteA(nullptr, "open", assetPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#else
    std::string command = "xdg-open \"" + assetPath + "\" &";
    std::system(command.c_str());
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// DocumentManager Implementation
// ─────────────────────────────────────────────────────────────────────────────

DocumentManager::DocumentManager(ImGuiVisualizers::ImGuiVisualizerManager& visualizerManager)
    : m_visualizerManager(visualizerManager)
{
}

void DocumentManager::InitializeLaunchers()
{
    // ObjJson launchers
    m_launchers["DefObjJson"] = std::make_unique<ObjJsonLauncher>(*this, ".def.obj.json", "CEntityDefinition");
    m_launchers["EntityObjJson"] = std::make_unique<ObjJsonLauncher>(*this, ".entity.obj.json", "CEntityInstance");
    m_launchers["MeshObjJson"] = std::make_unique<ObjJsonLauncher>(*this, ".mesh.obj.json", "CMeshComponent");

    // Material launcher (TODO)
    m_launchers["Material"] = std::make_unique<NoOpLauncher>();

    // Texture launcher (TODO)
    m_launchers["Texture"] = std::make_unique<NoOpLauncher>();
}

IAssetLauncher* DocumentManager::GetLauncher(const char* name)
{
    auto it = m_launchers.find(name);
    return (it != m_launchers.end()) ? it->second.get() : nullptr;
}

std::vector<DocumentManager::AssetTypeConfig> DocumentManager::GetAssetTypeConfigs()
{
    return 
    {
        // ObjJson types
        { ".mesh.obj.json",     "Mesh Component",         IM_COL32(100, 200, 255, 255),   "O",  "MeshObjJson",   true, false },
        { ".def.obj.json",      "Definition Object",      IM_COL32(100, 200, 255, 255),   "O",  "DefObjJson",    true, false },
        { ".entity.obj.json",   "Entity Object",          IM_COL32(100, 200, 255, 255),   "O",  "EntityObjJson", true, false },        
        { ".png",               "PNG Texture",            IM_COL32(200, 150, 255, 255),   "T",  "Texture",      false,  true },
        { ".bmp",               "BMP Texture",            IM_COL32(200, 150, 255, 255),   "T",  "Texture",      false,  true },
        { ".jpg",               "JPG Texture",            IM_COL32(200, 150, 255, 255),   "T",  "Texture",      false,  true },
        { ".dds",               "DDS Texture",            IM_COL32(200, 150, 255, 255),   "T",  "Texture",      false,  true },
    };
}

void DocumentManager::RegisterAssetTypes()
{
    // Initialize all launchers
    InitializeLaunchers();

    auto assetBrowser = std::make_unique<ImGuiVisualizers::AssetBrowser>();
    auto& registry = assetBrowser->GetRegistry();

    // Register all asset types from the configuration table
    auto configs = GetAssetTypeConfigs();

    for (const auto& config : configs) {
        // Register basic asset type info
        registry.Register(config.extension, config.displayName, config.color, config.icon);

        // Configure launch options
        if (auto* assetType = registry.FindMutable(config.extension)) {
            // Set up primary launch
            if (config.primaryLauncherName != nullptr) {
                if (auto* launcher = GetLauncher(config.primaryLauncherName)) {
                    assetType->primaryLaunch = {
                        config.displayName == std::string("Texture") ? "Texture Preview" :
                        config.displayName == std::string("Material") ? "Material Editor" : "Object Editor",
                        ImGuiVisualizers::AssetLaunchMode::Internal,
                        [launcher](const std::string& path) { launcher->Launch(path); }
                    };
                }
            } else {
                // External-only (e.g., Lua scripts)
                assetType->primaryLaunch = {
                    "Open in External Editor",
                    ImGuiVisualizers::AssetLaunchMode::External,
                    nullptr,
                    ""
                };
            }

            // Add secondary launch options
            if (config.hasExternalJsonFallback) {
                assetType->secondaryLaunches.push_back({
                    "Open as JSON (External)",
                    ImGuiVisualizers::AssetLaunchMode::External,
                    nullptr,
                    ""
                });
            }

            if (config.hasExternalEditorFallback) {
                assetType->secondaryLaunches.push_back({
                    "Open in External Editor",
                    ImGuiVisualizers::AssetLaunchMode::External,
                    nullptr,
                    ""
                });
            }
        }
    }

    m_visualizerManager.Register("Asset Browser", std::move(assetBrowser), true);
}

void DocumentManager::EnqueueEditor(const std::string& key,
                                    const std::string& filePath,
                                    const std::string& className)
{
    m_pendingEditors.push_back({ key, filePath, className });
}

void DocumentManager::ProcessPendingEditors()
{
    for (auto& pending : m_pendingEditors) {
        // Guard against duplicates (e.g. rapid double-clicks across frames)
        if (m_openEditorKeys.contains(pending.key)) {
            m_visualizerManager.SetVisible(pending.key, true);
            continue;
        }

        auto editor = std::make_unique<ImGuiVisualizers::ObjJsonEditor>();
        editor->Open(pending.filePath, pending.className);
        m_visualizerManager.Register(pending.key, std::move(editor), true);
        m_openEditorKeys.insert(pending.key);
    }
    m_pendingEditors.clear();
}

void DocumentManager::CleanupClosedEditors()
{
    std::erase_if(m_openEditorKeys, [this](const std::string& key) {
        if (!m_visualizerManager.IsVisible(key)) {
            m_visualizerManager.Unregister(key);
            return true;
        }
        return false;
    });
}
