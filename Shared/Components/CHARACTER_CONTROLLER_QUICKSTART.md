# Quick Start: Character Controller Tuning

## Basic Setup
```cpp
auto* entity = scene->CreateEntity("Player");
auto* transform = entity->CreateChild<CTransformComponent>();
auto* character = entity->CreateChild<CCharacterComponent>();
auto* input = entity->CreateChild<CThirdPersonInputComponent>();

// Wire the input manager
input->SetActionManager(actionManager);

// Set basic movement speed
input->SetMoveSpeed(6.0f);
input->SetJumpImpulse(7.0f);
```

## Game Feel Presets

### Fast-Paced Action (Doom/Quake style)
```cpp
input->SetMoveSpeed(8.0f);
input->SetSprintMultiplier(1.5f);
input->SetVelocityBlendFactor(0.4f);  // Responsive
input->SetMaxSlopeAngle(50.0f);
input->SetControlMovementDuringJump(true);
input->SetJumpImpulse(8.0f);
```

### Careful Platformer (Celeste/Hollow Knight style)
```cpp
input->SetMoveSpeed(5.0f);
input->SetSprintMultiplier(1.2f);
input->SetVelocityBlendFactor(0.15f);  // Sluggish, precise
input->SetMaxSlopeAngle(35.0f);
input->SetControlMovementDuringJump(false);
input->SetJumpImpulse(6.5f);
```

### Realistic Walk (Dark Souls style)
```cpp
input->SetMoveSpeed(4.0f);
input->SetSprintMultiplier(1.3f);
input->SetVelocityBlendFactor(0.2f);   // Heavy, grounded
input->SetMaxSlopeAngle(40.0f);
input->SetControlMovementDuringJump(true);
input->SetJumpImpulse(6.0f);
```

## Parameter Reference

### `SetVelocityBlendFactor(float 0.0-1.0)`
Controls how quickly the character accelerates/decelerates.
- **0.1** = Very smooth, sluggish (200 frames to reach full speed)
- **0.25** = Balanced (80 frames to reach full speed) ? **Default**
- **0.5** = Quick (40 frames to reach full speed)
- **1.0** = Instant (no blending, arcade feel)

**Example: Tuning responsiveness**
```cpp
// Feels floaty?
input->SetVelocityBlendFactor(0.35f);  // Increase blend

// Feels sluggish?
input->SetVelocityBlendFactor(0.15f);  // Decrease blend
```

### `SetMaxSlopeAngle(float degrees)`
Prevents character from climbing surfaces steeper than this angle.
- **30°** = Very restrictive (like ice levels)
- **45°** = Balanced, typical game ? **Default**
- **60°** = Very permissive (Parkour games)
- **90°** = Can climb vertical walls

**Example: No slope restrictions**
```cpp
input->SetMaxSlopeAngle(90.0f);  // Can climb anything
```

### `SetControlMovementDuringJump(bool)`
Determines if player can steer while airborne.
- **true** = Responsive, forgiving ? **Default**
- **false** = Committed jumps, precise platforming

**Example: Arcade style (Mega Man)**
```cpp
input->SetControlMovementDuringJump(true);
```

**Example: Precision style (Super Meat Boy)**
```cpp
input->SetControlMovementDuringJump(false);
```

### `IsGrounded()`
Query if character is currently supported by ground.
```cpp
if (input->IsGrounded()) {
    // Can jump
    // No fall damage
}
```

## Debugging Tips

### Check if movement is being applied
```cpp
bool grounded = input->IsGrounded();
bool canMove = input->GetControlMovementDuringJump() || grounded;
if (!canMove) {
    // Movement is disabled while in air
}
```

### Verify slope blocking
```cpp
// If character can't climb, check:
float maxSlope = input->GetMaxSlopeAngle();
if (maxSlope < 45.0f) {
    // Might be too restrictive
}
```

### Adjust feel if movement feels wrong
```cpp
float blend = input->GetVelocityBlendFactor();
if (blend > 0.4f) {
    // Very responsive, might feel twitchy
}
if (blend < 0.15f) {
    // Very smooth, might feel unresponsive
}
```

## Common Issues & Solutions

### "Character slides down slopes"
? Slope detection is working as intended
- Reduce slope angle to prevent sliding: `SetMaxSlopeAngle(35.0f)`
- Or check your ground plane normal

### "Movement feels sluggish"
- Increase blend factor: `SetVelocityBlendFactor(0.35f)`
- Increase move speed: `SetMoveSpeed(7.0f)`

### "Can't climb terrain"
- Increase max slope: `SetMaxSlopeAngle(60.0f)`
- Check your terrain geometry

### "Jump feels weak"
- Increase impulse: `SetJumpImpulse(8.0f)`
- Or decrease move speed if aerodynamics matter

### "Can steer mid-jump when shouldn't"
- Disable air control: `SetControlMovementDuringJump(false)`

### "Character stuck after jump"
- Make sure `IsGrounded()` returns true when touching ground
- Check if Y velocity is being modified elsewhere
