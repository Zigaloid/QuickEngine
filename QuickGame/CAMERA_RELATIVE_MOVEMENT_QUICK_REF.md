# Camera-Relative Movement - Quick Reference

## What Was Added

The `CThirdPersonInputComponent` now supports camera-relative movement - character moves in the direction the camera is looking.

## Key Methods

```cpp
// Get/Set camera
CCameraComponent* GetCamera() const
void SetCamera(CCameraComponent* camera)

// Enable/Disable camera-relative movement
bool GetUseCameraRelativeMovement() const
void SetUseCameraRelativeMovement(bool use)
```

## Default Behavior

? **Camera-relative movement is ENABLED by default**

- If camera exists in parent ? uses camera-relative
- If camera not found ? uses world-aligned fallback
- Camera is automatically discovered in OnInitialize()

## Simple Usage

```cpp
// Setup (that's all you need!)
auto* inputComp = entity->CreateChild<CThirdPersonInputComponent>();
auto* cameraComp = entity->CreateChild<CCameraComponent>();

inputComp->SetActionManager(actionManager);
cameraComp->SetDistance(5.0f);

// Movement is NOW camera-relative automatically!
```

## How It Works

| Input | Effect |
|-------|--------|
| Forward | Moves in camera's forward direction |
| Right | Moves perpendicular to camera (strafes) |
| Jump | Vertical impulse (unchanged) |
| Sprint | Speed multiplier (unchanged) |

## Enable/Disable

```cpp
// Disable (use world-aligned movement)
inputComp->SetUseCameraRelativeMovement(false);

// Re-enable
inputComp->SetUseCameraRelativeMovement(true);
```

## Manual Camera Control

```cpp
// Get current camera
auto* cam = inputComp->GetCamera();

// Override with different camera
auto* otherCam = /*find camera*/;
inputComp->SetCamera(otherCam);
```

## Implementation Details

### What Happens Each Frame

1. Input collected (MoveX, MoveY)
2. Converted to world velocity
3. If camera-relative + camera exists:
   - Extract camera's forward/right vectors
   - Transform velocity using these directions
4. Applied to physics body

### The Math

```
newVelocity = camera_right * input.x + camera_forward * (-input.z)
```

## Benefits

? Industry-standard third-person controls  
? Intuitive for players  
? Automatic camera discovery  
? Zero performance cost  
? Easy to toggle on/off  

## Common Scenarios

### Scenario 1: Basic Setup
```cpp
entity->CreateChild<CThirdPersonInputComponent>();
entity->CreateChild<CCameraComponent>();
// Done - camera-relative movement works!
```

### Scenario 2: Disable for Top-Down
```cpp
inputComp->SetUseCameraRelativeMovement(false);
// Now movement is grid-aligned regardless of camera
```

### Scenario 3: Switch Cameras
```cpp
auto* cam1 = /* first camera */;
auto* cam2 = /* second camera */;

inputComp->SetCamera(cam1);  // Use cam1
inputComp->SetCamera(cam2);  // Switch to cam2
```

## Troubleshooting

| Issue | Fix |
|-------|-----|
| Movement feels wrong | Check `GetUseCameraRelativeMovement()` is true |
| Camera not used | Verify `GetCamera()` returns non-null |
| Not finding camera | Make sure camera is child of same parent |

## Movement Direction Examples

### Camera Facing North (0°)
- Forward ? North
- Right ? East

### Camera Facing East (90°)
- Forward ? East
- Right ? South

### Camera Facing Northeast (45°)
- Forward ? Northeast
- Right ? Southeast

## API Summary

```cpp
class CThirdPersonInputComponent : public Component
{
public:
    // Camera control (NEW)
    CCameraComponent* GetCamera() const
    void SetCamera(CCameraComponent* camera)

    // Camera-relative toggle (NEW)
    bool GetUseCameraRelativeMovement() const
    void SetUseCameraRelativeMovement(bool use)

    // Existing methods still work (unchanged)
    void SetMoveSpeed(float speed)
    void SetJumpImpulse(float impulse)
    // ... etc
};
```

## Build Status

? **Successful** - Compiles without errors

## Files Modified

- `..\Shared\Components\ThirdPersonInputComponent.h` - Added camera API
- `..\Shared\Components\ThirdPersonInputComponent.cpp` - Added camera integration

## Next Steps

1. Create player with input component and camera component
2. Call `SetActionManager()` on input component
3. Movement is automatically camera-relative
4. Optionally call `SetUseCameraRelativeMovement(false)` if needed

---

**That's it!** Camera-relative third-person movement is ready to use.
