#pragma once

#include <string>
#include <unordered_map>
#include <functional>
#include "Viewport.h"
#include "CameraComponent.h"

// Forward declarations
class Viewport;
class Camera;
namespace ComponentSystem {
    class CameraComponent;
}

namespace Rendering {

/**
 * @brief Manages viewport caching functionality for storing and restoring viewport configurations
 * 
 * This class handles caching of viewport settings including camera configurations,
 * render callbacks, and viewport properties. It provides functionality to save
 * viewport states and restore them later for consistent user experience.
 */
class ViewportCache {
public:
    // Camera position and orientation for caching
    struct CachedCameraTransform {
        // Position
        float posX = 0.0f;
        float posY = 5.0f;
        float posZ = 10.0f;

        // Orientation (quaternion)
        float orientW = 1.0f;
        float orientX = 0.0f;
        float orientY = 0.0f;
        float orientZ = 0.0f;
    };

    // Cached camera settings to avoid forward declaration issues
    struct CachedCameraSettings {
        // Camera settings
        float movementSpeed = 5.0f;
        float rotationSpeed = 2.0f;
        float sprintMultiplier = 2.5f;
        float smoothing = 0.1f;
        bool invertPitch = false;
        bool invertYaw = false;
        float mouseSensitivity = 0.005f;

        // Camera position and orientation
        CachedCameraTransform transform;
    };

    // Viewport cache entry to store viewport settings
    struct ViewportCacheEntry {
        Viewport::ViewportConfig config;
        // NOTE: Render callbacks are not cached as they should be consistent across
        // all viewports and are managed by ViewportLayoutManager
        
        // Store camera settings using our own struct to avoid forward declaration issues
        CachedCameraSettings cameraSettings;
        bool hasCameraSettings = false; // Flag to track if camera settings are valid
    };

    ViewportCache() = default;
    ~ViewportCache() = default;

    // Non-copyable but movable
    ViewportCache(const ViewportCache&) = delete;
    ViewportCache& operator=(const ViewportCache&) = delete;
    ViewportCache(ViewportCache&&) = default;
    ViewportCache& operator=(ViewportCache&&) = default;

    /**
     * @brief Cache the current settings of a viewport
     * @param name The name identifier for the cached viewport
     * @param viewport The viewport to cache settings from
     */
    void cacheViewportSettings(const std::string& name, const Viewport* viewport);

    /**
     * @brief Load cached settings into a viewport
     * @param name The name identifier of the cached viewport
     * @param viewport The viewport to load settings into
     * @return true if settings were successfully loaded from cache, false otherwise
     */
    bool loadCachedViewportSettings(const std::string& name, Viewport* viewport);

    /**
     * @brief Check if a viewport configuration exists in cache
     * @param name The name identifier to check
     * @return true if the viewport exists in cache, false otherwise
     */
    bool hasViewportInCache(const std::string& name) const;

    /**
     * @brief Remove a specific viewport from cache
     * @param name The name identifier of the viewport to remove
     */
    void removeViewportFromCache(const std::string& name);

    /**
     * @brief Clear all cached viewport configurations
     */
    void clearCache();

    /**
     * @brief Get the number of cached viewport configurations
     * @return The number of entries in the cache
     */
    size_t getCacheSize() const;

    /**
     * @brief Get all cached viewport names
     * @return Vector of cached viewport names
     */
    std::vector<std::string> getCachedViewportNames() const;

    /**
     * @brief Print cache status for debugging
     */
    void printCacheStatus() const;

private:
    // Cache storage
    std::unordered_map<std::string, ViewportCacheEntry> m_cache;

    // Helper methods for camera settings conversion
    CachedCameraSettings convertToCachedSettings(const ComponentSystem::CameraComponent::CameraSettings& settings, const Camera* camera);
    ComponentSystem::CameraComponent::CameraSettings convertFromCachedSettings(const CachedCameraSettings& cached);
    void applyCachedTransform(Camera* camera, const CachedCameraTransform& transform);
};

} // namespace Rendering