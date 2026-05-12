# Level Camera Discovery - API Reference

## Public API

### GameApp Methods

```cpp
// Set the active camera (or nullptr for default)
void SetActiveCamera(CCameraComponent* camera);

// Get the currently active camera (may be nullptr)
CCameraComponent* GetActiveCamera() const;
```

## How It Works

### Automatic Discovery

When levels are loaded, the app searches the hierarchy:

```cpp
// In Initialize(), LoadLevel(), ReloadLevel()
if (m_RootLevel)
{
    if (auto* levelCamera = FindFirstCameraInHierarchy(m_RootLevel))
        SetActiveCamera(levelCamera);
    else
        SetActiveCamera(nullptr);
}
```

### Search Function

```cpp
// File scope helper in Game.cpp
static CCameraComponent* FindFirstCameraInHierarchy(
    ComponentSystem::Component* root);
```

## Usage Examples

### Example 1: Automatic Setup

```cpp
// Just create a level with a camera
auto* level = levelComponent;
auto* player = level->CreateChild<CEntityComponent>();
auto* camera = player->CreateChild<CCameraComponent>();
camera->SetDistance(5.0f);

level->Initialize();
// Camera automatically found and activated!
```

### Example 2: Manual Override

```cpp
// Get a specific camera
auto* myCamera = FindMyCamera();

// Use it
gameApp->SetActiveCamera(myCamera);

// Check what's active
if (gameApp->GetActiveCamera() == myCamera)
{
    // It's active
}
```

### Example 3: Switch Back to Default

```cpp
// Use default orbit camera
gameApp->SetActiveCamera(nullptr);
```

## When Auto-Discovery Happens

| Method | Triggers Search |
|--------|-----------------|
| `GameApp::Initialize()` | Yes - searches initial level |
| `GameApp::LoadLevel()` | Yes - searches new level |
| `GameApp::ReloadLevel()` | Yes - searches reloaded level |
| `SetActiveCamera()` | No - just updates pointer |
| `Render()` | No - just renders with active camera |

## Return Values

### FindFirstCameraInHierarchy()

```cpp
// Returns pointer to first CCameraComponent found
// or nullptr if no camera in hierarchy
CCameraComponent* camera = FindFirstCameraInHierarchy(root);

if (camera)
{
    // Found a camera
}
else
{
    // No camera in hierarchy
}
```

### GetActiveCamera()

```cpp
// Returns currently active camera component
// or nullptr if using default camera
CCameraComponent* active = gameApp->GetActiveCamera();

if (active)
{
    // Using a component camera
}
else
{
    // Using default orbit camera (m_camera)
}
```

## Search Order

Searches in depth-first order:

```
Root
?? Child 1 (checked)
?  ?? Grandchild 1A (checked)
?  ?  ?? Camera ? FOUND HERE
?  ?? Grandchild 1B (not reached)
?? Child 2 (not reached)
```

## Configuration After Discovery

Once attached, configure the camera:

```cpp
auto* camera = gameApp->GetActiveCamera();
if (camera)
{
    camera->SetDistance(7.0f);
    camera->SetFOV(45.0f);
    camera->Pan(1.0f, 0.5f);
    camera->Zoom(-2.0f);
}
```

## Level Structure

Recommended hierarchy:

```
Level
??? Player Entity
    ??? CCharacterComponent
    ??? CThirdPersonInputComponent
    ??? CCameraComponent ? Automatically found
```

Camera can be at any depth:

```
Level
??? Parent 1
?   ??? Parent 2
?       ??? Player Entity
?           ??? CCameraComponent ? Still found
```

## Key Behaviors

| Situation | Behavior |
|-----------|----------|
| No camera in level | Uses default `m_camera` |
| Camera found in level | Automatically activated |
| Multiple cameras | First found (depth-first) is used |
| `SetActiveCamera(nullptr)` | Reverts to default `m_camera` |
| Load new level | Searches and updates camera |
| Reload level | Searches and updates camera |

## Common Operations

### Check if component camera is active
```cpp
if (gameApp->GetActiveCamera())
{
    // Component camera is active
}
```

### Force use of default camera
```cpp
gameApp->SetActiveCamera(nullptr);
```

### Get active camera and control it
```cpp
auto* cam = gameApp->GetActiveCamera();
if (cam)
{
    cam->Orbit(0.1f, -0.05f);
}
```

### Switch to a specific camera
```cpp
auto* newCamera = player->FindChild<CCameraComponent>();
if (newCamera)
{
    gameApp->SetActiveCamera(newCamera);
}
```

## Build Status

? **Successful** - Compiles without errors

## Related Documentation

- `GameApp_LevelCamera_AutoDiscovery.md` - Full documentation
- `Game_ActiveCamera_Usage.md` - Manual camera control
- `CameraComponent.h` - Camera component interface
