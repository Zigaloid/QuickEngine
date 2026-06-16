# RESOURCE SYSTEM SKILL

## Overview
The QuickEngine Resource System is an asynchronous, thread-safe resource loading pipeline that manages file I/O, memory management, and GPU resource creation. It uses a worker-thread architecture to load resources in the background while keeping expensive API calls (like bgfx resource creation) on the main thread.

## Core Concepts

### Architecture: Three-Stage Pipeline
```
????????????????      ????????????????      ????????????????
?  Initialize  ? ???> ?    Update    ? ???> ?   Finalize   ?
? (Main Thread)?      ?(Worker Thread)?      ?(Main Thread) ?
????????????????      ????????????????      ????????????????
     Setup state         Load file data        Create GPU resources
```

**1. Initialize**: Setup resource state, called once when resource is created  
**2. Update**: Load file data from disk (runs on worker thread, can block on I/O)  
**3. Finalize**: Create GPU/API resources (runs on main thread, requires rendering context)

### Thread Safety Model
- **ResourceManager** owns all resources and coordinates thread-safe access via mutexes
- **Worker Thread** runs Update() to load file data asynchronously
- **Main Thread** runs UpdateFinalization() to process completed loads
- **Queue-based** communication between threads using condition variables

---

## Core Classes

### 1. Resource (ResourceManager.h)
**Purpose**: Base class for all loadable resources with three-stage lifecycle.

**Key Features**:
- Atomic state flags: `m_isInitialized`, `m_isLoaded`, `m_isFinalized`
- Virtual lifecycle hooks: `Initialize()`, `Update()`, `Finalize()`
- Extension validation via `GetSupportedExtensions()`
- Hot-reload support via `Reload()`
- Reflection support via `CReflectedBase`

**Lifecycle Hooks**:
```cpp
virtual bool Initialize()                                    // Setup: runs once when created
virtual bool Update(FileSystem::FileSystemManager& fs)       // I/O: loads file data (worker thread)
virtual void Finalize()                                      // GPU: creates API resources (main thread)
virtual bool Reload()                                        // Hot-reload: resets state and re-enqueues
```

**State Queries**:
```cpp
bool IsInitialized() const  // Has Initialize() succeeded?
bool IsLoaded() const       // Has Update() loaded file data?
bool IsFinalized() const    // Has Finalize() created GPU resources?
const std::string& GetPath() const
const std::vector<uint8_t>& GetData() const
size_t GetLoadedSize() const
```

**Extension Validation**:
```cpp
static std::vector<std::string_view> GetSupportedExtensions()
// Example: { ".dds" } for textures, { ".shader.bin" } for shaders
// Empty list = no restriction enforced
```

**Base Implementation**:
- `Initialize()`: Sets `m_isInitialized = true`, returns true by default
- `Update()`: Reads file bytes via FileSystemManager, populates `m_data`, sets `m_isLoaded = true`
- `Finalize()`: Sets `m_isFinalized = true` (derived types override for GPU creation)
- `Reload()`: Clears state flags and data, calls Initialize(), returns true

### 2. ResourceManager (ResourceManager.h)
**Purpose**: Central resource registry with asynchronous loading pipeline.

**Key Features**:
- Type-safe resource creation via templates
- Worker thread for background file loading
- Queue-based communication with condition variables
- Path-based resource deduplication (same path = same resource instance)
- Extension validation to prevent type mismatches
- Hot-reload support via `ReloadResource()`

**Thread Management**:
```cpp
bool Start()   // Spawns worker thread, starts processing loading queue
void Stop()    // Signals worker to stop, joins thread
bool IsRunning() const
```

**Resource Creation**:
```cpp
template<typename T = Resource>
std::shared_ptr<T> RequestResource(const std::string& path)
// 1. Resolves path via AppConfig
// 2. Validates extension matches T::GetSupportedExtensions()
// 3. Returns existing resource if already loaded
// 4. Creates new resource and enqueues for loading
// 5. Returns shared_ptr immediately (resource loads asynchronously)
```

**Resource Queries**:
```cpp
template<typename T = Resource>
std::shared_ptr<T> GetResource(const std::string& path)
// Returns resource only if IsLoaded() == true, else nullptr

bool IsResourceLoaded(const std::string& path) const
bool IsResourceFinalized(const std::string& path) const
size_t GetLoadedResourceCount() const
size_t GetPendingLoadCount() const
size_t GetPendingFinalizationCount() const
```

**Hot-Reload**:
```cpp
bool ReloadResource(const std::string& path)
// 1. Finds existing resource by path
// 2. Calls resource->Reload() to reset state
// 3. Re-enqueues resource for loading
// 4. Returns true if successful
```

**Resource Removal**:
```cpp
bool RemoveResource(const std::string& path)
void ClearResources()  // Removes all resources
```

**Main Thread Integration**:
```cpp
void UpdateFinalization()
// MUST be called every frame on main thread
// Processes finalization queue, calls Finalize() on completed loads
// Retries failed finalizations on next frame
```

**Internal Architecture**:
- `m_loadedResources`: `unordered_map<string, shared_ptr<Resource>>` (deduplicated storage)
- `m_loadingQueue`: Resources awaiting Update() (worker thread processes)
- `m_finalizationQueue`: Resources awaiting Finalize() (main thread processes)
- `m_loadingQueueCondition`: Signals worker thread when new work arrives

**Worker Thread Loop**:
```cpp
void WorkerThreadFunction()
{
    while (!m_shouldStop) {
        // 1. Wait for resource in loading queue
        // 2. Call resource->Initialize() if not initialized
        // 3. Call resource->Update() in loop until complete
        // 4. Push to finalization queue when done
    }
}
```

**Extension Validation**:
```cpp
template<typename T>
static bool ValidateExtension(const std::string& path)
// Extracts compound extension (e.g., ".staticmesh.json")
// Checks against T::GetSupportedExtensions()
// Logs error and returns false on mismatch
```

### 3. CResourceReference (ResourceManager.h)
**Purpose**: Reflected, serializable reference to a resource by file path.

**Key Features**:
- Stores `m_resourceFileName` (relative asset path)
- Provides `GetResource()` and `GetResourceAs<T>()` accessors
- Path change detection with automatic reload
- Conversion from absolute to relative paths
- Virtual `ReloadResource()` for derived types

**Core API**:
```cpp
const std::string GetResourceFileName() const
std::shared_ptr<Resource> GetResource() const
template<typename T>
std::shared_ptr<T> GetResourceAs() const

void SetResourceFileName(const std::string& fileName)
void SetFromAbsolutePath(const std::string& absolutePath)  // Converts and loads
virtual void OnLoaded()  // Override to trigger resource loading
virtual void ReloadResource() const  // Override to handle path changes
virtual std::string GetReourceTypeName() const  // Returns "Resource"
```

**Path Change Detection**:
```cpp
// Enabled via CResourceReference_MonitorPathChange global flag
// GetResource() checks if m_resource->GetPath() != resolved m_resourceFileName
// If mismatch detected, calls ReloadResource() and logs warning
// Useful for hot-reload scenarios where asset paths change
```

**Path Conversion**:
```cpp
static std::string MakeAssetPath(const std::string& absolutePath)
// Converts "C:/Projects/QuickEngine/assets/mesh.obj" to "./assets/mesh.obj"
// Normalizes slashes and case, finds "/assets/" marker
// Returns original path if marker not found
```

### 4. CTypedResourceReference<TResource> (ResourceManager.h)
**Purpose**: Type-safe resource reference that automatically requests resources on load.

**Key Features**:
- Template parameter specifies resource type (e.g., `CTypedResourceReference<CTextureResource>`)
- Automatically calls `ResourceManager::RequestResource<TResource>()` in `OnLoaded()`
- Overrides `ReloadResource()` to re-request on path change
- Returns typed resource type name via RTTI

**Implementation**:
```cpp
template<typename TResource>
class CTypedResourceReference : public CResourceReference
{
    void OnLoaded() override
    {
        // Automatically requests resource from ResourceManager
        if (!fileName.empty() && fileName != "undifined")
            m_resource = ResourceManager::RequestResource<TResource>(fileName);
    }

    void ReloadResource() const override
    {
        // Re-requests resource on path change
        m_resource = ResourceManager::RequestResource<TResource>(fileName);
    }

    std::string GetReourceTypeName() const override
    {
        return typeid(TResource).name();
    }
};
```

**Usage Pattern**:
```cpp
// In component or data class
CMaterialResourceReference m_material;  // Derived from CTypedResourceReference<CMaterialResource>

// Reflection triggers OnLoaded() when deserialized
// m_material automatically requests CMaterialResource from ResourceManager

// Access resource
auto material = m_material.GetResourceAs<CMaterialResource>();
if (material && material->IsReady()) {
    bgfx::ProgramHandle shader = material->GetShaderProgram();
}
```

---

## Common Resource Types

### CTextureResource (TextureResource.h)
**Purpose**: Loads texture files (.dds) and creates bgfx texture handles.

**Key Features**:
- Extension: `.dds`
- Creates `bgfx::TextureHandle` in Finalize()
- Uses `loadTextureFromMemory()` helper from bgfx_utils
- Destroys texture handle in destructor

**API**:
```cpp
bgfx::TextureHandle GetTextureHandle() const
```

**Lifecycle**:
- `Update()`: Loads .dds file bytes via base Resource::Update()
- `Finalize()`: Calls `loadTextureFromMemory()`, validates handle

**Usage**:
```cpp
auto texture = resourceManager->RequestResource<CTextureResource>("./assets/wood.dds");
// ... wait for loading ...
if (texture->IsFinalized()) {
    bgfx::TextureHandle handle = texture->GetTextureHandle();
    bgfx::setTexture(0, sampler, handle);
}
```

### CShaderResource (ShaderResource.h)
**Purpose**: Loads compiled shader binaries (.shader.bin) and creates bgfx shader handles.

**Key Features**:
- Extension: `.shader.bin`
- Creates `bgfx::ShaderHandle` in Finalize()
- Uses `loadShaderFromMemory()` helper from bgfx_utils
- Destroys shader handle in destructor

**API**:
```cpp
bgfx::ShaderHandle GetShaderHandle() const
```

**Lifecycle**:
- `Update()`: Loads shader binary via base Resource::Update()
- `Finalize()`: Calls `loadShaderFromMemory()`, validates handle

**Usage**:
```cpp
auto vsShader = resourceManager->RequestResource<CShaderResource>("./shaders/vs_mesh.shader.bin");
auto fsShader = resourceManager->RequestResource<CShaderResource>("./shaders/fs_mesh.shader.bin");
// ... wait for loading ...
if (vsShader->IsFinalized() && fsShader->IsFinalized()) {
    bgfx::ProgramHandle program = bgfx::createProgram(
        vsShader->GetShaderHandle(),
        fsShader->GetShaderHandle(),
        false  // don't destroy shaders (resource owns them)
    );
}
```

### CMaterialResource (MaterialResource.h)
**Purpose**: Composite resource that loads shader programs, textures, and material properties.

**Key Features**:
- Extension: `.mat.obj.json`
- Contains `CShaderResourceReference` for vertex/fragment shaders
- Contains `std::vector<CTextureResourceReference>` for textures
- Creates `bgfx::ProgramHandle` from shader handles in Finalize()
- Stores material properties: `m_materialColor`, `m_ambientColor`, texture flags/stages

**API**:
```cpp
const bgfx::ProgramHandle GetShaderProgram() const
std::shared_ptr<CShaderResource> GetVertexShaderResource() const
std::shared_ptr<CShaderResource> GetFragmentShaderResource() const
int GetNumberOfTextures() const
bgfx::TextureHandle GetTexture(int index) const
const Vector4f& GetMaterialColor() const
const Vector4f& GetAmbientColor() const
bool IsReady() const  // Returns bgfx::isValid(m_shader)
bool IsLoaded() const  // Checks if all sub-resources are loaded
```

**Lifecycle**:
- `Update()`: Loads JSON descriptor, requests shader and texture resources
- `Finalize()`: Waits for shader resources to finalize, creates bgfx::ProgramHandle
- Destructor: Destroys program handle

**Sub-Resource Dependencies**:
```cpp
// Material depends on shaders and textures
CShaderResourceReference m_vertexShaderResource;
CShaderResourceReference m_fragmentShaderResource;
std::vector<std::unique_ptr<CTextureResourceReference>> m_textureResources;

// Finalize() must wait for all sub-resources to be ready
bool IsLoaded() const {
    return vertexShader->IsFinalized() && 
           fragmentShader->IsFinalized() &&
           all textures are finalized;
}
```

### CStaticMeshResource (StaticMeshResource.h)
**Purpose**: Composite resource that combines mesh geometry and material.

**Key Features**:
- Extension: `.smesh.obj.json`
- Contains `CMeshResourceReference` for geometry
- Contains `CMaterialResourceReference` for rendering
- JSON descriptor specifies mesh and material paths

**API**:
```cpp
std::shared_ptr<CMeshResource> GetMeshResource() const
std::shared_ptr<CMaterialResource> GetMaterialResource() const
bool IsReady() const  // Returns IsLoaded()
bool IsLoaded() const  // Checks if mesh and material are ready
void Reset()  // Releases sub-resources
```

**Lifecycle**:
- `Update()`: Loads JSON descriptor, requests mesh and material resources
- `Finalize()`: Waits for sub-resources to finalize
- Reflects the typical composite pattern where one resource depends on others

---

## Usage Examples

### Creating a Custom Resource Type

**1. Define Resource Header (MyResource.h)**:
```cpp
#pragma once
#include "ResourceManager/ResourceManager.h"

class CMyResource : public ResourceSystem::Resource
{
public:
    // Specify supported file extensions (compound extensions allowed)
    static std::vector<std::string_view> GetSupportedExtensions()
    {
        return { ".myformat.json", ".myformat.bin" };
    }

    explicit CMyResource(const std::string& path)
        : Resource(path)
        , m_customHandle(INVALID_HANDLE)
    {
    }

    ~CMyResource() override
    {
        if (m_customHandle != INVALID_HANDLE) {
            DestroyCustomHandle(m_customHandle);
        }
    }

    // Optional: Override Update() if you need custom file loading
    bool Update(FileSystem::FileSystemManager& fileSystem) override
    {
        // Load file data using base implementation
        if (!Resource::Update(fileSystem)) {
            return false;
        }

        // Optional: Parse data on worker thread (keep it fast!)
        // Parse JSON, validate format, etc.
        return true;
    }

    // Required: Override Finalize() to create GPU/API resources
    void Finalize() override
    {
        if (GetLoadedSize() == 0 || GetData().empty()) {
            m_isFinalized = false;
            return;
        }

        // Create GPU resources on main thread
        m_customHandle = CreateCustomHandleFromMemory(GetData().data(), GetLoadedSize());
        m_isFinalized = (m_customHandle != INVALID_HANDLE);
    }

    // Optional: Override Reload() if you need custom cleanup
    bool Reload() override
    {
        // Clean up GPU resources before reloading
        if (m_customHandle != INVALID_HANDLE) {
            DestroyCustomHandle(m_customHandle);
            m_customHandle = INVALID_HANDLE;
        }

        // Call base implementation to reset state and re-initialize
        return Resource::Reload();
    }

    // Public accessors
    CustomHandle GetHandle() const { return m_customHandle; }
    bool IsReady() const { return m_customHandle != INVALID_HANDLE; }

private:
    CustomHandle m_customHandle = INVALID_HANDLE;
};

// Typed reference for serialization
class CMyResourceReference : public CTypedResourceReference<CMyResource>
{
public:
    REFL_DECLARE_OBJECT(CMyResourceReference, CResourceReference);
};
```

**2. Implement Resource Source (MyResource.cpp)**:
```cpp
#include "MyResource.h"

REFL_DEFINE_OBJECT(CMyResourceReference)
REFL_DEFINE_END
```

**3. Use in Code**:
```cpp
// Request resource (returns immediately, loads asynchronously)
auto myResource = resourceManager->RequestResource<CMyResource>("./assets/data.myformat.json");

// Later, check if ready
if (myResource->IsFinalized() && myResource->IsReady()) {
    CustomHandle handle = myResource->GetHandle();
    // Use the resource
}
```

### Requesting and Using Resources

```cpp
// Get ResourceManager from CoreSystem
auto* resourceManager = Core::CoreSystem::GetResourceManager();

// Request resources (returns immediately, loading happens in background)
auto texture = resourceManager->RequestResource<CTextureResource>("./assets/wood.dds");
auto shader = resourceManager->RequestResource<CShaderResource>("./shaders/vs_mesh.shader.bin");
auto material = resourceManager->RequestResource<CMaterialResource>("./materials/metal.mat.obj.json");

// In main loop, finalize loaded resources
void MainLoop() {
    // MUST call every frame to process completed loads
    resourceManager->UpdateFinalization();

    // Check if resources are ready before using
    if (texture->IsFinalized()) {
        bgfx::TextureHandle handle = texture->GetTextureHandle();
        // Use texture
    }

    if (material->IsReady()) {
        bgfx::ProgramHandle shader = material->GetShaderProgram();
        // Use material
    }
}
```

### Using Resource References in Components

```cpp
// In component header
class MyRenderComponent : public ComponentSystem::Component
{
    REFL_DECLARE_OBJECT(MyRenderComponent, Component);

private:
    CStaticMeshResourceReference m_mesh;  // Serialized path reference
    CMaterialResourceReference m_material;
};

// In component source
REFL_DEFINE_OBJECT(MyRenderComponent)
    REFL_DEFINE_OBJECT_MEMBER(MyRenderComponent, m_mesh),
    REFL_DEFINE_OBJECT_MEMBER(MyRenderComponent, m_material),
REFL_DEFINE_END

// OnLoaded() is called by reflection system after deserialization
// References automatically request resources via OnLoaded()

// In component Update()
void MyRenderComponent::OnUpdate(double deltaTime)
{
    // Check if resources are ready
    auto mesh = m_mesh.GetResourceAs<CStaticMeshResource>();
    auto material = m_material.GetResourceAs<CMaterialResource>();

    if (mesh && mesh->IsReady() && material && material->IsReady()) {
        // Render with resources
        Render(mesh->GetMeshResource(), material->GetShaderProgram());
    }
}
```

### Hot-Reloading Resources

```cpp
// Reload a resource by path
bool success = resourceManager->ReloadResource("./assets/wood.dds");

// Resource will go through full pipeline again:
// 1. Reload() clears state and data
// 2. Re-enqueued for Update() on worker thread
// 3. File data re-loaded
// 4. Finalize() re-creates GPU resources on main thread

// Existing shared_ptr references remain valid (same instance)
// Next frame after finalization, GetTextureHandle() returns new handle
```

### Composite Resources (Dependencies)

```cpp
// CMaterialResource depends on CShaderResource instances
class CMaterialResource : public ResourceSystem::Resource
{
    bool Update(FileSystem::FileSystemManager& fileSystem) override
    {
        // 1. Load JSON descriptor
        if (!Resource::Update(fileSystem)) return false;

        // 2. Parse JSON to get shader paths
        // 3. Request sub-resources via ResourceManager
        auto* rm = Core::CoreSystem::GetResourceManager();
        m_vertexShaderResource.SetResourceFileName(vsPath);
        m_fragmentShaderResource.SetResourceFileName(fsPath);
        m_vertexShaderResource.OnLoaded();    // Requests vertex shader
        m_fragmentShaderResource.OnLoaded();  // Requests fragment shader

        return true;
    }

    void Finalize() override
    {
        // Wait for sub-resources to be ready
        auto vs = m_vertexShaderResource.GetResourceAs<CShaderResource>();
        auto fs = m_fragmentShaderResource.GetResourceAs<CShaderResource>();

        if (!vs || !vs->IsFinalized() || !fs || !fs->IsFinalized()) {
            m_isFinalized = false;  // Defer finalization, will retry next frame
            return;
        }

        // Create program from finalized shaders
        m_shader = bgfx::createProgram(
            vs->GetShaderHandle(),
            fs->GetShaderHandle(),
            false  // don't destroy (resources own handles)
        );
        m_isFinalized = bgfx::isValid(m_shader);
    }

private:
    CShaderResourceReference m_vertexShaderResource;
    CShaderResourceReference m_fragmentShaderResource;
    bgfx::ProgramHandle m_shader = BGFX_INVALID_HANDLE;
};
```

---

## Best Practices

### DO:
- ? Always call `UpdateFinalization()` every frame on main thread
- ? Check `IsFinalized()` before accessing GPU resources
- ? Use `GetSupportedExtensions()` to enforce file format contracts
- ? Keep `Update()` fast (worker thread blocks on it)
- ? Create GPU resources only in `Finalize()` (main thread, rendering context)
- ? Use `CTypedResourceReference<T>` for serialized resource references
- ? Call `Start()` on ResourceManager at application startup
- ? Call `Stop()` on ResourceManager before shutdown
- ? Use compound extensions for specificity (e.g., `.smesh.obj.json`)
- ? Store `shared_ptr<Resource>` to keep resources alive
- ? Use `ReloadResource()` for hot-reload workflows
- ? Defer finalization (return `m_isFinalized = false`) if dependencies not ready

### DON'T:
- ? Don't call GPU/API functions in `Update()` (wrong thread!)
- ? Don't block for long in `Update()` (blocks worker thread)
- ? Don't access resources before `IsFinalized() == true`
- ? Don't forget to call `UpdateFinalization()` every frame
- ? Don't call `RequestResource()` on hot paths (request once, cache result)
- ? Don't assume resources load instantly (always check state)
- ? Don't hold raw `Resource*` pointers long-term (use `shared_ptr`)
- ? Don't create ResourceManager instances (use `CoreSystem::GetResourceManager()`)
- ? Don't modify `m_data` after `Finalize()` (may be reused on Reload)
- ? Don't request resources with mismatched extensions (logs error, returns nullptr)

---

## Performance Considerations

### Memory
- Resources store full file data in `m_data` until Finalize() completes
- Loaded data persists for entire resource lifetime (enables Reload)
- Shared ownership via `shared_ptr` allows multiple references without duplication
- Resources are NOT automatically unloaded; call `RemoveResource()` explicitly

### Threading
- Worker thread blocks on file I/O in `Update()`
- Only one resource loads at a time (single worker thread)
- Main thread processes finalization queue serially
- Mutexes protect shared state (`m_loadedResources`, queues)
- Condition variable wakes worker thread on new requests

### CPU
- `RequestResource()` does path lookup in `unordered_map` (O(1) average)
- Extension validation is O(n) in number of supported extensions
- `UpdateFinalization()` iterates finalization queue linearly
- Failed finalizations retry every frame (keep `Finalize()` fast)

### I/O
- All file reads happen on worker thread (non-blocking for main thread)
- FileSystemManager handles actual I/O (may use memory mapping, caching, etc.)
- Large files block worker thread until fully loaded
- No read streaming or chunking (entire file loaded at once)

### GPU
- All GPU resource creation happens on main thread in `Finalize()`
- bgfx handles (textures, shaders, programs) created per-resource
- No resource sharing or deduplication at GPU level
- Destroying resource destroys GPU handles (can cause stalls if done during rendering)

### Optimization Tips
- Request resources early (e.g., during level load, not during gameplay)
- Batch resource requests to maximize worker thread utilization
- Keep `Update()` and `Finalize()` fast to avoid frame hitches
- Use resource pooling/caching at higher level if loading same resources repeatedly
- Profile `UpdateFinalization()` time; move expensive finalization to worker thread if possible
- Consider streaming or LOD systems for very large resources
- Implement progress tracking via `GetPendingLoadCount()` and `GetPendingFinalizationCount()`

---

## Integration Points

### CoreSystem
```cpp
// ResourceManager is owned by CoreSystem
ResourceSystem::ResourceManager* CoreSystem::GetResourceManager()

// Typical initialization
void CoreSystem::Initialize() {
    m_resourceManager = std::make_unique<ResourceSystem::ResourceManager>(m_fileSystemManager);
    m_resourceManager->Start();
}

void CoreSystem::Shutdown() {
    m_resourceManager->Stop();
    m_resourceManager.reset();
}

void CoreSystem::Update(double deltaTime) {
    // CRITICAL: Must call every frame
    m_resourceManager->UpdateFinalization();
}
```

### FileSystemManager
```cpp
// ResourceManager uses FileSystemManager for all file I/O
FileSystemManager::Result<std::vector<uint8_t>> ReadAllBytes(const std::string& path)

// Worker thread calls this in Resource::Update()
auto result = fileSystem.ReadAllBytes(m_path);
if (result.IsSuccess()) {
    m_data = result.GetValue();
}
```

### Reflection
```cpp
// Resources and references use reflection for serialization
REFL_DECLARE_OBJECT(CMyResource, ResourceSystem::Resource)
REFL_DEFINE_OBJECT(CMyResource)
REFL_DEFINE_END

// Resource references are serialized as members
REFL_DEFINE_OBJECT(MyComponent)
    REFL_DEFINE_OBJECT_MEMBER(MyComponent, m_textureRef),  // CTextureResourceReference
REFL_DEFINE_END

// OnLoaded() is called by reflection system after deserialization
```

### AppConfig
```cpp
// Resolves relative paths to absolute paths
std::string AppConfig::ResolvePath(const std::string& relativePath)

// Used in RequestResource to normalize paths
const std::string resolvedPath = Core::AppConfig::Instance().ResolvePath(path);
```

### bgfx (Rendering Backend)
```cpp
// Resources create bgfx handles in Finalize()
bgfx::TextureHandle loadTextureFromMemory(const void* data, uint32_t size, const char* name)
bgfx::ShaderHandle loadShaderFromMemory(const void* data, uint32_t size, const char* name)
bgfx::ProgramHandle bgfx::createProgram(bgfx::ShaderHandle vs, bgfx::ShaderHandle fs, bool destroy)

// Resources destroy handles in destructors
bgfx::destroy(m_texture);
bgfx::destroy(m_shader);
bgfx::destroy(m_program);
```

---

## Common Patterns

### Resource State Machine
```cpp
// Resource states (atomic flags)
bool m_isInitialized;  // Initialize() succeeded
bool m_isLoaded;       // Update() loaded file data
bool m_isFinalized;    // Finalize() created GPU resources

// State transitions
Created ? Initialize() ? Initialized ? Update() ? Loaded ? Finalize() ? Finalized

// Reload cycle
Finalized ? Reload() ? Initialized ? (re-enqueued for Update/Finalize)
```

### Deferred Finalization
```cpp
// If sub-resources aren't ready, defer finalization
void CMaterialResource::Finalize()
{
    auto vs = m_vertexShaderResource.GetResourceAs<CShaderResource>();
    auto fs = m_fragmentShaderResource.GetResourceAs<CShaderResource>();

    if (!vs || !vs->IsFinalized() || !fs || !fs->IsFinalized()) {
        m_isFinalized = false;  // Retry next frame
        return;
    }

    // Create program when all dependencies ready
    m_shader = bgfx::createProgram(vs->GetShaderHandle(), fs->GetShaderHandle(), false);
    m_isFinalized = bgfx::isValid(m_shader);
}
```

### Resource Pooling at Application Level
```cpp
// Cache frequently-used resources
class ResourceCache {
    std::unordered_map<std::string, std::shared_ptr<Resource>> m_cache;

    std::shared_ptr<Resource> Get(const std::string& path) {
        auto it = m_cache.find(path);
        if (it != m_cache.end()) return it->second;

        auto resource = resourceManager->RequestResource(path);
        m_cache[path] = resource;
        return resource;
    }
};
```

### Progress Tracking
```cpp
// Track loading progress for splash screen
class LoadingScreen {
    void Update() {
        size_t pending = resourceManager->GetPendingLoadCount() + 
                        resourceManager->GetPendingFinalizationCount();
        size_t total = resourceManager->GetLoadedResourceCount() + pending;
        float progress = (total > 0) ? (float)(total - pending) / total : 1.0f;
        DrawProgressBar(progress);
    }
};
```

---

## Debugging

### Enable Debug Output
```cpp
// At file scope in ResourceManager.h
#define RESOURCE_MANAGER_DEBUG 1

// Logs are output via ResourceManagerDebug channel
ResourceManagerDebug.printf("Requesting resource: %s\n", path.c_str());
ResourceManagerDebug.warning("Extension mismatch: %s\n", path.c_str());
```

### Common Issues

**Resource Never Finalizes**:
- Check that `UpdateFinalization()` is called every frame
- Check that sub-resources are finalized (for composite resources)
- Add logging to `Finalize()` to track failures

**Resource Loads Wrong Type**:
- Check extension in `GetSupportedExtensions()`
- Verify file extension matches resource type
- Look for "Resource type mismatch" error in logs

**GPU Handle is Invalid**:
- Check that `Finalize()` is called on main thread
- Verify bgfx is initialized before finalization
- Check return value of bgfx creation functions

**Path Not Found**:
- Check that path is relative to project root (e.g., "./assets/...")
- Verify `AppConfig::ResolvePath()` resolves correctly
- Check FileSystemManager mount points

**Memory Leak**:
- Check that resources are released (no dangling `shared_ptr`)
- Call `RemoveResource()` or `ClearResources()` when done
- Profile with memory profiler to find leaked resources

---

## File Structure
```
Core/ResourceManager/
??? ResourceManager.h           # ResourceManager, Resource, CResourceReference
??? ResourceManager.cpp         # Reflection definitions
??? RESOURCE_SYSTEM.skill.md   # This document

Shared/ResourceTypes/
??? TextureResource.h           # CTextureResource, CTextureResourceReference
??? TextureResource.cpp         # Reflection definitions
??? ShaderResource.h            # CShaderResource, CShaderResourceReference
??? ShaderResource.cpp          # Reflection definitions
??? MaterialResource.h          # CMaterialResource (composite)
??? MaterialResource.cpp        # Implementation + reflection
??? MeshResource.h              # CMeshResource
??? MeshResource.cpp            # Implementation + reflection
??? StaticMeshResource.h        # CStaticMeshResource (composite)
??? StaticMeshResource.cpp      # Implementation + reflection
??? ... (other resource types)
```

---

## Future Enhancements
- Multi-threaded loading (thread pool instead of single worker)
- Streaming support for large resources (chunked loading)
- Reference counting for automatic resource unloading
- Resource dependency graph visualization
- Async Finalize() for resources that don't need rendering context
- Resource compression/decompression in pipeline
- Progress callbacks for individual resources
- Priority queue for resource loading
- Resource aliasing (multiple paths to same resource)
- Hot-reload file watching integration

---

**Version**: 1.0  
**Last Updated**: 2024  
**Maintainer**: QuickEngine Core Team  
**Language**: C++20  
**Dependencies**: FileSystemManager, AppConfig, Reflection, bgfx  
**License**: See repository root
