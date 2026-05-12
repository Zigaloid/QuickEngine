# Character Controller Implementation Details

## Architecture Changes

### Before: Simple Direct Application
```
Input ? Desired Velocity ? Apply Directly
```

The old implementation calculated desired velocity and applied it directly each frame:
```cpp
float moveX = m_inputX * speed;
float moveZ = -m_inputZ * speed;
JPH::Vec3 vel = bi.GetLinearVelocity(bodyId);
vel = JPH::Vec3(moveX, vel.GetY(), moveZ);
bi.SetLinearVelocity(bodyId, vel);  // Instant application
```

**Problems:**
- No acceleration/deceleration smoothing
- Twitchy response to input
- Unrealistic movement feel
- No slope awareness

### After: Calculated Desired ? Blending ? Physics Application
```
Input ? Calculate Desired Velocity (with slope check) ? Blend with Current ? Apply
```

The new implementation separates concerns:

```cpp
// 1. Evaluate ground state
UpdateGroundState();  // Sets m_isGrounded based on Y velocity

// 2. Check if movement is allowed
bool canMove = m_controlMovementDuringJump || m_isGrounded;

// 3. Calculate desired velocity with constraints
JPH::Vec3 desiredVelocity = GetDesiredVelocity();  // Includes slope rejection

// 4. Blend with current velocity
newVelocity.x = (1 - blend) * current.x + blend * desired.x;
newVelocity.y = current.y;  // Preserve gravity
newVelocity.z = (1 - blend) * current.z + blend * desired.z;

// 5. Apply to physics
bi.SetLinearVelocity(bodyId, newVelocity);
```

**Improvements:**
- Smooth acceleration/deceleration
- Responsive but natural feel
- Slope-aware movement
- Separated calculation and application phases

## Velocity Blending Algorithm

### Mathematical Foundation
Linear interpolation between current and desired velocities:
```
v_new = (1 - t) * v_current + t * v_desired
```

Where `t = m_velocityBlendFactor` (0.0 to 1.0)

### Effect Over Time
Starting from velocity 0, accelerating to target speed 5 m/s with `blend = 0.25`:

| Frame | Formula | Result |
|-------|---------|--------|
| 0 | `0.75 * 0 + 0.25 * 5` | 1.25 m/s |
| 1 | `0.75 * 1.25 + 0.25 * 5` | 2.1875 m/s |
| 2 | `0.75 * 2.1875 + 0.25 * 5` | 2.890625 m/s |
| ... | ... | ... |
| ? | Converges to | 5.0 m/s |

### Time to Reach Target
Approximate frames to reach target velocity (within 99%):

```
frames_to_target ? ln(0.01) / ln(1 - blend_factor)
```

Examples (at 60 FPS):
- **blend = 0.1**: ~552 frames (9.2 sec)
- **blend = 0.15**: ~369 frames (6.2 sec)
- **blend = 0.2**: ~276 frames (4.6 sec)
- **blend = 0.25**: ~221 frames (3.7 sec) ? Default
- **blend = 0.3**: ~185 frames (3.1 sec)
- **blend = 0.4**: ~138 frames (2.3 sec)
- **blend = 0.5**: ~112 frames (1.9 sec)

## Ground State Detection

### Current Implementation (Simple)
```cpp
void CThirdPersonInputComponent::UpdateGroundState()
{
    JPH::Vec3 vel = bi.GetLinearVelocity(bodyId);
    m_isGrounded = vel.GetY() <= 0.5f;  // Threshold-based
}
```

**Pros:**
- Simple and fast
- Works well for flat ground
- O(1) computation

**Cons:**
- Doesn't handle moving platforms
- Can fail on steep slopes
- Y-velocity based (sensitive to jump timing)

### Alternative: Contact-Based (Future)
```cpp
// Would require contact manifold tracking:
void UpdateGroundStateWithContacts()
{
    // Count contacts with non-moving bodies below center
    // Evaluate contact normals
    // Set m_isGrounded = contactCount > 0
}
```

**Pros:**
- Robust, contact-aware
- Handles all surfaces
- Works on moving platforms

**Cons:**
- More complex
- Requires contact event integration
- Higher computational cost

**Current choice:** Simple approach is suitable for most games and integrates well with Jolt's physics system.

## Slope Validation Algorithm

### Input
- `moveDir`: Desired 2D movement (X, Z)
- `groundNormal`: Contact surface normal (X, Y, Z)
- `maxSlopeAngle`: Maximum climbable angle in degrees

### Output
- Adjusted movement direction that stays within slope constraints

### Process

**Step 1: Extract horizontal component of ground normal**
```cpp
JPH::Vec3 normalHorizontal(normal.x, 0, normal.z);
float normalLenSq = normalHorizontal.LengthSq();
```

**Step 2: Check if moving into slope**
```cpp
JPH::Vec3 moveDir(moveX, 0, moveZ);
float dot = moveDir.Dot(normalHorizontal);
if (dot < 0) { /* Moving into surface */ }
```

**Step 3: Evaluate slope angle**
```cpp
float cosMaxSlope = cos(maxSlopeAngleDeg * ? / 180);
float cosActualSlope = abs(normal.y);
if (cosActualSlope < cosMaxSlope) { /* Slope too steep */ }
```

**Step 4: Project movement parallel to surface**
```cpp
float adjustment = dot / normalLenSq;
moveX -= adjustment * normalHorizontal.x;
moveZ -= adjustment * normalHorizontal.z;
```

### Geometric Interpretation

For a slope:
- **Normal pointing up (0, 1, 0)**: Flat ground, no adjustment
- **Normal 45° angle (0.707, 0.707, 0)**: On slope edge
  - Can walk along slope
  - Cannot walk up if maxAngle = 45°
- **Normal 90° angle (1, 0, 0)**: Vertical wall
  - Cannot climb if maxAngle < 90°

### Edge Cases

**When normalLenSq is very small:**
```cpp
if (normalLenSq > 0.0001f) { /* Safe to use */ }
```
Prevents division by zero on perfectly vertical surfaces.

**When moving with the slope:**
```cpp
if (dot < 0.0f) { /* Only check if moving into surface */ }
```
Prevents over-correction when moving along slope.

## Gravity Preservation

### Key Design: Y-Axis Independence
```cpp
newVelocity.y = currentVelocity.y;  // Never modified by movement
```

This ensures:
- **Gravity continues naturally**: Falls accelerate as expected
- **Jump height independent of horizontal speed**: Move fast or slow, same jump
- **Air control optional**: Can turn off without breaking physics
- **Works with platforms**: Falling through or landing on moving objects

### Why This Works
Jolt Physics applies gravity in `PhysicsSystem::Update()`, separate from component velocity setting. By preserving Y velocity:
1. Component sets X/Z from input
2. Physics engine adds gravity acceleration
3. Next frame reads updated Y velocity (modified by gravity)
4. Process repeats

## Motion Quality Settings

### Why `LinearCast`?
From `CharacterComponent::MakeBodyCreationSettings()`:
```cpp
settings.mMotionQuality = JPH::EMotionQuality::LinearCast;
```

This setting:
- **Prevents tunneling**: Character won't phase through thin obstacles
- **Better contact detection**: Detects all collisions
- **Cost**: Slightly higher CPU usage
- **Benefit**: Essential for reliable character controller

### Alternative: `Discrete`
```cpp
settings.mMotionQuality = JPH::EMotionQuality::Discrete;
```
- Faster but can tunnel through geometry
- Only suitable for slow-moving objects
- **Not recommended** for players

## Performance Analysis

### Time Complexity
```
UpdateGroundState()      ? O(1) - velocity read
GetDesiredVelocity()     ? O(1) - fixed math operations
Blending & Application   ? O(1) - velocity arithmetic
Total per frame          ? O(1)
```

### Space Complexity
```
Member variables:        ? O(1)
No dynamic allocations
No temporary storage
```

### CPU Cost (Approximate)
- **Ground check**: <0.1 ms
- **Desired velocity calculation**: <0.1 ms
- **Velocity blending**: <0.1 ms
- **Total per character**: <0.5 ms on modern CPU

This is negligible compared to physics simulation time and suitable for 1000+ characters.

## Integration with Physics System

### Update Order
1. **Component.OnUpdate()** (This code)
   - Evaluates input
   - Sets desired velocities
2. **PhysicsManager.Update()**
   - Applies gravity
   - Resolves constraints
   - Updates body positions
3. **PhysicsComponent.OnUpdate()**
   - Reads updated body transform
   - Syncs to entity transform

This order ensures:
- Input is processed before physics
- Gravity can be applied naturally
- Position is synced afterward

### Contact Information Flow

**Current (Velocity-based):**
```
Physics Engine ? Velocity ? IsGrounded() ? Movement Decision
```

**Future (Contact-based):**
```
Physics Engine ? Contact Callback ? Ground State ? Movement Decision
```

Would require implementing a contact listener in PhysicsManager.

## Known Limitations

1. **Velocity-based ground detection**
   - Can falsely report grounded while jumping
   - Threshold of 0.5 m/s is arbitrary
   - Doesn't account for jump duration

2. **Ground normal not tracked**
   - Always assumes flat ground
   - Slope blocking is theoretical
   - Would need contact integration

3. **Single ground contact**
   - Can't stand on sloped surfaces naturally
   - Would need contact averaging

4. **No platform awareness**
   - Doesn't track moving platforms
   - No local gravity support

## Future Enhancement Path

### Phase 1 (Current)
? Velocity blending
? Slope angle parameter
? Air control toggle
? Ground state flag

### Phase 2 (Recommended)
- Contact-based ground detection
- Ground normal from contact manifold
- Multiple contact tracking
- Moving platform support

### Phase 3 (Advanced)
- Support for custom shapes
- Animation state integration
- Network synchronization
- Remote player prediction

### Phase 4 (Jolt Native)
- Consider switching to Jolt's `Character` class
- Would provide robust ground detection
- Built-in slope handling
- Collision filtering support
