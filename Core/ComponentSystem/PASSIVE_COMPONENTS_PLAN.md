# Passive Components Implementation Plan

## Executive Summary

This document outlines the plan to introduce **Passive Components** to the QuickEngine Component System. Passive Components are data-only component instances that are shared across multiple references, loaded from files via the Resource System, and do not participate in the Update loop. This contrasts with **Active Components**, which are unique instances with lifecycle management and Update callbacks.

**Key Goals**:
- ? Introduce passive components as data containers (no Update, no unique instances)
- ? Load passive components from files via Resource System integration
- ? Share single passive component instance across multiple references
- ? Maintain backward compatibility with existing active components
- ? Integrate with reflection system for serialization
- ? Provide type-safe reference mechanism similar to ResourceReference pattern

---

## Current State Analysis

### Active Components (Current Implementation)
```cpp
class Component : public CReflectedBase, public std::enable_shared_from_this<Component>
{
    // Unique instance per component
    ComponentId m_id;  // Auto-incremented ID

    // Lifecycle management
    virtual bool OnInitialize();
    virtual void OnUpdate(double deltaTime);  // Called every frame
    virtual void OnShutdown();

    // Hierarchical structure
    std::weak_ptr<Component> m_parent;
    std::vector<std::shared_ptr<Component>> m_children;

    // State management
    bool m_initialized;
    bool m_active;
};
```

**Characteristics**:
- Each component is a unique instance with unique ID
- Managed by ComponentPool (object pooling)
- Updated every frame via ComponentManager::UpdateAll()
- Participates in parent-child hierarchy
- Can be active/inactive with state propagation

### Resource System Pattern (Reference Model)
```cpp
// Resource pattern: single instance, multiple references
class CResourceReference : public CReflectedBase
{
    std::string m_resourceFileName;  // Serialized path
    mutable std::shared_ptr<Resource> m_resource;  // Shared instance

    void OnLoaded() { /* Request from ResourceManager */ }
    std::shared_ptr<Resource> GetResource() const;
};

template<typename T>
class CTypedResourceReference : public CResourceReference
{
    void OnLoaded() override {
        m_resource = ResourceManager::RequestResource<T>(GetResourceFileName());
    }
};
```

**Characteristics**:
- Resources are loaded from files asynchronously
- Single resource instance shared across all references
- Path-based deduplication (same path = same instance)
- Reflection-based serialization of file paths
- Type-safe access via templates

---

## Proposed Architecture

### 1. Passive Component Concept

**Definition**: A passive component is a data-only component that:
- Is loaded from a file (via Resource System)
- Has no Update loop (OnUpdate is never called)
- Is shared across multiple references (single instance per file)
- Does not participate in ComponentPool/ComponentManager lifecycle
- Cannot have children or participate in hierarchy
- Has no ComponentId (not a unique instance)

**Use Cases**:
- Configuration data (AI parameters, gameplay settings)
- Shared attribute definitions (weapon stats, character attributes)
- Template data (spawn parameters, effect definitions)
- Static lookup tables (damage tables, progression curves)

### 2. Class Hierarchy

```
CReflectedBase
??? Component (Active)
?   ??? MyActiveComponent
?   ??? ... (existing components)
?
??? PassiveComponent (New)
    ??? MyDataComponent
    ??? ... (new passive components)
```

**Alternative Design**: Passive components could inherit from Component with a flag:
```cpp
class Component {
    bool m_isPassive = false;  // Flag to skip Update
};
```

**Recommendation**: **Separate base class (`PassiveComponent`)** for clearer separation of concerns and to avoid confusion about lifecycle methods.

---

## Implementation Plan

### Phase 1: Core Infrastructure

#### 1.1 Create PassiveComponent Base Class

**File**: `Core/ComponentSystem/PassiveComponent.h`

```cpp
#pragma once
#include "Reflection/Reflection.h"
#include <memory>

namespace ComponentSystem {

/** @brief Base class for passive (data-only) components that are loaded from files.
 *  Passive components:
 *  - Do not participate in the Update loop
 *  - Are loaded from files via the Resource System
 *  - Are shared across multiple references (single instance per file)
 *  - Cannot have children or parents
 *  - Do not have unique ComponentIds */
class PassiveComponent : public CReflectedBase
{
public:
    REFL_DECLARE_OBJECT(PassiveComponent, CReflectedBase);

    PassiveComponent() = default;
    virtual ~PassiveComponent() = default;

    // Non-copyable but movable
    PassiveComponent(const PassiveComponent&) = delete;
    PassiveComponent& operator=(const PassiveComponent&) = delete;
    PassiveComponent(PassiveComponent&&) = default;
    PassiveComponent& operator=(PassiveComponent&&) = default;

    /** @brief Called once when the passive component is loaded from file.
     *  Override to perform any post-load initialization.
     *  @return true on success. */
    virtual bool OnLoaded() { return true; }

    /** @brief Returns the file path this component was loaded from. */
    const std::string& GetSourcePath() const { return m_sourcePath; }

    /** @brief Sets the source path. Called by PassiveComponentResource. */
    void SetSourcePath(const std::string& path) { m_sourcePath = path; }

private:
    std::string m_sourcePath;
};

} // namespace ComponentSystem
```

**File**: `Core/ComponentSystem/PassiveComponent.cpp`

```cpp
#include "PassiveComponent.h"

using namespace ComponentSystem;

REFL_DEFINE_OBJECT(PassiveComponent)
REFL_DEFINE_END
```

**Key Design Decisions**:
- ? No `OnUpdate()` - passive components are not updated
- ? No `m_id` - not unique instances
- ? No `m_parent`/`m_children` - not hierarchical
- ? No `m_initialized`/`m_active` - simple load state
- ? `OnLoaded()` for post-load initialization
- ? `m_sourcePath` to track origin file

---

#### 1.2 Create PassiveComponentResource

**File**: `Shared/ResourceTypes/PassiveComponentResource.h`

```cpp
#pragma once
#include "ResourceManager/ResourceManager.h"
#include "ComponentSystem/PassiveComponent.h"
#include <memory>

/** @brief Resource type for loading passive components from files.
 *  Passive components are loaded asynchronously via the Resource System
 *  and shared across all references to the same file. */
class CPassiveComponentResource : public ResourceSystem::Resource
{
public:
    static std::vector<std::string_view> GetSupportedExtensions()
    {
        return { ".passive.json" };  // Convention: .passive.json for passive components
    }

    explicit CPassiveComponentResource(const std::string& path)
        : Resource(path)
    {
    }

    ~CPassiveComponentResource() override = default;

    /** @brief Loads the passive component from file on worker thread.
     *  Uses reflection system to deserialize component from JSON. */
    bool Update(FileSystem::FileSystemManager& fileSystem) override;

    /** @brief Finalizes the component on main thread. */
    void Finalize() override;

    /** @brief Returns the loaded passive component instance. */
    std::shared_ptr<ComponentSystem::PassiveComponent> GetComponent() const 
    { 
        return m_component; 
    }

    /** @brief Reloads the component from file (hot-reload support). */
    bool Reload() override;

private:
    std::shared_ptr<ComponentSystem::PassiveComponent> m_component;
};
```

**File**: `Shared/ResourceTypes/PassiveComponentResource.cpp`

```cpp
#include "PassiveComponentResource.h"
#include "CoreSystem/CoreSystem.h"
#include "Reflection/ReflectionSerialization.h"
#include <nlohmann/json.hpp>

bool CPassiveComponentResource::Update(FileSystem::FileSystemManager& fileSystem)
{
    DECLARE_FUNC_LOW();

    if (m_isLoaded) {
        return true;
    }

    // Load JSON file
    auto result = fileSystem.ReadAllBytes(m_path);
    if (result.IsError()) {
        return false;
    }

    m_data = result.GetValue();

    // Parse JSON on worker thread
    try {
        std::string jsonStr(m_data.begin(), m_data.end());
        nlohmann::json jsonData = nlohmann::json::parse(jsonStr);

        // Deserialize via reflection system
        // Determine component type from JSON
        std::string className = jsonData.value("_class", "PassiveComponent");

        // Create component instance via ClassFactory
        auto* factory = Core::CoreSystem::GetClassFactory();
        if (!factory) {
            return false;
        }

        CReflectedBase* obj = factory->CreateObject(className);
        if (!obj) {
            ResourceManagerDebug.printf("Failed to create passive component: %s\n", className.c_str());
            return false;
        }

        // Verify it's a PassiveComponent
        m_component = std::dynamic_pointer_cast<ComponentSystem::PassiveComponent>(
            std::shared_ptr<CReflectedBase>(obj));

        if (!m_component) {
            delete obj;
            ResourceManagerDebug.printf("Created object is not a PassiveComponent: %s\n", className.c_str());
            return false;
        }

        // Deserialize properties
        m_component->SafeRead(m_path);
        m_component->SetSourcePath(m_path);

        m_isLoaded = true;
        return true;
    }
    catch (const std::exception& e) {
        ResourceManagerDebug.printf("Failed to parse passive component JSON: %s\n", e.what());
        return false;
    }
}

void CPassiveComponentResource::Finalize()
{
    if (!m_component || !m_isLoaded) {
        m_isFinalized = false;
        return;
    }

    // Call OnLoaded() on main thread
    if (!m_component->OnLoaded()) {
        ResourceManagerDebug.printf("PassiveComponent::OnLoaded() failed: %s\n", m_path.c_str());
        m_isFinalized = false;
        return;
    }

    m_isFinalized = true;
}

bool CPassiveComponentResource::Reload()
{
    // Clear existing component
    m_component.reset();

    // Call base Reload to reset state
    return Resource::Reload();
}
```

**Key Design Decisions**:
- ? Uses `.passive.json` extension convention
- ? Deserializes via reflection system (ClassFactory)
- ? Determines component type from `_class` field in JSON
- ? Calls `OnLoaded()` on main thread in Finalize
- ? Supports hot-reload via `Reload()`

---

#### 1.3 Create PassiveComponentReference

**File**: `Core/ComponentSystem/PassiveComponentReference.h`

```cpp
#pragma once
#include "PassiveComponent.h"
#include "Reflection/ReflectionBase.h"
#include <memory>
#include <string>

namespace ComponentSystem {

/** @brief Base reference class for passive components.
 *  Similar to CResourceReference, stores a file path and provides access
 *  to the shared passive component instance. */
class CPassiveComponentReference : public CReflectedBase
{
public:
    REFL_DECLARE_OBJECT(CPassiveComponentReference, CReflectedBase);

    CPassiveComponentReference() = default;
    virtual ~CPassiveComponentReference() = default;

    /** @brief Returns the file path to the passive component. */
    const std::string& GetComponentFileName() const { return m_componentFileName; }

    /** @brief Sets the file path and optionally triggers loading. */
    void SetComponentFileName(const std::string& fileName, bool autoLoad = true);

    /** @brief Returns the loaded passive component (may be nullptr if not loaded). */
    std::shared_ptr<PassiveComponent> GetComponent() const;

    /** @brief Type-safe accessor for derived passive component types. */
    template<typename T>
    std::shared_ptr<T> GetComponentAs() const
    {
        static_assert(std::is_base_of_v<PassiveComponent, T>, "T must derive from PassiveComponent");
        return std::dynamic_pointer_cast<T>(GetComponent());
    }

    /** @brief Returns true if the component is loaded and finalized. */
    bool IsReady() const;

    /** @brief Returns the type name of the referenced component. */
    virtual std::string GetComponentTypeName() const { return "PassiveComponent"; }

    /** @brief Called by reflection system after deserialization.
     *  Triggers loading of the passive component. */
    void OnLoaded() override;

protected:
    /** @brief Requests the component from the resource manager.
     *  Override in derived classes for type-specific loading. */
    virtual void RequestComponent();

    mutable std::shared_ptr<ResourceSystem::Resource> m_resource;

private:
    std::string m_componentFileName = "undefined";
};

/** @brief Typed reference for type-safe passive component access. */
template<typename TPassiveComponent>
class CTypedPassiveComponentReference : public CPassiveComponentReference
{
public:
    static_assert(std::is_base_of_v<PassiveComponent, TPassiveComponent>, 
                  "TPassiveComponent must derive from PassiveComponent");

    void OnLoaded() override
    {
        RequestComponent();
    }

    std::string GetComponentTypeName() const override
    {
        return typeid(TPassiveComponent).name();
    }

protected:
    void RequestComponent() override
    {
        const std::string fileName = GetComponentFileName();
        if (!fileName.empty() && fileName != "undefined")
        {
            auto* resourceManager = Core::CoreSystem::GetResourceManager();
            if (resourceManager)
            {
                m_resource = resourceManager->RequestResource<CPassiveComponentResource>(fileName);
            }
        }
    }
};

} // namespace ComponentSystem
```

**File**: `Core/ComponentSystem/PassiveComponentReference.cpp`

```cpp
#include "PassiveComponentReference.h"
#include "CoreSystem/CoreSystem.h"
#include "Shared/ResourceTypes/PassiveComponentResource.h"

using namespace ComponentSystem;

REFL_DEFINE_OBJECT(CPassiveComponentReference)
    REFL_DEFINE_STRING_MEMBER(CPassiveComponentReference, m_componentFileName),
REFL_DEFINE_END

void CPassiveComponentReference::SetComponentFileName(const std::string& fileName, bool autoLoad)
{
    m_componentFileName = fileName;
    if (autoLoad)
    {
        RequestComponent();
    }
}

std::shared_ptr<PassiveComponent> CPassiveComponentReference::GetComponent() const
{
    if (!m_resource)
    {
        return nullptr;
    }

    auto passiveResource = std::dynamic_pointer_cast<CPassiveComponentResource>(m_resource);
    if (!passiveResource || !passiveResource->IsFinalized())
    {
        return nullptr;
    }

    return passiveResource->GetComponent();
}

bool CPassiveComponentReference::IsReady() const
{
    auto component = GetComponent();
    return component != nullptr;
}

void CPassiveComponentReference::OnLoaded()
{
    RequestComponent();
}

void CPassiveComponentReference::RequestComponent()
{
    const std::string fileName = GetComponentFileName();
    if (!fileName.empty() && fileName != "undefined")
    {
        auto* resourceManager = Core::CoreSystem::GetResourceManager();
        if (resourceManager)
        {
            m_resource = resourceManager->RequestResource<CPassiveComponentResource>(fileName);
        }
    }
}
```

**Key Design Decisions**:
- ? Mirrors `CResourceReference` pattern for familiarity
- ? Stores file path, not component instance (resource system owns instance)
- ? Provides type-safe access via `CTypedPassiveComponentReference<T>`
- ? Automatic loading via `OnLoaded()` hook
- ? Integrates with reflection for serialization

---

### Phase 2: Integration with Existing Systems

#### 2.1 Update ComponentSystem.h

**File**: `Core/ComponentSystem/ComponentSystem.h`

```cpp
#pragma once
// ComponentSystem.h - umbrella header; include this for full ComponentSystem access.
#include "Component.h"
#include "ComponentFactory.h"
#include "ComponentPool.h"
#include "ComponentManager.h"
#include "ComponentReference.h"
#include "ComponentRegistry.h"
#include "PassiveComponent.h"               // NEW
#include "PassiveComponentReference.h"      // NEW
```

#### 2.2 Update ComponentRegistry

**Goal**: Allow passive components to be registered for editor discovery.

**File**: `Core/ComponentSystem/ComponentRegistry.h`

```cpp
struct ComponentTypeInfo
{
    std::string className;      // Class name for ClassFactory::CreateObject
    std::string displayName;    // Human-readable name for UI
    std::string category;       // Optional category for grouping
    bool isPassive = false;     // NEW: Flag to identify passive components
};
```

**Update Registration Macros**:

```cpp
// Add new macro for passive component registration
#define REGISTER_PASSIVE_COMPONENT(_className, _prettyName, _category) \
    AutoRegisterComponent _className::s_autoRegister(#_className, _prettyName, _category, true);
```

**Update Constructor**:

```cpp
class AutoRegisterComponent
{
public:
    AutoRegisterComponent(const char* className, const char* prettyName, 
                         const char* category, bool isPassive = false);
};
```

#### 2.3 Add Reflection Type Support

**File**: `Core/Reflection/Types/passive_component.h` (NEW)

```cpp
#pragma once
#include "Reflection/ReflectionMacros.h"
#include "ComponentSystem/PassiveComponentReference.h"

// Reflection support for passive component references
REFL_DECLARE_TYPE_MEMBER_PASSIVECOMPONENT(PassiveComponentReference)
REFL_DECLARE_TYPE_MEMBER_VECTOR_PASSIVECOMPONENT(PassiveComponentReferenceVector)

// Macro for defining passive component reference members in reflected objects
#define REFL_DEFINE_PASSIVECOMPONENT_MEMBER(ClassName, MemberName) \
    REFL_DEFINE_MEMBER(ClassName, MemberName, PassiveComponentReference)
```

---

### Phase 3: Example Implementation

#### 3.1 Create Example Passive Component

**File**: `Shared/PassiveComponents/WeaponDataComponent.h`

```cpp
#pragma once
#include "ComponentSystem/PassiveComponent.h"

/** @brief Example passive component that stores weapon configuration data.
 *  Multiple weapons can reference the same shared data instance. */
class CWeaponDataComponent : public ComponentSystem::PassiveComponent
{
public:
    REFL_DECLARE_OBJECT(CWeaponDataComponent, ComponentSystem::PassiveComponent);
    DECLARE_COMPONENT();  // For registry

    CWeaponDataComponent() = default;
    ~CWeaponDataComponent() override = default;

    bool OnLoaded() override
    {
        // Validate data after loading
        if (m_damage < 0.0f || m_fireRate <= 0.0f)
        {
            return false;
        }
        return true;
    }

    // Getters
    float GetDamage() const { return m_damage; }
    float GetFireRate() const { return m_fireRate; }
    int GetAmmoCapacity() const { return m_ammoCapacity; }
    const std::string& GetWeaponName() const { return m_weaponName; }

private:
    // Serialized data (reflected members)
    std::string m_weaponName = "Unnamed Weapon";
    float m_damage = 10.0f;
    float m_fireRate = 1.0f;  // Shots per second
    int m_ammoCapacity = 30;
};

// Typed reference for type-safe access
class CWeaponDataReference : public ComponentSystem::CTypedPassiveComponentReference<CWeaponDataComponent>
{
public:
    REFL_DECLARE_OBJECT(CWeaponDataReference, ComponentSystem::CPassiveComponentReference);
};
```

**File**: `Shared/PassiveComponents/WeaponDataComponent.cpp`

```cpp
#include "WeaponDataComponent.h"

REFL_DEFINE_OBJECT(CWeaponDataComponent)
    REFL_DEFINE_STRING_MEMBER(CWeaponDataComponent, m_weaponName),
    REFL_DEFINE_FLOAT_MEMBER(CWeaponDataComponent, m_damage),
    REFL_DEFINE_FLOAT_MEMBER(CWeaponDataComponent, m_fireRate),
    REFL_DEFINE_INT_MEMBER(CWeaponDataComponent, m_ammoCapacity),
REFL_DEFINE_END

REGISTER_PASSIVE_COMPONENT(CWeaponDataComponent, "Weapon Data", "Gameplay/Data");

REFL_DEFINE_OBJECT(CWeaponDataReference)
REFL_DEFINE_END
```

**Example JSON File**: `assets/weapons/rifle.passive.json`

```json
{
    "_class": "CWeaponDataComponent",
    "m_weaponName": "Assault Rifle",
    "m_damage": 25.0,
    "m_fireRate": 10.0,
    "m_ammoCapacity": 30
}
```

#### 3.2 Use in Active Component

**File**: `Shared/Components/WeaponComponent.h`

```cpp
#pragma once
#include "ComponentSystem/ComponentSystem.h"
#include "Shared/PassiveComponents/WeaponDataComponent.h"

/** @brief Active component that uses passive weapon data.
 *  Multiple weapon instances can share the same CWeaponDataComponent. */
class CWeaponComponent : public ComponentSystem::Component
{
public:
    REFL_DECLARE_OBJECT(CWeaponComponent, ComponentSystem::Component);
    DECLARE_COMPONENT();

protected:
    bool OnInitialize() override
    {
        // Check if weapon data is loaded
        if (!m_weaponData.IsReady())
        {
            return false;  // Wait for passive component to load
        }

        m_currentAmmo = m_weaponData.GetComponentAs<CWeaponDataComponent>()->GetAmmoCapacity();
        return true;
    }

    void OnUpdate(double deltaTime) override
    {
        auto weaponData = m_weaponData.GetComponentAs<CWeaponDataComponent>();
        if (!weaponData)
        {
            return;  // Data not loaded yet
        }

        m_fireTimer += deltaTime;
        float fireInterval = 1.0f / weaponData->GetFireRate();

        if (m_fireTimer >= fireInterval && m_currentAmmo > 0)
        {
            Fire(weaponData->GetDamage());
            m_fireTimer = 0.0f;
            m_currentAmmo--;
        }
    }

private:
    void Fire(float damage)
    {
        // Fire weapon logic...
    }

    CWeaponDataReference m_weaponData;  // Reference to shared passive component
    int m_currentAmmo = 0;              // Instance-specific state
    double m_fireTimer = 0.0;           // Instance-specific state
};
```

**Key Pattern**:
- ? Active component references passive component via `CWeaponDataReference`
- ? Passive component stores shared configuration (damage, fire rate)
- ? Active component stores instance-specific state (current ammo, timer)
- ? Multiple `CWeaponComponent` instances can reference same `CWeaponDataComponent`

---

### Phase 4: Testing & Validation

#### 4.1 Unit Tests

**File**: `Tests/ComponentSystem/PassiveComponentTests.cpp`

```cpp
#include "gtest/gtest.h"
#include "ComponentSystem/PassiveComponent.h"
#include "ComponentSystem/PassiveComponentReference.h"
#include "Shared/PassiveComponents/WeaponDataComponent.h"

TEST(PassiveComponentTest, LoadFromFile)
{
    auto* rm = Core::CoreSystem::GetResourceManager();
    auto resource = rm->RequestResource<CPassiveComponentResource>("./test_data/weapon.passive.json");

    // Wait for loading
    while (!resource->IsFinalized()) {
        rm->UpdateFinalization();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    ASSERT_NE(resource->GetComponent(), nullptr);
    ASSERT_EQ(resource->GetComponent()->GetSourcePath(), "./test_data/weapon.passive.json");
}

TEST(PassiveComponentTest, SharedInstance)
{
    auto* rm = Core::CoreSystem::GetResourceManager();

    // Request same component twice
    auto resource1 = rm->RequestResource<CPassiveComponentResource>("./test_data/weapon.passive.json");
    auto resource2 = rm->RequestResource<CPassiveComponentResource>("./test_data/weapon.passive.json");

    // Should be same resource instance
    ASSERT_EQ(resource1.get(), resource2.get());

    // Wait for loading
    while (!resource1->IsFinalized()) {
        rm->UpdateFinalization();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Should be same component instance
    ASSERT_EQ(resource1->GetComponent().get(), resource2->GetComponent().get());
}

TEST(PassiveComponentReferenceTest, TypedAccess)
{
    CWeaponDataReference ref;
    ref.SetComponentFileName("./test_data/weapon.passive.json");

    auto* rm = Core::CoreSystem::GetResourceManager();
    while (!ref.IsReady()) {
        rm->UpdateFinalization();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    auto weaponData = ref.GetComponentAs<CWeaponDataComponent>();
    ASSERT_NE(weaponData, nullptr);
    ASSERT_GT(weaponData->GetDamage(), 0.0f);
}
```

#### 4.2 Integration Tests

- Test passive component loading in entity serialization
- Test multiple active components referencing same passive component
- Test hot-reload of passive components
- Test passive component validation (OnLoaded failures)
- Test editor integration (add passive component to entity)

---

### Phase 5: Documentation & Migration

#### 5.1 Update COMPONENT_SYSTEM.skill

Add new section:

```markdown
### Passive Components

**Purpose**: Data-only components loaded from files and shared across references.

**Key Features**:
- No Update loop (no OnUpdate callback)
- Loaded via Resource System
- Single instance per file (path-based deduplication)
- No hierarchy (no parent/children)
- No unique ComponentId
- Shared across multiple references

**Usage Pattern**:
```cpp
// Define passive component
class CMyDataComponent : public PassiveComponent {
    float m_value;
    // ... data members
};

// Reference from active component
class CMyActiveComponent : public Component {
    CTypedPassiveComponentReference<CMyDataComponent> m_data;

    void OnUpdate(double dt) override {
        auto data = m_data.GetComponentAs<CMyDataComponent>();
        if (data) {
            UseData(data->m_value);
        }
    }
};
```

**When to Use**:
- ? Shared configuration data (weapon stats, AI parameters)
- ? Static lookup tables (damage curves, progression data)
- ? Template definitions (spawn parameters, effect configs)
- ? Dynamic state (use active components)
- ? Per-instance data (use active component members)
```

#### 5.2 Create Migration Guide

**File**: `Core/ComponentSystem/PASSIVE_COMPONENTS_MIGRATION.md`

Document:
- How to convert data-heavy active components to passive
- How to refactor shared data into passive components
- How to reference passive components from active components
- How to handle component dependencies (active depends on passive)

#### 5.3 Update Editor Documentation

- Add UI for creating passive component files
- Add drag-drop support for passive component references
- Add visual indicator for passive vs active components
- Add inspector for passive component references

---

## Implementation Checklist

### Core Infrastructure
- [ ] Create `PassiveComponent` base class
- [ ] Create `CPassiveComponentResource`
- [ ] Create `CPassiveComponentReference`
- [ ] Create `CTypedPassiveComponentReference`
- [ ] Update `ComponentSystem.h` umbrella header
- [ ] Add reflection type support (`passive_component.h`)

### ComponentRegistry Integration
- [ ] Add `isPassive` flag to `ComponentTypeInfo`
- [ ] Add `REGISTER_PASSIVE_COMPONENT` macro
- [ ] Update `AutoRegisterComponent` constructor
- [ ] Update editor to filter passive/active components

### Resource System Integration
- [ ] Register `.passive.json` extension
- [ ] Implement JSON deserialization via reflection
- [ ] Add hot-reload support for passive components
- [ ] Add progress tracking for passive component loading

### Example Implementation
- [ ] Create `CWeaponDataComponent` example
- [ ] Create `CWeaponDataReference` typed reference
- [ ] Create example JSON files
- [ ] Create example active component using passive data

### Testing
- [ ] Unit tests for `PassiveComponent`
- [ ] Unit tests for `PassiveComponentReference`
- [ ] Integration tests for resource loading
- [ ] Integration tests for shared instances
- [ ] Performance tests (loading time, memory usage)

### Documentation
- [ ] Update `COMPONENT_SYSTEM.skill` with passive component section
- [ ] Create `PASSIVE_COMPONENTS_MIGRATION.md` guide
- [ ] Add inline documentation to all new classes
- [ ] Add examples to documentation
- [ ] Update editor help system

### Editor Integration
- [ ] Add passive component creation UI
- [ ] Add passive component file browser
- [ ] Add visual indicator for passive references
- [ ] Add inspector for passive component references
- [ ] Add validation for passive component files

---

## Design Decisions & Rationale

### Why Separate Base Class?
**Decision**: Create `PassiveComponent` as separate base class, not a flag on `Component`.

**Rationale**:
- ? Clearer separation of concerns (data vs behavior)
- ? Avoids confusion about lifecycle methods (OnUpdate, etc.)
- ? Prevents accidental pooling of passive components
- ? Allows different inheritance patterns
- ? Slightly more code duplication (reflection boilerplate)

**Alternative**: Add `bool m_isPassive` flag to `Component` and skip Update if true.

### Why Use Resource System?
**Decision**: Load passive components via Resource System, not ComponentManager.

**Rationale**:
- ? Reuses existing async loading infrastructure
- ? Automatic path-based deduplication
- ? Hot-reload support out of the box
- ? Consistent with other shared assets (textures, materials)
- ? Main thread finalization for safety
- ? More complex integration

**Alternative**: Create separate `PassiveComponentManager` with custom loading.

### Why .passive.json Extension?
**Decision**: Use `.passive.json` convention for passive component files.

**Rationale**:
- ? Clearly identifies passive components
- ? Prevents confusion with active component serialization
- ? Allows different parsing/validation rules
- ? Editor can provide specialized handling
- ? Longer file names

**Alternative**: Use same `.json` extension, rely on type detection.

### Why No Hierarchy?
**Decision**: Passive components cannot have parent/children.

**Rationale**:
- ? Simplifies implementation (no tree management)
- ? Passive components are pure data, not scene graph
- ? Hierarchy is for active components with Update loop
- ? Cannot nest passive component data

**Alternative**: Allow hierarchy but document as "data-only".

---

## Performance Considerations

### Memory
- **Single instance per file**: Significant memory savings vs per-instance duplication
- **Shared ownership**: `shared_ptr` overhead (16 bytes per reference)
- **No pooling**: Passive components are not pooled (always allocated)

### Loading
- **Async loading**: No frame hitches from file I/O
- **Deduplication**: Second request returns cached instance instantly
- **Finalization**: `OnLoaded()` called on main thread (can be expensive)

### Runtime
- **No Update overhead**: Passive components never enter Update loop
- **Pointer indirection**: One extra indirection via reference (negligible)
- **Cache efficiency**: Shared data improves cache locality

### Optimization Tips
- Pre-load commonly used passive components at startup
- Use passive components for large shared datasets
- Keep `OnLoaded()` fast (runs on main thread)
- Consider caching frequently accessed passive component pointers

---

## Security & Validation

### File Validation
- Validate JSON schema on load
- Check `_class` field matches expected type
- Validate passive component properties (ranges, constraints)
- Handle missing or corrupt files gracefully

### Type Safety
- `CTypedPassiveComponentReference<T>` enforces compile-time type safety
- Runtime `dynamic_pointer_cast` for safe downcasting
- Null checks before accessing passive component data

### Hot-Reload Safety
- Active components must handle null passive component pointers
- Check `IsReady()` before accessing data
- Re-validate on hot-reload (OnLoaded may fail)

---

## Future Enhancements

### Phase 2+ Features
- **Passive component dependencies**: Allow passive components to reference other passive components
- **Versioning**: Track passive component file versions for migration
- **Diff/Patch**: Hot-reload only changed properties
- **Binary format**: Support faster loading than JSON
- **Compression**: Compress large passive component files
- **Validation DSL**: Define validation rules in JSON schema
- **Editor tools**: Bulk convert active ? passive, refactoring tools

---

## Questions & Decisions Needed

1. **Naming**: Is "PassiveComponent" the right name? Alternatives: `DataComponent`, `SharedComponent`, `ConfigComponent`
2. **Extension**: Use `.passive.json` or something else? (`.data.json`, `.config.json`)
3. **Validation**: Should `OnLoaded()` failures prevent finalization or log warnings?
4. **Hot-reload**: Should active components be notified when passive component reloads?
5. **Editor**: Should editor allow creating passive components from templates?
6. **Migration**: Should we provide tools to auto-convert data-heavy active components?

---

## Timeline Estimate

| Phase | Tasks | Estimated Time |
|-------|-------|----------------|
| Phase 1 | Core infrastructure | 2-3 days |
| Phase 2 | System integration | 1-2 days |
| Phase 3 | Example implementation | 1 day |
| Phase 4 | Testing & validation | 2-3 days |
| Phase 5 | Documentation & migration | 1-2 days |
| **Total** | | **7-11 days** |

---

**Version**: 1.0  
**Status**: Proposed  
**Author**: Development Team  
**Date**: 2024  
**Related Documents**: COMPONENT_SYSTEM.skill, RESOURCE_SYSTEM.skill.md
