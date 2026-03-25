#pragma once

#include "Window.h"
#include "Viewport.h"

// Forward declarations
namespace Rendering {
    class LevelComponent;
    class RenderComponent;
    class GLTFRenderComponent;
}

namespace ComponentSystem {
    class ComponentManager;
}

/**
 * LevelManager handles all level-related operations for the ProfilerApp.
 * This includes level creation, RenderComponent management, UI rendering, and console commands.
 */
class LevelManager {
public:
    LevelManager();
    ~LevelManager();

    // Initialization and cleanup
    bool Initialize(ComponentSystem::ComponentManager* componentManager);
    void Shutdown();

    // Level management
    bool CreateLevel(const std::string& levelName = "Main Scene");
    void ClearLevel();
    Rendering::LevelComponent* GetCurrentLevel() const { return m_level; }

    // Level serialization
    bool SaveCurrentLevel(const std::string& fileName);
    bool LoadLevel(const std::string& fileName);

    // RenderComponent operations
    Rendering::RenderComponent* AddRenderComponentInFrontOfCamera(Viewport* activeViewport);
    void RemoveAllRenderComponents();

    // GLTF support
    Rendering::GLTFRenderComponent* AddGLTFRenderComponentInFrontOfCamera(Viewport* activeViewport, const std::string& gltfPath = "");
    void RemoveAllGLTFRenderComponents();
    size_t GetGLTFRenderComponentCount() const;

    // UI rendering - now accepts activeViewport parameter
    void RenderLevelMenu(Viewport* activeViewport = nullptr);

private:
    // Internal helper methods
    void setupInitialLevelContent();
    Rendering::GLTFRenderComponent* createGLTFRenderComponent(const Vector3f& position, const std::string& gltfPath);

    // Dependencies
    ComponentSystem::ComponentManager* m_componentManager = nullptr;
    
    // Level state
    Rendering::LevelComponent* m_level = nullptr;
    
    // Configuration
    static constexpr float DEFAULT_SPAWN_DISTANCE = 3.0f;
};