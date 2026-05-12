# CameraComponent Quick Reference

## Create a Camera Component

```cpp
auto* cameraComp = playerEntity->CreateChild<CCameraComponent>();
```

## Basic Configuration

```cpp
cameraComp->SetDistance(5.0f);      // How far behind the character
cameraComp->SetFOV(60.0f);          // Field of view in degrees
```

## Get Matrices for Rendering

```cpp
float viewMtx[16];
float projMtx[16];
const float aspect = (float)screenWidth / (float)screenHeight;

cameraComp->GetViewMatrix(viewMtx);
cameraComp->GetProjectionMatrix(projMtx, aspect);

bgfx::setViewTransform(0, viewMtx, projMtx);
```

## Interactive Control

```cpp
// Rotate camera around character
cameraComp->Orbit(deltaYaw, deltaPitch);

// Pan camera up/down and left/right
cameraComp->Pan(deltaRight, deltaUp);

// Zoom in/out
cameraComp->Zoom(delta);  // negative = zoom in
```

## Direct Camera Access

```cpp
BgfxGameCamera& camera = cameraComp->GetCamera();
// Use camera directly if needed
```

## Configuration Parameters

| Method | Range | Default | Purpose |
|--------|-------|---------|---------|
| `SetDistance(d)` | 0.1 - 500 | 5.0 | Distance from character |
| `SetFOV(deg)` | 10 - 120 | 60.0 | Field of view |
| `SetNearPlane(n)` | > 0 | 0.1 | Near clip plane |
| `SetFarPlane(f)` | > near | 1000.0 | Far clip plane |

## Component Registration

Already done in `Game.cpp`:

```cpp
componentManager->RegisterComponentType<CCameraComponent>();
scheduler->RegisterComponentType<CCameraComponent>(0, "Camera");
```

## Full Example

```cpp
// Create player
auto* playerEntity = componentManager->CreateComponent<CEntityComponent>();

// Add character
auto* character = playerEntity->CreateChild<CCharacterComponent>();
character->SetCapsuleGeometry(0.5f, 0.3f);

// Add input
auto* input = playerEntity->CreateChild<CThirdPersonInputComponent>();
input->SetActionManager(inputActionManager);

// Add camera ? NEW!
auto* camera = playerEntity->CreateChild<CCameraComponent>();
camera->SetDistance(5.0f);
camera->SetFOV(60.0f);

// In render loop
float viewMtx[16], projMtx[16];
const bgfx::Stats* stats = bgfx::getStats();
const float aspect = (float)stats->width / (float)stats->height;

camera->GetViewMatrix(viewMtx);
camera->GetProjectionMatrix(projMtx, aspect);
bgfx::setViewTransform(0, viewMtx, projMtx);
```

## Key Points

- ? Automatically follows parent entity
- ? Updates each frame in OnUpdate()
- ? Requires parent with CTransformComponent
- ? Uses same camera system as game camera
- ? Zero setup overhead beyond creation

## Troubleshooting

| Issue | Solution |
|-------|----------|
| Camera not following | Parent entity's transform not updating |
| Camera too close/far | Adjust `SetDistance()` |
| Camera height wrong | Modify height offset in code (default 1.5) |
| FOV looks wrong | Try values 45-90, adjust to taste |

See `CAMERA_COMPONENT_USAGE.md` for complete documentation.
