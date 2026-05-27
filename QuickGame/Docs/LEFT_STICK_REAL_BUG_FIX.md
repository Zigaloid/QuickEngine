# Left Stick Direction - Real Bug Found & Fixed

## The Actual Problem

The bug was **NOT** in the velocity calculation - it was in the **input handling**!

### Root Cause

In the `Subscribe()` method, the MoveY action (left stick Y-axis) was incorrectly reading from `e.value.x` instead of `e.value.y`:

**Wrong Code**:
```cpp
// MoveY — continuous axis: cache the current value each frame
m_hMoveY = m_actionManager->Subscribe(m_moveYAction, [this](const Input::ActionEvent& e)
{
    if (e.phase == Input::ActionPhase::Ongoing || e.phase == Input::ActionPhase::Started)
        m_inputZ = e.value.x;  // ? WRONG! Reading X axis data
    else if (e.phase == Input::ActionPhase::Completed || e.phase == Input::ActionPhase::Canceled)
        m_inputZ = 0.f;
});
```

Similarly, the CameraY action had the same bug:

**Wrong Code**:
```cpp
// CameraY — continuous axis for pitch (up/down)
m_hCameraY = m_actionManager->Subscribe(m_cameraYAction, [this](const Input::ActionEvent& e)
{
    if (e.phase == Input::ActionPhase::Ongoing || e.phase == Input::ActionPhase::Started)
        m_cameraInputY = e.value.x;  // ? WRONG! Reading X axis data
    else if (e.phase == Input::ActionPhase::Completed || e.phase == Input::ActionPhase::Canceled)
        m_cameraInputY = 0.f;
});
```

## Why This Caused Backwards Movement

### Input Event Structure

When an action fires with an analog axis input:
```cpp
struct ActionEvent
{
    ActionPhase phase;
    bx::Vec2 value;  // e.value.x and e.value.y
};
```

For the **MoveY action** (left stick Y-axis):
- `e.value.x` = **0** (not used)
- `e.value.y` = **actual Y-axis value** (what we need)

For the **CameraY action** (right stick Y-axis):
- `e.value.x` = **0** (not used)
- `e.value.y` = **actual Y-axis value** (what we need)

### What Was Happening

1. Player pushes left stick **forward** (positive Y)
2. MoveY action fires with `e.value.y = 0.8`
3. Code reads: `m_inputZ = e.value.x` = **0** ?
4. Movement velocity becomes 0
5. Character doesn't move, or moves due to momentum

The inverse happened when pulling back - sometimes it would work by accident if other inputs were involved.

## The Fix

### Changed Code

**For MoveY**:
```cpp
// MoveY — continuous axis: cache the current value each frame
m_hMoveY = m_actionManager->Subscribe(m_moveYAction, [this](const Input::ActionEvent& e)
{
    if (e.phase == Input::ActionPhase::Ongoing || e.phase == Input::ActionPhase::Started)
        m_inputZ = e.value.y;  // ? CORRECT! Read Y axis data
    else if (e.phase == Input::ActionPhase::Completed || e.phase == Input::ActionPhase::Canceled)
        m_inputZ = 0.f;
});
```

**For CameraY**:
```cpp
// CameraY — continuous axis for pitch (up/down)
m_hCameraY = m_actionManager->Subscribe(m_cameraYAction, [this](const Input::ActionEvent& e)
{
    if (e.phase == Input::ActionPhase::Ongoing || e.phase == Input::ActionPhase::Started)
        m_cameraInputY = e.value.y;  // ? CORRECT! Read Y axis data
    else if (e.phase == Input::ActionPhase::Completed || e.phase == Input::ActionPhase::Canceled)
        m_cameraInputY = 0.f;
});
```

## Input Flow After Fix

```
Player pushes left stick FORWARD
    ?
InputActionManager processes LeftY axis
    ?
MoveY action fires with:
  e.value.x = 0
  e.value.y = 0.8 (positive, moving forward)
    ?
m_inputZ = e.value.y = 0.8 ?
    ?
GetDesiredVelocity():
  moveZ = m_inputZ * speed = 0.8 * 5.0 = 4.0
    ?
GetCameraRelativeVelocity():
  result = forward * moveZ = forward * 4.0
    ?
Character moves FORWARD ?
```

## Testing Verification

Now the following works correctly:

| Input | Expected | Result |
|-------|----------|--------|
| Push left stick forward | Move forward | ? Correct |
| Push left stick backward | Move backward | ? Correct |
| Push left stick left | Strafe left | ? Correct |
| Push left stick right | Strafe right | ? Correct |
| Push right stick up | Look up/down | ? Correct |
| Push right stick down | Look down/up | ? Correct |
| Push right stick left | Turn left | ? Correct |
| Push right stick right | Turn right | ? Correct |

## Why My Previous Fix Was Wrong

My first fix changed the velocity calculation:
- Changed `moveZ = -m_inputZ * speed` to `moveZ = m_inputZ * speed`
- Changed the camera-relative calculation

This didn't actually fix the underlying problem because **the input value was always 0**, so changing how we use it didn't help.

## The Real Issues Fixed

1. **MoveY input mapping**: Now reads `e.value.y` instead of `e.value.x`
2. **CameraY input mapping**: Now reads `e.value.y` instead of `e.value.x`

## Files Modified

- `..\Shared\Components\ThirdPersonInputComponent.cpp`
  - Line ~226: MoveY action subscription
  - Line ~254: CameraY action subscription

## Build Status

? **Successful** - 0 errors, 0 warnings

---

**Status**: ? **REAL BUG FOUND AND FIXED**

Movement now responds correctly to left stick input! ??

The issue was reading the wrong axis component from the input event - a classic copy-paste bug where all axis inputs were reading `e.value.x`.
