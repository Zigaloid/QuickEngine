# Component Dependency Setup - Quick Reference

## One-liner Summary
Add component update order dependencies using string class names for data-driven configuration.

## Quick Start

### Template-based (Compile-time)
```cpp
scheduler->AddDependency<CThirdPersonInputComponent, CPhysicsBodyComponent>();
```

### String-based (Runtime/Data-driven)
```cpp
scheduler->AddDependencyByName("CThirdPersonInputComponent", "CPhysicsBodyComponent");
```

## API Cheat Sheet

### Look up component type by name
```cpp
std::optional<std::type_index> typeOpt = 
    manager->GetTypeIndexByClassName("CPhysicsBodyComponent");
if (typeOpt) { /* found */ }
```

### Add string-based dependency
```cpp
bool success = scheduler->AddDependencyByName(
    "DependentComponent",      // Type that has the dependency
    "DependencyComponent"      // Type it depends on
);
if (!success) { /* not registered */ }
```

## Common Patterns

### Pattern A: Static Configuration
```cpp
auto* scheduler = CoreSystem::GetComponentSystemScheduler();

scheduler->RegisterComponentType<CTransformComponent>(0);
scheduler->RegisterComponentType<CPhysicsBodyComponent>(1);
scheduler->RegisterComponentType<CThirdPersonInputComponent>(2);

scheduler->AddDependency<CPhysicsBodyComponent, CTransformComponent>();
scheduler->AddDependency<CThirdPersonInputComponent, CPhysicsBodyComponent>();

scheduler->Initialize();
```

### Pattern B: Data-Driven Configuration
```cpp
// Load from JSON file
std::string json = LoadJsonFile("dependencies.json");

// Parse JSON (pseudo-code)
auto dependencies = ParseJsonDependencies(json);

// Apply dependencies
for (const auto& [dependent, dependency] : dependencies) {
    auto* scheduler = CoreSystem::GetComponentSystemScheduler();
    if (!scheduler->AddDependencyByName(dependent, dependency)) {
        LOG_WARNING("Failed: %s -> %s", dependent.c_str(), dependency.c_str());
    }
}

scheduler->Initialize();
```

### Pattern C: Mixed Approach
```cpp
auto* scheduler = CoreSystem::GetComponentSystemScheduler();

// Known dependencies: use template API
scheduler->AddDependency<CPhysicsBodyComponent, CTransformComponent>();

// Plugin dependencies: use string API
scheduler->AddDependencyByName("CCustomInputComponent", "CPhysicsBodyComponent");

scheduler->Initialize();
```

## JSON Configuration Example

```json
{
  "componentSchedule": {
    "registrations": [
      {
        "className": "CTransformComponent",
        "priority": 0,
        "name": "Transform"
      },
      {
        "className": "CPhysicsBodyComponent",
        "priority": 1,
        "name": "Physics"
      },
      {
        "className": "CThirdPersonInputComponent",
        "priority": 2,
        "name": "Input"
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
      }
    ]
  }
}
```

## Troubleshooting

### Issue: AddDependencyByName returns false
**Cause:** One or both component types not registered
**Solution:** Ensure both types are registered with ComponentManager and scheduler before adding dependency
```cpp
manager->RegisterComponentType<CPhysicsBodyComponent>();
scheduler->RegisterComponentType<CPhysicsBodyComponent>(priority, "Name");
// Now it's safe to add dependencies referencing it
```

### Issue: Dependency not taking effect
**Cause:** Scheduler not initialized after adding dependency
**Solution:** Call Initialize() after adding all dependencies
```cpp
scheduler->AddDependencyByName("A", "B");
scheduler->AddDependencyByName("B", "C");
scheduler->Initialize();  // Required!
```

### Issue: GetTypeIndexByClassName returns empty optional
**Cause:** Component not registered or wrong class name
**Solution:** Verify class name matches exactly (case-sensitive)
```cpp
// Check actual class name
class CMyComponent : public Component {
    REFL_DECLARE_OBJECT(CMyComponent, Component);  // Name is "CMyComponent"
};

auto opt = manager->GetTypeIndexByClassName("CMyComponent");  // Correct
auto opt2 = manager->GetTypeIndexByClassName("MyComponent");  // Wrong
```

## Key Points

- **Both APIs coexist**: Use template when you know types at compile time, string API for runtime/dynamic setup
- **Reflection integration**: Class names come from `GetRflClassName()` macro automatically
- **Return types matter**: `std::optional<>` and `bool` signal success/failure
- **Registration order**: Types must be registered before dependencies
- **ExecutionPolicy**: Set to `Custom` to use dependency graph for execution order
- **Hash-based lookup**: Type name lookup is O(1) via hash map

## Related Methods

| Method | Purpose | Return Type |
|--------|---------|-------------|
| `RegisterComponentType<T>()` | Register for pooling/creation | void |
| `AddDependency<T, Dep>()` | Template-based dependency | void |
| `AddDependencyByName()` | String-based dependency | bool |
| `GetTypeIndexByClassName()` | Look up type by name | optional<type_index> |
| `GetRflClassName()` | Get component's class name | const char* |

## File Locations

- **Implementation**: `..\Core\ComponentSystem\ComponentSystem.h` (ComponentManager)
- **Scheduler**: `..\Core\ComponentSystem\ComponentSystemScheduler.h`
- **Full documentation**: See `DEPENDENCY_STRING_NAME_SETUP.md`
