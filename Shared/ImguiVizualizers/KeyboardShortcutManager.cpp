#include "KeyboardShortcutManager.h"
#include "KeyboardShortcutManager.h"
#include <iostream>
#include <sstream>
#include <algorithm>

#include "imgui.h"

namespace Input {

    KeyboardShortcutManager& KeyboardShortcutManager::Instance() {
        // Use a leaked heap-allocated singleton to avoid static destruction
        // order issues: this ensures the manager remains valid during
        // program shutdown even if other globals are destroyed earlier.
        static KeyboardShortcutManager* instance = []() {
            return new KeyboardShortcutManager();
        }();
        return *instance;
    }

    KeyboardShortcutManager::KeyboardShortcutManager()
        : m_initialized(false)
        , m_globalEnabled(true)        
    {
    }

    bool KeyboardShortcutManager::Initialize() {
        if (m_initialized) {
            return true;
        }

        m_initialized = true;
        std::cout << "KeyboardShortcutManager initialized" << std::endl;        
        return true;
    }

    void KeyboardShortcutManager::Update(double deltaTime) {
        if (!m_globalEnabled || !m_initialized) {
            return;
        }

        // If ImGui is capturing keyboard input, we still allow processing of
        // registered shortcuts that explicitly requested to ignore ImGui capture.
        bool wantCapture = ImGui::GetIO().WantCaptureKeyboard;

        KeyModifier currentMods = GetCurrentImGuiModifiers();

        // Poll all registered keys via ImGui
        for (auto& [combo, info] : m_shortcuts) {
            if (!info->enabled) continue;

            bool triggered = false;
            switch (combo.action) {
                case KeyAction::Press:   triggered = ImGui::IsKeyPressed(combo.key, false); break;
                case KeyAction::Release: triggered = ImGui::IsKeyReleased(combo.key); break;
                case KeyAction::Repeat:  triggered = ImGui::IsKeyPressed(combo.key, true); break;
            }

            if (!triggered) continue;

            if (currentMods != combo.modifiers) continue;

            if (wantCapture && !info->ignoreImguiCapture) {
                // ImGui is capturing input and this shortcut doesn't request to
                // bypass capture, so skip it.
                continue;
            }

            ExecuteShortcut(*info);
        }
    }

    void KeyboardShortcutManager::Shutdown() {
        UnregisterAllShortcuts();
        m_initialized = false;
        std::cout << "KeyboardShortcutManager shut down" << std::endl;
    }

    bool KeyboardShortcutManager::RegisterShortcut(const std::string& name, 
                                                  const std::string& description,
                                                  const KeyCombination& combination, 
                                                  ShortcutCallback callback,
                                                  bool ignoreImguiCapture) {
        if (!IsValidKeyCombination(combination)) {
            std::cerr << "Invalid key combination for shortcut: " << name << std::endl;
            return false;
        }

        if (!callback) {
            std::cerr << "Null callback for shortcut: " << name << std::endl;
            return false;
        }

        // Check if shortcut name already exists
        if (HasShortcut(name)) {
            std::cerr << "Shortcut with name '" << name << "' already exists" << std::endl;
            return false;
        }

        // Check if key combination is already in use
        auto combIt = m_shortcuts.find(combination);
        if (combIt != m_shortcuts.end()) {
            std::cerr << "Key combination " << KeyCombinationToString(combination) 
                      << " already registered for shortcut: " << combIt->second->name << std::endl;
            return false;
        }

        // Create and register the shortcut
        auto shortcut = std::make_shared<ShortcutInfo>(name, description, combination, std::move(callback), ignoreImguiCapture);
        
        m_shortcuts[combination] = shortcut;
        m_shortcutsByName[name] = shortcut;

        LogShortcutRegistration(*shortcut);
        return true;
    }

    bool KeyboardShortcutManager::RegisterShortcut(const std::string& name,
                                                  const std::string& description,
                                                  ImGuiKey key,
                                                  KeyModifier modifiers,
                                                  ShortcutCallback callback,
                                                  KeyAction action,
                                                  bool ignoreImguiCapture) {
        return RegisterShortcut(name, description, KeyCombination(key, modifiers, action), std::move(callback), ignoreImguiCapture);
    }

    bool KeyboardShortcutManager::UnregisterShortcut(const std::string& name) {
        auto nameIt = m_shortcutsByName.find(name);
        if (nameIt == m_shortcutsByName.end()) {
            return false;
        }

        auto shortcut = nameIt->second;
        
        // Remove from both maps
        m_shortcuts.erase(shortcut->combination);
        m_shortcutsByName.erase(nameIt);

        std::cout << "Unregistered shortcut: " << name << std::endl;
        return true;
    }

    void KeyboardShortcutManager::UnregisterAllShortcuts() {
        size_t count = m_shortcuts.size();
        m_shortcuts.clear();
        m_shortcutsByName.clear();
        
        if (count > 0) {
            std::cout << "Unregistered " << count << " shortcuts" << std::endl;
        }
    }

    bool KeyboardShortcutManager::EnableShortcut(const std::string& name, bool enabled) {
        auto it = m_shortcutsByName.find(name);
        if (it == m_shortcutsByName.end()) {
            return false;
        }

        it->second->enabled = enabled;
        return true;
    }

    bool KeyboardShortcutManager::DisableShortcut(const std::string& name) {
        return EnableShortcut(name, false);
    }

    bool KeyboardShortcutManager::IsShortcutEnabled(const std::string& name) const {
        auto it = m_shortcutsByName.find(name);
        return (it != m_shortcutsByName.end()) ? it->second->enabled : false;
    }

    bool KeyboardShortcutManager::HasShortcut(const std::string& name) const {
        return m_shortcutsByName.find(name) != m_shortcutsByName.end();
    }

    std::vector<std::string> KeyboardShortcutManager::GetShortcutNames() const {
        std::vector<std::string> names;
        names.reserve(m_shortcutsByName.size());
        
        for (const auto& pair : m_shortcutsByName) {
            names.push_back(pair.first);
        }
        
        std::sort(names.begin(), names.end());
        return names;
    }

    const ShortcutInfo* KeyboardShortcutManager::GetShortcutInfo(const std::string& name) const {
        auto it = m_shortcutsByName.find(name);
        return (it != m_shortcutsByName.end()) ? it->second.get() : nullptr;
    }

    KeyModifier KeyboardShortcutManager::GetCurrentImGuiModifiers() {
        KeyModifier result = KeyModifier::None;

        if (ImGui::IsKeyDown(ImGuiMod_Shift))   result = result | KeyModifier::Shift;
        if (ImGui::IsKeyDown(ImGuiMod_Ctrl))    result = result | KeyModifier::Ctrl;
        if (ImGui::IsKeyDown(ImGuiMod_Alt))     result = result | KeyModifier::Alt;
        if (ImGui::IsKeyDown(ImGuiMod_Super))   result = result | KeyModifier::Super;

        return result;
    }

    std::string KeyboardShortcutManager::KeyCombinationToString(const KeyCombination& combo) {
        std::ostringstream oss;
        
        // Add modifiers
        std::string modStr = ModifierToString(combo.modifiers);
        if (!modStr.empty()) {
            oss << modStr << "+";
        }
        
        // Add key
        oss << KeyToString(combo.key);
        
        // Add action if not press
        if (combo.action != KeyAction::Press) {
            oss << " (";
            switch (combo.action) {
                case KeyAction::Release: oss << "Release"; break;
                case KeyAction::Repeat:  oss << "Repeat"; break;
                default: oss << "Unknown"; break;
            }
            oss << ")";
        }
        
        return oss.str();
    }

    std::string KeyboardShortcutManager::ModifierToString(KeyModifier modifier) {
        std::vector<std::string> parts;
        
        if ((modifier & KeyModifier::Ctrl) != KeyModifier::None)  parts.push_back("Ctrl");
        if ((modifier & KeyModifier::Alt) != KeyModifier::None)   parts.push_back("Alt");
        if ((modifier & KeyModifier::Shift) != KeyModifier::None) parts.push_back("Shift");
        if ((modifier & KeyModifier::Super) != KeyModifier::None) parts.push_back("Super");
        
        std::ostringstream oss;
        for (size_t i = 0; i < parts.size(); ++i) {
            if (i > 0) oss << "+";
            oss << parts[i];
        }
        
        return oss.str();
    }

    std::string KeyboardShortcutManager::KeyToString(ImGuiKey key) {
        const char* name = ImGui::GetKeyName(key);
        if (name && name[0] != '\0') {
            return std::string(name);
        }
        return "Key" + std::to_string(static_cast<int>(key));
    }

    bool KeyboardShortcutManager::IsValidKeyCombination(const KeyCombination& combination) const {
        // Check if key is valid (basic validation)
        return combination.key > ImGuiKey_None && combination.key < ImGuiKey_COUNT;
    }

    void KeyboardShortcutManager::LogShortcutRegistration(const ShortcutInfo& shortcut) {
        std::cout << "Registered shortcut: " << shortcut.name 
                  << " (" << KeyCombinationToString(shortcut.combination) << ")"
                  << " - " << shortcut.description << std::endl;
    }

    void KeyboardShortcutManager::ExecuteShortcut(const ShortcutInfo& shortcut) {
        try {
            shortcut.callback();
        } catch (const std::exception& e) {
            std::cerr << "Error executing shortcut '" << shortcut.name << "': " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "Unknown error executing shortcut: " << shortcut.name << std::endl;
        }
    }

} // namespace Input