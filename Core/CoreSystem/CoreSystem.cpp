#include "CoreSystem.h"

#include "ComponentSystem/ComponentSystem.h"
#include "ComponentSystem/ComponentSystemScheduler.h"
#include "ComponentSystem/ComponentRegistry.h"
#include "ResourceManager/ResourceManager.h"
#include "FunctionCallManager.h"
#include "FileSystem/StandardFileSystem.h"
#include "Net/NexusClient.h"
#include "Input/InputActionManager.h"

#include <iostream>

using namespace ComponentSystem;

namespace Core {

// ── Static member definitions ─────────────────────────────────────────────────

std::unique_ptr<FileSystem::FileSystemManager>             CoreSystem::s_fileSystemManager = nullptr;
std::unique_ptr<ResourceSystem::ResourceManager>           CoreSystem::s_resourceManager = nullptr;
std::unique_ptr<ComponentSystem::ComponentManager>         CoreSystem::s_componentManager = nullptr;
std::unique_ptr<FunctionCall::FunctionCallManager>         CoreSystem::s_functionManager = nullptr;
std::unique_ptr<JobSystem>                                 CoreSystem::s_jobSystem = nullptr;
std::unique_ptr<ComponentSystem::ComponentSystemScheduler> CoreSystem::s_componentSystemScheduler = nullptr;
std::unique_ptr<CThreadSafeLog>                           CoreSystem::s_log = nullptr;
std::unique_ptr<CNexusClient>                             CoreSystem::s_nexusClient = nullptr;
std::shared_ptr<Input::InputActionManager>                CoreSystem::s_actionManager = nullptr;

bool          CoreSystem::s_initialized        = false;
InitFlag      CoreSystem::s_initFlags          = InitFlag::None;
FunctionQueue CoreSystem::s_renderFunctionQueue;

// ── Lifecycle ─────────────────────────────────────────────────────────────────

bool CoreSystem::Initialize(InitFlag flags)
{
    if (s_initialized)
    {
        CoreDebug.printf("CoreSystem: Already initialized\n");
        return true;
    }
    s_initFlags = flags;

    try
    {
        // Initialize Thread-Safe Log (first, so other systems can use it)
        if (HasFlag(flags, InitFlag::Log))
        {
            s_log = std::make_unique<CThreadSafeLog>();
            s_log->Start();
        }

        CoreDebug.printf("CoreSystem: Initializing core engine systems...\n");

        // Initialize File System Manager
        if (HasFlag(flags, InitFlag::FileSystem))
        {
            CoreDebug.printf("CoreSystem: Creating FileSystemManager...\n");
            s_fileSystemManager = std::make_unique<FileSystem::FileSystemManager>();

            auto standardFs = std::make_unique<FileSystem::StandardFileSystem>();
            s_fileSystemManager->AddFileSystem(std::move(standardFs));
        }

        // Initialize Resource Manager
        if (HasFlag(flags, InitFlag::ResourceManager))
        {
            if (!s_fileSystemManager)
            {
                CoreDebug.warning("CoreSystem: ResourceManager requires FileSystem - skipping\n");
            }
            else
            {
                CoreDebug.printf("CoreSystem: Creating ResourceManager...\n");
                s_resourceManager = std::make_unique<ResourceSystem::ResourceManager>(s_fileSystemManager.get());

                if (!s_resourceManager->Start())
                {
                    CoreDebug.warning("CoreSystem: Failed to start ResourceManager\n");
                    return false;
                }
            }
        }

        // Initialize Job System
        if (HasFlag(flags, InitFlag::JobSystem))
        {
            CoreDebug.printf("CoreSystem: Creating JobSystem...\n");
            s_jobSystem = std::make_unique<::JobSystem>(8);
            CoreDebug.printf("CoreSystem: JobSystem initialized with %d worker threads\n", s_jobSystem->GetThreadCount());
        }

        // Initialize Component Manager
        if (HasFlag(flags, InitFlag::ComponentManager))
        {
            CoreDebug.printf("CoreSystem: Creating ComponentManager...\n");
            s_componentManager = std::make_unique<ComponentSystem::ComponentManager>();

            if (!s_componentManager->Initialize())
            {
                CoreDebug.warning("CoreSystem: Failed to initialize ComponentManager\n");
                return false;
            }
        }

        // Initialize Function Call Manager
        if (HasFlag(flags, InitFlag::FunctionCallManager))
        {
            CoreDebug.printf("CoreSystem: Creating FunctionCallManager...\n");
            s_functionManager = std::make_unique<FunctionCall::FunctionCallManager>(false);
        }

        // Initialize Job System Scheduler
        if (HasFlag(flags, InitFlag::Scheduler))
        {
            if (!s_componentManager)
            {
                CoreDebug.warning("CoreSystem: Scheduler requires ComponentManager - skipping\n");
            }
            else
            {
                CoreDebug.printf("CoreSystem: Creating ComponentSystemScheduler...\n");
                s_componentSystemScheduler = std::make_unique<ComponentSystem::ComponentSystemScheduler>(
                    s_componentManager.get()
                );

                if (!s_componentSystemScheduler->Initialize())
                {
                    CoreDebug.warning("CoreSystem: Failed to initialize JobSystemScheduler\n");
                    return false;
                }

                s_componentSystemScheduler->SetExecutionPolicy(ComponentSystem::ExecutionPolicy::Custom);
            }
        }

        // Initialize Nexus Client
        if (HasFlag(flags, InitFlag::NexusClient))
        {
            CoreDebug.printf("CoreSystem: Creating NexusClient...\n");
            s_nexusClient = std::make_unique<CNexusClient>();
        }

        // Initialize Input Action Manager
        if (HasFlag(flags, InitFlag::ActionManager))
        {
            CoreDebug.printf("CoreSystem: Creating InputActionManager...\n");
            s_actionManager = std::make_unique<Input::InputActionManager>();

            if (!s_actionManager->Initialize())
            {
                CoreDebug.warning("CoreSystem: Failed to initialize InputActionManager\n");
                return false;
            }
        }

        s_initialized = true;
        CoreDebug.printf("CoreSystem: All core systems initialized successfully!\n");
        return true;
    }
    catch (const std::exception& e)
    {
        CoreDebug.warning("CoreSystem: Exception during initialization: %s\n", e.what());
        Shutdown();
        return false;
    }
}

void CoreSystem::Shutdown()
{
    if (!s_initialized) return;

    CoreDebug.printf("CoreSystem: Shutting down core engine systems...\n");

    if (s_actionManager)
    {
        CoreDebug.printf("CoreSystem: Shutting down InputActionManager...\n");
        s_actionManager->Shutdown();
        s_actionManager.reset();
    }

    if (s_nexusClient)
    {
        CoreDebug.printf("CoreSystem: Shutting down NexusClient...\n");
        s_nexusClient->Disconnect();
        s_nexusClient.reset();
    }

    if (s_componentSystemScheduler)
    {
        CoreDebug.printf("CoreSystem: Shutting down JobSystemScheduler...\n");
        s_componentSystemScheduler->Shutdown();
        s_componentSystemScheduler.reset();
    }

    if (s_componentManager)
    {
        CoreDebug.printf("CoreSystem: Shutting down ComponentManager...\n");
        s_componentManager->Shutdown();
        s_componentManager.reset();
    }

    if (s_functionManager)
    {
        CoreDebug.printf("CoreSystem: Shutting down FunctionCallManager...\n");
        s_functionManager.reset();
    }

    if (s_jobSystem)
    {
        CoreDebug.printf("CoreSystem: Shutting down JobSystem...\n");
        s_jobSystem->Shutdown();
        s_jobSystem.reset();
    }

    if (s_resourceManager)
    {
        CoreDebug.printf("CoreSystem: Shutting down ResourceManager...\n");
        s_resourceManager.reset();
    }

    if (s_fileSystemManager)
    {
        CoreDebug.printf("CoreSystem: Shutting down FileSystemManager...\n");
        s_fileSystemManager.reset();
    }

    if (s_log)
    {
        CoreDebug.printf("CoreSystem: Shutting down ThreadSafeLog...\n");
        s_log->Stop();
        s_log.reset();
    }

    s_initialized = false;
    s_initFlags   = InitFlag::None;
    CoreDebug.printf("CoreSystem: All core systems shutdown complete\n");
}

// ── Utility ───────────────────────────────────────────────────────────────────

void CoreSystem::PrintSystemStatus()
{
    CoreDebug.printf("\n=== Core System Status ===\n");
    CoreDebug.printf("Initialized: %s\n", (s_initialized ? "Yes" : "No"));

    if (!s_initialized)
    {
        CoreDebug.printf("========================\n\n");
        return;
    }

    CoreDebug.printf("FileSystemManager: %s\n", (s_fileSystemManager ? "Active" : "Null"));

    if (s_fileSystemManager)
    {
        CoreDebug.printf("  File Systems: %d\n", s_fileSystemManager->GetFileSystemCount());
    }

    CoreDebug.printf("ResourceManager: %s\n", (s_resourceManager ? "Active" : "Null"));

    if (s_resourceManager)
    {
        CoreDebug.printf("  Running: %s\n", (s_resourceManager->IsRunning() ? "Yes" : "No"));
        CoreDebug.printf("  Loaded Resources: %zu\n", s_resourceManager->GetLoadedResourceCount());
        CoreDebug.printf("  Pending Loads: %zu\n", s_resourceManager->GetPendingLoadCount());
        CoreDebug.printf("  Pending Finalizations: %zu\n", s_resourceManager->GetPendingFinalizationCount());
    }

    CoreDebug.printf("JobSystem: %s\n", (s_jobSystem ? "Active" : "Null"));

    if (s_jobSystem)
    {
        CoreDebug.printf("  Worker Threads: %zu\n", s_jobSystem->GetThreadCount());
        CoreDebug.printf("  Active Jobs: %zu\n", s_jobSystem->GetActiveJobCount());
        CoreDebug.printf("  Pending Jobs: %zu\n", s_jobSystem->GetPendingJobCount());
        CoreDebug.printf("  Shutting Down: %s\n", (s_jobSystem->IsShuttingDown() ? "Yes" : "No"));
    }

    CoreDebug.printf("ComponentManager: %s\n", (s_componentManager ? "Active" : "Null"));

    if (s_componentManager)
    {
        CoreDebug.printf("  Active Components: %zu\n", s_componentManager->GetActiveComponentCount<ComponentSystem::Component>());
        CoreDebug.printf("  Total Components: %zu\n", s_componentManager->GetTotalComponentCount<ComponentSystem::Component>());
    }

    CoreDebug.printf("JobSystemScheduler: %s\n", (s_componentSystemScheduler ? "Active" : "Null"));

    if (s_componentSystemScheduler)
    {
        CoreDebug.printf("  Registered Phases: %zu\n", s_componentSystemScheduler->GetRegisteredPhaseCount());
        CoreDebug.printf("  Active Jobs: %zu\n", s_componentSystemScheduler->GetActiveJobCount());
        CoreDebug.printf("  Pending Jobs: %zu\n", s_componentSystemScheduler->GetPendingJobCount());

        const char* policyNames[] = { "Sequential", "Parallel", "Custom" };
        int policyIndex = static_cast<int>(s_componentSystemScheduler->GetExecutionPolicy());
        CoreDebug.printf("  Execution Policy: %s\n", policyNames[policyIndex]);
    }

    CoreDebug.printf("FunctionCallManager: %s\n", (s_functionManager ? "Active" : "Null"));

    if (s_functionManager)
    {
        CoreDebug.printf("  Registered Functions: %zu\n", s_functionManager->GetFunctionCount());
    }

    CoreDebug.printf("ThreadSafeLog: %s\n", (s_log ? "Active" : "Null"));

    if (s_log)
    {
        CoreDebug.printf("  Running: %s\n", (s_log->IsRunning() ? "Yes" : "No"));
        CoreDebug.printf("  Pending Messages: %zu\n", s_log->GetPendingCount());
    }

    CoreDebug.printf("NexusClient: %s\n", (s_nexusClient ? "Active" : "Null"));

    if (s_nexusClient)
    {
        CoreDebug.printf("  Connected: %s\n", (s_nexusClient->IsConnected() ? "Yes" : "No"));
        CoreDebug.printf("  App Name: %s\n", s_nexusClient->GetAppName().c_str());
    }

    CoreDebug.printf("=========================\n\n");
}

void CoreSystem::SetActionManager(Input::InputActionManager* mgr)
{
    if (!mgr)
    {
        s_actionManager.reset();
        return;
    }

    // Create a non-owning shared_ptr wrapper so CoreSystem can reference the
    // component instance without taking ownership or trying to delete it.
    s_actionManager = std::shared_ptr<Input::InputActionManager>(mgr, [](Input::InputActionManager*) {});
}

} // namespace Core
