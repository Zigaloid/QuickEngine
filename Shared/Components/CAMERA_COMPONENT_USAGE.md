# CameraComponent - Third-Person Camera for Character

## Overview

The `CCameraComponent` is a new component that provides third-person camera support for player characters. It automatically follows a character entity and maintains a configurable orbit distance and angle.

## Features

- **Automatic Follow**: Tracks the parent entity's position in real-time
- **Orbit Control**: Configurable yaw (left/right) and pitch (up/down) angles
- **Distance Control**: Adjustable distance from the character
- **Height Offset**: Optional vertical offset to position the camera at eye level or above
- **Smooth Updates**: Camera matrices updated each frame for smooth animation
- **Integrated with Game Camera**: Uses the same `BgfxGameCamera` class as the main game camera

## Integration with Player Character

### Step 1: Create the Camera Component

When creating your player character entity, add a camera component as a sibling or child:

```cpp
// Create player entity
auto* playerEntity = componentManager->CreateComponent<CEntityComponent>();

// Add character controller
auto* characterComp = playerEntity->CreateChild<CCharacterComponent>();
characterComp->SetCapsuleGeometry(0.5f, 0.3f);

// Add input component
auto* inputComp = playerEntity->CreateChild<CThirdPersonInputComponent>();
inputComp->SetActionManager(actionManager);

// Add camera component
auto* cameraComp = playerEntity->CreateChild<CCameraComponent>();
cameraComp->SetDistance(5.0f);      // 5 meters behind character
cameraComp->SetFOV(60.0f);          // 60 degree field of view
```

### Step 2: Apply Camera to Rendering

In your render loop, use the camera component to set the view and projection matrices:

```cpp
// In GameApp::Render()
auto* playerEntity = /* get reference to player entity */;
auto* cameraComp = playerEntity->GetComponentOfType<CCameraComponent>();

if (cameraComp)
{
    float viewMtx[16];
    float projMtx[16];

    const bgfx::Stats* stats = bgfx::getStats();
    const float aspect = static_cast<float>(stats->width) / static_cast<float>(stats->height);

    cameraComp->GetViewMatrix(viewMtx);
    cameraComp->GetProjectionMatrix(projMtx, aspect);

    bgfx::setViewTransform(0, viewMtx, projMtx);
}
```

## Configuration

### Camera Position

```cpp
// Set distance behind character (in meters)
cameraComp->SetDistance(5.0f);

// Pan camera up/down and left/right
cameraComp->Pan(0.5f, 0.2f);  // right, up

// Zoom in/out (negative = zoom in)
cameraComp->Zoom(-1.0f);      // zoom in by 1 unit
```

### Camera Projection

```cpp
// Set field of view (degrees, 10-120)
cameraComp->SetFOV(60.0f);

// Set near/far planes (for depth rendering)
cameraComp->SetNearPlane(0.1f);
cameraComp->SetFarPlane(1000.0f);
```

### Camera Orbit

```cpp
// Rotate camera around character
cameraComp->Orbit(0.05f, -0.02f);  // yaw (left/right), pitch (up/down)
```

### Direct Camera Access

For advanced control, you can access the underlying `BgfxGameCamera`:

```cpp
BgfxGameCamera& camera = cameraComp->GetCamera();
camera.Zoom(delta);  // Direct API access
```

## Default Settings

| Setting | Default Value | Range | Description |
|---------|---------------|-------|-------------|
| Distance | 5.0 m | 0.1 - 500.0 m | Distance behind character |
| FOV | 60.0° | 10° - 120° | Field of view |
| Height Offset | (0, 1.5, 0) | Any | Vertical offset above character |
| Near Plane | 0.1 m | > 0 | Near clip plane |
| Far Plane | 1000.0 m | > near | Far clip plane |

## How It Works

### Lifecycle

1. **OnInitialize()**: Component is initialized and ready to follow entity
2. **OnUpdate(double deltaTime)**: Each frame:
   - Gets parent entity's world position
   - Applies height offset to create camera target
   - Updates internal camera to look at target
   - Camera maintains configured distance and orbit angles

### Dependencies

The camera component requires:
- Parent entity with a `CTransformComponent` to track position
- The entity being part of the component hierarchy

### Automatic Follow

The camera automatically follows the character by:
1. Reading the parent entity's `CTransformComponent` world position
2. Applying the height offset to calculate the look-at target
3. Maintaining distance and angles relative to that target
4. Updating view matrix each frame

## Example: Complete Setup

```cpp
// In Game.cpp - RegisterComponents()
componentManager->RegisterComponentType<CCameraComponent>();
scheduler->RegisterComponentType<CCameraComponent>(0, "Camera");

// In Game.cpp - Initialize()
auto* playerEntity = componentManager->CreateComponent<CEntityComponent>();

// Character physics
auto* character = playerEntity->CreateChild<CCharacterComponent>();
character->SetCapsuleGeometry(0.5f, 0.3f);

// Character input
auto* input = playerEntity->CreateChild<CThirdPersonInputComponent>();
input->SetActionManager(inputActionManager);
input->SetMoveSpeed(6.0f);
input->SetJumpImpulse(7.0f);

// Third-person camera
auto* camera = playerEntity->CreateChild<CCameraComponent>();
camera->SetDistance(5.0f);
camera->SetFOV(60.0f);

// In Game.cpp - Render()
auto* camera = playerEntity->GetComponentOfType<CCameraComponent>();
if (camera)
{
    float viewMtx[16];
    float projMtx[16];

    const bgfx::Stats* stats = bgfx::getStats();
    const float aspect = static_cast<float>(stats->width) / static_cast<float>(stats->height);

    camera->GetViewMatrix(viewMtx);
    camera->GetProjectionMatrix(projMtx, aspect);

    bgfx::setViewTransform(0, viewMtx, projMtx);
}
```

## Camera Input Integration

The camera is independent of input handling. To add mouse control:

1. Create a `GameCameraController` (already exists in your project)
2. Register it with the `MouseManager`
3. Add reference to camera component for input handling

```cpp
auto* cameraController = std::make_shared<GameCameraController>(cameraComp->GetCamera());
mouseManager->AddInputHandler(cameraController);
```

## Performance

- **Update Cost**: O(1) - Fixed-cost position update per frame
- **Rendering Cost**: Included in standard matrix generation
- **Memory Overhead**: ~200 bytes per component instance

## Troubleshooting

### Camera not following character
- Verify parent entity has `CTransformComponent`
- Check that entity's transform is being updated

### Camera position incorrect
- Adjust `SetDistance()` for desired distance
- Use `Pan()` to adjust height offset
- Check height offset value (default 1.5)

### View matrix not updating
- Ensure `GetViewMatrix()` is called before `bgfx::setViewTransform()`
- Verify component is initialized and active
- Check that parent entity position is being updated

## See Also

- `BgfxGameCamera` - Underlying camera implementation
- `GameCameraController` - Mouse input handler for camera control
- `CThirdPersonInputComponent` - Character movement controller
- `CCharacterComponent` - Physics-based character controller
