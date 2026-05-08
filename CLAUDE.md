# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build System

QuickScope uses **Premake5** to generate Visual Studio solution files. Each sub-project has its own `premake5.lua` and `.sln`.

**Regenerate project files** (run from the project root or sub-project directory):
```
BuildTools\premake5\premake5.exe vs2022
```

**Build from command line** (MSBuild):
```
msbuild QuickScope\QuickScope.sln /p:Configuration=Debug /p:Platform=x64
msbuild Core\Core.sln /p:Configuration=Debug /p:Platform=x64
```

**Run the app** (after build):
```
QuickScope\bin\Debug\QuickScope.exe
```

**Build and run unit tests:**
```
msbuild UnitTests\CoreTests\CoreSystemTests.sln /p:Configuration=Debug /p:Platform=x64
UnitTests\CoreTests\bin\Debug\CoreUnitTests.exe
# Run a single test filter:
UnitTests\CoreTests\bin\Debug\CoreUnitTests.exe --gtest_filter=ReflectionTests.*
```

**Build order matters** — `Core` must be built before any application project (`QuickScope`, `QuickEditor`, `QuickGame`, `NexusServer`). bgfx libraries must also be pre-built under `External/bgfx/.build/projects/vs2019/`.

**Compiler requirements:** MSVC v143, C++20, x64, static runtime (`/MT`). The `/Zc:preprocessor` and `/Zc:__cplusplus` flags are required.

## Architecture

### Project Structure

| Project | Type | Purpose |
|---------|------|---------|
| `Core/` | Static lib | Engine systems: ECS, resources, jobs, networking, reflection |
| `Shared/` | Source included directly | UI visualizers, rendering components, resource types |
| `QuickScope/` | Executable | Main profiling visualization app |
| `QuickEditor/` | Executable | Asset/level editor |
| `QuickGame/` | Executable | Game/engine test harness |
| `NexusServer/` | Executable | Telemetry aggregation server |
| `External/` | Vendored | bgfx, ImGui (docking), ufbx, RapidJSON, STB |
| `Tools/FbxToBgfxMesh/` | Executable | Offline FBX → bgfx binary mesh converter |
| `Tools/CodeCompare/` | Executable | Standalone diff/compare tool with AI assistant integration |
| `UnitTests/CoreTests/` | GTest executable | Unit tests for Core subsystems |

HTML documentation for all Core subsystems lives in `Core/Documentation/index.html`.

### Core Engine (`Core/`)

`CoreSystem` is a static singleton that owns and initializes all engine subsystems. Applications call `Core::CoreSystem::Initialize(flags)` with bitmask flags to select which subsystems to start. Subsystems:

- **ResourceManager** — Two-phase async loading. Worker thread calls `Resource::Update()` (file I/O); main thread calls `Resource::Finalize()` (GPU resource creation via `UpdateFinalization()`). Resources are requested by type via `RequestResource<T>(path)`.
- **ComponentSystem** — ECS architecture. Components derive from `ComponentSystem::Component`, which derives from `CReflectedBase`. Components are registered with `REGISTER_COMPONENT(ClassName, "Display Name", "Category")` and declared with `REFL_DECLARE_OBJECT` / `REFL_DEFINE_OBJECT` macros. `ComponentSystemScheduler` runs `OnUpdate()` async on all active components each frame.
- **JobSystem** — Multi-threaded job scheduler.
- **Nexus (Net)** — IPC protocol. `CNexusClient` connects to `NexusServer` and exchanges pipe messages. Use the `NEXUS_*` macros in `CoreSystem.h` for subscribe/send.
- **FunctionCallManager** — Runtime-callable function registry; used by `CommandConsole`.
- **Reflection** — All serializable objects derive from `CReflectedBase`. Class members are registered via `REFL_DEFINE_OBJECT_MEMBER` macros and support JSON serialization.

### Frame Loop (`QuickScope/Main.cpp`)

The entry point follows the bgfx `entry::AppI` pattern:
1. `init()` — Initialize Core, bgfx, ImGui, then `QuickScopeApp::Initialize()`
2. `update()` — Per frame: process input, `QuickScopeApp::Update()`, ImGui frame, `ResourceManager::UpdateFinalization()`, `ComponentSystemScheduler::UpdateAllAsync()` + `WaitForCompletion()`, `bgfx::frame()`
3. `shutdown()` — Shutdown Core, ImGui, bgfx

### Shared UI System (`Shared/ImguiVizualizers/`)

Visualizers implement `IImGuiVisualizer` and are registered with `ImGuiVisualizerManager`:
```cpp
manager.Register("key", std::make_unique<MyVisualizer>(), /*visibleByDefault=*/true);
manager.Initialize();
// Each frame:
manager.Update(dt);
manager.RenderMenuBar();  // inside a MenuBar window
manager.RenderAll();
```

`UnifiedActionManager` allows registering named actions (`path`, `description`, `callback`) that appear simultaneously in menus, toolbar, and the `CommandConsole`.

### Rendering

All rendering uses **bgfx** with view IDs managed by `BgfxViewIdAllocator` (singleton, IDs 1–199 available). 3D viewports use `Bgfx3DViewport` + `Bgfx3DCamera`. Mesh loading goes through the `CMeshResource` / `CFbxMeshResource` resource types, which call `meshLoad()` / `meshUnload()` in `Finalize()`. Materials are defined via `CMaterialDefinition` (JSON-serializable, loaded through `CResourceReference` fields).

### Asset/Entity System (`AssetClasses/`)

`EntityInstance` owns a list of `ComponentSystem::Component*` loaded from an `.ent.obj.json` file. `EntityDefinition` is the serializable data class. Both use the reflection system for JSON round-trip.

## Coding Conventions

The canonical reference file is `Shared/ImguiVizualizers/ImGui3DViewVisualizer.h/.cpp`. The full conventions spec is at `.claude/skills/conventions/SKILL.md`. Key rules:

- **Naming:** `m_camelCase` members, `PascalCase` methods/classes, `camelCase` parameters/locals
- **Braces:** Allman style (opening brace on own line) for all function bodies, `if`/`for`/`while` — except short inline getters in headers
- **Section banners:** `// ── Section Name ─────────────────────────────────────────────`  
- **Single-arg constructors:** always `explicit`
- **Virtuals:** always `override` on overridden methods; never use `virtual` alone when overriding
- **Casts:** `static_cast<T>()` only — no C-style casts
- **Null:** `nullptr` only — not `NULL` or `0`
- **Include order:** `.cpp` includes own header first, then project headers (quotes), then system headers (angle brackets)
- Run `/conventions` on files in `Shared/` to auto-check and fix violations

### Editor-specific systems (`Shared/ImguiVizualizers/`)

- **`SelectionManager`** — single source of truth for selected entities; visualizers subscribe to selection changes rather than storing selections themselves.
- **`PropertyInspector`** — reflection-driven inspector; renders editable widgets for any `CReflectedBase` object. Type-specific renderers are registered in `PropertyWidgetMapRegistry`.
- **`DocumentManager`** — tracks open editor documents and routes asset-open requests to the correct `IImGuiVisualizer` launcher.

## Key Patterns

**Registering a new component:**
```cpp
// In header: derive from ComponentSystem::Component, add REFL_DECLARE_OBJECT + DECLARE_COMPONENT
// In cpp:
REGISTER_COMPONENT(CMyComponent, "My Component", "Category")
REFL_DEFINE_OBJECT(CMyComponent)
    REFL_DEFINE_OBJECT_MEMBER(CMyComponent, m_someRef),
REFL_DEFINE_END
```

**Adding a new visualizer:**
```cpp
// Derive from IImGuiVisualizer, implement Initialize/Shutdown/Update/Render/GetName
// Register: manager.Register("key", std::make_unique<CMyVisualizer>());
```

**Implementing an undoable operation (QuickEditor):**
```cpp
// Derive from ICommand (Shared/ImguiVizualizers/ICommand.h)
class CMyCommand : public ImGuiVisualizers::ICommand
{
public:
    void Execute() override { /* apply */ }
    void Undo()    override { /* revert */ }
    const char* GetLabel() const override { return "My Operation"; }
};
// Push to history (calls Execute immediately):
m_commandHistory.Push(std::make_unique<CMyCommand>(...));
```

**Requesting a resource:**
```cpp
auto res = Core::CoreSystem::GetResourceManager()->RequestResource<CMeshResource>("meshes/foo.bin");
// Resource loads async; check res->IsFinalized() before use
```
