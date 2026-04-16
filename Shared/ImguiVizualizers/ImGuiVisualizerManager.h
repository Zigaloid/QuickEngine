#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

namespace ImGuiVisualizers {

class IImGuiVisualizer;

/**
 * @brief Manages the lifecycle, visibility, and rendering of registered
 *        IImGuiVisualizer instances.
 *
 * Visualizers can be registered with a unique string key. The manager handles:
 *  - Initialization / shutdown of each visualizer
 *  - Per-frame Update (always) and Render (when visible)
 *  - An ImGui "Windows" menu for toggling visibility
 *  - An optional ImGui menu bar with File / Windows menus
 *
 * Usage:
 * @code
 *   ImGuiVisualizerManager mgr;
 *   mgr.Register("console", std::make_unique<MyConsoleVisualizer>());
 *   mgr.Register("heatmap", std::make_unique<MyHeatMapVisualizer>());
 *   mgr.Initialize();
 *   // In your frame loop:
 *   mgr.Update(dt);
 *   mgr.RenderMenuBar();   // optional
 *   mgr.RenderAll();
 * @endcode
 */
class ImGuiVisualizerManager
{
public:
    ImGuiVisualizerManager();
    ~ImGuiVisualizerManager();

    // Non-copyable, movable
    ImGuiVisualizerManager(const ImGuiVisualizerManager&) = delete;
    ImGuiVisualizerManager& operator=(const ImGuiVisualizerManager&) = delete;
    ImGuiVisualizerManager(ImGuiVisualizerManager&&) noexcept;
    ImGuiVisualizerManager& operator=(ImGuiVisualizerManager&&) noexcept;

    // ── Registration ────────────────────────────────────────────────────

    /**
     * @brief Register a visualizer under a unique key.
     * @param key   Unique identifier (used for lookups and menu ordering).
     * @param viz   Owning pointer to the visualizer.
     * @param visibleByDefault Whether the window starts visible.
     */
    void Register(const std::string& key,
                  std::unique_ptr<IImGuiVisualizer> viz,
                  bool visibleByDefault = false);

    /**
     * @brief Unregister and destroy a visualizer by key.
     * Calls Shutdown() on the visualizer before removal.
     */
    void Unregister(const std::string& key);

    // ── Lifecycle ───────────────────────────────────────────────────────

    /**
     * @brief Initialize all currently registered visualizers.
     * Call once after all initial registrations.
     */
    void Initialize();

    /**
     * @brief Shut down all registered visualizers and clear the registry.
     */
    void Shutdown();

    // ── Per-frame ───────────────────────────────────────────────────────

    /**
     * @brief Update all registered visualizers (called every frame).
     * @param deltaTime Elapsed time in seconds since the last frame.
     */
    void Update(float deltaTime);

    /**
     * @brief Render all visible visualizers.
     */
    void RenderAll();

    /**
     * @brief Render a full ImGui menu bar with File and Windows menus.
     * Call inside an ImGui window that has ImGuiWindowFlags_MenuBar.
     */
    void RenderMenuBar();

    /**
     * @brief Render just the Windows menu contents.
     * Useful if you build your own menu bar and only need the toggle items.
     */
    void RenderWindowsMenu();

    // ── Visibility ──────────────────────────────────────────────────────

    void SetVisible(const std::string& key, bool visible);
    bool IsVisible(const std::string& key) const;
    void ToggleVisible(const std::string& key);

    // ── Accessors ───────────────────────────────────────────────────────

    /**
     * @brief Retrieve a registered visualizer by key (non-owning).
     * @return Pointer to the visualizer, or nullptr if not found.
     */
    IImGuiVisualizer* GetVisualizer(const std::string& key) const;

    /**
     * @brief Retrieve a registered visualizer cast to a concrete type.
     * @return Typed pointer, or nullptr if not found or type mismatch.
     */
    template <typename T>
    T* GetVisualizerAs(const std::string& key) const
    {
        return dynamic_cast<T*>(GetVisualizer(key));
    }

    /**
     * @brief Return the number of registered visualizers.
     */
    std::size_t GetCount() const;

    /**
     * @brief Set an optional callback invoked from the File menu (e.g. Exit).
     */
    void SetFileMenuCallback(std::function<void()> callback);

private:
    struct Entry {
        std::string key;
        std::unique_ptr<IImGuiVisualizer> visualizer;
        bool visible = false;
        bool initialized = false;
    };

    std::vector<Entry> m_entries;
    std::unordered_map<std::string, std::size_t> m_keyIndex;
    bool m_initialized = false;
    std::function<void()> m_fileMenuCallback;

    // Internal helpers
    Entry* FindEntry(const std::string& key);
    const Entry* FindEntry(const std::string& key) const;
    void RenderFileMenu();
};

} // namespace ImGuiVisualizers