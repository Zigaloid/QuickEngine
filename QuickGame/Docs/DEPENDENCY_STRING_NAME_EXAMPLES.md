# Component Dependency Setup - Practical Examples

## Example 1: Game Initialization with Data-Driven Configuration

### Scenario
You have a game with multiple component types and want to configure their update order from a JSON file.

### Components
```cpp
class CTransformComponent : public Component {
    REFL_DECLARE_OBJECT(CTransformComponent, Component);
};

class CPhysicsBodyComponent : public Component {
    REFL_DECLARE_OBJECT(CPhysicsBodyComponent, Component);
};

class CThirdPersonInputComponent : public Component {
    REFL_DECLARE_OBJECT(CThirdPersonInputComponent, Component);
};

class CRenderComponent : public Component {
    REFL_DECLARE_OBJECT(CRenderComponent, Component);
};
```

### Configuration File (game_config.json)
```json
{
  "componentSchedule": {
    "registrations": [
      {
        "className": "CTransformComponent",
        "priority": 0,
        "name": "Transform Updates"
      },
      {
        "className": "CPhysicsBodyComponent",
        "priority": 1,
        "name": "Physics Simulation"
      },
      {
        "className": "CThirdPersonInputComponent",
        "priority": 2,
        "name": "Player Input"
      },
      {
        "className": "CRenderComponent",
        "priority": 3,
        "name": "Rendering"
      }
    ],
    "dependencies": [
      {
        "dependent": "CPhysicsBodyComponent",
        "dependsOn": "CTransformComponent"
      },
      {
        "dependent": "CThirdPersonInputComponent",
        "dependsOn": "CPhysicsBodyComponent"
      },
      {
        "dependent": "CRenderComponent",
        "dependsOn": "CTransformComponent"
      }
    ]
  }
}
```

### Setup Code
```cpp
void Game::InitializeComponentScheduler()
{
    auto* manager = Core::CoreSystem::GetComponentManager();
    auto* scheduler = Core::CoreSystem::GetComponentSystemScheduler();

    // Load configuration
    std::string configJson = LoadFileAsString("assets/game_config.json");

    // Parse JSON (using your JSON parser)
    auto config = ParseGameConfig(configJson);

    // Register all component types
    for (const auto& regEntry : config.registrations)
    {
        // Note: In a real system, you'd use reflection or ClassFactory
        // to instantiate component types by name
        if (regEntry.className == "CTransformComponent") {
            manager->RegisterComponentType<CTransformComponent>();
            scheduler->RegisterComponentType<CTransformComponent>(
                regEntry.priority, regEntry.name);
        }
        else if (regEntry.className == "CPhysicsBodyComponent") {
            manager->RegisterComponentType<CPhysicsBodyComponent>();
            scheduler->RegisterComponentType<CPhysicsBodyComponent>(
                regEntry.priority, regEntry.name);
        }
        // ... etc for other types
    }

    // Add all dependencies
    for (const auto& depEntry : config.dependencies)
    {
        bool success = scheduler->AddDependencyByName(
            depEntry.dependent,
            depEntry.dependsOn
        );

        if (!success) {
            LOG_ERROR("Failed to add dependency: %s -> %s",
                     depEntry.dependent.c_str(),
                     depEntry.dependsOn.c_str());
        }
    }

    // Configure and initialize scheduler
    scheduler->SetExecutionPolicy(ExecutionPolicy::Custom);
    if (!scheduler->Initialize()) {
        LOG_ERROR("Failed to initialize component scheduler");
        return;
    }

    LOG_INFO("Component scheduler initialized with %zu component types",
            manager->GetRegisteredTypeCount());
}
```

---

## Example 2: Plugin System with Dynamic Component Registration

### Scenario
Your game supports plugins that can register new component types at runtime. Dependencies are discovered dynamically.

### Plugin Interface
```cpp
class IGamePlugin {
public:
    virtual ~IGamePlugin() = default;
    virtual void RegisterComponents(ComponentManager* manager,
                                   ComponentSystemScheduler* scheduler) = 0;
    virtual std::vector<std::pair<std::string, std::string>> GetDependencies() const = 0;
};
```

### Example Plugin Implementation
```cpp
class PhysicsPlugin : public IGamePlugin {
public:
    void RegisterComponents(ComponentManager* manager,
                           ComponentSystemScheduler* scheduler) override
    {
        // Register the physics component type
        manager->RegisterComponentType<CAdvancedPhysicsComponent>();
        scheduler->RegisterComponentType<CAdvancedPhysicsComponent>(
            100,  // priority
            "Advanced Physics"
        );
    }

    std::vector<std::pair<std::string, std::string>> GetDependencies() const override
    {
        return {
            {"CAdvancedPhysicsComponent", "CTransformComponent"}
        };
    }
};
```

### Plugin Manager
```cpp
class PluginManager {
private:
    std::vector<std::unique_ptr<IGamePlugin>> m_plugins;

public:
    void LoadPlugin(std::unique_ptr<IGamePlugin> plugin)
    {
        auto* manager = Core::CoreSystem::GetComponentManager();
        auto* scheduler = Core::CoreSystem::GetComponentSystemScheduler();

        // Register the plugin's components
        plugin->RegisterComponents(manager, scheduler);

        // Declare the plugin's dependencies
        for (const auto& [dependent, dependency] : plugin->GetDependencies())
        {
            if (!scheduler->AddDependencyByName(dependent, dependency))
            {
                LOG_WARNING("Failed to add dependency from plugin: %s -> %s",
                           dependent.c_str(), dependency.c_str());
            }
        }

        m_plugins.push_back(std::move(plugin));
    }

    void InitializeScheduler()
    {
        auto* scheduler = Core::CoreSystem::GetComponentSystemScheduler();
        scheduler->SetExecutionPolicy(ExecutionPolicy::Custom);
        scheduler->Initialize();
    }
};
```

### Usage
```cpp
void Game::InitializeWithPlugins()
{
    PluginManager pluginMgr;

    // Load all plugins
    pluginMgr.LoadPlugin(std::make_unique<PhysicsPlugin>());
    pluginMgr.LoadPlugin(std::make_unique<AudioPlugin>());
    pluginMgr.LoadPlugin(std::make_unique<AnimationPlugin>());

    // Initialize scheduler with all dependencies
    pluginMgr.InitializeScheduler();
}
```

---

## Example 3: Error Handling and Validation

### Scenario
You want to validate your dependency configuration before running the game.

### Validation System
```cpp
class DependencyValidator {
private:
    ComponentManager* m_manager;
    ComponentSystemScheduler* m_scheduler;
    std::vector<std::string> m_errors;
    std::vector<std::string> m_warnings;

public:
    DependencyValidator(ComponentManager* mgr, ComponentSystemScheduler* sched)
        : m_manager(mgr), m_scheduler(sched) {}

    bool ValidateDependencies(const std::vector<std::pair<std::string, std::string>>& deps)
    {
        m_errors.clear();
        m_warnings.clear();

        for (const auto& [dependent, dependency] : deps)
        {
            // Check if dependent type is registered
            auto depTypeOpt = m_manager->GetTypeIndexByClassName(dependent);
            if (!depTypeOpt)
            {
                m_errors.push_back(
                    "Dependent component not registered: " + dependent
                );
                continue;
            }

            // Check if dependency type is registered
            auto depOnTypeOpt = m_manager->GetTypeIndexByClassName(dependency);
            if (!depOnTypeOpt)
            {
                m_errors.push_back(
                    "Dependency component not registered: " + dependency
                );
                continue;
            }

            // Try to add the dependency
            if (!m_scheduler->AddDependencyByName(dependent, dependency))
            {
                m_errors.push_back(
                    "Failed to add dependency: " + dependent + " -> " + dependency
                );
            }
        }

        return m_errors.empty();
    }

    bool HasErrors() const { return !m_errors.empty(); }
    bool HasWarnings() const { return !m_warnings.empty(); }

    void PrintReport() const
    {
        if (!m_errors.empty())
        {
            LOG_ERROR("Dependency validation failed with %zu errors:",
                     m_errors.size());
            for (const auto& error : m_errors)
            {
                LOG_ERROR("  - %s", error.c_str());
            }
        }

        if (!m_warnings.empty())
        {
            LOG_WARNING("Dependency validation had %zu warnings:",
                       m_warnings.size());
            for (const auto& warning : m_warnings)
            {
                LOG_WARNING("  - %s", warning.c_str());
            }
        }

        if (m_errors.empty() && m_warnings.empty())
        {
            LOG_INFO("Dependency validation passed!");
        }
    }
};
```

### Usage
```cpp
void Game::ValidateDependencyConfiguration()
{
    auto* manager = Core::CoreSystem::GetComponentManager();
    auto* scheduler = Core::CoreSystem::GetComponentSystemScheduler();

    // Load configuration
    auto config = LoadGameConfig("assets/game_config.json");

    // Validate
    DependencyValidator validator(manager, scheduler);
    if (!validator.ValidateDependencies(config.dependencies))
    {
        validator.PrintReport();
        // Exit or fall back to default configuration
        return;
    }

    validator.PrintReport();

    // Configuration is valid, initialize scheduler
    scheduler->SetExecutionPolicy(ExecutionPolicy::Custom);
    scheduler->Initialize();
}
```

---

## Example 4: Mixing Template and String-based APIs

### Scenario
You have core components known at compile time and want to support plugin dependencies.

### Setup Code
```cpp
void Game::InitializeMixedDependencies()
{
    auto* manager = Core::CoreSystem::GetComponentManager();
    auto* scheduler = Core::CoreSystem::GetComponentSystemScheduler();

    // Register core components
    manager->RegisterComponentType<CTransformComponent>();
    manager->RegisterComponentType<CPhysicsBodyComponent>();
    manager->RegisterComponentType<CThirdPersonInputComponent>();
    manager->RegisterComponentType<CRenderComponent>();

    scheduler->RegisterComponentType<CTransformComponent>(0, "Transform");
    scheduler->RegisterComponentType<CPhysicsBodyComponent>(1, "Physics");
    scheduler->RegisterComponentType<CThirdPersonInputComponent>(2, "Input");
    scheduler->RegisterComponentType<CRenderComponent>(3, "Render");

    // Core dependencies using template API (compile-time checked)
    scheduler->AddDependency<CPhysicsBodyComponent, CTransformComponent>();
    scheduler->AddDependency<CThirdPersonInputComponent, CPhysicsBodyComponent>();
    scheduler->AddDependency<CRenderComponent, CTransformComponent>();

    // Load plugin dependencies from configuration
    auto pluginConfig = LoadPluginConfig("assets/plugins.json");
    for (const auto& [dependent, dependency] : pluginConfig.dependencies)
    {
        if (!scheduler->AddDependencyByName(dependent, dependency))
        {
            LOG_WARNING("Plugin dependency not available: %s -> %s",
                       dependent.c_str(), dependency.c_str());
        }
    }

    scheduler->SetExecutionPolicy(ExecutionPolicy::Custom);
    scheduler->Initialize();
}
```

---

## Example 5: Runtime Dependency Graph Debugging

### Scenario
You want to visualize and debug the component dependency graph at runtime.

### Debugging Utility
```cpp
class DependencyGraphDebugger {
public:
    static void PrintDependencyGraph(ComponentSystemScheduler* scheduler)
    {
        LOG_INFO("=== Component Dependency Graph ===");
        // Note: You would need to add getter methods to ComponentSystemScheduler
        // to access m_phases for this to work

        // This is a conceptual example
        LOG_INFO("Component dependency graph printed to log");
    }

    static void PrintTypeInfo(ComponentManager* manager, 
                             const std::string& className)
    {
        auto typeOpt = manager->GetTypeIndexByClassName(className);
        if (typeOpt) {
            LOG_INFO("Component '%s' is registered", className.c_str());
        } else {
            LOG_WARNING("Component '%s' is NOT registered", className.c_str());
        }
    }

    static void ValidateNoCycles(ComponentSystemScheduler* scheduler)
    {
        // This would require cycle detection algorithm
        LOG_INFO("Cycle detection not yet implemented");
    }
};
```

### Usage
```cpp
void Game::DebugComponentDependencies()
{
    auto* manager = Core::CoreSystem::GetComponentManager();
    auto* scheduler = Core::CoreSystem::GetComponentSystemScheduler();

    DependencyGraphDebugger::PrintDependencyGraph(scheduler);
    DependencyGraphDebugger::PrintTypeInfo(manager, "CPhysicsBodyComponent");
    DependencyGraphDebugger::PrintTypeInfo(manager, "CUnknownComponent");
    DependencyGraphDebugger::ValidateNoCycles(scheduler);
}
```

---

## Summary

These examples demonstrate:
1. **Data-driven configuration** with JSON files
2. **Plugin systems** with dynamic registration
3. **Error handling and validation**
4. **Mixed API usage** for flexibility
5. **Debugging and inspection tools**

Choose the pattern that best fits your application's architecture!
