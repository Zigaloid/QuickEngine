# Component Hierarchy Search - Quick Reference

## New Generic Methods

### Find Single Component

```cpp
// Find first descendant of type T (any depth, any state)
T* FindDescendant<T>()

// Find first active descendant of type T
T* FindActiveDescendant<T>()
```

**Example**:
```cpp
auto* camera = level->FindDescendant<CCameraComponent>();
auto* activeEnemy = level->FindActiveDescendant<CEnemyComponent>();
```

---

### Find Multiple Components

```cpp
// Find all descendants of type T
std::vector<T*> FindDescendants<T>()

// Find all active descendants of type T
std::vector<T*> FindActiveDescendants<T>()
```

**Example**:
```cpp
auto meshes = level->FindDescendants<CMeshComponent>();
auto activeCameras = level->FindActiveDescendants<CCameraComponent>();
```

---

## Comparison with Direct Children Methods

| Need | Use This |
|------|----------|
| First direct child | `FindChild<T>()` |
| All direct children | `FindChildren<T>()` |
| **First in hierarchy** | **`FindDescendant<T>()`** ? NEW |
| **All in hierarchy** | **`FindDescendants<T>()`** ? NEW |
| First active direct child | `FindActiveChild<T>()` |
| All active direct children | `FindActiveChildren<T>()` |
| **First active in hierarchy** | **`FindActiveDescendant<T>()`** ? NEW |
| **All active in hierarchy** | **`FindActiveDescendants<T>()`** ? NEW |

---

## Usage Patterns

### Pattern 1: Find and Use
```cpp
if (auto* camera = level->FindDescendant<CCameraComponent>())
{
    camera->SetDistance(5.0f);
}
```

### Pattern 2: Batch Operations
```cpp
auto meshes = level->FindDescendants<CMeshComponent>();
for (auto* mesh : meshes)
{
    mesh->Render();
}
```

### Pattern 3: Conditional Search
```cpp
auto activeCharacters = level->FindActiveDescendants<CCharacterComponent>();
if (activeCharacters.size() > 0)
{
    // Process characters
}
```

---

## Real-World Examples

### Example 1: Camera Discovery (Original Use Case)
```cpp
// OLD
auto* camera = FindFirstCameraInHierarchy(level);

// NEW
auto* camera = level->FindDescendant<CCameraComponent>();
```

### Example 2: Level Initialization
```cpp
// Find all entities that need initialization
auto entities = level->FindDescendants<CEntityComponent>();
for (auto* entity : entities)
{
    entity->Initialize();
}
```

### Example 3: Game Loop Update
```cpp
// Update all active physics bodies
auto physics = level->FindActiveDescendants<CPhysicsBodyComponent>();
for (auto* body : physics)
{
    body->Update(deltaTime);
}
```

### Example 4: Resource Collection
```cpp
// Count all meshes in level
int meshCount = level->FindDescendants<CMeshComponent>().size();

// Get all audio sources
auto sounds = level->FindDescendants<CAudioComponent>();
```

---

## Search Scope Reference

```
FindChild()           ? Direct child only
FindDescendant()      ? Entire subtree ?
FindDescendants()     ? Entire subtree ?
```

---

## Return Values

| Method | Returns | Empty If |
|--------|---------|----------|
| `FindDescendant<T>()` | `T*` | Not found |
| `FindActiveDescendant<T>()` | `T*` | Not found or inactive |
| `FindDescendants<T>()` | `vector<T*>` | None found |
| `FindActiveDescendants<T>()` | `vector<T*>` | None found or all inactive |

---

## Performance Tips

| Operation | Time | Notes |
|-----------|------|-------|
| `FindDescendant<T>()` | ~O(n) avg | Early exit when found |
| `FindDescendants<T>()` | O(n) | Must visit all nodes |
| Active checks | O(depth) | Per component |

**Tip**: Use `FindDescendant()` instead of `FindDescendants()` if you only need one result

---

## Files Modified

- `..\Core\ComponentSystem\ComponentSystem.h` - Added 4 new template methods
- `Game.cpp` - Updated to use new generic methods

---

## Benefits

? Generic - Works with any component type  
? Consistent - Follows existing naming patterns  
? Type-safe - Compile-time checking  
? Efficient - Early exit for single searches  
? Flexible - Single or multiple results  
? Active-aware - Optional active state filtering  

---

## Build Status

? **Successful** - Compiles without errors

---

## See Also

- `COMPONENT_HIERARCHY_SEARCH_API.md` - Full documentation
- `ComponentSystem.h` - Component class definition
