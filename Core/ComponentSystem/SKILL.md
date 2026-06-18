# Component System — QuickScope ECS Architecture

## Overview

The **ComponentSystem** (`Core/ComponentSystem/`) is QuickScope's Entity Component System (ECS) implementation. It provides:

- **Hierarchical component architecture** — Components form parent-child trees
- **Reflection-driven serialization** — All components derive from `CReflectedBase` and support JSON round-trip
- **Async update scheduling** — Components execute on worker threads via `ComponentSystemScheduler`
- **Object pooling** — `ComponentPool` recycles component instances for allocation efficiency
- **Runtime registration** — New component types are registered via macros at static init time

### Key Files

| File | Purpose |
|------|---------|
| `Component.h/.cpp` | Base class; lifecycle, hierarchy, search, activation |
| `ComponentManager.h` | Pool ownership; create/release/query by type |
| `ComponentPool.h` | Object pool for one component type; acquire/release recycling |
| `ComponentFactory.h` | Factory abstraction for pool creation |
| `ComponentSystemScheduler.h` | Async job dispatch; priority/dependency scheduling |
| `ComponentRegistry.h` | Runtime metadata for component types |
| `ComponentReference.h` | Type-safe pointer wrapper (serializable) |
| `ComponentSystem.h` | Umbrella header |

---

## Quick Start

### Registering a New Component Type

**1. Declare in header (`.h`):**
```cpp
#include "ComponentSystem/Component.h"

class CMyComponent : public ComponentSystem::Component
{
public:
    REFL_DECLARE_OBJECT(CMyComponent, ComponentSystem::Component);
    DECLARE_COMPONENT()

protected:
    virtual bool OnInitialize() override;
    virtual void OnUpdate(double deltaTime) override;
    virtual void OnShutdown() override;

private:
    std::string m_name = "Default";
    float m_value = 0.0f;
};
```

**2. Define in `.cpp`:**
```cpp
#include "CMyComponent.h"

REGISTER_COMPONENT(CMyComponent, "My Component", "Category")
REFL_DEFINE_OBJECT(CMyComponent)
    REFL_DEFINE_OBJECT_MEMBER(CMyComponent, m_name),
    REFL_DEFINE_OBJECT_MEMBER(CMyComponent, m_value),
REFL_DEFINE_END

bool CMyComponent::OnInitialize()
{
    // Initialization logic here
    return true;
}

void CMyComponent::OnUpdate(double deltaTime)
{
    // Per-frame update
}

void CMyComponent::OnShutdown()
{
    // Cleanup before component destruction
}
```

### Creating and Using Components

```cpp
// Create via ComponentManager (auto-registers pool if needed)
ComponentSystem::ComponentManager* mgr = Core::CoreSystem::GetComponentManager();
CMyComponent* comp = mgr->CreateComponent<CMyComponent>();
comp->Initialize();

// Attach as child to another component
parentComponent->AddChild(comp);

// Search for sibling or child components
CMyComponent* sibling = comp->FindSibling<CMyComponent>();
CMyComponent* child = comp->FindChild<CMyComponent>();

// Toggle activation
comp->SetActive(false);
```

---

## Core Concepts

### Component Hierarchy

Components form a parent-child tree. Each component maintains:
- `m_parent` — weak_ptr to parent (or null if root)
- `m_children` — vector of shared_ptr to children

**Parent-child invariant:** If a parent is inactive, all descendants are inactive (via `IsActiveInHierarchy()`).

```cpp
// Attach existing child
root->AddChild(childPtr);

// Create and attach child inline
CMyComponent* child = root->CreateChild<CMyComponent>();
child->Initialize();

// Detach and destroy child
root->RemoveChild(childPtr);
```

### Lifecycle

Each component follows this sequence:

1. **Constructor** — Called when the component is pooled or created
2. **Initialize()** — Calls `OnInitialize()` and recursively initializes children
3. **Update(dt)** — Per-frame; calls `OnUpdate(dt)` (children are NOT updated recursively; scheduler handles all component updates)
4. **Shutdown()** — Calls `OnShutdown()` before release
5. **Destructor** — Never called during normal operation (pooled recycling)

```cpp
// Full reinit cycle
comp->ReInitialize();  // Shutdown + Initialize

// Query state
if (comp->IsInitialized()) { /* ... */ }
if (comp->IsActive()) { /* ... */ }
if (comp->IsActiveInHierarchy()) { /* respects parent state */ }
```

### Activation

Components can be active or inactive. Setting `SetActive(false)` propagates to all children:

```cpp
comp->SetActive(false);  // Calls OnDeactivate(), propagates to children
comp->Activate();
comp->Deactivate();
comp->ToggleActive();
```

`IsActiveInHierarchy()` returns true only if the component AND all ancestors are active.

### Component Search

All search methods are templated, O(n) linear scans:

```cpp
// Sibling search (direct children of parent, excluding self)
CMyComponent* sibling = comp->FindSibling<CMyComponent>();

// Direct children only
CMyComponent* child = comp->FindChild<CMyComponent>();
std::vector<CMyComponent*> children = comp->FindChildren<CMyComponent>();
std::vector<CMyComponent*> active = comp->FindActiveChildren<CMyComponent>();

// Recursive (depth-first)
CMyComponent* desc = comp->FindDescendant<CMyComponent>();
std::vector<CMyComponent*> descs = comp->FindDescendants<CMyComponent>();
std::vector<CMyComponent*> activeDescs = comp->FindActiveDescendants<CMyComponent>();
```

---

## ComponentManager

Owns and manages all component pools. Provides factory methods for creation and queries.

### Pool Management

```cpp
ComponentManager* mgr = Core::CoreSystem::GetComponentManager();

// Register a component type (auto-creates pool)
mgr->RegisterComponentType<CMyComponent>(/*initialSize=*/10, /*maxSize=*/1000);

// Or with string name lookup
mgr->RegisterComponentType<CMyComponent>("MyComponent", 10, 1000);
```

### Creation

```cpp
// Template overload (type-safe)
CMyComponent* comp = mgr->CreateComponent<CMyComponent>();

// String overload
Component* comp = mgr->CreateComponentByName("MyComponent");

// Shared_ptr (pool co-owns)
std::shared_ptr<CMyComponent> sp = mgr->CreateComponentShared<CMyComponent>();
```

### Queries

```cpp
// Active count in pool only (not hierarchy-respecting)
size_t count = mgr->GetActiveComponentCount<CMyComponent>();

// Filtered by IsActiveInHierarchy()
size_t count = mgr->GetActiveInHierarchyComponentCount<CMyComponent>();

// Fetch all active components
std::vector<CMyComponent*> active = mgr->GetActiveComponentsOfType<CMyComponent>();

// Type index lookup
auto typeOpt = mgr->GetTypeIndexByClassName("CMyComponent");
```

### Release

```cpp
// Returns component to pool (inactive, ready for reuse)
mgr->ReleaseComponent(comp);
```

---

## ComponentPool

Object pool for one component type. Maintains active/inactive lists and recycling.

```cpp
// Acquire (from inactive or create if space)
Component* comp = pool->Acquire();

// Return to pool
pool->Release(comp);

// Queries
size_t active = pool->GetActiveCount();
size_t inactive = pool->GetInactiveCount();
size_t total = pool->GetTotalCount();
```

---

## ComponentSystemScheduler

Async job dispatch for component updates. Respects priorities and inter-component dependencies.

### Registration

```cpp
ComponentSystemScheduler scheduler(mgr);
scheduler.Initialize();

// Register update phases (lower priority runs first)
scheduler.RegisterComponentType<CTransformComponent>(0, "Transform");
scheduler.RegisterComponentType<CPhysicsComponent>(1, "Physics");
scheduler.RegisterComponentType<CRenderComponent>(2, "Render");
```

### Dependencies

```cpp
// Generic template overload
scheduler.AddDependency<CPhysicsComponent, CTransformComponent>();

// String-name overload (for JSON configs)
scheduler.AddDependencyByName("CPhysicsComponent", "CTransformComponent");

// Batch from JSON
ComponentDependencyDefinitionList defs = /* load from JSON */;
scheduler.AddDependencies(defs);
```

### Execution Policies

```cpp
enum class ExecutionPolicy {
    Sequential,  // Components run one type after another (slow, deterministic)
    Parallel,    // All types run concurrently (fast, potential races)
    Custom       // Respects registered dependencies (balanced)
};

scheduler.SetExecutionPolicy(ExecutionPolicy::Custom);
```

### Per-Frame Dispatch

In the main loop (frame tick):

```cpp
// Submit all jobs (non-blocking)
scheduler.UpdateAllAsync(dt);

// Do other work...

// Block until all component updates finish
scheduler.WaitForCompletion();
```

Or with timeout:

```cpp
if (!scheduler.WaitForCompletion(std::chrono::milliseconds(16))) {
    // Timeout — not all jobs finished
}
```

### Queries

```cpp
size_t phases = scheduler.GetRegisteredPhaseCount();
size_t pending = scheduler.GetPendingJobCount();
size_t active = scheduler.GetActiveJobCount();

const auto& phases = scheduler.GetPhases();
ExecutionPolicy policy = scheduler.GetExecutionPolicy();
```

---

## ComponentReference

Type-safe serializable pointer to a component. Used in reflection to maintain component relationships.

```cpp
ComponentSystem::ComponentReference<CMyComponent> ref;
ref.SetComponent(comp);

if (CMyComponent* dereferenced = ref.GetComponent()) {
    // Use it
}

// Serializes to JSON as component ID reference
```

---

## Registry & Auto-Registration

`ComponentRegistry` holds metadata for component types registered at static init time.

### Macro-Based Registration

```cpp
// In header
class CMyComponent : public ComponentSystem::Component {
    DECLARE_COMPONENT()
    // ...
};

// In .cpp (once per type)
REGISTER_COMPONENT(CMyComponent, "My Component", "Category")
```

This macro creates a static `AutoRegisterComponent` object that registers the type at program startup.

### Runtime Lookup

```cpp
ComponentRegistry& reg = GetComponentRegistry();
const auto& allTypes = reg.GetAll();

if (const auto* info = reg.Find("CMyComponent")) {
    // info->className, info->displayName, info->category
}
```

---

## Patterns & Best Practices

### Pattern: Component-as-Behavioral Node

Use components for discrete behaviors (transform, physics, rendering):

```cpp
class CMovementComponent : public ComponentSystem::Component {
protected:
    bool OnInitialize() override {
        // Find sibling transform (assumes hierarchical layout)
        auto* transform = FindSibling<CTransformComponent>();
        return transform != nullptr;
    }
    void OnUpdate(double dt) override {
        auto* transform = FindSibling<CTransformComponent>();
        if (transform) {
            // Update position via transform
        }
    }
};
```

### Pattern: Polling for Dependencies

Within `OnUpdate()`, search for required siblings:

```cpp
void CMyComponent::OnUpdate(double dt) {
    auto* phys = FindSibling<CPhysicsComponent>();
    auto* render = FindSibling<CRenderComponent>();
    if (phys && render) {
        // Use both
    }
}
```

### Pattern: Async Execution with Scheduler

For CPU-bound updates, register phases and dependencies:

```cpp
// Main app init
scheduler.RegisterComponentType<CTransformComponent>(0, "Transform");
scheduler.RegisterComponentType<CPhysicsComponent>(1, "Physics");
scheduler.AddDependency<CPhysicsComponent, CTransformComponent>();

// Main loop
scheduler.UpdateAllAsync(dt);
// ... render, input handling ...
scheduler.WaitForCompletion();
```

### Pattern: Hierarchy-Based Traversal

Walk the tree to apply operations:

```cpp
void UpdateAllInTree(Component* root, std::function<void(Component*)> fn) {
    fn(root);
    for (auto* child : root->GetChildren()) {
        UpdateAllInTree(child, fn);
    }
}
```

### Pattern: Batch Queries

For high-frequency lookups, cache results:

```cpp
std::vector<CRenderComponent*> renderers = 
    mgr->GetActiveComponentsOfType<CRenderComponent>();
for (auto* r : renderers) {
    // Render all
}
```

---

## Integration with Core Systems

### CoreSystem Initialization

```cpp
// In CoreSystem::Initialize()
m_componentManager = std::make_unique<ComponentManager>();
m_componentManager->Initialize();

m_componentScheduler = std::make_unique<ComponentSystemScheduler>(m_componentManager.get());
m_componentScheduler->Initialize();

// Register all component types
m_componentManager->RegisterComponentType<CTransformComponent>();
m_componentManager->RegisterComponentType<CPhysicsComponent>();
// ... etc ...
```

### Frame Loop Integration

```cpp
void update() {
    // ... input, logic ...
    
    m_scheduler->UpdateAllAsync(dt);
    
    // ... render ...
    
    m_scheduler->WaitForCompletion();
}
```

---

## Debugging

### Print Component Tree

```cpp
void PrintComponentTree(Component* root, int depth = 0) {
    std::string indent(depth * 2, ' ');
    printf("%s%s (id=%zu, active=%d)\n", 
           indent.c_str(), typeid(*root).name(), root->GetId(), root->IsActive());
    for (auto* child : root->GetChildren()) {
        PrintComponentTree(child, depth + 1);
    }
}
```

### Validate Active State

```cpp
// Check if a component is truly active in hierarchy
if (comp->IsActive() && comp->IsActiveInHierarchy()) {
    // Should be receiving OnUpdate calls
}
```

### Monitor Job Queue

```cpp
printf("Scheduler — Phases: %zu, Pending: %zu, Active: %zu\n",
       scheduler.GetRegisteredPhaseCount(),
       scheduler.GetPendingJobCount(),
       scheduler.GetActiveJobCount());
```

---

## Common Pitfalls

| Pitfall | Solution |
|---------|----------|
| Component outlives parent without being detached | Always call `RemoveChild()` or let parent shutdown do it |
| Finding sibling of wrong type returns null | Use `FindSibling<T>()` with the exact type you need |
| Comparing with null after search | Searches return nullptr; check before dereferencing |
| Race conditions in parallel scheduler | Use `Custom` policy + dependencies, or keep update immutable |
| Pool exhaustion | Set `maxPoolSize` high enough or monitor via `GetTotalCount()` |
| Child stays active when parent deactivates | Automatic; `IsActiveInHierarchy()` respects hierarchy |

---

## See Also

- **CLAUDE.md** — Project-wide conventions and frame loop integration
- **Reflection** (`Core/Reflection/`) — JSON serialization and field inspection
- **JobSystem** (`Core/Jobsystem/`) — Worker threads for async scheduling
- **CoreSystem** (`Core/CoreSystem/`) — Singleton owning ComponentManager + Scheduler
