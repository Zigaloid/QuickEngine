# Using CCameraComponent as the Active Game Camera

## Overview

The `GameApp` now supports using a `CCameraComponent` as the active camera for rendering. This allows you to:

- Use a camera component attached to your player character for third-person view
- Dynamically switch between different cameras
- Fall back to the default orbit camera when no component camera is active

## How It Works

### Architecture

```
GameApp::Render()
    ?
    Has m_activeCamera set?
    ?? YES ? Use m_activeCamera (CCameraComponent)
    ?? NO  ? Use m_camera (BgfxGameCamera default)
    ?
    Apply view/projection matrices
    ?
    Render scene
```

### Key Components

1. **Default Camera** (`m_camera`)
   - `BgfxGameCamera` instance
   - Always available as fallback
   - Used for orbit/pan/zoom control in editor mode

2. **Active Camera** (`m_activeCamera`)
   - Optional `CCameraComponent*`
   - Takes precedence over default camera when set
   - Can be attached to player character

## Usage

### Setting the Active Camera

```cpp
// Get the camera component from the player entity
auto* playerEntity = /* get player entity */;
auto* cameraComp = playerEntity->FindChild<CCameraComponent>();

if (cameraComp)
{
    // Set it as the active camera for rendering
    gameApp->SetActiveCamera(cameraComp);
}
```

### Checking the Active Camera

```cpp
// Get the currently active camera
CCameraComponent* activeCamera = gameApp->GetActiveCamera();

if (activeCamera)
{
    // A camera component is active
}
else
{
    // Using default orbit camera
}
```

### Switching Back to Default Camera

```cpp
// Stop using camera component, return to default
gameApp->SetActiveCamera(nullptr);
```

## Complete Example

Here's how to set up a player character with a camera and activate it:

```cpp
// In GameApp::Initialize()

auto* componentManager = Core::CoreSystem::GetComponentManager();

// Create player entity
auto* playerEntity = componentManager->CreateComponent<CEntityComponent>();

// Add character controller
auto* characterComp = playerEntity->CreateChild<CCharacterComponent>();
characterComp->SetCapsuleGeometry(0.5f, 0.3f);

// Add input controller
auto* inputComp = playerEntity->CreateChild<CThirdPersonInputComponent>();
inputComp->SetActionManager(inputActionManager);
inputComp->SetMoveSpeed(6.0f);
inputComp->SetJumpImpulse(7.0f);

// Add camera component
auto* cameraComp = playerEntity->CreateChild<CCameraComponent>();
cameraComp->SetDistance(5.0f);      // 5 meters behind
cameraComp->SetFOV(60.0f);          // 60 degree field of view

// Set it as the active camera!
SetActiveCamera(cameraComp);

// Initialize everything
componentManager->Initialize();
```

## Switching Between Cameras

You can easily switch between cameras at runtime:

```cpp
// In a console command or input handler
void SwitchToPlayerCamera()
{
    auto* componentManager = Core::CoreSystem::GetComponentManager();
    auto* playerEntity = /* get player entity */;
    auto* cameraComp = playerEntity->FindChild<CCameraComponent>();

    if (cameraComp)
    {
        SetActiveCamera(cameraComp);  // Use player camera
    }
}

void SwitchToEditorCamera()
{
    SetActiveCamera(nullptr);  // Use default orbit camera
}
```

## Camera Control

### When Using Default Camera (m_camera)

- **Left-drag**: Orbit
- **Middle-drag**: Pan
- **Scroll**: Zoom
- Controlled by `GameCameraController` and mouse input

### When Using Component Camera (m_activeCamera)

The camera automatically follows the parent entity's position and applies configured offset/distance. You can control it via:

```cpp
auto* cameraComp = gameApp->GetActiveCamera();
if (cameraComp)
{
    // Adjust distance
    cameraComp->SetDistance(7.0f);

    // Adjust FOV
    cameraComp->SetFOV(45.0f);

    // Programmatic control
    cameraComp->Orbit(0.05f, -0.02f);
    cameraComp->Pan(0.5f, 0.2f);
    cameraComp->Zoom(-1.0f);
}
```

## Default Behavior

- On startup, `m_activeCamera` is `nullptr`
- The default `m_camera` is used for rendering
- Attach the mouse controller to `m_camera` for interactive orbit/pan/zoom
- When you set `m_activeCamera`, it immediately takes over rendering next frame

## Code Changes

### Game.h

```cpp
// Add forward declaration
class CCameraComponent;

// Add to GameApp class
private:
    CCameraComponent*         m_activeCamera = nullptr;

public:
    void SetActiveCamera(CCameraComponent* camera) { m_activeCamera = camera; }
    CCameraComponent* GetActiveCamera() const { return m_activeCamera; }
```

### Game.cpp::Render()

```cpp
// Use active camera component if set, otherwise use default m_camera
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
```

## Advantages

? **Flexible Camera System**
- Multiple cameras supported via components
- Easy to switch at runtime
- Always have a fallback (default orbit camera)

? **Clean Integration**
- No breaking changes to existing code
- Optional feature - use it when you need it
- Works alongside existing GameCameraController

? **Easy to Extend**
- Can create different camera components for different game modes
- Cinematic cameras, strategy cameras, etc.
- All with the same rendering pipeline

## Troubleshooting

### Camera not updating
- Verify the camera component's parent entity is being updated
- Check that `SetActiveCamera()` was called with the correct component
- Ensure the component is initialized before being set as active

### Wrong view/projection matrices
- Check camera component's distance, FOV, and offset settings
- Verify the parent entity's transform is updating correctly
- Look for exceptions in the camera component's OnUpdate()

### Switched to component camera but still seeing editor camera
- Make sure `SetActiveCamera(nullptr)` was called, not just `SetActiveCamera(m_camera)`
- The `m_activeCamera` pointer should be `nullptr` to use the default camera
- Check that no other code is resetting it to `nullptr`

## See Also

- `CameraComponent.h/cpp` - Camera component implementation
- `BgfxGameCamera.h` - Default orbit camera
- `GameCameraController.h` - Mouse input handler for default camera
- `CThirdPersonInputComponent.h` - Character movement controller
