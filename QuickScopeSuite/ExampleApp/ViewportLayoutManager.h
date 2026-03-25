#pragma once

#include <functional>
#include <string>

// Forward declarations
class Window;
class Viewport;

enum class ViewportLayout {
    Single,
    HorizontalSplit,
    VerticalSplit,
    Quad
};

/**
 * @brief Manages viewport layouts and configurations
 * 
 * This class handles viewport layout switching, split ratio management,
 * and viewport callback setup, centralizing all viewport-related logic.
 */
class ViewportLayoutManager {
public:
    using ViewportRenderCallback = std::function<void(Viewport&, const class Matrix4f&, const class Matrix4f&)>;

    ViewportLayoutManager();
    ~ViewportLayoutManager();

    // Initialize with window reference
    void Initialize(Window* window);
    void Shutdown();

    // Layout management
    void SetLayout(ViewportLayout layout);
    ViewportLayout GetCurrentLayout() const { return m_currentLayout; }
    std::string GetCurrentLayoutName() const;
    bool IsCurrentLayout(ViewportLayout layout) const { return m_currentLayout == layout; }

    // Split ratio management (for split layouts)
    void SetSplitRatio(float ratio);
    float GetSplitRatio() const { return m_splitRatio; }

    // Viewport configuration
    void SetupViewportCallbacks(ViewportRenderCallback renderCallback);
    void ConfigureViewportSettings();

    // Active viewport management
    Viewport* GetActiveViewport() const;
    void UpdateActiveViewport();

    // Window reference management
    void SetWindow(Window* window) { m_window = window; }
    Window* GetWindow() const { return m_window; }

    // Viewport information
    size_t GetViewportCount() const;
    Viewport* GetViewport(size_t index) const;

private:
    void applyLayout();
    void setupDefaultViewportConfig(Viewport* viewport);

    Window* m_window = nullptr;
    ViewportLayout m_currentLayout = ViewportLayout::Single;
    float m_splitRatio = 0.5f;
    ViewportRenderCallback m_renderCallback;
};