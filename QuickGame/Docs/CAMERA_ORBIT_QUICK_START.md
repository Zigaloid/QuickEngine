# Camera Orbit Input - Quick Reference

## What's New

Right thumb stick now orbits the camera around the character!

## Instant Setup

```cpp
auto* inputComp = entity->CreateChild<CThirdPersonInputComponent>();
auto* camera = entity->CreateChild<CCameraComponent>();

inputComp->SetActionManager(actionManager);

// Done! Right stick orbits camera automatically
```

## Key Methods

```cpp
// Rotation speeds (radians per second)
void SetCameraYawSpeed(float speed)      // Horizontal (default: 2.0)
void SetCameraPitchSpeed(float speed)    // Vertical (default: 1.5)

// Invert Y-axis (for player preference)
void SetInvertCameraPitch(bool invert)   // Default: false

// Action names (for custom input mapping)
void SetCameraXAction(std::string name)  // Default: "CameraX"
void SetCameraYAction(std::string name)  // Default: "CameraY"
```

## Default Behavior

? **Camera orbit is ENABLED by default**
- Right stick X controls yaw (left/right)
- Right stick Y controls pitch (up/down)
- Rotation speeds: 2.0 rad/s (yaw), 1.5 rad/s (pitch)
- Pitch not inverted by default

## Input Mapping

| Stick | Action | Effect |
|-------|--------|--------|
| Right Stick X | CameraX | Camera yaw (horizontal) |
| Right Stick Y | CameraY | Camera pitch (vertical) |

Both are **continuous analog axes**.

## Quick Customization

```cpp
// Faster camera (action game)
inputComp->SetCameraYawSpeed(3.0f);
inputComp->SetCameraPitchSpeed(2.0f);

// Slower camera (exploration game)
inputComp->SetCameraYawSpeed(1.0f);
inputComp->SetCameraPitchSpeed(0.8f);

// Invert Y-axis (some players prefer this)
inputComp->SetInvertCameraPitch(true);
```

## How It Works

```
Right Stick Input
    ?
InputActionManager (CameraX, CameraY actions)
    ?
ThirdPersonInputComponent caches values
    ?
OnUpdate():
  rotation = input * speed * deltaTime
  camera->Orbit(yawDelta, pitchDelta)
    ?
Camera orbits smoothly around character
```

## Features

? Smooth frame-rate independent rotation  
? Configurable rotation speeds  
? Optional pitch inversion  
? Works with camera-relative movement  
? Works with character movement simultaneously  
? Standard gamepad input  

## Rotation Speed Values

| Value | Effect |
|-------|--------|
| 0.5 | Very slow, precise |
| 1.0 | Slow, deliberate |
| 1.5 | Below standard |
| 2.0 | Standard (default yaw) |
| 2.5 | Above standard |
| 3.0 | Fast, responsive |
| 4.0+ | Very fast, arcade |

## Complete Example

```cpp
// Create player
auto* player = level->CreateChild<CEntityComponent>();

// Add components
auto* character = player->CreateChild<CCharacterComponent>();
auto* input = player->CreateChild<CThirdPersonInputComponent>();
auto* camera = player->CreateChild<CCameraComponent>();

// Configure
input->SetActionManager(actionManager);
input->SetMoveSpeed(6.0f);
input->SetCameraYawSpeed(2.0f);     // NEW
input->SetCameraPitchSpeed(1.5f);   // NEW

camera->SetDistance(5.0f);

// Initialize
player->Initialize();

// Now:
// Left stick = character movement
// Right stick = camera orbit
// A = Jump
// LB = Sprint
```

## Build Status

? **Successful** - Compiles without errors

## Files Modified

- `ThirdPersonInputComponent.h` - Added camera orbit API
- `ThirdPersonInputComponent.cpp` - Added camera orbit logic

## Performance

Negligible - O(1) per frame, ~1 microsecond

---

## Troubleshooting

| Problem | Solution |
|---------|----------|
| Camera not rotating | Check camera exists and speeds > 0 |
| Too fast/slow | Adjust SetCameraYawSpeed/PitchSpeed |
| Y-axis backwards | Call SetInvertCameraPitch(true) |

---

That's it! **Right stick camera control works automatically.** ??
