#include "ViewportLayoutManager.h"
#include "CameraComponent.h"
#include "DebugChannel/DebugChannel.h"
#include "Window.h"
#include "Viewport.h"

// External debug channel
extern DebugChannels::CDebugChannel AppDebug;

ViewportLayoutManager::ViewportLayoutManager()
{
}

ViewportLayoutManager::~ViewportLayoutManager()
{
    Shutdown();
}

void ViewportLayoutManager::Initialize(Window* window)
{
    if (!window) {
        AppDebug.printf("ViewportLayoutManager: Cannot initialize with null window\n");
        return;
    }

    m_window = window;
    
    // Set initial layout to single viewport
    SetLayout(ViewportLayout::Single);    
    AppDebug.printf("ViewportLayoutManager initialized\n");
}

void ViewportLayoutManager::Shutdown()
{
    m_window = nullptr;
    m_renderCallback = nullptr;
    AppDebug.printf("ViewportLayoutManager shutdown\n");
}

void ViewportLayoutManager::SetLayout(ViewportLayout layout)
{	
	m_currentLayout = layout;
    applyLayout();
    AppDebug.printf("Viewport layout changed to: %s\n", GetCurrentLayoutName().c_str());
}

std::string ViewportLayoutManager::GetCurrentLayoutName() const
{
    switch (m_currentLayout) {
    case ViewportLayout::Single: return "Single";
    case ViewportLayout::HorizontalSplit: return "Horizontal Split";
    case ViewportLayout::VerticalSplit: return "Vertical Split";
    case ViewportLayout::Quad: return "Quad";
    default: return "Unknown";
    }
}

void ViewportLayoutManager::SetSplitRatio(float ratio)
{
    // Clamp ratio to valid range
    ratio = std::clamp(ratio, 0.1f, 0.9f);
    
    if (m_splitRatio != ratio) {
        m_splitRatio = ratio;
        
        // Re-apply layout if we're in a split mode
        if (m_currentLayout == ViewportLayout::HorizontalSplit ||
            m_currentLayout == ViewportLayout::VerticalSplit) {
            applyLayout();
        }
    }
}

void ViewportLayoutManager::SetupViewportCallbacks(ViewportRenderCallback renderCallback)
{
    if (!m_window) {
        AppDebug.printf("ViewportLayoutManager: Cannot setup callbacks - no window set\n");
        return;
    }

    m_renderCallback = renderCallback;

    // Set render callback for all viewports
    for (size_t i = 0; i < m_window->getViewportCount(); ++i) {
        Viewport* viewport = m_window->getViewport(i);
        if (viewport && m_renderCallback) {
            viewport->setRenderCallback(m_renderCallback);
            AppDebug.printf("Set render callback for viewport: %s\n", viewport->getName().c_str());
        }
    }

    // Configure viewport settings (this does not override cached settings)
    ConfigureViewportSettings();

    AppDebug.printf("Viewport callbacks set up for %zu viewports\n", m_window->getViewportCount());
}

void ViewportLayoutManager::ConfigureViewportSettings()
{
    if (!m_window) return;

    // Configure all viewports with default settings
    for (size_t i = 0; i < m_window->getViewportCount(); ++i) {
        Viewport* viewport = m_window->getViewport(i);
        if (viewport) {
            setupDefaultViewportConfig(viewport);
        }
    }
}

Viewport* ViewportLayoutManager::GetActiveViewport() const
{
    return m_window ? m_window->getActiveViewport() : nullptr;
}

void ViewportLayoutManager::UpdateActiveViewport()
{
    if (m_window) {
    }
}

size_t ViewportLayoutManager::GetViewportCount() const
{
    return m_window ? m_window->getViewportCount() : 0;
}

Viewport* ViewportLayoutManager::GetViewport(size_t index) const
{
    return m_window ? m_window->getViewport(index) : nullptr;
}

void ViewportLayoutManager::applyLayout()
{
    if (!m_window) {
        AppDebug.printf("ViewportLayoutManager: Cannot apply layout - no window set\n");
        return;
    }

    AppDebug.printf("Applying viewport layout: %s\n", GetCurrentLayoutName().c_str());

    switch (m_currentLayout) {
    case ViewportLayout::Single:
        m_window->setSingleViewport();
        break;

    case ViewportLayout::HorizontalSplit:
        m_window->setHorizontalSplit(m_splitRatio);
        break;

    case ViewportLayout::VerticalSplit:
        m_window->setVerticalSplit(m_splitRatio);
        break;

    case ViewportLayout::Quad:
        m_window->setQuadLayout();
        break;
    }

    // IMPORTANT: Re-setup viewport callbacks after layout change
    // This ensures render callbacks are restored to the new viewports
    if (m_renderCallback) {
        SetupViewportCallbacks(m_renderCallback);
    }

    AppDebug.printf("Applied viewport layout: %s with %zu viewports\n", GetCurrentLayoutName().c_str(), GetViewportCount());
}

void ViewportLayoutManager::setupDefaultViewportConfig(Viewport* viewport)
{
    if (!viewport) return;

    // Only apply default configuration if the viewport doesn't have custom settings
    // This prevents overriding settings that were loaded from cache
    auto* cameraComponent = viewport->getCameraComponent();
    bool hasExistingCameraSettings = false;
    
    if (cameraComponent) {
        Camera* camera = cameraComponent->GetCamera();
        if (camera) {
            // Check if camera has non-default position (indicating cached settings)
            Vector3f pos = camera->getPosition();
            hasExistingCameraSettings = (pos.getX() != 0.0f || pos.getY() != 5.0f || pos.getZ() != 10.0f);
        }
    }

    // Configure viewport settings (but be careful not to override cached camera settings)
    Viewport::ViewportConfig config;
    config.backgroundColor = Vector3f(0.1f, 0.1f, 0.1f);
    config.showGrid = true;
    config.showAxes = true;
    config.gridColor = Vector3f(0.5f, 0.5f, 0.5f);
    config.enableDepthTest = true;
    config.enableCulling = true;

    viewport->setConfig(config);

    // Only reset camera if it doesn't have cached settings
    if (cameraComponent && !hasExistingCameraSettings) {
        cameraComponent->ResetCamera();
        AppDebug.printf("Reset camera for viewport: %s (no cached settings found)\n", viewport->getName().c_str());
    } else if (hasExistingCameraSettings) {
        AppDebug.printf("Preserved cached camera settings for viewport: %s\n", viewport->getName().c_str());
    }
}