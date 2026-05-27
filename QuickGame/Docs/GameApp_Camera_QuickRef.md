# GameApp Camera System - Quick Reference

## What Changed

The `GameApp` class now supports using a `CCameraComponent` as the active camera for rendering.

## API

### Set Active Camera
```cpp
gameApp->SetActiveCamera(cameraComponent);
```

### Get Active Camera
```cpp
CCameraComponent* activeCamera = gameApp->GetActiveCamera();
```

## How It Works

| Scenario | What Happens |
|----------|--------------|
| `m_activeCamera = nullptr` | Uses default `m_camera` (orbit/pan/zoom) |
| `m_activeCamera = valid CCameraComponent*` | Uses component camera's view/projection |

## Complete Example

```cpp
// 1. Create player entity with camera
auto* playerEntity = componentManager->CreateComponent<CEntityComponent>();

auto* character = playerEntity->CreateChild<CCharacterComponent>();
character->SetCapsuleGeometry(0.5f, 0.3f);

auto* input = playerEntity->CreateChild<CThirdPersonInputComponent>();
input->SetActionManager(inputActionManager);

auto* camera = playerEntity->CreateChild<CCameraComponent>();
camera->SetDistance(5.0f);
camera->SetFOV(60.0f);

// 2. Activate it
SetActiveCamera(camera);

// 3. Done! Renderer will use it automatically
```

## Switching Cameras

```cpp
// To player camera
SetActiveCamera(playerEntity->FindChild<CCameraComponent>());

// Back to default
SetActiveCamera(nullptr);
```

## From Console Command

```cpp
funcManager->RegisterFunction<void>(
    "Camera_ActivatePlayer", 
    [this]() { 
        auto* player = /* get player */;
        auto* cam = player->FindChild<CCameraComponent>();
        if (cam) SetActiveCamera(cam);
    });

funcManager->RegisterFunction<void>(
    "Camera_ActivateDefault", 
    [this]() { 
        SetActiveCamera(nullptr);
    });
```

## Render Loop

The `Render()` method now does this:

```cpp
if (m_activeCamera)
    m_activeCamera->GetViewMatrix/ProjectionMatrix(...)
else
    m_camera.GetViewMatrix/ProjectionMatrix(...)

bgfx::setViewTransform(0, viewMtx, projMtx);
```

## Key Points

? Simple on/off switching  
? No overhead when not in use  
? Always have fallback camera  
? Works with existing systems  
? Zero breaking changes  

## Files Modified

| File | Changes |
|------|---------|
| `Game.h` | Added `m_activeCamera`, `SetActiveCamera()`, `GetActiveCamera()` |
| `Game.cpp` | Modified `Render()` to use active camera |

## Build Status

? **Successful** - All changes compile without errors
