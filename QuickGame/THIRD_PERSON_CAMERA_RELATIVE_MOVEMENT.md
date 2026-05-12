# CThirdPersonInputComponent - Camera-Relative Movement Guide

## Overview

The `CThirdPersonInputComponent` now supports **camera-relative movement**, allowing character movement to be relative to the camera's viewing direction rather than fixed world axes. This is the standard behavior for third-person games like Zelda, Uncharted, and most modern action games.

## Features Added

### 1. Automatic Camera Discovery
- Automatically finds `CCameraComponent` in parent entity
- No additional setup required
- Falls back gracefully if no camera is available

### 2. Camera-Relative Movement
- Movement is relative to camera facing direction
- Forward input moves in the direction the camera is looking
- Right input moves perpendicular to camera view
- Enables intuitive, camera-centric gameplay

### 3. Configurable Behavior
- Toggle camera-relative movement on/off
- Manually override camera if needed
- Get current camera reference

## API

### Methods

#### Camera Management

```cpp
// Get the camera component
CCameraComponent* GetCamera() const { return m_camera; }

// Manually set the camera component
void SetCamera(CCameraComponent* camera) { m_camera = camera; }

// Check if camera-relative movement is enabled
bool GetUseCameraRelativeMovement() const 
    { return m_useCameraRelativeMovement; }

// Enable/disable camera-relative movement
void SetUseCameraRelativeMovement(bool use) 
    { m_useCameraRelativeMovement = use; }
```

#### Private Helper Methods

```cpp
// Find camera in parent entity (called automatically)
CCameraComponent* FindCamera() const;

// Transform velocity to camera-relative coordinates
JPH::Vec3 GetCameraRelativeVelocity(const JPH::Vec3& inputVelocity) const;
```

## How It Works

### Movement Transformation

The character input is transformed from world-aligned to camera-relative:

```
1. Get player input (MoveX, MoveY)
2. Convert to world velocity (GetDesiredVelocity)
3. If camera-relative enabled and camera exists:
   - Extract camera's forward and right vectors
   - Project onto horizontal plane
   - Transform velocity using camera directions
4. Apply to character physics
```

### Camera Direction Extraction

```cpp
// From camera's view matrix:
forward = camera view's -Z direction
right = camera view's X direction

// Project to horizontal plane (ignore Y)
forward.y = 0
right.y = 0

// Input transformation:
newVelocity = right * inputX + forward * (-inputZ)
```

### Example

**Camera facing north (0°)**:
- Forward input ? moves north
- Right input ? moves east

**Camera facing east (90°)**:
- Forward input ? moves east  
- Right input ? moves south

**Camera facing northeast (45°)**:
- Forward input ? moves northeast
- Right input ? moves southeast

## Default Behavior

By default, `m_useCameraRelativeMovement` is **enabled** (`true`). This means:

- If a camera is found ? uses camera-relative movement
- If no camera is found ? falls back to world-aligned movement
- Can be disabled with `SetUseCameraRelativeMovement(false)`

## Usage Examples

### Example 1: Default Setup

```cpp
// Just create the components
auto* inputComp = entity->CreateChild<CThirdPersonInputComponent>();
auto* cameraComp = entity->CreateChild<CCameraComponent>();

inputComp->SetActionManager(actionManager);
cameraComp->SetDistance(5.0f);

// Camera-relative movement is automatically enabled!
```

### Example 2: Disable Camera-Relative Movement

```cpp
auto* inputComp = entity->GetComponentOfType<CThirdPersonInputComponent>();
inputComp->SetUseCameraRelativeMovement(false);
// Movement is now world-aligned
```

### Example 3: Manual Camera Control

```cpp
auto* inputComp = entity->GetComponentOfType<CThirdPersonInputComponent>();

// Check if camera is available
if (auto* camera = inputComp->GetCamera())
{
    camera->SetDistance(7.0f);
    camera->Orbit(0.05f, -0.02f);
}
```

### Example 4: Override Camera

```cpp
auto* inputComp = entity->GetComponentOfType<CThirdPersonInputComponent>();
auto* specificCamera = level->FindDescendant<CCameraComponent>();

if (specificCamera)
{
    inputComp->SetCamera(specificCamera);
}
```

## Integration with Game Loop

### Typical Third-Person Setup

```cpp
// Initialize
auto* player = level->CreateChild<CEntityComponent>();

// Physics & Character
auto* character = player->CreateChild<CCharacterComponent>();
character->SetCapsuleGeometry(0.5f, 0.3f);

// Input (with automatic camera discovery)
auto* input = player->CreateChild<CThirdPersonInputComponent>();
input->SetActionManager(actionManager);
input->SetMoveSpeed(6.0f);
input->SetJumpImpulse(7.0f);

// Camera (automatically found and used)
auto* camera = player->CreateChild<CCameraComponent>();
camera->SetDistance(5.0f);
camera->SetFOV(60.0f);

// Initialization
player->Initialize();

// That's it! Movement is now camera-relative
// - Forward input moves where camera looks
// - Right input strafes perpendicular to camera
// - Camera is automatically used for movement direction
```

## Input Action Mapping

The action mappings remain the same, but their effect is now camera-relative:

| Action | World-Aligned | Camera-Relative |
|--------|---------------|-----------------|
| MoveX (right) | Moves right in world | Moves right relative to camera |
| MoveY (forward) | Moves forward -Z | Moves in camera's forward direction |
| Jump | Vertical impulse | Vertical impulse (same) |
| Sprint | Speed multiplier | Speed multiplier (same) |

## Implementation Details

### Camera Vector Extraction

```cpp
// From view matrix (column-major):
forward = normalize((-viewMtx[2], -viewMtx[6], -viewMtx[10]))
right = normalize((viewMtx[0], viewMtx[4], viewMtx[8]))

// Project to horizontal
forward.y = 0
right.y = 0
renormalize both
```

### Velocity Transformation

```cpp
input_velocity = (moveX, 0, -moveZ)

camera_relative = right * moveX + forward * (-moveZ)
                = camera_right * input.x + camera_forward * (-input.z)
```

### Preserve Gravity

```cpp
// Input velocity Y is always preserved
// Only X and Z are transformed
return JPH::Vec3(transformed.x, inputVelocity.GetY(), transformed.z)
```

## Performance

- **Camera Discovery**: O(n) on first update (then cached)
- **Movement Transformation**: O(1) per frame
- **View Matrix Extraction**: O(1)
- **Overall Impact**: Negligible (~1-2 microseconds per update)

## Edge Cases

### No Camera Available
```cpp
if (!m_camera && m_useCameraRelativeMovement)
{
    m_camera = FindCamera();  // Try to find it again
}

// Falls back to world-aligned if still not found
if (m_useCameraRelativeMovement && m_camera)
{
    desiredVelocity = GetCameraRelativeVelocity(desiredVelocity);
}
```

### Camera Straight Down/Up
- If pitch is ±90°, forward/right vectors are nearly parallel
- Still works, but movement will be very sideways
- Generally avoided in game design (max pitch ~85°)

### Zero Input
- Velocity is zero, no transformation needed
- No issues

## Troubleshooting

### Character Not Moving Right

**Symptom**: Movement doesn't feel right relative to camera

**Solutions**:
```cpp
// 1. Check if camera-relative is enabled
if (!inputComp->GetUseCameraRelativeMovement())
    inputComp->SetUseCameraRelativeMovement(true);

// 2. Check if camera exists
if (!inputComp->GetCamera())
{
    // Try to find it
    auto* cam = entity->FindChild<CCameraComponent>();
    inputComp->SetCamera(cam);
}

// 3. Check movement input values
// Use game console to debug input
```

### Movement Feels Sluggish

**Symptom**: Camera-relative transformation seems to slow movement

**Solution**: This is usually input blending, not the camera transformation
```cpp
// Increase velocity blend factor for snappier response
inputComp->SetVelocityBlendFactor(0.5f);  // More responsive
```

### Camera Not Found

**Symptom**: Camera-relative movement not working

**Solution**: Make sure camera is in the entity hierarchy
```cpp
// Should be a child of the same parent as input component
auto* player = /*entity*/;
auto* input = player->CreateChild<CThirdPersonInputComponent>();
auto* camera = player->CreateChild<CCameraComponent>();  // Same parent!
```

## Advanced: Custom Movement Direction

To use a custom direction instead of camera-relative:

```cpp
// Disable camera-relative movement
inputComp->SetUseCameraRelativeMovement(false);

// Now movement is world-aligned
// Or override SetCamera() to use a different camera
```

## Comparison: World-Aligned vs Camera-Relative

### World-Aligned Movement
```
        N (forward -Z)
        ?
W ? E (right X)
```
- Fixed grid-based movement
- Same input always goes same direction
- Used in isometric or top-down games

### Camera-Relative Movement (Now Supported!)
```
Camera facing NE:
    \?/ (camera forward)
   ? ?  (camera right/left)

Forward input ? NE direction
Right input ? SE direction
```
- Intuitive for third-person action games
- Movement follows camera direction
- Industry standard for modern games

## See Also

- `CCameraComponent` - Camera component documentation
- `CCharacterComponent` - Character physics
- `BgfxGameCamera` - Underlying camera implementation
- ThirdPersonInputComponent Documentation
