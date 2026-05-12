# Level Hierarchy Camera Discovery - Implementation Summary

## Overview

The `GameApp` now automatically searches the `m_RootLevel` component hierarchy for the first `CCameraComponent` and attaches it as the active camera during initialization and level loading.

## What Was Added

### 1. Helper Function: `FindFirstCameraInHierarchy()`

**Location**: `Game.cpp` (file scope)

```cpp
static CCameraComponent* FindFirstCameraInHierarchy(ComponentSystem::Component* root)
{
    if (!root)
        return nullptr;

    // Check direct children first
    for (const auto& child : root->GetChildren())
    {
        if (auto* camera = dynamic_cast<CCameraComponent*>(child))
        {
            return camera;
        }
    }

    // Recursively search deeper in the hierarchy
    for (const auto& child : root->GetChildren())
    {
        if (auto* foundCamera = FindFirstCameraInHierarchy(child))
        {
            return foundCamera;
        }
    }

    return nullptr;
}
```

**Features**:
- Performs depth-first recursive search
- Returns first `CCameraComponent` found
- Safe null checking
- O(n) complexity where n = number of components

### 2. Integration Points

#### GameApp::Initialize()
```cpp
// After level is initialized...
if (m_RootLevel)
{
    if (auto* levelCamera = FindFirstCameraInHierarchy(m_RootLevel))
    {
        SetActiveCamera(levelCamera);
    }
}
```

#### GameApp::LoadLevel()
```cpp
m_RootLevel->Initialize();
m_lastLevelPath = resolvedPath;

// Search new level for camera
if (m_RootLevel)
{
    if (auto* levelCamera = FindFirstCameraInHierarchy(m_RootLevel))
    {
        SetActiveCamera(levelCamera);
    }
    else
    {
        SetActiveCamera(nullptr);  // Revert to default
    }
}
```

#### GameApp::ReloadLevel()
```cpp
m_RootLevel->Initialize();

// Search reloaded level for camera
if (m_RootLevel)
{
    if (auto* levelCamera = FindFirstCameraInHierarchy(m_RootLevel))
    {
        SetActiveCamera(levelCamera);
    }
    else
    {
        SetActiveCamera(nullptr);  // Revert to default
    }
}
```

### 3. Bug Fix: extern Declaration

**Game.h**: Moved `extern GameApp theApp;` to end of file (after class definition)

This fixes compilation issues where the forward declaration couldn't reference a class that wasn't yet defined.

## How It Works

### Flow Diagram

```
GameApp::Initialize() / LoadLevel() / ReloadLevel()
    ?
    ?? Create/Load level into m_RootLevel
    ?
    ?? Initialize level
    ?
    ?? Call FindFirstCameraInHierarchy(m_RootLevel)
        ?
        ?? Traverses component tree (depth-first)
        ?
        ?? Finds first CCameraComponent
        ?
        ?? SetActiveCamera(foundCamera) or SetActiveCamera(nullptr)
            ?
            ?? Next Render() uses that camera
```

### Search Algorithm

```
DepthFirstSearch(component)
    1. Check direct children for CCameraComponent
    2. For each child, recursively search their subtree
    3. Return on first match
    4. Return nullptr if no camera found
```

## Usage Patterns

### Level Creation

```cpp
// Build a level with a player containing a camera
auto* level = componentManager->CreateComponent<CLevelComponent>();

auto* player = level->CreateChild<CEntityComponent>();
auto* character = player->CreateChild<CCharacterComponent>();
auto* input = player->CreateChild<CThirdPersonInputComponent>();
auto* camera = player->CreateChild<CCameraComponent>();  // ? Auto-discovered!

camera->SetDistance(5.0f);
camera->SetFOV(60.0f);

level->Initialize();
// Camera automatically attached to app!
```

### Manual Override

```cpp
// If you need a specific camera
auto* myCamera = FindSpecificCamera();
gameApp->SetActiveCamera(myCamera);

// Back to default
gameApp->SetActiveCamera(nullptr);
```

### Check Active Camera

```cpp
if (auto* activeCamera = gameApp->GetActiveCamera())
{
    // Camera component is active
}
else
{
    // Using default orbit camera
}
```

## Behavior

| Scenario | Behavior |
|----------|----------|
| Camera found in level | Automatically activated as scene camera |
| No camera in level | Falls back to default orbit camera (m_camera) |
| Multiple cameras in level | First found (depth-first order) is used |
| Manual SetActiveCamera() called | Overrides auto-discovery |
| Load new level | Searches new level and updates active camera |
| Reload level | Searches reloaded level and updates active camera |

## Key Properties

? **Automatic** - No code needed to activate camera  
? **Flexible** - Works with any level structure  
? **Fallback** - Always have default orbit camera  
? **Non-Breaking** - Existing code continues to work  
? **Efficient** - Only runs on Initialize/Load/Reload  
? **Overridable** - Manual SetActiveCamera() always wins  

## Rendering Integration

The `Render()` method was already modified to use the active camera:

```cpp
void GameApp::Render(double deltaTime)
{
    // ... setup ...

    if (m_activeCamera)
    {
        m_activeCamera->GetViewMatrix(viewMtx);
        m_activeCamera->GetProjectionMatrix(projMtx, aspect);
    }
    else
    {
        m_camera.GetViewMatrix(viewMtx);
        m_camera.GetProjectionMatrix(projMtx, aspect);
    }

    bgfx::setViewTransform(0, viewMtx, projMtx);
    // ... render scene ...
}
```

## Files Modified

| File | Changes |
|------|---------|
| `Game.cpp` | Added `FindFirstCameraInHierarchy()` helper; modified Initialize(), LoadLevel(), ReloadLevel() |
| `Game.h` | Moved `extern GameApp theApp;` to end of file |

## Build Status

? **Successful** - 0 errors, 0 warnings

## Example Level Structure

```
Level (CLevelComponent)
??? Player (CEntityComponent)
?   ??? CCharacterComponent
?   ?   ??? [Physics capsule for movement]
?   ??? CThirdPersonInputComponent
?   ?   ??? [Handles input, controls movement]
?   ??? CCameraComponent ? FOUND AND ACTIVATED
?       ??? [Follows player, provides view/projection]
?
??? Terrain (CEntityComponent)
?   ??? CMeshComponent
?   ??? CPhysicsBodyComponent
?
??? NPC (CEntityComponent)
?   ??? CMeshComponent
?   ??? CCharacterComponent
?
??? Props
    ??? [Various entities]
```

## Performance Notes

- **Time Complexity**: O(n) where n = total components in level
- **When**: Only on Initialize, LoadLevel, ReloadLevel
- **Not Per-Frame**: No performance impact on rendering loop
- **Early Exit**: Returns on first camera found

## Next Steps

1. Create your level with a player entity
2. Add `CCameraComponent` as a child of the player
3. Configure camera distance, FOV, offset as needed
4. Load/initialize the level
5. Camera automatically discovered and used!

## Troubleshooting

### Camera not found
- Verify camera component exists in level
- Check it's attached to a component in the level
- Use `GetActiveCamera()` to verify state

### Wrong camera being used
- Multiple cameras detected; first found is used
- Check hierarchy structure and component order

### Using default camera
- No camera found in level hierarchy
- Add a `CCameraComponent` to the level

## See Also

- `CameraComponent.h/cpp` - Camera component
- `Game.h/cpp` - GameApp implementation
- `Game_ActiveCamera_Usage.md` - Manual camera control
- `GameApp_LevelCamera_AutoDiscovery.md` - Detailed documentation
