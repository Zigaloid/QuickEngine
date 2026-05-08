#pragma once

#include <string>
#include "KeyboardShortcutManager.h"

namespace ImGuiVisualizers {

/**
 * @brief Abstract base class for all ImGui-based visualizers.
 *
 * Derive from this class and implement the virtual methods to create a
 * visualizer that can be registered with ImGuiVisualizerManager.
 */
class IImGuiVisualizer {
public:
    virtual ~IImGuiVisualizer() = default;

    /**
     * @brief Called once after the visualizer is registered with the manager.
     * Use this for deferred setup that requires external systems to be ready.
     */
    virtual void Initialize() {}

    /**
     * @brief Called once when the visualizer is unregistered or the manager
     * is destroyed. Use this to release resources.
     */
    virtual void Shutdown() {}

    /**
     * @brief Called every frame regardless of visibility.
     * Use this for logic updates that must run continuously (e.g. data polling).
     * @param deltaTime Elapsed time in seconds since the last update.
     */
    virtual void Update(float deltaTime) { (void)deltaTime; }

    /**
     * @brief Render the visualizer's ImGui window.
     * Only called when the visualizer is visible.
     * @param isOpen Pointer to a bool controlling window visibility.
     *              Set to false to hide the window.
     * @return true if the window was rendered, false otherwise.
     */
    virtual bool Render(bool* isOpen) = 0;

    /**
     * @brief Return a display name used in menus and window titles.
     */
    virtual const char* GetName() const = 0;

    /**
     * @brief Optional keyboard shortcut key. Return ImGuiKey_None if no shortcut is assigned.
     * Visualizers should return the key and implement GetShortcutModifiers() to provide modifiers.
     */
    virtual ImGuiKey GetShortcut() const { return ImGuiKey_None; }

    /**
     * @brief Optional modifier mask for the shortcut returned by GetShortcut().
     * Return Input::KeyModifier::None by default. Implement this to expose
     * combinations like Ctrl+Alt for a visualizer's shortcut.
     */
    virtual Input::KeyModifier GetShortcutModifiers() const { return Input::KeyModifier::None; }

    /**
     * @brief Optional menu category for grouping in the Windows menu.
     * Return nullptr to place the item at the root level.
     */
    virtual const char* GetMenuCategory() const { return nullptr; }
};

} // namespace ImGuiVisualizers