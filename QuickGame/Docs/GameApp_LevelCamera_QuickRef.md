# Level Camera Auto-Discovery - Quick Reference

## What Changed

The `GameApp` now automatically searches the level hierarchy and attaches the first `CCameraComponent` as the active camera.

## When It Happens

| Event | Action |
|-------|--------|
| Initialize | Searches initial level for camera |
| LoadLevel() | Searches new level for camera |
| ReloadLevel() | Searches reloaded level for camera |
| Manual | None (unless you use SetActiveCamera) |

## How It Works

```
Level Hierarchy Tree
    ?
FindFirstCameraInHierarchy(m_RootLevel)
    ?
Depth-first search for CCameraComponent
    ?
Found? ? SetActiveCamera(camera)
Not Found? ? SetActiveCamera(nullptr) [use default]
```

## Usage

Just add a camera component to your level - it's found automatically:

```cpp
// In level creation
auto* player = levelComponent->CreateChild<CEntityComponent>();
auto* camera = player->CreateChild<CCameraComponent>();
camera->SetDistance(5.0f);

// When level loads, camera is automatically discovered and activated!
```

## Manual Override

```cpp
// To use a specific camera
auto* myCamera = player->FindChild<CCameraComponent>();
gameApp->SetActiveCamera(myCamera);

// To go back to default
gameApp->SetActiveCamera(nullptr);
```

## Search Details

- **Method**: Depth-first recursive traversal
- **Returns**: First `CCameraComponent` found
- **Fallback**: Default orbit camera if not found
- **When**: Initialize, LoadLevel, ReloadLevel only (not per-frame)

## Level Structure

```
Level (CLevelComponent)
??? Player (CEntityComponent)
?   ??? CCharacterComponent
?   ??? CThirdPersonInputComponent
?   ??? CCameraComponent ? Found and activated
??? Terrain
??? Props
??? ...
```

## Code Location

| Component | Location |
|-----------|----------|
| Helper Function | Game.cpp (file scope) |
| Integration | GameApp::Initialize(), LoadLevel(), ReloadLevel() |
| API | GameApp::SetActiveCamera(), GetActiveCamera() |

## Benefits

? **Zero Configuration** - Just add camera to level  
? **Automatic** - No code changes needed  
? **Fallback Support** - Always have default camera  
? **Level Flexibility** - Each level can have its own camera  

## Troubleshooting

| Issue | Solution |
|-------|----------|
| Camera not used | Check it's in the level hierarchy |
| Wrong camera used | First found is used; check tree order |
| Using default camera | No camera found in level; add one |

## Example

```cpp
// Create player with camera
auto* player = level->CreateChild<CEntityComponent>();
auto* camera = player->CreateChild<CCameraComponent>();
camera->SetDistance(5.0f);
camera->SetFOV(60.0f);

// Level initializes - camera automatically discovered!
level->Initialize();

// Verify
if (gameApp->GetActiveCamera())
{
    // Camera was found and activated
}
```

## Build Status

? **Successful** - All changes compile without errors
