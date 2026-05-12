# Level Hierarchy Camera Discovery

## Overview

The `GameApp` now automatically searches the level hierarchy for the first `CCameraComponent` and attaches it as the active camera. This happens automatically on level load/reload.

## Features

? **Automatic Discovery** - Searches level hierarchy on initialize/load/reload  
? **Recursive Search** - Traverses entire component tree to find cameras  
? **Seamless Fallback** - Reverts to default orbit camera if no camera found  
? **No Configuration Needed** - Just add a camera component to your level  

## How It Works

### Architecture

```
GameApp::Initialize() / LoadLevel() / ReloadLevel()
    ?
    Call FindFirstCameraInHierarchy(m_RootLevel)
    ?
    Depth-first recursive search through component tree
    ?
    Found CCameraComponent?
    ?? YES ? SetActiveCamera(foundCamera)
    ?? NO  ? SetActiveCamera(nullptr)
```

### Helper Function

The `FindFirstCameraInHierarchy()` function:

```cpp
static CCameraComponent* FindFirstCameraInHierarchy(ComponentSystem::Component* root)
{
    // Check direct children first
    for (const auto& child : root->GetChildren())
    {
        if (auto* camera = dynamic_cast<CCameraComponent*>(child))
            return camera;
    }

    // Recursively search deeper
    for (const auto& child : root->GetChildren())
    {
        if (auto* foundCamera = FindFirstCameraInHierarchy(child))
            return foundCamera;
    }

    return nullptr;
}
```

- Performs **depth-first traversal**
- Returns **first camera found** (order depends on component tree structure)
- Works with any hierarchy depth

## Integration Points

### 1. GameApp::Initialize()

When the app initializes:
```cpp
m_RootLevel = componentManager->CreateComponent<CLevelComponent>();
m_RootLevel->SafeRead(levelPath);
m_lastLevelPath = levelPath;

componentManager->Initialize();

// Automatically search and attach camera
if (m_RootLevel)
{
    if (auto* levelCamera = FindFirstCameraInHierarchy(m_RootLevel))
    {
        SetActiveCamera(levelCamera);
    }
}
```

### 2. GameApp::LoadLevel()

When a new level is loaded:
```cpp
m_RootLevel = componentManager->CreateComponent<CLevelComponent>();
m_RootLevel->SafeRead(resolvedPath);
m_RootLevel->Initialize();

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

### 3. GameApp::ReloadLevel()

When the current level is reloaded:
```cpp
m_RootLevel = componentManager->CreateComponent<CLevelComponent>();
m_RootLevel->SafeRead(m_lastLevelPath);
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

## Usage

### Setup Level with Camera

Create your level with a player entity containing a camera component:

```
Level
??? Player Entity (CEntityComponent)
?   ??? CCharacterComponent
?   ??? CThirdPersonInputComponent
?   ??? CCameraComponent ? Found and auto-attached!
??? Terrain Entity
??? Enemy Entity
??? ...
```

### No Code Changes Needed

Simply add the camera component to your level file - the app will find it automatically:

```cpp
// In your level JSON or level creation code
auto* playerEntity = levelComponent->CreateChild<CEntityComponent>();
auto* cameraComp = playerEntity->CreateChild<CCameraComponent>();
cameraComp->SetDistance(5.0f);
// ... rest of setup

// When level is loaded, camera is automatically discovered and activated
```

### Manual Override

You can still manually set the camera if needed:

```cpp
// Override auto-discovery
gameApp->SetActiveCamera(myCustomCamera);

// Revert to default
gameApp->SetActiveCamera(nullptr);
```

## Search Order

The recursive search visits components in this order:

1. **Breadth-first of direct children** - Checks each immediate child
2. **Depth-first of subtrees** - Recursively searches each child's subtree
3. **Returns on first match** - Stops at the first `CCameraComponent` found

Example with this hierarchy:

```
Root
??? Entity A
?   ??? Child A1
?   ?   ??? Camera X ? Found first
?   ??? Child A2
?       ??? Camera Y
??? Entity B
    ??? Camera Z
```

The function returns **Camera X** because it's the first one encountered in depth-first order.

## Fallback Behavior

If no camera is found in the level:

```cpp
if (auto* levelCamera = FindFirstCameraInHierarchy(m_RootLevel))
{
    SetActiveCamera(levelCamera);
}
else
{
    SetActiveCamera(nullptr);  // Use default orbit camera
}
```

- Reverts to the default `m_camera` (BgfxGameCamera)
- Allows normal orbit/pan/zoom editor controls
- No errors or exceptions

## Performance

- **Complexity**: O(n) where n = number of components in level
- **When**: Only on Initialize, LoadLevel, ReloadLevel (not per-frame)
- **Cost**: Negligible - simple tree traversal

## Code Changes

### Game.cpp

Added `FindFirstCameraInHierarchy()` helper function at file scope.

Modified three methods:
- `Initialize()` - Search after level load
- `LoadLevel()` - Search in new level
- `ReloadLevel()` - Search in reloaded level

### Game.h

No changes to the public API - automatic behavior.

## Troubleshooting

### Camera not found
- Verify camera component exists in level
- Check that it's a direct or indirect child of the level
- Multiple cameras? First one found will be used

### Using wrong camera
- Check component hierarchy order
- Camera found may not be the one you expected
- Use `GetActiveCamera()` to verify

### Switch cameras manually
```cpp
auto* desiredCamera = /* find your camera */;
gameApp->SetActiveCamera(desiredCamera);
```

## Example Level Structure

```cpp
// Create a level with camera
auto* level = componentManager->CreateComponent<CLevelComponent>();

// Player with camera
auto* player = level->CreateChild<CEntityComponent>();
auto* character = player->CreateChild<CCharacterComponent>();
auto* input = player->CreateChild<CThirdPersonInputComponent>();
auto* camera = player->CreateChild<CCameraComponent>();  // ? Will be found
camera->SetDistance(5.0f);

// Terrain
auto* terrain = level->CreateChild<CEntityComponent>();
auto* terrainMesh = terrain->CreateChild<CMeshComponent>();

// When level initializes, camera is automatically discovered!
level->Initialize();
```

## See Also

- `CameraComponent.h/cpp` - Camera component implementation
- `Game.h/cpp` - GameApp implementation
- `Game_ActiveCamera_Usage.md` - Manual camera control
- `CAMERA_COMPONENT_USAGE.md` - Camera configuration
