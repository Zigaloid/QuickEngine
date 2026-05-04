#pragma once

#include "FileSystem/FileSystemManager.h"
#include "componentSystem/ComponentSystemScheduler.h"
#include "JobSystem/JobSystem.h"
#include "DebugChannel/DebugChannel.h"
#include "CoreSystem/FunctionCallManager.h"
#include "CoreSystem/FunctionQueue.h"
#include "Log/ThreadSafeLog.h"

#include <memory>
#include <iostream>

// Forward-declare ResourceManager to avoid circular include
namespace ResourceSystem { class ResourceManager; }

namespace FileSystem {
    class FileSystemManager;
}
class CNexusClient;
class ComponentRegistry;

// Helper macros for simplified Nexus client usage in application code
#define NEXUS_SUBSCRIBE(pipe, app) Core::CoreSystem::GetNexusClient()->Subscribe(pipe, app);
#define NEXUS_SUBSCRIBE_CALLBACK(pipe, app, callback) Core::CoreSystem::GetNexusClient()->Subscribe(pipe, app, [this](const SNexusMessage& msg) { callback(msg.body); });
#define NEXUS_SUBSCRIBE_BINARY_CALLBACK(pipe, app, callback) Core::CoreSystem::GetNexusClient()->SubscribeBinary(pipe, app, [this](const SNexusBinaryMessage& msg) { callback(msg.data); });
#define NEXUS_CONNECT_AND_REGISTER(ipa,port,app,user)  Core::CoreSystem::GetNexusClient()->Connect(ipa, port); Core::CoreSystem::GetNexusClient()->Register(app, user);
#define NEXUS_SEND_MESSAGE(pipe, type, value) Core::CoreSystem::GetNexusClient()->SendPipeMessage(pipe,type,value)
#define NEXUS_SEND_BINARY_MESSAGE(pipe, value, size) Core::CoreSystem::GetNexusClient()->SendBinaryMessage(pipe, value, size)

namespace Core {

// ── InitFlag ──────────────────────────────────────────────────────────────────

/** @brief Flags to control which subsystems CoreSystem::Initialize() creates.
 *  Combine with bitwise OR. Pass InitFlag::All (the default) to init everything.
 */
enum class InitFlag : uint32_t {
    None                = 0,
    Log                 = 1 << 0,
    FileSystem          = 1 << 1,
    ResourceManager     = 1 << 2,
    JobSystem           = 1 << 3,
    ComponentManager    = 1 << 4,
    FunctionCallManager = 1 << 5,
    Scheduler           = 1 << 6,
    NexusClient         = 1 << 7,

    All = Log | FileSystem | ResourceManager | JobSystem
        | ComponentManager | FunctionCallManager | Scheduler | NexusClient
};

// Bitwise operators for InitFlag
inline constexpr InitFlag operator|(InitFlag a, InitFlag b)
{
    return static_cast<InitFlag>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline constexpr InitFlag operator&(InitFlag a, InitFlag b)
{
    return static_cast<InitFlag>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}
inline constexpr InitFlag operator~(InitFlag a)
{
    return static_cast<InitFlag>(~static_cast<uint32_t>(a));
}
inline constexpr bool HasFlag(InitFlag flags, InitFlag test)
{
    return (flags & test) == test;
}

// ── CoreSystem ────────────────────────────────────────────────────────────────

/** @brief Central static manager that initializes and owns all core engine subsystems. */
class CoreSystem {
private:
    static std::unique_ptr<FileSystem::FileSystemManager>             s_fileSystemManager;
    static std::unique_ptr<ResourceSystem::ResourceManager>           s_resourceManager;
    static std::unique_ptr<ComponentSystem::ComponentManager>         s_componentManager;
    static std::unique_ptr<JobSystem>                                 s_jobSystem;
    static std::unique_ptr<ComponentSystem::ComponentSystemScheduler> s_componentSystemScheduler;
    static std::unique_ptr<FunctionCall::FunctionCallManager>         s_functionManager;
    static std::unique_ptr<CThreadSafeLog>                           s_log;
    static std::unique_ptr<CNexusClient>                             s_nexusClient;
    static bool                                                       s_initialized;
    static InitFlag                                                   s_initFlags;
    static FunctionQueue                                              s_renderFunctionQueue;

    CoreSystem() = delete;
    ~CoreSystem() = delete;

public:
    // ── Lifecycle ─────────────────────────────────────────────────────────────

    /** @brief Initialize all requested subsystems.
     *  @param flags Bitmask of InitFlag values selecting which subsystems to start.
     *  @return true on success, false if any subsystem failed to initialize.
     */
    static bool Initialize(InitFlag flags = InitFlag::All);

    /** @brief Shut down all active subsystems in reverse initialization order. */
    static void Shutdown();

    /** @brief Return true if CoreSystem has been successfully initialized. */
    static bool IsInitialized() { return s_initialized; }

    /** @brief Return the flags that were passed to the last Initialize() call. */
    static InitFlag GetInitFlags() { return s_initFlags; }

    // ── Accessors ─────────────────────────────────────────────────────────────

    static FileSystem::FileSystemManager* GetFileSystemManager()
    {
        return s_fileSystemManager.get();
    }

    static ResourceSystem::ResourceManager* GetResourceManager()
    {
        return s_resourceManager.get();
    }

    static ComponentSystem::ComponentManager* GetComponentManager()
    {
        return s_componentManager.get();
    }

    static FunctionCall::FunctionCallManager* GetFunctionManager()
    {
        return s_functionManager.get();
    }

    static JobSystem* GetJobSystem()
    {
        return s_jobSystem.get();
    }

    static ComponentSystem::ComponentSystemScheduler* GetJobSystemScheduler()
    {
        return s_componentSystemScheduler.get();
    }

    static CThreadSafeLog* GetLog()
    {
        return s_log.get();
    }

    static CNexusClient* GetNexusClient()
    {
        return s_nexusClient.get();
    }

    static FunctionQueue* GetRenderFunctionQueue()
    {
        return &s_renderFunctionQueue;
    }

    // ── Component Retrieval ───────────────────────────────────────────────────

    template<typename T>
    static std::vector<T*> GetComponentsOfType()
    {
        if (s_componentManager)
        {
            return s_componentManager->GetComponentsOfType<T>();
        }
        return {};
    }

    template<typename T>
    static std::vector<T*> GetActiveComponentsOfType()
    {
        if (s_componentManager)
        {
            return s_componentManager->GetActiveComponentsOfType<T>();
        }
        return {};
    }

    template<typename T>
    static T* GetFirstComponentOfType()
    {
        if (s_componentManager)
        {
            auto components = s_componentManager->GetComponentsOfType<T>();
            return components.empty() ? nullptr : components[0];
        }
        return nullptr;
    }

    template<typename T>
    static T* GetFirstActiveComponentOfType()
    {
        if (s_componentManager)
        {
            auto components = s_componentManager->GetActiveComponentsOfType<T>();
            return components.empty() ? nullptr : components[0];
        }
        return nullptr;
    }

    // ── Utility ───────────────────────────────────────────────────────────────

    /** @brief Print a human-readable status summary of all subsystems to the debug channel. */
    static void PrintSystemStatus();
};

} // namespace Core
