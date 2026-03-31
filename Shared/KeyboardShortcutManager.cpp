#include "KeyboardShortcutManager.h"
#include "CoreSystem/CoreSystem.h"
#include <iostream>
#include <sstream>
#include <algorithm>

// GLFW includes for key codes
#include "glfw/glfw3.h"

namespace Input {

    KeyboardShortcutManager::KeyboardShortcutManager()
        : ComponentSystem::Component()
        , m_globalEnabled(true)        
    {
    }

    bool KeyboardShortcutManager::OnInitialize() {
        if (IsInitialized()) {
            return true;
        }

        std::cout << "KeyboardShortcutManager initialized" << std::endl;        
        return true;
    }

    void KeyboardShortcutManager::OnUpdate(double deltaTime) {
        // No per-frame updates needed for keyboard shortcuts
        // All handling is done via callbacks
    }

    void KeyboardShortcutManager::OnShutdown() {
        UnregisterAllShortcuts();        
        std::cout << "KeyboardShortcutManager shut down" << std::endl;
    }

    bool KeyboardShortcutManager::RegisterShortcut(const std::string& name, 
                                                  const std::string& description,
                                                  const KeyCombination& combination, 
                                                  ShortcutCallback callback) {
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
        auto shortcut = std::make_shared<ShortcutInfo>(name, description, combination, std::move(callback));
        
        m_shortcuts[combination] = shortcut;
        m_shortcutsByName[name] = shortcut;

        LogShortcutRegistration(*shortcut);
        return true;
    }

    bool KeyboardShortcutManager::RegisterShortcut(const std::string& name,
                                                  const std::string& description,
                                                  int key,
                                                  KeyModifier modifiers,
                                                  ShortcutCallback callback,
                                                  KeyAction action) {
        return RegisterShortcut(name, description, KeyCombination(key, modifiers, action), std::move(callback));
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

    void KeyboardShortcutManager::HandleKeyInput(int key, int scancode, int action, int mods) {
        if (!m_globalEnabled || !IsInitialized()) {
            return;
        }

        // Convert GLFW parameters to our types
        KeyModifier modifiers = GlfwModsToKeyModifier(mods);
        KeyAction keyAction;
        
        switch (action) {
            case GLFW_PRESS:   keyAction = KeyAction::Press; break;
            case GLFW_RELEASE: keyAction = KeyAction::Release; break;
            case GLFW_REPEAT:  keyAction = KeyAction::Repeat; break;
            default: return; // Unknown action
        }

        // Create key combination for lookup
        KeyCombination combination(key, modifiers, keyAction);
        
        // Find matching shortcut
        auto it = m_shortcuts.find(combination);
        if (it != m_shortcuts.end() && it->second->enabled) {
            ExecuteShortcut(*it->second);
        }
    }

    KeyModifier KeyboardShortcutManager::GlfwModsToKeyModifier(int glfwMods) {
        KeyModifier result = KeyModifier::None;
        
        if (glfwMods & GLFW_MOD_SHIFT)    result = result | KeyModifier::Shift;
        if (glfwMods & GLFW_MOD_CONTROL)  result = result | KeyModifier::Ctrl;
        if (glfwMods & GLFW_MOD_ALT)      result = result | KeyModifier::Alt;
        if (glfwMods & GLFW_MOD_SUPER)    result = result | KeyModifier::Super;
        if (glfwMods & GLFW_MOD_CAPS_LOCK) result = result | KeyModifier::CapsLock;
        if (glfwMods & GLFW_MOD_NUM_LOCK)  result = result | KeyModifier::NumLock;
        
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

    std::string KeyboardShortcutManager::KeyToString(int key) {
        // Handle special keys
        switch (key) {
            case GLFW_KEY_SPACE:         return "Space";
            case GLFW_KEY_APOSTROPHE:    return "'";
            case GLFW_KEY_COMMA:         return ",";
            case GLFW_KEY_MINUS:         return "-";
            case GLFW_KEY_PERIOD:        return ".";
            case GLFW_KEY_SLASH:         return "/";
            case GLFW_KEY_SEMICOLON:     return ";";
            case GLFW_KEY_EQUAL:         return "=";
            case GLFW_KEY_LEFT_BRACKET:  return "[";
            case GLFW_KEY_BACKSLASH:     return "\\";
            case GLFW_KEY_RIGHT_BRACKET: return "]";
            case GLFW_KEY_GRAVE_ACCENT:  return "`";
            case GLFW_KEY_ESCAPE:        return "Escape";
            case GLFW_KEY_ENTER:         return "Enter";
            case GLFW_KEY_TAB:           return "Tab";
            case GLFW_KEY_BACKSPACE:     return "Backspace";
            case GLFW_KEY_INSERT:        return "Insert";
            case GLFW_KEY_DELETE:        return "Delete";
            case GLFW_KEY_RIGHT:         return "Right";
            case GLFW_KEY_LEFT:          return "Left";
            case GLFW_KEY_DOWN:          return "Down";
            case GLFW_KEY_UP:            return "Up";
            case GLFW_KEY_PAGE_UP:       return "PageUp";
            case GLFW_KEY_PAGE_DOWN:     return "PageDown";
            case GLFW_KEY_HOME:          return "Home";
            case GLFW_KEY_END:           return "End";
            case GLFW_KEY_CAPS_LOCK:     return "CapsLock";
            case GLFW_KEY_SCROLL_LOCK:   return "ScrollLock";
            case GLFW_KEY_NUM_LOCK:      return "NumLock";
            case GLFW_KEY_PRINT_SCREEN:  return "PrintScreen";
            case GLFW_KEY_PAUSE:         return "Pause";
        }
        
        // Function keys
        if (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F25) {
            return "F" + std::to_string(key - GLFW_KEY_F1 + 1);
        }
        
        // Number keys
        if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) {
            return std::string(1, static_cast<char>('0' + (key - GLFW_KEY_0)));
        }
        
        // Letter keys
        if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) {
            return std::string(1, static_cast<char>('A' + (key - GLFW_KEY_A)));
        }
        
        // Keypad keys
        if (key >= GLFW_KEY_KP_0 && key <= GLFW_KEY_KP_9) {
            return "Keypad" + std::to_string(key - GLFW_KEY_KP_0);
        }
        
        switch (key) {
            case GLFW_KEY_KP_DECIMAL:  return "Keypad.";
            case GLFW_KEY_KP_DIVIDE:   return "Keypad/";
            case GLFW_KEY_KP_MULTIPLY: return "Keypad*";
            case GLFW_KEY_KP_SUBTRACT: return "Keypad-";
            case GLFW_KEY_KP_ADD:      return "Keypad+";
            case GLFW_KEY_KP_ENTER:    return "KeypadEnter";
            case GLFW_KEY_KP_EQUAL:    return "Keypad=";
        }
        
        // Default for unknown keys
        return "Key" + std::to_string(key);
    }

    bool KeyboardShortcutManager::IsValidKeyCombination(const KeyCombination& combination) const {
        // Check if key is valid (basic validation)
        return combination.key >= 0 && combination.key <= GLFW_KEY_LAST;
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