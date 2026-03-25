#include "ViewportCache.h"
#include "Viewport.h"
#include "CameraComponent.h"
#include "math/Vector3f.h"
#include "math/Quaternion.h"
#include <iostream>
#include <algorithm>

namespace Rendering {

void ViewportCache::cacheViewportSettings(const std::string& name, const Viewport* viewport) {
    if (!viewport) {
        std::cerr << "Warning: Cannot cache settings - viewport is null for: " << name << std::endl;
        return;
    }

    ViewportCacheEntry entry;
    entry.config = viewport->getConfig();
    
    // Cache camera settings and transform if available
    auto* cameraComponent = viewport->getCameraComponent();
    if (cameraComponent) {
        Camera* camera = cameraComponent->GetCamera();
        entry.cameraSettings = convertToCachedSettings(cameraComponent->GetSettings(), camera);
        entry.hasCameraSettings = true;
        std::cout << "Cached camera settings and transform for viewport: " << name << std::endl;
    } else {
        entry.hasCameraSettings = false;
        std::cout << "No camera component found for viewport: " << name << " (viewport config cached only)" << std::endl;
    }
    
    // NOTE: Render callbacks are not cached as std::function is not easily copyable
    // and they should be consistent across all viewports anyway.
    // The ViewportLayoutManager will restore render callbacks after layout changes.
    
    m_cache[name] = entry;
    std::cout << "Cached viewport settings for: " << name << std::endl;
}

bool ViewportCache::loadCachedViewportSettings(const std::string& name, Viewport* viewport) {
    if (!viewport) {
        std::cerr << "Warning: Cannot load settings - viewport is null for: " << name << std::endl;
        return false;
    }

    auto it = m_cache.find(name);
    if (it != m_cache.end()) {
        // Load viewport configuration
        viewport->setConfig(it->second.config);
        
        // Load camera settings and transform if available
        if (it->second.hasCameraSettings) {
            auto* cameraComponent = viewport->getCameraComponent();
            if (cameraComponent) {
                // Apply camera settings
                cameraComponent->SetSettings(convertFromCachedSettings(it->second.cameraSettings));
                
                // Apply camera transform
                Camera* camera = cameraComponent->GetCamera();
                if (camera) {
                    applyCachedTransform(camera, it->second.cameraSettings.transform);
                    std::cout << "Loaded cached camera settings and transform for viewport: " << name << std::endl;
                } else {
                    std::cout << "Warning: Cached camera transform found but no camera for viewport: " << name << std::endl;
                }
            } else {
                std::cout << "Warning: Cached camera settings found but no camera component for viewport: " << name << std::endl;
            }
        }
        
        // NOTE: Render callback is not restored here as it should be set by ViewportLayoutManager
        // after all viewports are created to ensure consistency
        std::cout << "Loaded cached viewport settings for: " << name << std::endl;
        return true;
    }
    
    std::cout << "No cached settings found for viewport: " << name << " - using defaults" << std::endl;
    return false;
}

bool ViewportCache::hasViewportInCache(const std::string& name) const {
    return m_cache.find(name) != m_cache.end();
}

void ViewportCache::removeViewportFromCache(const std::string& name) {
    auto it = m_cache.find(name);
    if (it != m_cache.end()) {
        m_cache.erase(it);
        std::cout << "Removed viewport from cache: " << name << std::endl;
    } else {
        std::cout << "Warning: Viewport not found in cache for removal: " << name << std::endl;
    }
}

void ViewportCache::clearCache() {
    size_t oldSize = m_cache.size();
    m_cache.clear();
    std::cout << "Viewport cache cleared (removed " << oldSize << " entries)" << std::endl;
}

size_t ViewportCache::getCacheSize() const {
    return m_cache.size();
}

std::vector<std::string> ViewportCache::getCachedViewportNames() const {
    std::vector<std::string> names;
    names.reserve(m_cache.size());
    
    for (const auto& entry : m_cache) {
        names.push_back(entry.first);
    }
    
    // Sort names for consistent ordering
    std::sort(names.begin(), names.end());
    
    return names;
}

void ViewportCache::printCacheStatus() const {
    std::cout << "=== Viewport Cache Status ===" << std::endl;
    std::cout << "Total cached viewports: " << m_cache.size() << std::endl;
    
    if (m_cache.empty()) {
        std::cout << "Cache is empty" << std::endl;
    } else {
        for (const auto& entry : m_cache) {
            std::cout << "  - " << entry.first;
            if (entry.second.hasCameraSettings) {
                const auto& transform = entry.second.cameraSettings.transform;
                std::cout << " (Camera: pos=[" << transform.posX << "," << transform.posY << "," << transform.posZ << "])";
            } else {
                std::cout << " (No camera settings)";
            }
            std::cout << std::endl;
        }
    }
    std::cout << "=============================" << std::endl;
}

ViewportCache::CachedCameraSettings ViewportCache::convertToCachedSettings(
    const ComponentSystem::CameraComponent::CameraSettings& settings, 
    const Camera* camera) {
    
    CachedCameraSettings cached;
    
    // Copy camera settings
    cached.movementSpeed = settings.movementSpeed;
    cached.rotationSpeed = settings.rotationSpeed;
    cached.sprintMultiplier = settings.sprintMultiplier;
    cached.smoothing = settings.smoothing;
    cached.invertPitch = settings.invertPitch;
    cached.invertYaw = settings.invertYaw;
    cached.mouseSensitivity = settings.mouseSensitivity;
    
    // Copy camera transform if camera is available
    if (camera) {
        const Vector3f& position = camera->getPosition();
        const Quaternion& orientation = camera->getOrientation();
        
        cached.transform.posX = position.getX();
        cached.transform.posY = position.getY();
        cached.transform.posZ = position.getZ();
        
        cached.transform.orientW = orientation.getW();
        cached.transform.orientX = orientation.getX();
        cached.transform.orientY = orientation.getY();
        cached.transform.orientZ = orientation.getZ();
    }
    
    return cached;
}

ComponentSystem::CameraComponent::CameraSettings ViewportCache::convertFromCachedSettings(
    const CachedCameraSettings& cached) {
    
    ComponentSystem::CameraComponent::CameraSettings settings;
    settings.movementSpeed = cached.movementSpeed;
    settings.rotationSpeed = cached.rotationSpeed;
    settings.sprintMultiplier = cached.sprintMultiplier;
    settings.smoothing = cached.smoothing;
    settings.invertPitch = cached.invertPitch;
    settings.invertYaw = cached.invertYaw;
    settings.mouseSensitivity = cached.mouseSensitivity;
    return settings;
}

void ViewportCache::applyCachedTransform(Camera* camera, const CachedCameraTransform& transform) {
    if (!camera) {
        std::cerr << "Warning: Cannot apply cached transform - camera is null" << std::endl;
        return;
    }
    
    Vector3f position(transform.posX, transform.posY, transform.posZ);
    Quaternion orientation(transform.orientW, transform.orientX, transform.orientY, transform.orientZ);
    
    camera->setPosition(position);
    camera->setOrientation(orientation);
}

} // namespace Rendering