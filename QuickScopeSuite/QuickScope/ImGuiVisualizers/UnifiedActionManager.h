#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <cstdint>

// Forward declarations
class CommandConsole;
namespace Input { class KeyboardShortcutManager; }

namespace UI {

    // -------------------------------------------------------------------------
    // Flags controlling where an action appears
    // -------------------------------------------------------------------------
    enum class ActionTarget : uint8_t {
        None    = 0,
        Menu    = 1 << 0,
        Toolbar = 1 << 1,
        Console = 1 << 2,
        All     = Menu | Toolbar | Console
    };

    inline constexpr ActionTarget operator|(ActionTarget a, ActionTarget b) {
        return static_cast<ActionTarget>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
    }
    inline constexpr ActionTarget operator&(ActionTarget a, ActionTarget b) {
        return static_cast<ActionTarget>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
    }
    inline constexpr bool HasTarget(ActionTarget flags, ActionTarget test) {
        return (flags & test) == test;
    }

    // -------------------------------------------------------------------------
    // Descriptor for a single action
    // -------------------------------------------------------------------------
    struct ActionDescriptor {
        std::string                path;          // "Owner.Category.ActionName"
        std::string                description;   // Tooltip / console help text
        std::string                shortcutHint;  // Display string, e.g. "Ctrl+H"
        std::string                iconID;        // Optional icon identifier for toolbar
        ActionTarget               targets = ActionTarget::All;
        std::function<void()>      callback;      // The action to execute
        std::function<bool()>      isEnabled;     // Optional enabled-state query (nullptr = always enabled)
        std::function<bool()>      isChecked;     // Optional checked-state query (nullptr = not a toggle)
        int                        sortPriority = 0; // Lower values appear first within a group
    };

    // -------------------------------------------------------------------------
    // Internal tree node for hierarchical menu/toolbar rendering
    // -------------------------------------------------------------------------
    struct ActionNode {
        std::string                              segment;    // This level's name segment
        std::string                              fullPath;   // Reconstructed dotted path
        ActionDescriptor*                        action = nullptr; // Non-null only on leaf nodes
        std::vector<std::unique_ptr<ActionNode>> children;
        int                                      sortPriority = 0;
    };

    // -------------------------------------------------------------------------
    // UnifiedActionManager
    //
    // Central registry for actions that can surface as menu items, toolbar
    // buttons, and/or console commands.  Actions are identified by a
    // dot-delimited path: "Owner.Category.ActionName".
    // -------------------------------------------------------------------------
    class UnifiedActionManager {
    public:
        UnifiedActionManager();
        ~UnifiedActionManager() = default;

        // -- Registration ----------------------------------------------------

        /// Register a new action. Overwrites any existing action at the same path.
        void RegisterAction(const ActionDescriptor& descriptor);

        /// Unregister an action by its dotted path.
        void UnregisterAction(const std::string& path);

        /// Remove all registered actions.
        void ClearAllActions();

        // -- Execution -------------------------------------------------------

        /// Execute the action at the given path. Returns true if found and executed.
        bool ExecuteAction(const std::string& path);

        // -- Queries ---------------------------------------------------------

        /// Find an action by its dotted path. Returns nullptr if not found.
        const ActionDescriptor* FindAction(const std::string& path) const;

        /// Get all registered action descriptors.
        std::vector<const ActionDescriptor*> GetAllActions() const;

        /// Get action paths matching a prefix (for console auto-complete).
        std::vector<std::string> GetSuggestions(const std::string& prefix) const;

        /// Get the total number of registered actions.
        size_t GetActionCount() const;

        // -- Rendering -------------------------------------------------------

        /// Render the full menu bar by walking the action tree.
        /// Call between ImGui::BeginMenuBar() / EndMenuBar().
        void RenderMenuBar();

        /// Render toolbar buttons for all Toolbar-targeted actions.
        void RenderToolbar();

        // -- Integration -----------------------------------------------------

        /// Push all Console-targeted actions into a CommandConsole and install
        /// a command callback that routes typed paths through ExecuteAction.
        void SyncToConsole(CommandConsole& console);

        /// Bind shortcutHint strings to a KeyboardShortcutManager.
        void SyncToKeyboardShortcuts(Input::KeyboardShortcutManager& ksm);

    private:
        // Flat storage keyed by dotted path
        std::unordered_map<std::string, ActionDescriptor> m_actions;

        // Tree root for hierarchical rendering
        ActionNode m_root;

        // Dirty flag — set when actions are added/removed, cleared after rebuild
        bool m_treeDirty = true;

        // -- Internal helpers ------------------------------------------------
        void RebuildTree();
        void InsertIntoTree(ActionDescriptor& descriptor);
        void RemoveFromTree(const std::string& path);
        void RenderMenuNode(const ActionNode& node);
        void RenderToolbarNode(const ActionNode& node);
        void SortChildren(ActionNode& node);

        static std::vector<std::string> SplitPath(const std::string& path);
        static std::string ToLower(const std::string& str);
    };

} // namespace UI