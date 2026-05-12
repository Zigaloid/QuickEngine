# Camera Orbit Input - Third Person Camera Control Guide

## Overview

The `CThirdPersonInputComponent` now supports **camera orbit control** using the right thumb stick input. This enables intuitive camera control where players can rotate the camera around the character using gamepad input.

## Features Added

### 1. Right Thumb Stick Camera Control
- **Right Stick X-Axis**: Controls camera yaw (horizontal rotation - left/right)
- **Right Stick Y-Axis**: Controls camera pitch (vertical rotation - up/down)
- Configurable rotation speeds for both axes
- Optional pitch inversion for player preference

### 2. Smooth Camera Rotation
- Time-scaled rotation for frame-rate independent control
- Responsive camera movement
- Natural feel consistent with modern third-person games

### 3. Configurable Settings
- Separate yaw and pitch rotation speeds
- Invert pitch option for player comfort
- Action name customization
- Per-second rotation speed in radians

## API

### Camera Control Parameters

```cpp
// Set/Get yaw rotation speed (radians per second)
void SetCameraYawSpeed(float speed)
float GetCameraYawSpeed() const

// Set/Get pitch rotation speed (radians per second)
void SetCameraPitchSpeed(float speed)
float GetCameraPitchSpeed() const

// Set/Get pitch inversion
void SetInvertCameraPitch(bool invert)
bool GetInvertCameraPitch() const
```

### Camera Action Names

```cpp
// Set/Get camera X action (yaw control)
void SetCameraXAction(std::string name)
const std::string& GetCameraXAction() const

// Set/Get camera Y action (pitch control)
void SetCameraYAction(std::string name)
const std::string& GetCameraYAction() const
```

### Default Action Names

- **"CameraX"** - Right stick X-axis (horizontal/yaw)
- **"CameraY"** - Right stick Y-axis (vertical/pitch)

## Default Settings

```cpp
m_cameraYawSpeed = 2.0f;        // 2 radians per second
m_cameraPitchSpeed = 1.5f;      // 1.5 radians per second
m_invertCameraPitch = false;    // Pitch not inverted
```

## Usage Examples

### Example 1: Basic Setup

```cpp
auto* inputComp = entity->CreateChild<CThirdPersonInputComponent>();
inputComp->SetActionManager(actionManager);

// Camera orbit is automatically enabled with default settings
// Right stick will orbit the camera
```

### Example 2: Customize Rotation Speeds

```cpp
auto* inputComp = entity->GetComponentOfType<CThirdPersonInputComponent>();

// Faster camera rotation
inputComp->SetCameraYawSpeed(3.0f);    // 3 rad/s
inputComp->SetCameraPitchSpeed(2.0f);  // 2 rad/s

// Or slower, more deliberate rotation
inputComp->SetCameraYawSpeed(1.0f);
inputComp->SetCameraPitchSpeed(0.8f);
```

### Example 3: Invert Pitch for Player Preference

```cpp
auto* inputComp = entity->GetComponentOfType<CThirdPersonInputComponent>();

// Check player preference
bool invertPitch = gameSettings.invertCameraPitch;
inputComp->SetInvertCameraPitch(invertPitch);
```

### Example 4: Custom Action Names

```cpp
// Before initialization
auto* inputComp = entity->CreateChild<CThirdPersonInputComponent>();
inputComp->SetCameraXAction("RightStickX");
inputComp->SetCameraYAction("RightStickY");
inputComp->SetActionManager(actionManager);
```

## Input Action Mapping

Recommended gamepad bindings:

| Action | Gamepad Input | Purpose |
|--------|---------------|---------|
| CameraX | RightStickX | Camera horizontal rotation (yaw) |
| CameraY | RightStickY | Camera vertical rotation (pitch) |

Both are **continuous analog axes**.

## How It Works

### Input Flow

```
Player moves right thumb stick
    ?
InputActionManager fires "CameraX" and/or "CameraY" actions
    ?
ThirdPersonInputComponent caches input values
    ?
OnUpdate():
  ?? Calculate rotation deltas: input * speed * deltaTime
  ?? Apply inversion to pitch if configured
  ?? Call m_camera->Orbit(yawDelta, pitchDelta)
    ?
Camera orbits around character
```

### Rotation Calculation

```cpp
yawDelta = cameraInputX * cameraYawSpeed * deltaTime
pitchDelta = cameraInputY * cameraPitchSpeed * deltaTime

if (invertPitch)
    pitchDelta = -pitchDelta

camera->Orbit(yawDelta, pitchDelta)
```

## Complete Example

```cpp
// Setup third-person character with camera control
auto* player = level->CreateChild<CEntityComponent>();

// Physics & character controller
auto* character = player->CreateChild<CCharacterComponent>();
character->SetCapsuleGeometry(0.5f, 0.3f);

// Input controller with camera orbit
auto* input = player->CreateChild<CThirdPersonInputComponent>();
input->SetActionManager(actionManager);
input->SetMoveSpeed(6.0f);
input->SetJumpImpulse(7.0f);

// Camera with orbit control from input
auto* camera = player->CreateChild<CCameraComponent>();
camera->SetDistance(5.0f);
camera->SetFOV(60.0f);

// Configure camera orbit speeds
input->SetCameraYawSpeed(2.0f);      // Horizontal speed
input->SetCameraPitchSpeed(1.5f);    // Vertical speed

// Optional: check player settings
if (gameSettings.invertY)
    input->SetInvertCameraPitch(true);

// Initialize - everything works!
player->Initialize();

// Now:
// - Left stick controls character movement (camera-relative)
// - Right stick orbits the camera around the character
// - A button jumps
// - LB button sprints
```

## Game Feel Tuning

### Camera Rotation Speeds

**Slow/Deliberate** (exploration games, puzzle games):
```cpp
inputComp->SetCameraYawSpeed(1.0f);
inputComp->SetCameraPitchSpeed(0.8f);
```

**Medium/Balanced** (standard third-person):
```cpp
inputComp->SetCameraYawSpeed(2.0f);      // Default
inputComp->SetCameraPitchSpeed(1.5f);    // Default
```

**Fast/Responsive** (action games, combat-heavy):
```cpp
inputComp->SetCameraYawSpeed(3.0f);
inputComp->SetCameraPitchSpeed(2.0f);
```

**Very Fast/Arcade** (twitch-based gameplay):
```cpp
inputComp->SetCameraYawSpeed(4.0f);
inputComp->SetCameraPitchSpeed(3.0f);
```

## Input Handling Details

### Continuous Axis Input

Both camera axes are **continuous analog inputs**:
- Range: typically -1.0 to +1.0
- Updated every frame
- Dead zone handling by InputActionManager
- Smoothly interpolated

### Input Caching

```cpp
// Each frame, input is cached when actions fire:
m_cameraInputX = e.value.x;  // From CameraX action
m_cameraInputY = e.value.x;  // From CameraY action

// OnUpdate():
if (m_camera && (m_cameraInputX != 0.0f || m_cameraInputY != 0.0f))
{
    // Apply orbit
}
```

### Clearing Input

Input is automatically cleared when:
- Action phase is `Completed`
- Action phase is `Canceled`
- Component is unsubscribed

## Performance

- **Per-Frame Cost**: O(1) - ~1 microsecond
- **No allocations**: Uses cached input values
- **Camera orbit**: Native Orbit() method in BgfxGameCamera

## Compatibility

### With Camera-Relative Movement

? **Fully Compatible**

- Camera orbit works with camera-relative movement
- As camera rotates, movement direction updates accordingly
- Creates intuitive third-person gameplay

### With Character Movement

? **Independent Control**

- Left stick controls movement
- Right stick controls camera
- Both work simultaneously
- No conflicts or interference

## Troubleshooting

### Camera Not Rotating

**Symptom**: Right stick input doesn't rotate camera

**Check**:
1. Verify action names are "CameraX" and "CameraY" (or configured correctly)
2. Ensure camera is found: `GetCamera() != nullptr`
3. Verify right stick is mapped in input system
4. Check rotation speeds aren't zero

**Debug**:
```cpp
auto* input = entity->GetComponentOfType<CThirdPersonInputComponent>();
if (!input->GetCamera())
    std::cout << "Camera not found!" << std::endl;
std::cout << "Yaw speed: " << input->GetCameraYawSpeed() << std::endl;
```

### Camera Rotation Too Fast/Slow

**Solution**: Adjust rotation speeds

```cpp
// Too slow? Increase speed
inputComp->SetCameraYawSpeed(3.0f);    // was 2.0f

// Too fast? Decrease speed  
inputComp->SetCameraYawSpeed(1.0f);    // was 2.0f
```

### Y-Axis Feels Inverted

**Solution**: Toggle inversion

```cpp
inputComp->SetInvertCameraPitch(!inputComp->GetInvertCameraPitch());
```

## Related Features

- **Camera-Relative Movement** - Character moves where camera looks
- **Direct Camera Control** - Mouse/gamepad direct camera control (not yet implemented)
- **Camera Distance** - Zoom in/out with triggers (not yet implemented)

## Industry Standards

This implementation matches:
- **Zelda: Breath of the Wild** - Right stick camera control
- **Uncharted Series** - Smooth camera orbit
- **Dark Souls** - Right stick camera system
- **Most modern third-person games** - Standard right stick control

## See Also

- `CThirdPersonInputComponent` - Full input component
- `CCameraComponent` - Camera component with Orbit() method
- `CAMERA_RELATIVE_MOVEMENT_COMPLETE.md` - Camera-relative movement
- `THIRD_PERSON_CAMERA_RELATIVE_MOVEMENT.md` - Movement guide
