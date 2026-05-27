# Component Dependency Setup by String Name

## Overview

The component system now supports data-driven dependency configuration through reflection class names, allowing you to establish update order dependencies without requiring compile-time knowledge of component types.

## Key Features

- **String-based dependency registration**: Use component class names to express dependencies
- **Data-driven configuration**: Load dependencies from JSON/configuration files
- **Reflection integration**: Leverages `GetRflClassName()` for type identification
- **Type-safe lookup**: Uses optional return types to handle registration errors gracefully

## New API

### ComponentManager::GetTypeIndexByClassName()

Retrieves the internal type index for a registered component by its reflection class name.

```cpp
std::optional<std::type_index> GetTypeIndexByClassName(const std::string& className) const;
```

**Parameters:**
- `className`: The reflection class name (e.g., from `GetRflClassName()` or `ClassName()`)

**Returns:**
- `std::optional<std::type_index>` containing the type index if found, `std::nullopt` otherwise

**Example:**
```cpp
ComponentManager* manager = CoreSystem::GetComponentManager();
auto typeOpt = manager->GetTypeIndexByClassName("CPhysicsBodyComponent");
if (typeOpt) {
    // Type is registered
}
```

### ComponentSystemScheduler::AddDependencyByName()

Declares that one component type must wait for another to finish updating before proceeding.

```cpp
bool AddDependencyByName(const std::string& dependentClassName, 
                         const std::string& dependencyClassName);
```

**Parameters:**
- `dependentClassName`: The class name of the component that has the dependency
- `dependencyClassName`: The class name of the component it depends on

**Returns:**
- `true` if the dependency was successfully added
- `false` if either component type is not registered with the ComponentManager

**Behavior:**
- Both component types must be registered with the ComponentManager before calling this method
- Dependencies are processed during scheduler initialization
- Multiple dependencies can be added to a single component type

**Example:**
```cpp
auto* scheduler = CoreSystem::GetComponentSystemScheduler();

// Input components should update after physics components
bool success = scheduler->AddDependencyByName("CThirdPersonInputComponent", 
                                               "CPhysicsBodyComponent");
if (!success) {
    // One or both types were not registered
}
```

## Usage Patterns

### Pattern 1: Compile-time Setup (Recommended for Development)

Use the template-based API if you know your dependency graph at compile time:

```cpp
// In initialization code
scheduler->RegisterComponentType<CPhysicsBodyComponent>();
scheduler->RegisterComponentType<CThirdPersonInputComponent>();

// Template-based (still preferred for known dependencies)
scheduler->AddDependency<CThirdPersonInputComponent, CPhysicsBodyComponent>();
```

### Pattern 2: Data-Driven Setup (Recommended for Production)

Configure dependencies through JSON or configuration files:

```json
{
  "dependencies": [
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
```

Load and apply in code:

```cpp
void LoadComponentDependencies(const std::string& jsonPath)
{
    // Parse JSON...
    for (const auto& dep : dependencies)
    {
        auto* scheduler = CoreSystem::GetComponentSystemScheduler();
        bool success = scheduler->AddDependencyByName(dep.dependent, dep.dependsOn);
        if (!success)
        {
            LOG_WARNING("Failed to add dependency: %s -> %s",
                       dep.dependent.c_str(), dep.dependsOn.c_str());
        }
    }
}
```

### Pattern 3: Reflection-based Configuration

Integrate with reflection system for automatic configuration:

```cpp
class DependencyConfig : public CReflectedBase
{
public:
    struct Dependency {
        std::string dependent;
        std::string dependsOn;
    };

    std::vector<Dependency> dependencies;

    // Reflection setup...
};

// Load from JSON
DependencyConfig config;
config.ReadFromJsonString(jsonData);

// Apply dependencies
for (const auto& dep : config.dependencies)
{
    auto* scheduler = CoreSystem::GetComponentSystemScheduler();
    scheduler->AddDependencyByName(dep.dependent, dep.dependsOn);
}
```

## Implementation Details

### ComponentManager Registration

When components are registered via `RegisterComponentType()`, their class names are automatically stored in an internal map:

```cpp
template<typename T>
void RegisterComponentType(const std::string& className, ...)
{
    std::type_index typeIndex(typeid(T));
    m_nameToTypeIndex[className] = typeIndex;  // Internal mapping
    // ... rest of registration
}
```

### Type Resolution Flow

```
dependentClassName (string)
    ?
ComponentManager::GetTypeIndexByClassName()
    ?
std::type_index found?
    ? (yes)
m_typeToPhaseIndex lookup
    ?
Add dependency to phase
    ?
Return true (success)
```

## Error Handling

The string-based API returns `bool` or `std::optional<>` to indicate success/failure:

```cpp
// Check if type lookup succeeded
auto typeOpt = manager->GetTypeIndexByClassName("UnknownComponent");
if (!typeOpt) {
    // Component not registered - handle error
}

// Check if dependency addition succeeded
bool success = scheduler->AddDependencyByName("A", "B");
if (!success) {
    // Either A or B not registered - handle error
}
```

## Component Reflection Class Names

Each component exposes its class name through the reflection system:

```cpp
class CPhysicsBodyComponent : public Component
{
    REFL_DECLARE_OBJECT(CPhysicsBodyComponent, Component);
    // ...
};

// This macro generates:
// - GetRflClassName() ? "CPhysicsBodyComponent"
// - ClassName() ? "CPhysicsBodyComponent"
```

Use `GetRflClassName()` on any component instance to get its reflection class name:

```cpp
Component* comp = manager->CreateComponent<CPhysicsBodyComponent>();
std::string name = comp->GetRflClassName();  // "CPhysicsBodyComponent"
```

## Complete Example

```cpp
// Initialize components
auto* manager = CoreSystem::GetComponentManager();
auto* scheduler = CoreSystem::GetComponentSystemScheduler();

// Register component types
manager->RegisterComponentType<CTransformComponent>();
manager->RegisterComponentType<CPhysicsBodyComponent>();
manager->RegisterComponentType<CThirdPersonInputComponent>();
manager->RegisterComponentType<CRenderComponent>();

scheduler->RegisterComponentType<CTransformComponent>(0, "Transform");
scheduler->RegisterComponentType<CPhysicsBodyComponent>(1, "Physics");
scheduler->RegisterComponentType<CThirdPersonInputComponent>(2, "Input");
scheduler->RegisterComponentType<CRenderComponent>(3, "Render");

// Option 1: Direct string-based setup (data-driven)
scheduler->AddDependencyByName("CThirdPersonInputComponent", "CPhysicsBodyComponent");
scheduler->AddDependencyByName("CRenderComponent", "CTransformComponent");

// Option 2: Mix with template-based for known dependencies
scheduler->AddDependency<CPhysicsBodyComponent, CTransformComponent>();

// Initialize and run
scheduler->Initialize();
scheduler->SetExecutionPolicy(ExecutionPolicy::Custom);

// Each frame
while (running)
{
    scheduler->UpdateAllAsync(deltaTime);
}
```

## Comparison: Template vs String-based API

| Aspect | Template API | String API |
|--------|--------------|-----------|
| Compile-time checking | ? Type-safe | Runtime only |
| Data-driven | ? Code-only | ? Yes |
| Reflection integration | ? Manual | ? Automatic |
| Setup timing | Compile-time | Runtime |
| Documentation source | Source code | Config files |
| Learning curve | Steeper (templates) | Easier |
| Best for | Static configs | Dynamic configs |

Both APIs can be mixed in the same application.

## Best Practices

1. **Register types before adding dependencies**: Always ensure both component types are registered before calling `AddDependencyByName()`

2. **Validate return values**: Always check the `bool` return value to catch configuration errors early

3. **Use reflection class names consistently**: Store configuration using exact class names (e.g., "CPhysicsBodyComponent", not "Physics")

4. **Document your dependency graph**: Even with data-driven setup, maintain documentation of dependency relationships

5. **Order initialization carefully**: Register component types before loading dependency configuration

6. **Consider mixing APIs**: Use template API for core dependencies known at compile time, string API for plugin/extension dependencies

## Limitations and Notes

- Both component types must be registered **before** adding the dependency
- Dependencies are validated at scheduler initialization time
- Type lookup is O(1) due to hash map implementation
- Dependencies are stored as `std::type_index` internally for efficiency
- Circular dependencies are not automatically detected (application responsibility)

## Related API

- `Component::GetRflClassName()` - Get component type name
- `ComponentManager::RegisterComponentType()` - Register a component type
- `ComponentSystemScheduler::RegisterComponentType()` - Register for scheduling
- `ComponentSystemScheduler::AddDependency<T, Dep>()` - Template-based dependencies
- `ComponentSystemScheduler::SetExecutionPolicy()` - Set scheduling policy
