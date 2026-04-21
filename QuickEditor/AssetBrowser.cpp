#include "AssetBrowser.h"
#include "CoreSystem/CoreSystem.h"

#include <sstream>
#include <iomanip>
#include <cctype>
#include <cstring>
#include <cstdlib>

#ifdef _WIN32
#include <Windows.h>
#include <shellapi.h>
// Windows.h defines macros that conflict with FileSystemManager method names
#undef GetCurrentDirectory
#undef CreateFile
#undef DeleteFile
#undef CopyFile
#undef MoveFile
#undef CreateDirectory
#endif

using namespace FileSystem;

namespace ImGuiVisualizers {

// ═════════════════════════════════════════════════════════════════════════════
// AssetTypeRegistry
// ═════════════════════════════════════════════════════════════════════════════

void AssetTypeRegistry::Register(const std::string& extension,
                                  const std::string& displayName,
                                  ImU32 color,
                                  const std::string& icon)
{
    AssetTypeInfo info;
    info.extension   = ToLower(extension);
    info.displayName = displayName;
    info.color       = color;
    info.icon        = icon;
    Register(info);
}

void AssetTypeRegistry::Register(const AssetTypeInfo& info)
{
    // Remove existing entry with the same extension (case-insensitive)
    std::string extLower = ToLower(info.extension);
    std::erase_if(m_types, [&](const AssetTypeInfo& t) {
        return ToLower(t.extension) == extLower;
    });

    AssetTypeInfo copy = info;
    copy.extension = extLower;
    m_types.push_back(std::move(copy));

    // Sort longest extension first so multi-part extensions match before
    // shorter ones (e.g. ".scene.json" before ".json").
    std::sort(m_types.begin(), m_types.end(),
              [](const AssetTypeInfo& a, const AssetTypeInfo& b) {
                  return a.extension.size() > b.extension.size();
              });
}

void AssetTypeRegistry::Unregister(const std::string& extension)
{
    std::string extLower = ToLower(extension);
    std::erase_if(m_types, [&](const AssetTypeInfo& t) {
        return ToLower(t.extension) == extLower;
    });
}

void AssetTypeRegistry::Clear()
{
    m_types.clear();
}

const AssetTypeInfo* AssetTypeRegistry::Find(const std::string& filename) const
{
    std::string nameLower = ToLower(filename);
    for (const auto& type : m_types) { // already sorted longest-first
        if (EndsWith(nameLower, type.extension)) {
            return &type;
        }
    }
    return nullptr;
}

AssetTypeInfo* AssetTypeRegistry::FindMutable(const std::string& extension)
{
    std::string extLower = ToLower(extension);
    for (auto& type : m_types) {
        if (type.extension == extLower) {
            return &type;
        }
    }
    return nullptr;
}

bool AssetTypeRegistry::IsRegisteredAsset(const std::string& filename) const
{
    return Find(filename) != nullptr;
}

std::string AssetTypeRegistry::ToLower(const std::string& str) const
{
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

bool AssetTypeRegistry::EndsWith(const std::string& str, const std::string& suffix) const
{
    if (suffix.size() > str.size()) return false;
    return str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// ═════════════════════════════════════════════════════════════════════════════
// AssetBrowser
// ═════════════════════════════════════════════════════════════════════════════

AssetBrowser::AssetBrowser(const std::string& rootPath)
    : m_rootPath(rootPath)
{
    std::memset(m_filterBuffer, 0, sizeof(m_filterBuffer));
}

// ── IImGuiVisualizer ────────────────────────────────────────────────────────

void AssetBrowser::Initialize()
{
    m_fileSystem = Core::CoreSystem::GetFileSystemManager();

    if (m_rootPath.empty() && m_fileSystem) {
        auto result = m_fileSystem->GetCurrentDirectory();
        if (result.IsSuccess()) {
            m_rootPath = result.GetValue();
            m_rootPath = m_rootPath + "/assets";
        }
    }

    m_selectedFolder    = m_rootPath;
    m_treeNeedsRebuild  = true;
    m_needsRefresh      = true;

    // Initialize type filter visibility from existing registrations
    for (const auto& type : m_registry.GetAll()) {
        m_typeFilterVisible[type.extension] = type.visibleByDefault;
    }    
}



void AssetBrowser::Shutdown()
{
    m_assets.clear();
    m_rootNode = {};
}

void AssetBrowser::Update(float /*deltaTime*/)
{
    if (m_treeNeedsRebuild) {
        RebuildFolderTree();
        m_treeNeedsRebuild = false;
    }
    if (m_needsRefresh) {
        RefreshAssetList();
        m_needsRefresh = false;
    }
}

bool AssetBrowser::Render(bool* isOpen)
{
    if (!ImGui::Begin(GetName(), isOpen, ImGuiWindowFlags_None)) {
        ImGui::End();
        return false;
    }

    // ── Top bar: filter + options ───────────────────────────────────────
    RenderFilterBar();

    ImGui::Separator();

    // ── Splitter layout ─────────────────────────────────────────────────
    float availWidth = ImGui::GetContentRegionAvail().x;
    float availHeight = ImGui::GetContentRegionAvail().y;

    // Left pane: folder tree
    ImGui::BeginChild("##FolderTree", ImVec2(m_treePanelWidth, availHeight), true);
    RenderFolderTree();
    ImGui::EndChild();

    ImGui::SameLine();

    // Splitter handle
    ImGui::Button("##Splitter", ImVec2(4.0f, availHeight));
    if (ImGui::IsItemActive()) {
        m_treePanelWidth += ImGui::GetIO().MouseDelta.x;
        if (m_treePanelWidth < 100.0f) m_treePanelWidth = 100.0f;
        if (m_treePanelWidth > availWidth - 100.0f) m_treePanelWidth = availWidth - 100.0f;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }

    ImGui::SameLine();

    // Right pane: asset list
    ImGui::BeginChild("##AssetList", ImVec2(0, availHeight), true);
    RenderAssetList();
    ImGui::EndChild();

    ImGui::End();

    // Render create asset dialog if active
    RenderCreateAssetDialog();

    // Render rename asset dialog if active
    RenderRenameAssetDialog();

    return true;
}

const char* AssetBrowser::GetName() const
{
    return "Asset Browser";
}

const char* AssetBrowser::GetShortcut() const
{
    return nullptr;
}

const char* AssetBrowser::GetMenuCategory() const
{
    return "Show";
}

// ── Configuration ───────────────────────────────────────────────────────────

void AssetBrowser::SetRootPath(const std::string& path)
{
    m_rootPath          = path;
    m_selectedFolder    = path;
    m_treeNeedsRebuild  = true;
    m_needsRefresh      = true;
}

// ── Tree construction ───────────────────────────────────────────────────────

void AssetBrowser::RebuildFolderTree()
{
    m_rootNode = {};
    m_rootNode.name     = m_rootPath;
    m_rootNode.fullPath = m_rootPath;
    m_rootNode.expanded = true;

    BuildFolderTreeRecursive(m_rootNode);
}

void AssetBrowser::BuildFolderTreeRecursive(FolderNode& node)
{
    if (!m_fileSystem) return;

    auto dirResult = m_fileSystem->OpenDirectory(node.fullPath);
    if (!dirResult.IsSuccess()) return;

    auto& dir = dirResult.GetValue();
    auto subDirsResult = dir->GetDirectories();
    if (!subDirsResult.IsSuccess()) return;

    for (const auto& info : subDirsResult.GetValue()) {
        FolderNode child;
        child.name     = info.name;
        child.fullPath = info.fullPath;
        node.children.push_back(std::move(child));
    }

    // Sort children alphabetically
    std::sort(node.children.begin(), node.children.end(),
              [](const FolderNode& a, const FolderNode& b) {
                  return a.name < b.name;
              });

    // Recurse into children
    for (auto& child : node.children) {
        BuildFolderTreeRecursive(child);
    }
}

// ── Asset list ──────────────────────────────────────────────────────────────

void AssetBrowser::RefreshAssetList()
{
    m_assets.clear();
    if (!m_fileSystem || m_selectedFolder.empty()) return;

    auto dirResult = m_fileSystem->OpenDirectory(m_selectedFolder);
    if (!dirResult.IsSuccess()) return;

    auto& dir = dirResult.GetValue();
    auto filesResult = dir->GetFiles();
    if (!filesResult.IsSuccess()) return;

    for (const auto& info : filesResult.GetValue()) {
        const AssetTypeInfo* typeInfo = m_registry.Find(info.name);

        // Skip non-registered files unless "Show All" is on
        if (!typeInfo && !m_showAllFiles) continue;

        AssetEntry entry;
        entry.fileName = info.name;
        entry.fullPath = info.fullPath;
        entry.size     = info.size;
        entry.typeInfo = typeInfo;
        m_assets.push_back(std::move(entry));
    }

    // Sort alphabetically
    std::sort(m_assets.begin(), m_assets.end(),
              [](const AssetEntry& a, const AssetEntry& b) {
                  return a.fileName < b.fileName;
              });
}

// ── Rendering helpers ───────────────────────────────────────────────────────

void AssetBrowser::RenderFilterBar()
{
    // Text filter
    ImGui::SetNextItemWidth(200.0f);
    if (ImGui::InputTextWithHint("##Filter", "Filter assets...", m_filterBuffer, sizeof(m_filterBuffer))) {
        // filtering is applied dynamically in RenderAssetList
    }

    ImGui::SameLine();

    // Show All Files toggle
    if (ImGui::Checkbox("Show All Files", &m_showAllFiles)) {
        m_needsRefresh = true;
    }

    ImGui::SameLine();

    // Refresh button
    if (ImGui::Button("Refresh")) {
        m_treeNeedsRebuild = true;
        m_needsRefresh     = true;
    }

    // Type filter combo (only when there are registered types)
    if (!m_registry.GetAll().empty()) {
        ImGui::SameLine();
        if (ImGui::BeginCombo("##TypeFilter", "Type Filter", ImGuiComboFlags_NoPreview)) {
            for (const auto& type : m_registry.GetAll()) {
                bool visible = true;
                auto it = m_typeFilterVisible.find(type.extension);
                if (it != m_typeFilterVisible.end()) {
                    visible = it->second;
                }
                if (ImGui::Checkbox(type.displayName.c_str(), &visible)) {
                    m_typeFilterVisible[type.extension] = visible;
                }
            }
            ImGui::EndCombo();
        }
    }
}

void AssetBrowser::RenderFolderTree()
{
    if (m_rootNode.fullPath.empty()) {
        ImGui::TextDisabled("No root path set.");
        return;
    }
    RenderFolderNode(m_rootNode);
}

void AssetBrowser::RenderFolderNode(FolderNode& node)
{
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

    if (node.children.empty()) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }
    if (node.fullPath == m_selectedFolder) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    if (node.expanded && !node.children.empty()) {
        flags |= ImGuiTreeNodeFlags_DefaultOpen;
    }

    bool opened = ImGui::TreeNodeEx(node.name.c_str(), flags);

    // Track if we manually toggled the state via double-click
    bool manualToggle = false;

    // Selection handling — select on click, even if the node was toggled open/closed
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        if (m_selectedFolder != node.fullPath) {
            m_selectedFolder = node.fullPath;
            m_needsRefresh   = true;
        }
    }

    // Double-click to toggle folder expansion
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        if (!node.children.empty()) {
            node.expanded = !node.expanded;
            manualToggle = true;
        }
    }

    // Right-click context menu
    RenderFolderContextMenu(node);

    // Drop target for moving assets
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_MOVE")) {
            const char* assetPath = static_cast<const char*>(payload->Data);
            MoveAsset(assetPath, node.fullPath);
        }
        ImGui::EndDragDropTarget();
    }

    if (opened && !node.children.empty()) {
        // Only sync state if not manually toggled
        if (!manualToggle) {
            node.expanded = true;
        }
        for (auto& child : node.children) {
            RenderFolderNode(child);
        }
        ImGui::TreePop();
    }
    else if (!opened) {
        // Only sync state if not manually toggled
        if (!manualToggle) {
            node.expanded = false;
        }
    }
}

void AssetBrowser::RenderAssetList()
{
    if (m_selectedFolder.empty()) {
        ImGui::TextDisabled("Select a folder.");
        return;
    }

    ImGui::Text("Folder: %s", m_selectedFolder.c_str());
    ImGui::Separator();

    if (m_assets.empty()) {
        ImGui::TextDisabled("No assets in this folder.");
        return;
    }

    // Build a lowercase copy of the filter once
    std::string filterLower;
    if (m_filterBuffer[0] != '\0') {
        filterLower = m_filterBuffer;
        std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    }

    // Table layout: Icon | Name | Type | Size
    if (ImGui::BeginTable("##AssetTable", 4,
            ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_Sortable  | ImGuiTableFlags_ScrollY)) {

        ImGui::TableSetupColumn("",     ImGuiTableColumnFlags_WidthFixed, 24.0f); // icon
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableHeadersRow();

        for (const auto& asset : m_assets) {
            // Text filter
            if (!filterLower.empty()) {
                std::string nameLower = asset.fileName;
                std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (nameLower.find(filterLower) == std::string::npos) {
                    continue;
                }
            }

            // Type visibility filter
            if (asset.typeInfo) {
                auto it = m_typeFilterVisible.find(asset.typeInfo->extension);
                if (it != m_typeFilterVisible.end() && !it->second) {
                    continue;
                }
            }

            ImGui::TableNextRow();

            // Column 0: Icon
            ImGui::TableSetColumnIndex(0);
            if (asset.typeInfo && !asset.typeInfo->icon.empty()) {
                ImGui::TextUnformatted(asset.typeInfo->icon.c_str());
            }

            // Column 1: File name (colored if type is known) — selectable for interactions
            ImGui::TableSetColumnIndex(1);
            {
                ImGuiSelectableFlags selFlags = ImGuiSelectableFlags_SpanAllColumns
                                              | ImGuiSelectableFlags_AllowDoubleClick;
                bool selected = false;

                if (asset.typeInfo) {
                    ImGui::PushStyleColor(ImGuiCol_Text, asset.typeInfo->color);
                    selected = ImGui::Selectable(asset.fileName.c_str(), false, selFlags);
                    ImGui::PopStyleColor();
                }
                else {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                    selected = ImGui::Selectable(asset.fileName.c_str(), false, selFlags);
                    ImGui::PopStyleColor();
                }

                // Double-click: launch primary action
                if (selected && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    if (const auto* launchOption = GetEffectiveLaunchOption(asset)) {
                        LaunchAsset(asset, *launchOption);
                    }
                }

                // Drag source for moving assets
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                    // Set payload to carry the asset full path
                    ImGui::SetDragDropPayload("ASSET_MOVE", asset.fullPath.c_str(), asset.fullPath.size() + 1);
                    ImGui::Text("Move: %s", asset.fileName.c_str());
                    ImGui::EndDragDropSource();
                }

                // Right-click: context menu
                RenderAssetContextMenu(asset);
            }

            // Column 2: Type display name
            ImGui::TableSetColumnIndex(2);
            if (asset.typeInfo) {
                ImGui::TextUnformatted(asset.typeInfo->displayName.c_str());
            }
            else {
                ImGui::TextDisabled("Unknown");
            }

            // Column 3: Size
            ImGui::TableSetColumnIndex(3);
            ImGui::TextUnformatted(FormatFileSize(asset.size).c_str());
        }

        ImGui::EndTable();
    }
}

void AssetBrowser::RenderAssetContextMenu(const AssetEntry& asset)
{
    // Use the file path as a stable ID for the popup
    ImGui::PushID(asset.fullPath.c_str());

    if (ImGui::BeginPopupContextItem("##AssetCtx")) {
        // Get the effective launch option (primary or first secondary)
        const AssetLaunchOption* effectiveLaunch = GetEffectiveLaunchOption(asset);
        
        // Show effective launch option at the top (if available)
        if (effectiveLaunch) {
            std::string primaryLabel = effectiveLaunch->label + " (Default)";
            if (ImGui::MenuItem(primaryLabel.c_str())) {
                LaunchAsset(asset, *effectiveLaunch);
            }
            ImGui::Separator();
        }

        // "Open With" submenu containing remaining secondary options
        if (asset.typeInfo && !asset.typeInfo->secondaryLaunches.empty()) {
            if (ImGui::BeginMenu("Open With")) {
                for (const auto& option : asset.typeInfo->secondaryLaunches) {
                    // Skip the option if it's already shown as the default
                    if (effectiveLaunch && &option == effectiveLaunch) {
                        continue;
                    }
                    if (ImGui::MenuItem(option.label.c_str())) {
                        LaunchAsset(asset, option);
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();
        }

        // Always available: open with OS default
        if (ImGui::MenuItem("Open with System Default")) {
            AssetLaunchOption sysDefault;
            sysDefault.mode = AssetLaunchMode::External;
            LaunchAsset(asset, sysDefault);
        }

        ImGui::Separator();

        // Rename asset
        if (ImGui::MenuItem("Rename...")) {
            OpenRenameAssetDialog(asset);
        }

        // Delete asset
        if (ImGui::MenuItem("Delete")) {
            DeleteAsset(asset.fullPath);
        }

        ImGui::EndPopup();
    }

    ImGui::PopID();
}

void AssetBrowser::RenderFolderContextMenu(FolderNode& node)
{
    ImGui::PushID(node.fullPath.c_str());

    if (ImGui::BeginPopupContextItem("##FolderCtx")) {
        if (ImGui::MenuItem("New Asset...")) {
            OpenCreateAssetDialog(node.fullPath);
        }
        ImGui::EndPopup();
    }

    ImGui::PopID();
}

// ── Launch helpers ──────────────────────────────────────────────────────────

const AssetLaunchOption* AssetBrowser::GetEffectiveLaunchOption(const AssetEntry& asset) const
{
    if (!asset.typeInfo) {
        return nullptr;
    }

    // Use primary if it has a handler or is external
    if (!asset.typeInfo->primaryLaunch.label.empty() &&
        (asset.typeInfo->primaryLaunch.internalHandler || 
         asset.typeInfo->primaryLaunch.mode == AssetLaunchMode::External)) {
        return &asset.typeInfo->primaryLaunch;
    }

    // Otherwise use first secondary option if available
    if (!asset.typeInfo->secondaryLaunches.empty()) {
        return &asset.typeInfo->secondaryLaunches[0];
    }

    return nullptr;
}

void AssetBrowser::LaunchAsset(const AssetEntry& asset, const AssetLaunchOption& option)
{
    switch (option.mode) {
    case AssetLaunchMode::Internal:
        if (option.internalHandler) {
            option.internalHandler(asset.fullPath);
        }
        break;

    case AssetLaunchMode::External:
        LaunchExternal(asset.fullPath, option.externalProgram);
        break;
    }
}

void AssetBrowser::LaunchExternal(const std::string& assetPath, const std::string& program)
{
#ifdef _WIN32
    if (program.empty()) {
        // Open with the OS default application
        ShellExecuteA(nullptr, "open", assetPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
    else {
        // Open with a specific external program
        ShellExecuteA(nullptr, "open", program.c_str(), assetPath.c_str(), nullptr, SW_SHOWNORMAL);
    }
#else
    // Fallback for non-Windows platforms
    std::string command;
    if (program.empty()) {
        command = "xdg-open \"" + assetPath + "\" &";
    }
    else {
        command = "\"" + program + "\" \"" + assetPath + "\" &";
    }
    std::system(command.c_str());
#endif
}

// ── Asset Creation ──────────────────────────────────────────────────────────

void AssetBrowser::OpenCreateAssetDialog(const std::string& folderPath)
{
    m_showCreateAssetDialog = true;
    m_createAssetFolder = folderPath;
    std::memset(m_createAssetNameBuffer, 0, sizeof(m_createAssetNameBuffer));
    m_selectedAssetTypeIndex = 0;
}

void AssetBrowser::RenderCreateAssetDialog()
{
    if (!m_showCreateAssetDialog) {
        return;
    }

    ImGui::OpenPopup("Create New Asset");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Create New Asset", &m_showCreateAssetDialog, 
                                ImGuiWindowFlags_AlwaysAutoResize)) {

        const auto& allTypes = m_registry.GetAll();

        if (allTypes.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "No asset types registered!");
            ImGui::Separator();
            if (ImGui::Button("Close", ImVec2(120, 0))) {
                m_showCreateAssetDialog = false;
            }
            ImGui::EndPopup();
            return;
        }

        ImGui::Text("Folder: %s", m_createAssetFolder.c_str());
        ImGui::Separator();

        // Asset type selection
        ImGui::Text("Asset Type:");
        const char* currentTypeName = (m_selectedAssetTypeIndex >= 0 && m_selectedAssetTypeIndex < static_cast<int>(allTypes.size()))
            ? allTypes[m_selectedAssetTypeIndex].displayName.c_str()
            : "";

        if (ImGui::BeginCombo("##AssetType", currentTypeName)) {
            for (int i = 0; i < static_cast<int>(allTypes.size()); ++i) {
                bool isSelected = (m_selectedAssetTypeIndex == i);
                if (ImGui::Selectable(allTypes[i].displayName.c_str(), isSelected)) {
                    m_selectedAssetTypeIndex = i;
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        // Asset name input
        ImGui::Text("Asset Name:");
        ImGui::SetNextItemWidth(300.0f);
        ImGui::InputTextWithHint("##AssetName", "Enter asset name...", 
                                  m_createAssetNameBuffer, sizeof(m_createAssetNameBuffer));

        ImGui::Separator();

        // Buttons
        bool canCreate = (m_createAssetNameBuffer[0] != '\0') && 
                         (m_selectedAssetTypeIndex >= 0 && m_selectedAssetTypeIndex < static_cast<int>(allTypes.size()));

        if (!canCreate) {
            ImGui::BeginDisabled();
        }

        if (ImGui::Button("Create", ImVec2(120, 0))) {
            const AssetTypeInfo* selectedType = &allTypes[m_selectedAssetTypeIndex];
            std::string assetName = m_createAssetNameBuffer;

            if (CreateAssetFromTemplate(m_createAssetFolder, assetName, selectedType)) {
                m_showCreateAssetDialog = false;
                m_needsRefresh = true;
            }
        }

        if (!canCreate) {
            ImGui::EndDisabled();
        }

        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_showCreateAssetDialog = false;
        }

        ImGui::EndPopup();
    }
}

bool AssetBrowser::CreateAssetFromTemplate(const std::string& folderPath, 
                                           const std::string& assetName, 
                                           const AssetTypeInfo* typeInfo)
{
    if (!m_fileSystem || !typeInfo) {
        return false;
    }

    // Find the template file
    std::string templatePath = FindTemplateFile(typeInfo->extension);
    if (templatePath.empty()) {
        // No template found, show error message
        // For now, just return false
        return false;
    }

    // Construct the destination path
    std::string destFileName = assetName + typeInfo->extension;
    std::string destPath = folderPath;
    if (!destPath.empty() && destPath.back() != '/' && destPath.back() != '\\') {
        destPath += "/";
    }
    destPath += destFileName;

    // Copy the template to the destination
    auto result = m_fileSystem->CopyFile(templatePath, destPath);
    if (!result.IsSuccess()) {
        return false;
    }

    return true;
}

std::string AssetBrowser::FindTemplateFile(const std::string& extension) const
{
    if (!m_fileSystem) {
        return "";
    }

    // Template file name format: "template" + extension
    // For example: "template.scene.json" for ".scene.json"
    std::string templateFileName = "template" + extension;
    std::string templatePath = m_templateAssetsPath;
    if (!templatePath.empty() && templatePath.back() != '/' && templatePath.back() != '\\') {
        templatePath += "/";
    }
    templatePath += templateFileName;

    // Check if the template file exists
    if (m_fileSystem->Exists(templatePath) && m_fileSystem->IsFile(templatePath)) {
        return templatePath;
    }

    return "";
}

// ── Asset Operations ────────────────────────────────────────────────────────

void AssetBrowser::OpenRenameAssetDialog(const AssetEntry& asset)
{
    m_showRenameAssetDialog = true;
    m_renameAssetPath = asset.fullPath;
    m_renameAssetOldName = asset.fileName;

    // Pre-fill the buffer with the current name (without extension)
    std::string nameWithoutExt = asset.fileName;
    if (asset.typeInfo) {
        size_t extPos = nameWithoutExt.rfind(asset.typeInfo->extension);
        if (extPos != std::string::npos) {
            nameWithoutExt = nameWithoutExt.substr(0, extPos);
        }
    }

    strncpy_s(m_renameAssetNameBuffer, sizeof(m_renameAssetNameBuffer), nameWithoutExt.c_str(), _TRUNCATE);

    m_renameAssetNameBuffer[sizeof(m_renameAssetNameBuffer) - 1] = '\0';
}

void AssetBrowser::RenderRenameAssetDialog()
{
    if (!m_showRenameAssetDialog) {
        return;
    }

    ImGui::OpenPopup("Rename Asset");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Rename Asset", &m_showRenameAssetDialog, 
                                ImGuiWindowFlags_AlwaysAutoResize)) {

        ImGui::Text("Current name: %s", m_renameAssetOldName.c_str());
        ImGui::Separator();

        // Asset name input
        ImGui::Text("New Name:");
        ImGui::SetNextItemWidth(300.0f);
        ImGui::InputTextWithHint("##NewAssetName", "Enter new name...", 
                                  m_renameAssetNameBuffer, sizeof(m_renameAssetNameBuffer));

        ImGui::Separator();

        // Buttons
        bool canRename = (m_renameAssetNameBuffer[0] != '\0');

        if (!canRename) {
            ImGui::BeginDisabled();
        }

        if (ImGui::Button("Rename", ImVec2(120, 0))) {
            std::string newName = m_renameAssetNameBuffer;
            if (RenameAsset(m_renameAssetPath, newName)) {
                m_showRenameAssetDialog = false;
                m_needsRefresh = true;
            }
        }

        if (!canRename) {
            ImGui::EndDisabled();
        }

        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_showRenameAssetDialog = false;
        }

        ImGui::EndPopup();
    }
}

bool AssetBrowser::RenameAsset(const std::string& oldPath, const std::string& newName)
{
    if (!m_fileSystem) {
        return false;
    }

    // Extract the directory from the old path
    size_t lastSlash = oldPath.find_last_of("/\\");
    std::string directory = (lastSlash != std::string::npos) ? oldPath.substr(0, lastSlash + 1) : "";

    // Extract the filename from the old path
    std::string oldFileName = (lastSlash != std::string::npos) ? oldPath.substr(lastSlash + 1) : oldPath;

    // Get the full extension (everything from the first dot onwards)
    // This preserves multi-part extensions like .obj.json
    std::string extension;
    size_t firstDot = oldFileName.find('.');
    if (firstDot != std::string::npos) {
        extension = oldFileName.substr(firstDot);
    }

    // Construct the new path
    std::string newPath = directory + newName + extension;

    // Check if the new path already exists
    if (m_fileSystem->Exists(newPath)) {
        // Could show an error message here
        return false;
    }

    // Rename (move) the file
    auto result = m_fileSystem->MoveFile(oldPath, newPath);
    return result.IsSuccess();
}

bool AssetBrowser::DeleteAsset(const std::string& assetPath)
{
    if (!m_fileSystem) {
        return false;
    }

    // Delete the file
    auto result = m_fileSystem->DeleteFile(assetPath);
    if (result.IsSuccess()) {
        m_needsRefresh = true;
        return true;
    }
    return false;
}

bool AssetBrowser::MoveAsset(const std::string& assetPath, const std::string& targetFolder)
{
    if (!m_fileSystem) {
        return false;
    }

    // Extract the filename from the asset path
    size_t lastSlash = assetPath.find_last_of("/\\");
    std::string fileName = (lastSlash != std::string::npos) ? assetPath.substr(lastSlash + 1) : assetPath;

    // Construct the new path
    std::string newPath = targetFolder;
    if (!newPath.empty() && newPath.back() != '/' && newPath.back() != '\\') {
        newPath += "/";
    }
    newPath += fileName;

    // Check if source and destination are the same
    if (assetPath == newPath) {
        return false;
    }

    // Check if the destination already exists
    if (m_fileSystem->Exists(newPath)) {
        // Could show an error message here
        return false;
    }

    // Move the file
    auto result = m_fileSystem->MoveFile(assetPath, newPath);
    if (result.IsSuccess()) {
        m_needsRefresh = true;
        return true;
    }
    return false;
}

// ── Utility ─────────────────────────────────────────────────────────────────

std::string AssetBrowser::FormatFileSize(uint64_t bytes) const
{
    const char* units[] = { "B", "KB", "MB", "GB" };
    int idx = 0;
    double size = static_cast<double>(bytes);
    while (size >= 1024.0 && idx < 3) {
        size /= 1024.0;
        ++idx;
    }
    std::ostringstream oss;
    if (idx == 0) {
        oss << bytes << " B";
    }
    else {
        oss << std::fixed << std::setprecision(1) << size << " " << units[idx];
    }
    return oss.str();
}

} // namespace ImGuiVisualizers