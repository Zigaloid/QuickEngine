#include "DocumentManager.h"
#include "ObjJsonEditor.h"

DocumentManager::DocumentManager(ImGuiVisualizers::ImGuiVisualizerManager& visualizerManager)
    : m_visualizerManager(visualizerManager)
{
}

void DocumentManager::RegisterAssetTypes()
{
    auto assetBrowser = std::make_unique<ImGuiVisualizers::AssetBrowser>();
    auto& registry = assetBrowser->GetRegistry();

    // Register asset types by multi-part extension
    registry.Register(".def.obj.json", "Definition Object", IM_COL32(100, 200, 255, 255), "O");
    registry.Register(".png", "Texture", IM_COL32(200, 150, 255, 255), "T");
    registry.Register(".bmp", "Texture", IM_COL32(200, 150, 255, 255), "T");
    registry.Register(".dds", "Texture", IM_COL32(200, 150, 255, 255), "T");

    // ── Configure launch options per asset type ─────────────────────────

    // .obj.json files: use the generic ObjJsonEditor
    if (auto* objType = registry.FindMutable(".def.obj.json")) {
        objType->primaryLaunch = {
            "Object Editor",
            ImGuiVisualizers::AssetLaunchMode::Internal,
            [this](const std::string& path) {
                std::string fileName = path;
                auto lastSlash = fileName.find_last_of("/\\");
                if (lastSlash != std::string::npos) {
                    fileName = fileName.substr(lastSlash + 1);
                }
                const std::string suffix = ".def.obj.json";
                std::string className = "CEntityDefinition";

                if (!className.empty()) {
                    std::string key = ImGuiVisualizers::ObjJsonEditor::MakeDocumentKey(path);

                    // If already open, just bring it to front
                    if (m_openEditorKeys.contains(key)) {
                        m_visualizerManager.SetVisible(key, true);
                        return;
                    }

                    // Defer registration — calling Register() here would mutate
                    // m_entries while RenderAll() is iterating over it.
                    EnqueueEditor(key, path, className);
                }
            }
        };
        objType->secondaryLaunches.push_back({
            "Open as JSON (External)",
            ImGuiVisualizers::AssetLaunchMode::External,
            nullptr,
            ""  // empty = OS default for .json
            });
    }

    // Material files: internal material editor
    if (auto* mat = registry.FindMutable(".mat.json")) {
        mat->primaryLaunch = {
            "Material Editor",
            ImGuiVisualizers::AssetLaunchMode::Internal,
            [](const std::string& path) {
                // TODO: open the internal material editor
            }
        };
        mat->secondaryLaunches.push_back({
            "Open as JSON (External)",
            ImGuiVisualizers::AssetLaunchMode::External,
            nullptr,
            ""
            });
    }

    // Textures: internal preview as primary, external editor as secondary
    for (const char* ext : { ".png", ".bmp", ".jpg", ".dds" }) {
        if (auto* tex = registry.FindMutable(ext)) {
            tex->primaryLaunch = {
                "Texture Preview",
                ImGuiVisualizers::AssetLaunchMode::Internal,
                [](const std::string& path) {
                    // TODO: open the internal texture previewer
                }
            };
            tex->secondaryLaunches.push_back({
                "Open in External Editor",
                ImGuiVisualizers::AssetLaunchMode::External,
                nullptr,
                ""
                });
        }
    }

    // Lua scripts: open externally by default
    if (auto* lua = registry.FindMutable(".lua")) {
        lua->primaryLaunch = {
            "Open in External Editor",
            ImGuiVisualizers::AssetLaunchMode::External,
            nullptr,
            ""
        };
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
