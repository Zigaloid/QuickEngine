#pragma once

#pragma once

#include <functional>
#include <unordered_map>
#include <string>
#include <vector>
#include <memory>
#include "imgui.h"

namespace Input {

    // Modifier flags that can be combined
    enum class KeyModifier : int {
        None = 0,
        Shift = 1 << 0,
        Ctrl = 1 << 1,
        Alt = 1 << 2,
        Super = 1 << 3,
        CapsLock = 1 << 4,
        NumLock = 1 << 5
    };

    // Bitwise operators for KeyModifier
    inline KeyModifier operator|(KeyModifier a, KeyModifier b) {
        return static_cast<KeyModifier>(static_cast<int>(a) | static_cast<int>(b));
    }

    inline KeyModifier operator&(KeyModifier a, KeyModifier b) {
        return static_cast<KeyModifier>(static_cast<int>(a) & static_cast<int>(b));
    }

    inline bool operator!=(KeyModifier a, KeyModifier b) {
        return static_cast<int>(a) != static_cast<int>(b);
    }

    inline bool operator==(KeyModifier a, int b) {
        return static_cast<int>(a) == b;
    }

    // Key action types
    enum class KeyAction {
        Press,
        Release,
        Repeat
    };

    // Shortcut key combination
    struct KeyCombination {
        ImGuiKey key;               // ImGui key code
        KeyModifier modifiers;      // Combined modifier flags
        KeyAction action;           // When to trigger

        KeyCombination(ImGuiKey k, KeyModifier mod = KeyModifier::None, KeyAction act = KeyAction::Press)
            : key(k), modifiers(mod), action(act) {}

        // Generate unique hash for this combination
        size_t getHash() const {
            return std::hash<int>()(static_cast<int>(key)) ^ 
                   (std::hash<int>()(static_cast<int>(modifiers)) << 1) ^
                   (std::hash<int>()(static_cast<int>(action)) << 2);
        }

        bool operator==(const KeyCombination& other) const {
            return key == other.key && modifiers == other.modifiers && action == other.action;
        }
    };

    // Callback function type for shortcuts
    using ShortcutCallback = std::function<void()>;

    // Shortcut registration info
    struct ShortcutInfo {
        std::string name;
        std::string description;
        KeyCombination combination;
        ShortcutCallback callback;
        bool enabled;
        // If true, this shortcut will be processed even when ImGui wants to capture keyboard input
        bool ignoreImguiCapture;

        ShortcutInfo(const std::string& n, const std::string& desc,
                    const KeyCombination& combo, ShortcutCallback cb,
                    bool ignoreCapture = false)
            : name(n), description(desc), combination(combo), callback(std::move(cb)), enabled(true), ignoreImguiCapture(ignoreCapture) {}
    };

    // Custom hash function for KeyCombination
    struct KeyCombinationHash {
        size_t operator()(const KeyCombination& combo) const {
            return combo.getHash();
        }
    };

    // Keyboard shortcut manager - singleton
    class KeyboardShortcutManager {
    public:
        // Singleton access
        static KeyboardShortcutManager& Instance();

        // Non-copyable, non-movable
        KeyboardShortcutManager(const KeyboardShortcutManager&) = delete;
        KeyboardShortcutManager& operator=(const KeyboardShortcutManager&) = delete;

        // Lifecycle
        bool Initialize();
        void Update(double deltaTime);
        void Shutdown();
        bool IsInitialized() const { return m_initialized; }

        // Shortcut registration
        bool RegisterShortcut(const std::string& name, 
                            const std::string& description,
                            const KeyCombination& combination, 
                            ShortcutCallback callback,
                            bool ignoreImguiCapture = false);

        bool RegisterShortcut(const std::string& name,
                            const std::string& description,
                            ImGuiKey key,
                            KeyModifier modifiers,
                            ShortcutCallback callback,
                            KeyAction action = KeyAction::Press,
                            bool ignoreImguiCapture = false);

        // Shortcut management
        bool UnregisterShortcut(const std::string& name);
        void UnregisterAllShortcuts();

        bool EnableShortcut(const std::string& name, bool enabled = true);
        bool DisableShortcut(const std::string& name);
        bool IsShortcutEnabled(const std::string& name) const;

        // Query shortcuts
        bool HasShortcut(const std::string& name) const;
        std::vector<std::string> GetShortcutNames() const;
        const ShortcutInfo* GetShortcutInfo(const std::string& name) const;

        // Global enable/disable
        void SetEnabled(bool enabled) { m_globalEnabled = enabled; }
        bool IsEnabled() const { return m_globalEnabled; }

        // Utility functions
        static KeyModifier GetCurrentImGuiModifiers();
        static std::string KeyCombinationToString(const KeyCombination& combo);
        static std::string ModifierToString(KeyModifier modifier);
        static std::string KeyToString(ImGuiKey key);

    private:
        KeyboardShortcutManager();
        ~KeyboardShortcutManager() = default;

        // Shortcut storage
        std::unordered_map<KeyCombination, std::shared_ptr<ShortcutInfo>, KeyCombinationHash> m_shortcuts;
        std::unordered_map<std::string, std::shared_ptr<ShortcutInfo>> m_shortcutsByName;

        // Manager state
        bool m_initialized;
        bool m_globalEnabled;

        // Helper methods
        bool IsValidKeyCombination(const KeyCombination& combination) const;
        void LogShortcutRegistration(const ShortcutInfo& shortcut);
        void ExecuteShortcut(const ShortcutInfo& shortcut);
    };

} // namespace Input