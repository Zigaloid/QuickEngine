#include "ComponentSystemScheduler.h"
#include "CoreSystem/CoreSystem.h"

#include <iostream>
#include <cassert>
#include <chrono>

namespace ComponentSystem {

    // ComponentUpdateBatch implementation
    bool ComponentUpdateBatch::isCompleted() const {
        if (m_completed) return true;

        // A batch with no futures is not considered completed
        if (m_futures.empty()) return false;

        return std::all_of(m_futures.begin(), m_futures.end(),
            [](const std::future<void>& future) {
                return !future.valid() ||
                    future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
            });
    }

    // ComponentSystemScheduler implementation methods that need CoreSystem access
    JobSystem* ComponentSystemScheduler::getJobSystem() const {
        return Core::CoreSystem::GetJobSystem();
    }

    bool ComponentSystemScheduler::isCoreSystemInitialized() const {
        return Core::CoreSystem::IsInitialized();
    }

    // Dependency resolution
    std::vector<std::vector<std::type_index>> ComponentSystemScheduler::resolveDependencies() const {
        std::vector<std::vector<std::type_index>> batches;
        std::unordered_set<std::type_index> processed;

        while (processed.size() < m_phases.size()) {
            std::vector<std::type_index> currentBatch;

            // Find types with no unprocessed dependencies
            for (const auto& phase : m_phases) {
                // Skip if this type has already been processed
                if (processed.find(phase.componentType) != processed.end()) {
                    continue;
                }

                bool canExecute = true;
                for (const auto& dep : phase.dependencies) {
                    // Check if dependency has not been processed yet
                    if (processed.find(dep) == processed.end()) {
                        canExecute = false;
                        break;
                    }
                }

                if (canExecute) {
                    currentBatch.push_back(phase.componentType);
                }
            }

            if (currentBatch.empty()) {
                // Circular dependency detected
                assert(false && "Circular dependency detected in component phases");
                break;
            }

            // Mark current batch as processed
            for (const auto& type : currentBatch) {
                processed.insert(type);
            }

            batches.push_back(std::move(currentBatch));
        }

        return batches;
    }

    // Sort phases by priority
    void ComponentSystemScheduler::sortPhasesByPriority() {
        std::sort(m_phases.begin(), m_phases.end(),
            [](const ComponentPhase& a, const ComponentPhase& b) {
                return a.priority < b.priority;
            });

        // Rebuild type to index mapping
        m_typeToPhaseIndex.clear();
        for (size_t i = 0; i < m_phases.size(); ++i) {
            m_typeToPhaseIndex[m_phases[i].componentType] = i;
        }
    }

    // Generic version using type_index
    std::unique_ptr<ComponentUpdateBatch> ComponentSystemScheduler::updateComponentTypeByIndexAsync(std::type_index typeIndex, double deltaTime) {
        JobSystem* jobSystem = getJobSystem();
        if (!m_componentManager || !jobSystem) {
            return std::make_unique<ComponentUpdateBatch>(typeIndex, 0);
        }

        // Look up the registered update function for this type
        auto updateFuncIt = m_componentUpdateRegistry.find(typeIndex);
        if (updateFuncIt == m_componentUpdateRegistry.end()) {
            // Type not registered for async updates, return empty batch
            return std::make_unique<ComponentUpdateBatch>(typeIndex, 0);
        }

        // Call the registered update function which will handle the specific type
        return updateFuncIt->second(deltaTime);
    }

    // Private update methods
    void ComponentSystemScheduler::updateSequential(double deltaTime) {
        // Update component types one by one in priority order
        for (const auto& phase : m_phases) {
            // For sequential execution, we still use the job system but wait for completion
            auto batch = updateComponentTypeByIndexAsync(phase.componentType, deltaTime);
            if (batch) 
            {
                batch->waitForCompletion();
            }
        }
    }

    void ComponentSystemScheduler::updateParallel(double deltaTime) {
        // Submit all component types to job system simultaneously
        std::vector<std::unique_ptr<ComponentUpdateBatch>> batches;
        batches.reserve(m_phases.size());

        for (const auto& phase : m_phases) {
            auto batch = updateComponentTypeByIndexAsync(phase.componentType, deltaTime);
            if (batch) {
                batches.push_back(std::move(batch));
            }
        }

        // Wait for all batches to complete
        for (auto& batch : batches) {
            batch->waitForCompletion();
        }
    }

    void ComponentSystemScheduler::updateWithDependencies(double deltaTime) {
        auto dependencyBatches = resolveDependencies();

        // Execute each batch of independent types
        for (const auto& batch : dependencyBatches) {
            std::vector<std::unique_ptr<ComponentUpdateBatch>> currentBatches;
            currentBatches.reserve(batch.size());

            // Submit all types in current batch
            for (const auto& typeIndex : batch) {
                auto updateBatch = updateComponentTypeByIndexAsync(typeIndex, deltaTime);
                if (updateBatch) {
                    currentBatches.push_back(std::move(updateBatch));
                }
            }

            // Wait for current batch to complete before proceeding to next
            for (auto& updateBatch : currentBatches) {
                updateBatch->waitForCompletion();
            }
        }
    }

    // Example usage and utility functions
    namespace SchedulerUtils {
        
        // Template helper for bulk registration with priorities
        template<typename... ComponentTypes>
        void RegisterComponentTypes(ComponentSystemScheduler* scheduler, int basePriority = 0) {
            static_assert(sizeof...(ComponentTypes) > 0, "Must provide at least one component type");
            
            int currentPriority = basePriority;
            auto registerType = [&]<typename T>() {
                scheduler->RegisterComponentType<T>(currentPriority++);
            };
            
            (registerType.template operator()<ComponentTypes>(), ...);
        }
        
        // Helper to print scheduler statistics
        void PrintSchedulerStats(const ComponentSystemScheduler* scheduler) {
            if (!scheduler) return;
            
            std::cout << "ComponentSystemScheduler Statistics:\n";
            std::cout << "  Registered Phases: " << scheduler->GetRegisteredPhaseCount() << "\n";
            std::cout << "  Pending Jobs: " << scheduler->GetPendingJobCount() << "\n";
            std::cout << "  Active Jobs: " << scheduler->GetActiveJobCount() << "\n";
            std::cout << "  Execution Policy: ";
            
            switch (scheduler->GetExecutionPolicy()) {
                case ExecutionPolicy::Sequential:
                    std::cout << "Sequential\n";
                    break;
                case ExecutionPolicy::Parallel:
                    std::cout << "Parallel\n";
                    break;
                case ExecutionPolicy::Custom:
                    std::cout << "Custom (Dependencies)\n";
                    break;
            }
            
            std::cout << "  Registered Phases:\n";
            for (const auto& phase : scheduler->GetPhases()) {
                std::cout << "    - " << phase.name << " (Priority: " << phase.priority;
                if (!phase.dependencies.empty()) {
                    std::cout << ", Dependencies: " << phase.dependencies.size();
                }
                std::cout << ")\n";
            }
        }
    }

} // namespace ComponentSystem
