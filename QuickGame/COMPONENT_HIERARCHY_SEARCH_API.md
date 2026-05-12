# Generic Hierarchy Search Methods - API Documentation

## Overview

The `Component` base class now provides generic template methods for recursively searching the component hierarchy for any component type. These methods support both direct children and all descendants.

## New Methods Added to Component Base Class

### Searching for Single Component

#### `FindDescendant<T>()`

Recursively finds the first descendant component of type T using depth-first search.

```cpp
/** @brief Recursively finds the first descendant component of type T using depth-first search.
 *  Searches the entire hierarchy tree, not just direct children. */
template<typename T>
T* FindDescendant() const
```

**Usage**:
```cpp
// Find first CameraComponent in entire hierarchy
if (auto* camera = rootComponent->FindDescendant<CCameraComponent>())
{
    // Found a camera
}

// Find first MeshComponent
auto* mesh = entity->FindDescendant<CMeshComponent>();
```

**Returns**: 
- Pointer to first component of type T found, or `nullptr`

**Complexity**: O(n) worst case

---

#### `FindActiveDescendant<T>()`

Recursively finds the first active descendant component of type T.

```cpp
/** @brief Recursively finds the first active descendant component of type T using depth-first search.
 *  Searches the entire hierarchy tree respecting active state. */
template<typename T>
T* FindActiveDescendant() const
```

**Usage**:
```cpp
// Find first active camera in hierarchy
auto* activeCamera = level->FindActiveDescendant<CCameraComponent>();

// Only finds if both component and all ancestors are active
auto* activePhysics = entity->FindActiveDescendant<CPhysicsBodyComponent>();
```

**Returns**: 
- Pointer to first active component of type T found, or `nullptr`

**Note**: Component is considered active if both the component itself AND all ancestors are active

---

### Searching for Multiple Components

#### `FindDescendants<T>()`

Recursively finds all descendant components of type T using depth-first search.

```cpp
/** @brief Recursively finds all descendant components of type T using depth-first search.
 *  Searches the entire hierarchy tree, not just direct children. */
template<typename T>
std::vector<T*> FindDescendants() const
```

**Usage**:
```cpp
// Get all MeshComponents in the level
auto meshes = level->FindDescendants<CMeshComponent>();
for (auto* mesh : meshes)
{
    // Process each mesh
}

// Get all characters in the level
auto characters = level->FindDescendants<CCharacterComponent>();
```

**Returns**: 
- Vector of all components of type T found (empty if none)

**Complexity**: O(n) always (must traverse entire tree)

---

#### `FindActiveDescendants<T>()`

Recursively finds all active descendant components of type T.

```cpp
/** @brief Recursively finds all active descendant components of type T using depth-first search.
 *  Searches the entire hierarchy tree respecting active state. */
template<typename T>
std::vector<T*> FindActiveDescendants() const
```

**Usage**:
```cpp
// Get all active cameras in the level
auto cameras = level->FindActiveDescendants<CCameraComponent>();

// Get all active characters
auto activeCharacters = level->FindActiveDescendants<CCharacterComponent>();
for (auto* character : activeCharacters)
{
    character->Update();
}
```

**Returns**: 
- Vector of all active components of type T found (empty if none)

**Note**: Only includes components where both the component and all ancestors are active

---

## Existing Methods (for comparison)

### Direct Children Only

```cpp
// Find first direct child of type T
template<typename T> T* FindChild() const

// Find all direct children of type T
template<typename T> std::vector<T*> FindChildren() const

// Find first active direct child of type T
template<typename T> T* FindActiveChild() const

// Find all active direct children of type T
template<typename T> std::vector<T*> FindActiveChildren() const
```

---

## Method Comparison

| Method | Scope | Count | Active |
|--------|-------|-------|--------|
| `FindChild<T>()` | Direct children only | Single | Any |
| `FindChildren<T>()` | Direct children only | Multiple | Any |
| `FindActiveChild<T>()` | Direct children only | Single | Yes |
| `FindActiveChildren<T>()` | Direct children only | Multiple | Yes |
| `FindDescendant<T>()` | Entire hierarchy | Single | Any |
| `FindDescendants<T>()` | Entire hierarchy | Multiple | Any |
| `FindActiveDescendant<T>()` | Entire hierarchy | Single | Yes |
| `FindActiveDescendants<T>()` | Entire hierarchy | Multiple | Yes |

---

## Hierarchy Examples

### Example 1: Simple Search

```
Level
??? Player Entity
?   ??? CharacterComponent
?   ??? CameraComponent ? FindDescendant<CCameraComponent>()
??? Terrain Entity
    ??? MeshComponent
```

`level->FindDescendant<CCameraComponent>()` returns the CameraComponent

---

### Example 2: Depth-First Order

```
Root
??? Entity A
?   ??? Child A1
?   ?   ??? CameraComponent ? Found 1st
?   ??? Child A2
?       ??? CameraComponent (not found)
??? Entity B
    ??? CameraComponent (not found)
```

`root->FindDescendant<CCameraComponent>()` returns Camera in Child A1

---

### Example 3: Multiple Results

```
Level
??? Player
?   ??? MeshComponent ?
??? Terrain
?   ??? MeshComponent ?
??? Props
    ??? MeshComponent ?
    ??? Child
        ??? MeshComponent ?
```

`level->FindDescendants<CMeshComponent>()` returns all 4 meshes

---

### Example 4: Active State

```
Level (Active)
??? Player (Active)
?   ??? CameraComponent (Active) ?
??? DisabledEnemy (Inactive)
    ??? CameraComponent (Inactive - not found)
```

`level->FindActiveDescendant<CCameraComponent>()` finds the camera in Player but not in DisabledEnemy

---

## Use Cases

### Level Loading
```cpp
// Find all cameras in the level (for UI setup)
auto cameras = level->FindDescendants<CCameraComponent>();

// Find first active camera for gameplay
auto activeCamera = level->FindActiveDescendant<CCameraComponent>();
```

### Batch Updates
```cpp
// Update all physics bodies
auto physics = level->FindDescendants<CPhysicsBodyComponent>();
for (auto* body : physics)
{
    body->Update();
}

// Disable all enemies
auto enemies = level->FindDescendants<CEnemyComponent>();
for (auto* enemy : enemies)
{
    enemy->SetActive(false);
}
```

### Resource Management
```cpp
// Count meshes in level
auto meshCount = level->FindDescendants<CMeshComponent>().size();

// Find all audio sources
auto sounds = level->FindDescendants<CAudioComponent>();
```

### Game Logic
```cpp
// Find all collectibles
auto pickups = level->FindDescendants<CPickupComponent>();

// Find first valid spawn point
auto spawnPoint = level->FindDescendant<CSpawnPointComponent>();
```

---

## Performance Considerations

### Time Complexity
- **FindDescendant()**: O(n) average, O(n) worst case
- **FindDescendants()**: O(n) always (must visit all)
- **FindActiveDescendant()**: O(n) average, O(n) worst case
- **FindActiveDescendants()**: O(n) always (must visit all)

Where n = total components in hierarchy

### Space Complexity
- **Recursion stack**: O(depth of tree)
- **Result vector**: O(k) where k = results found

### Optimization Tips
- Use `FindDescendant()` when you only need the first match (early exit)
- Avoid repeated `FindDescendants()` calls - cache results
- Prefer `FindActiveDescendant()` if you only care about active components
- Keep hierarchies reasonably shallow (typical depth < 10)

---

## Migration Guide

### Old Pattern (using static function)
```cpp
// OLD: Static helper function
static CCameraComponent* FindFirstCameraInHierarchy(Component* root);
auto* camera = FindFirstCameraInHierarchy(level);
```

### New Pattern (using generic method)
```cpp
// NEW: Generic template method on Component
auto* camera = level->FindDescendant<CCameraComponent>();
```

**Benefits**:
- ? No static function needed
- ? Works with any component type
- ? Consistent with existing API
- ? Better IDE autocomplete
- ? Type-safe

---

## Type Safety

All methods are template-based with compile-time checks:

```cpp
// This compiles - T derives from Component
auto* camera = level->FindDescendant<CCameraComponent>();

// This will NOT compile - SomeClass doesn't derive from Component
// auto* obj = level->FindDescendant<SomeClass>();  // Error!
```

---

## See Also

- `Component::FindChild<T>()` - Find direct children
- `Component::FindChildren<T>()` - Find all direct children
- `Component::FindActiveChild<T>()` - Find active direct children
- `ComponentSystem.h` - Component base class implementation
