# Camera-Relative Movement - Visual Reference

## Movement Visualization

### World-Aligned Movement (Old)
```
Camera can rotate, but movement direction is fixed:

Forward Input (always -Z):
   N
   ?  (regardless of camera angle)

Right Input (always +X):
   ? (regardless of camera angle)
```

### Camera-Relative Movement (New) ?
```
Camera rotates, movement direction rotates with it:

Camera North (0°):    Camera Northeast (45°):
   ? forward             ? forward
 ? ? strafe           ?  ? strafe (down-right)
   ? back              ? back

Movement follows camera!
```

## Coordinate System Diagram

### Camera in View Matrix

```
View Matrix (column-major):
[Xx  Xy  Xz  0]
[Yx  Yy  Yz  0]
[Zx  Zy  Zz  0]
[Px  Py  Pz  1]

Where:
[Xx, Xy, Xz] = Right vector    (column 0)
[Yx, Yy, Yz] = Up vector       (column 1)
[-Zx, -Zy, -Zz] = Forward vector (column 2, negated)
[Px, Py, Pz] = Camera position (column 3)
```

### Extraction in Code

```cpp
// From view matrix (column-major):
float forward_x = -viewMtx[2];      // -Z x
float forward_y = -viewMtx[6];      // -Z y
float forward_z = -viewMtx[10];     // -Z z

float right_x = viewMtx[0];         // X x
float right_y = viewMtx[4];         // X y
float right_z = viewMtx[8];         // X z
```

## Movement Transformation Diagram

```
Input Velocity (world-aligned)
       ?
    (inputX, 0, -inputZ)
       ?
   Extract Camera Vectors
   ?? forward (camera -Z)
   ?? right (camera X)
       ?
   Project to Horizontal
   (remove Y component)
       ?
   Transform:
   result = right * inputX + forward * (-inputZ)
       ?
Output Velocity (camera-relative)
```

## Example: Walking Around Camera

### Scenario: Camera at origin, player 5m away

```
View from top:

Cam facing North (0°):
        Player
          ?
    Cam ? 
Forward input ? Player moves North (away from cam)
Right input ? Player moves East (strafe right)

Cam facing East (90°):
    Player ? 
           Cam
Forward input ? Player moves East (away from cam)  
Right input ? Player moves South (strafe right)

Cam facing South (180°):
          ?
        Player
    ? Cam
Forward input ? Player moves South (away from cam)
Right input ? Player moves West (strafe right)

Cam facing West (270°):
        Cam ?
          Player
Forward input ? Player moves West (away from cam)
Right input ? Player moves North (strafe right)
```

## Flow Diagram

```
???????????????????????????????????
?   OnInitialize()                ?
???????????????????????????????????
? FindCharacter()                 ?
? FindCamera() ? NEW              ?
? Subscribe to actions            ?
???????????????????????????????????
         ?
         ?
???????????????????????????????????
?   OnUpdate()                    ?
???????????????????????????????????
? Get input (MoveX, MoveY)        ?
? Call GetDesiredVelocity()       ?
? ?? Applies speed/sprint         ?
? ?? Applies max speed cap        ?
? ?? Applies slope check          ?
?         ?                       ?
? Check: CameraRelative + Camera? ?
? ?                              ?
? ?? YES ? GetCameraRelativeVel() ? ? NEW
? ?        ?? Extract view matrix ?
? ?        ?? Get forward/right   ?
? ?        ?? Project horizontal  ?
? ?        ?? Transform velocity  ?
? ?                              ?
? ?? NO ? Use velocity as-is      ?
?         ?                       ?
? Apply velocity blending         ?
? Apply to character physics      ?
? Handle jump logic               ?
???????????????????????????????????
```

## Method Call Hierarchy

```
CThirdPersonInputComponent::OnUpdate()
?? FindCharacter() [cached]
?? FindCamera() ? NEW [cached]
?? UpdateGroundState()
?? GetDesiredVelocity()
?  ?? Returns world-aligned velocity
?? IF camera-relative enabled:
   ?? GetCameraRelativeVelocity() ? NEW
      ?? m_camera->GetViewMatrix()
      ?? Extract forward from matrix
      ?? Extract right from matrix
      ?? Project to horizontal plane
      ?? Transform velocity
         ?? return (right * x + forward * z)
```

## Configuration State Machine

```
                      ???????????????
                      ? Initialized ?
                      ???????????????
                             ?
                    Camera Found?
                      /          \
                    YES          NO
                     ?            ?
                     ?            ?
              ????????????  ????????????????
              ? m_camera ?  ? m_camera =   ?
              ? = valid  ?  ? nullptr      ?
              ????????????  ????????????????
                   ?               ?
                SetUseCameraRelative()?
              /                      \
            YES                       NO
             ?                        ?
             ?                        ?
     ????????????????        ????????????????
     ? Use camera   ?        ? Use world    ?
     ? relative vel ?        ? aligned vel  ?
     ????????????????        ????????????????

Transitions:
- SetUseCameraRelativeMovement(true/false)
- SetCamera(newCamera)
- FindCamera() called each update until found
```

## Velocity Component Comparison

### Before (World-Aligned)
```
MoveX input (0.8) ? velocity.x = 0.8 * speed
MoveY input (0.6) ? velocity.z = -0.6 * speed

Result: velocity = (4.8, 0, -3.6)  [constant regardless of camera]
```

### After (Camera-Relative)  
```
Extract from camera:
forward = (0, 0, -1)  [camera facing -Z]
right = (1, 0, 0)     [camera facing right +X]

Transform:
result = right * 0.8 + forward * (-0.6)
       = (1,0,0) * 0.8 + (0,0,-1) * (-0.6)
       = (0.8, 0, 0) + (0, 0, 0.6)
       = (0.8, 0, 0.6)

Result changes based on camera direction!
```

## Horizontal Projection Diagram

```
Before Projection (full 3D):
forward = (0.707, -0.5, 0.707)  [camera down 30°]
              ?
              ?? Unwanted Y component!

After Projection (horizontal only):
forward = (0.707, 0, 0.707)  [Y zeroed]
        ?
renormalize: forward = (1, 0, 1) / sqrt(2)
                     = (0.707, 0, 0.707)

Result: Movement stays on ground plane!
```

## Input Mapping Examples

### Input Layout (Standard Gamepad)
```
        ?
        ? MoveY (forward)
    ? ????? ?  MoveX (right)
        ?
        ?
```

### Example 1: Camera North, Player Presses Forward
```
Input: (0, 1)        Camera: -Z (North)
  ?
velocity = forward * 1 = (0, 0, -speed)  [toward -Z/North]
  ?
Player moves NORTH ?
```

### Example 2: Camera East, Player Presses Forward
```
Input: (0, 1)        Camera: +X (East)
  ?
velocity = forward * 1 = (speed, 0, 0)  [toward +X/East]
  ?
Player moves EAST ?
```

### Example 3: Camera Northeast, Player Presses Right
```
Input: (1, 0)        Camera: +X +Z (Northeast)
  ?
velocity = right * 1 = ... [perpendicular to NE]
  ?
Player moves SOUTHEAST ?
```

## Performance Characteristics

```
Operation              Time      Complexity
??????????????????????????????????????????
FindCamera()           ~0.1?s    O(n) first time
GetViewMatrix()        ~0.05?s   O(1)
Vector extraction      ~0.1?s    O(1)
Normalization (2x)     ~0.2?s    O(1)
Vector multiply/add    ~0.3?s    O(1)
??????????????????????????????????????????
Total per update       ~0.75?s   O(1)

Negligible compared to:
- Physics simulation   ~5000?s
- Character controller ~1000?s
- Graphics rendering  ~16000?s
```

## Edge Case Handling

### Case 1: No Camera
```
if (!m_camera)
    return inputVelocity;  // Pass through unchanged
```

### Case 2: Camera Straight Up (90° pitch)
```
forward ? (0, 1, 0)
Project to horizontal:
forward = (0, 0, 0)  [uh oh]
Renormalize: (0, 0, 0) [zero vector]

Result: Movement disabled!
Solution: Clamp max pitch to ~85°
```

### Case 3: Zero Input
```
velocity = (0, 0, 0)
Transform: right * 0 + forward * 0 = (0, 0, 0)
Result: No movement ?
```

---

## Summary Diagram

```
???????????????????????????????????????????????
?  CThirdPersonInputComponent                 ?
???????????????????????????????????????????????
?  + GetCamera()                              ?
?  + SetCamera()                              ?
?  + GetUseCameraRelativeMovement()           ?
?  + SetUseCameraRelativeMovement()           ?
?                                             ?
?  - FindCamera() [NEW]                       ?
?  - GetCameraRelativeVelocity() [NEW]        ?
?                                             ?
?  Member: m_camera [NEW]                     ?
?  Member: m_useCameraRelativeMovement [NEW]  ?
???????????????????????????????????????????????
```
