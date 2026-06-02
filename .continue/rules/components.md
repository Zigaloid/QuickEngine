# Skill: Component System

Use this skill when making any changes to the component system — adding new component types,
modifying the scheduler, adjusting dependency definitions, or working with the dependency
graph tooling.

---

## Key Source Files

### Core — ComponentSystem filter

| File | Purpose |
|---|---|
| `Core/ComponentSystem/Component.h` | Base class for all components. Inherits `CReflectedBase` + `std::enable_shared_from_this`. Virtual lifecycle: `OnInitialize`, `OnUpdate`, `OnShutdown`, `OnActivate`, `OnDeactivate`. |
| `Core/ComponentSystem/ComponentManager.h` | Owns all `ComponentPool` instances. Typed `CreateComponent<T>()`, `CreateComponentShared<T>()`, string-name factory overloads, and `GetComponentsOfType<T>` / `GetActiveComponentsOfType<T>`. |
| `Core/ComponentSystem/ComponentSystemScheduler.h` | Drives per-frame updates. Supports `ExecutionPolicy::Sequential`, `Parallel`, and `Custom` (dependency-graph). Exposes `AddDependencyByName(dependent, dependsOn)`. |
| `Core/ComponentSystem/ComponentDependencyDefinition.h` | `ComponentDependencyDefinition` (dependent / dependsOn pair) and `ComponentDependencyDefinitionList` (reflection-serialisable container). Load from JSON with `Read()`; iterate and push to the scheduler. |
| `Core/CoreSystem/CoreSystem.h` | Static engine root. Access the component manager via `Core::CoreSystem::GetComponentManager()` and the scheduler via `Core::CoreSystem::GetJobSystemScheduler()`. Helper templates: `GetComponentsOfType<T>`, `GetActiveComponentsOfType<T>`, `GetFirstComponentOfType<T>`. |

---

## Reference Components

### `CPhysicsBodyComponent` (`Shared/Components/PhysicsBodyComponent.h/.cpp`)
- Derives from `CPhysicsComponent` ? `ComponentSystem::Component`.
- Uses `REFL_DECLARE_OBJECT` + `DECLARE_COMPONENT()` macros.
- Manages a Jolt physics body; owns a `Matrix4f` model matrix and a `Vector4f` bounding sphere as non-owning shared pointers (sentinel deleter pattern).
- Shape type stored as `EPhysicsShapeType` (serialisable `int` enum).

### `CThirdPersonInputComponent` (`Shared/Components/ThirdPersonInputComponent.h/.cpp`)
- Derives directly from `ComponentSystem::Component`.
- Consumes `Input::InputActionManager` / `ActionContext` action names (`MoveX`, `MoveY`, `CameraX`, `CameraY`, `Jump`, `Sprint`).
- **Scheduler dependency example**: must update *after* `CPhysicsBodyComponent` so physics results are available. In the dependency JSON this is expressed as:
  ```json
  { "m_dependent": "CThirdPersonInputComponent", "m_dependsOn": "CPhysicsBodyComponent" }
  ```

---

## Dependency Definition Workflow

1. **Define** dependencies in a JSON file (e.g., `Data/component_dependencies.json`):
   ```json
   {
     "dependencies": [
       { "m_dependent": "CThirdPersonInputComponent", "m_dependsOn": "CPhysicsBodyComponent" }
     ]
   }
   ```

2. **Load and apply** at startup:
   ```cpp
   ComponentDependencyDefinitionList depList;
   depList.Read("Data/component_dependencies.json");

   auto* scheduler = Core::CoreSystem::GetJobSystemScheduler();
   for (const auto& dep : depList.dependencies)
   {
       scheduler->AddDependencyByName(dep.m_dependent, dep.m_dependsOn);
   }
   ```

3. **Register component types** before the scheduler resolves them:
   ```cpp
   auto* mgr = Core::CoreSystem::GetComponentManager();
   mgr->RegisterComponentType<CPhysicsBodyComponent>("CPhysicsBodyComponent");
   mgr->RegisterComponentType<CThirdPersonInputComponent>("CThirdPersonInputComponent");
   ```

---

## Dependency Graph Tooling

**`Shared/ImguiVizualizers/ComponentDependencyGraphVisualizer.h/.cpp`**

An ImGui node-graph tool (`ImGuiVisualizers::ComponentDependencyGraphVisualizer`) that
visualises and edits a `ComponentDependencyDefinitionList` at runtime.

| API | Description |
|---|---|
| `LoadFromFile(path)` | Deserialises a JSON `ComponentDependencyDefinitionList` and rebuilds the graph. |
| `BuildGraph(list)` | Rebuilds from an already-populated list object (no file I/O). |
| `AddDependency(dependsOn, dependent)` | Adds a single edge live; creates nodes as needed; marks the graph dirty. |
| `SaveToFile()` | Serialises the current graph back to the last loaded file path. |
| `GetLastError()` | Returns the most recent error string (empty on success). |

Each component type becomes a `ComponentNode` (a `NodeGraphNode` subclass) with one
**In** and one **Out** execution pin. Edges flow left-to-right: `dependency ? dependent`.

Registered in the editor like any other `IImGuiVisualizer`:
```cpp
auto* vis = new ImGuiVisualizers::ComponentDependencyGraphVisualizer();
manager.Register(vis);
vis->LoadFromFile("Data/component_dependencies.json");
```

---

## Component Authoring Checklist

When creating a new component:

- [ ] Inherit from `ComponentSystem::Component` (or an appropriate subclass).
- [ ] Add `REFL_DECLARE_OBJECT(MyComponent, BaseClass)` in the header.
- [ ] Add `DECLARE_COMPONENT()` if the component needs pool registration support.
- [ ] Add `REFL_DEFINE_OBJECT` block in the `.cpp` to register reflected members.
- [ ] Override `OnInitialize`, `OnUpdate`, `OnShutdown` as needed (not the public wrappers).
- [ ] Register the type with `ComponentManager::RegisterComponentType<MyComponent>("MyComponent")`.
- [ ] Declare any ordering constraints in the dependency JSON and reload the visualizer.
- [ ] Follow the project [coding conventions](./../conventions/SKILL.md) (Allman braces, `m_` prefix, PascalCase methods, `REFL_DECLARE_OBJECT` macros, etc.).
