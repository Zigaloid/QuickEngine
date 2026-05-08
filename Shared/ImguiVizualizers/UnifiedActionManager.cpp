#include "UnifiedActionManager.h"
#include "CommandConsole.h"
#include "KeyboardShortcutManager.h"
#include "imgui.h"

#include <algorithm>
#include <sstream>
#include <cctype>

namespace UI {

    // -------------------------------------------------------------------------
    // Construction
    // -------------------------------------------------------------------------

    UnifiedActionManager::UnifiedActionManager()
    {
        m_root.segment = "";
        m_root.fullPath = "";
    }

    // -------------------------------------------------------------------------
    // Registration
    // -------------------------------------------------------------------------
    void UnifiedActionManager::RegisterAction(const ActionDescriptor& descriptor)
    {
        m_actions[descriptor.path] = descriptor;
        m_treeDirty = true;
    }

    void UnifiedActionManager::UnregisterAction(const std::string& path)
    {
        if (m_actions.erase(path) > 0) {
            m_treeDirty = true;
        }
    }

    void UnifiedActionManager::ClearAllActions()
    {
        m_actions.clear();
        m_root.children.clear();
        m_treeDirty = true;
    }

    // -------------------------------------------------------------------------
    // Execution
    // -------------------------------------------------------------------------

    bool UnifiedActionManager::ExecuteAction(const std::string& path)
    {
        // Try exact match first
        auto it = m_actions.find(path);

        // If not found, try case-insensitive match
        if (it == m_actions.end()) {
            std::string lowerPath = ToLower(path);
            for (auto& [key, desc] : m_actions) {
                if (ToLower(key) == lowerPath) {
                    it = m_actions.find(key);
                    break;
                }
            }
        }

        if (it == m_actions.end()) {
            return false;
        }

        const auto& action = it->second;

        // Check enabled state
        if (action.isEnabled && !action.isEnabled()) {
            return false;
        }

        if (action.callback) {
            action.callback();
            return true;
        }

        return false;
    }

    // -------------------------------------------------------------------------
    // Queries
    // -------------------------------------------------------------------------

    const ActionDescriptor* UnifiedActionManager::FindAction(const std::string& path) const
    {
        auto it = m_actions.find(path);
        return (it != m_actions.end()) ? &it->second : nullptr;
    }

    std::vector<const ActionDescriptor*> UnifiedActionManager::GetAllActions() const
    {
        std::vector<const ActionDescriptor*> result;
        result.reserve(m_actions.size());
        for (const auto& [path, desc] : m_actions) {
            result.push_back(&desc);
        }
        return result;
    }

    std::vector<std::string> UnifiedActionManager::GetSuggestions(const std::string& prefix) const
    {
        std::vector<std::string> results;
        std::string lowerPrefix = ToLower(prefix);

        for (const auto& [path, desc] : m_actions) {
            if (!HasTarget(desc.targets, ActionTarget::Console)) {
                continue;
            }

            std::string lowerPath = ToLower(path);
            if (lowerPath.find(lowerPrefix) == 0) {
                results.push_back(path);
            }
        }

        std::sort(results.begin(), results.end());
        return results;
    }

    size_t UnifiedActionManager::GetActionCount() const
    {
        return m_actions.size();
    }

    // -------------------------------------------------------------------------
    // Rendering — Menu Bar
    // -------------------------------------------------------------------------

    void UnifiedActionManager::RenderMenuBar()
    {
        if (m_treeDirty) {
            RebuildTree();
        }

        for (const auto& child : m_root.children) {
            RenderMenuNode(*child);
        }
    }

    void UnifiedActionManager::RenderMenuNode(const ActionNode& node)
    {
        // Leaf node with an action — render as a menu item
        if (node.action != nullptr) {
            if (!HasTarget(node.action->targets, ActionTarget::Menu)) {
                return;
            }

            bool enabled = true;
            if (node.action->isEnabled) {
                enabled = node.action->isEnabled();
            }

            // Toggle item (has isChecked)
            if (node.action->isChecked) {
                bool checked = node.action->isChecked();
                if (ImGui::MenuItem(node.segment.c_str(),
                                    node.action->shortcutHint.empty() ? nullptr : node.action->shortcutHint.c_str(),
                                    &checked,
                                    enabled)) {
                    if (node.action->callback) {
                        node.action->callback();
                    }
                }
            }
            else {
                // Simple action item
                if (ImGui::MenuItem(node.segment.c_str(),
                                    node.action->shortcutHint.empty() ? nullptr : node.action->shortcutHint.c_str(),
                                    false,
                                    enabled)) {
                    if (node.action->callback) {
                        node.action->callback();
                    }
                }
            }
            return;
        }

        // Branch node — render as a sub-menu
        if (ImGui::BeginMenu(node.segment.c_str())) {
            for (const auto& child : node.children) {
                RenderMenuNode(*child);
            }
            ImGui::EndMenu();
        }
    }

    // -------------------------------------------------------------------------
    // Rendering — Toolbar
    // -------------------------------------------------------------------------

    void UnifiedActionManager::RenderToolbar()
    {
        if (m_treeDirty) {
            RebuildTree();
        }

        for (const auto& child : m_root.children) {
            RenderToolbarNode(*child);
        }
    }

    void UnifiedActionManager::RenderToolbarNode(const ActionNode& node)
    {
        // Leaf node — render as a toolbar button
        if (node.action != nullptr) {
            if (!HasTarget(node.action->targets, ActionTarget::Toolbar)) {
                return;
            }

            bool enabled = true;
            if (node.action->isEnabled) {
                enabled = node.action->isEnabled();
            }

            // Use checked state to show active/pressed style
            bool isActive = node.action->isChecked ? node.action->isChecked() : false;

            // Dim disabled buttons via alpha
            if (!enabled) {
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
            }
            if (isActive) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            }

            // Build tooltip from description + shortcut
            std::string tooltip = node.action->description;
            if (!node.action->shortcutHint.empty()) {
                tooltip += " (" + node.action->shortcutHint + ")";
            }

            if (ImGui::Button(node.segment.c_str()) && enabled) {
                if (node.action->callback) {
                    node.action->callback();
                }
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && !tooltip.empty()) {
                ImGui::SetTooltip("%s", tooltip.c_str());
            }

            if (isActive) {
                ImGui::PopStyleColor();
            }
            if (!enabled) {
                ImGui::PopStyleVar();
            }

            ImGui::SameLine();
            return;
        }

        // Branch node — recurse into children unconditionally
        for (const auto& child : node.children) {
            RenderToolbarNode(*child);
        }
    }

    // -------------------------------------------------------------------------
    // Integration — Console
    // -------------------------------------------------------------------------

    void UnifiedActionManager::SyncToConsole(CommandConsole& console)
    {
        for (const auto& [path, desc] : m_actions) {
            if (HasTarget(desc.targets, ActionTarget::Console)) {
                console.RegisterCommand(path, desc.description);
            }
        }

        // Install a command callback that routes typed paths through ExecuteAction.
        // The console will try this callback for commands not handled by built-ins.
        console.SetCommandCallback([this](const std::string& commandLine) -> bool {
            return ExecuteAction(commandLine);
        });
    }

    // -------------------------------------------------------------------------
    // Integration — Keyboard Shortcuts
    // -------------------------------------------------------------------------

    void UnifiedActionManager::SyncToKeyboardShortcuts(Input::KeyboardShortcutManager& ksm)
    {
        for (const auto& [path, desc] : m_actions) {
            if (desc.shortcutHint.empty() || !desc.callback) {
                continue;
            }

            // Parse shortcut hint string like "Ctrl+H", "Ctrl+Shift+S", "F5"
            // into KeyModifier + key code for KeyboardShortcutManager.
            //
            // This is a best-effort parser for common patterns. Keys are mapped
            // to GLFW key codes.

            Input::KeyModifier mods = Input::KeyModifier::None;
            int keyCode = 0;

            std::string hint = desc.shortcutHint;
            // Tokenize on '+'
            std::vector<std::string> tokens;
            std::istringstream stream(hint);
            std::string token;
            while (std::getline(stream, token, '+')) {
                // Trim whitespace
                size_t start = token.find_first_not_of(" \t");
                size_t end = token.find_last_not_of(" \t");
                if (start != std::string::npos) {
                    tokens.push_back(token.substr(start, end - start + 1));
                }
            }

            for (size_t i = 0; i < tokens.size(); ++i) {
                std::string t = ToLower(tokens[i]);

                if (t == "ctrl") {
                    mods = mods | Input::KeyModifier::Ctrl;
                }
                else if (t == "shift") {
                    mods = mods | Input::KeyModifier::Shift;
                }
                else if (t == "alt") {
                    mods = mods | Input::KeyModifier::Alt;
                }
                else if (t == "super" || t == "win") {
                    mods = mods | Input::KeyModifier::Super;
                }
                else {
                    // Assume this is the key itself
                    // Single letter A-Z
                    if (t.size() == 1 && t[0] >= 'a' && t[0] <= 'z') {
                        keyCode = 'A' + (t[0] - 'a'); // GLFW uses uppercase ASCII
                    }
                    // Function keys F1-F12
                    else if (t.size() >= 2 && t[0] == 'f' && std::isdigit(t[1])) {
                        int fNum = std::stoi(t.substr(1));
                        if (fNum >= 1 && fNum <= 12) {
                            keyCode = 289 + (fNum - 1); // GLFW_KEY_F1 = 290, but 289+1=290
                        }
                    }
                }
            }

            if (keyCode != 0) {
                ksm.RegisterShortcut(path, desc.description, static_cast<ImGuiKey>(keyCode), mods, desc.callback);
            }
        }
    }

    // -------------------------------------------------------------------------
    // Tree building
    // -------------------------------------------------------------------------

    void UnifiedActionManager::RebuildTree()
    {
        m_root.children.clear();

        for (auto& [path, desc] : m_actions) {
            InsertIntoTree(desc);
        }

        SortChildren(m_root);
        m_treeDirty = false;
    }

    void UnifiedActionManager::InsertIntoTree(ActionDescriptor& descriptor)
    {
        std::vector<std::string> segments = SplitPath(descriptor.path);
        if (segments.empty()) {
            return;
        }

        ActionNode* current = &m_root;
        std::string builtPath;

        for (size_t i = 0; i < segments.size(); ++i) {
            if (!builtPath.empty()) {
                builtPath += ".";
            }
            builtPath += segments[i];

            bool isLeaf = (i == segments.size() - 1);

            // Find existing child
            ActionNode* found = nullptr;
            for (auto& child : current->children) {
                if (child->segment == segments[i]) {
                    found = child.get();
                    break;
                }
            }

            if (!found) {
                auto newNode = std::make_unique<ActionNode>();
                newNode->segment = segments[i];
                newNode->fullPath = builtPath;

                if (isLeaf) {
                    newNode->action = &descriptor;
                    newNode->sortPriority = descriptor.sortPriority;
                }

                found = newNode.get();
                current->children.push_back(std::move(newNode));
            }
            else if (isLeaf) {
                // Update existing leaf
                found->action = &descriptor;
                found->sortPriority = descriptor.sortPriority;
            }

            current = found;
        }
    }

    void UnifiedActionManager::SortChildren(ActionNode& node)
    {
        std::sort(node.children.begin(), node.children.end(),
            [](const std::unique_ptr<ActionNode>& a, const std::unique_ptr<ActionNode>& b) {
                if (a->sortPriority != b->sortPriority) {
                    return a->sortPriority < b->sortPriority;
                }
                return a->segment < b->segment;
            });

        for (auto& child : node.children) {
            SortChildren(*child);
        }
    }

    // -------------------------------------------------------------------------
    // Utilities
    // -------------------------------------------------------------------------

    std::vector<std::string> UnifiedActionManager::SplitPath(const std::string& path)
    {
        std::vector<std::string> segments;
        std::istringstream stream(path);
        std::string segment;
        while (std::getline(stream, segment, '.')) {
            if (!segment.empty()) {
                segments.push_back(segment);
            }
        }
        return segments;
    }

    std::string UnifiedActionManager::ToLower(const std::string& str)
    {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return result;
    }

} // namespace UI