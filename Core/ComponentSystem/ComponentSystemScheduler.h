#pragma once

#include <vector>
#include <unordered_map>
#include <typeindex>
#include <memory>
#include <future>
#include <functional>
#include <algorithm>
#include <cassert>
#include "ComponentSystem\ComponentSystem.h"
#include "Profiler\Profiler.h"
#include "Jobsystem\JobSystem.h"
#include <unordered_set>

// Forward declarations to avoid circular dependency
namespace Core {
	class CoreSystem;
}

namespace ComponentSystem {

	// Forward declaration
	class ComponentSystemScheduler;

	// Execution policy for component updates
	enum class ExecutionPolicy {
		Sequential,     // Execute component types one after another
		Parallel,       // Execute all component types in parallel
		Custom          // Use custom dependency graph
	};

	// Component update phase information
	struct ComponentPhase {
		std::type_index componentType;
		int priority;               // Lower values execute first
		std::vector<std::type_index> dependencies;  // Types that must complete before this one
		std::string name;           // Human-readable name for debugging

		ComponentPhase(std::type_index type, int prio = 0, std::string phaseName = "")
			: componentType(type), priority(prio), name(std::move(phaseName)) {
		}
	};

	// Job batch for tracking completion of component type updates
	class ComponentUpdateBatch {
	private:
		std::vector<std::future<void>> m_futures;
		std::type_index m_componentType;
		size_t m_componentCount;
		bool m_completed;

	public:
		ComponentUpdateBatch(std::type_index type, size_t count)
			: m_componentType(type), m_componentCount(count), m_completed(false) {
			m_futures.reserve(count);
		}

		void addFuture(std::future<void>&& future) {
			m_futures.push_back(std::move(future));
		}

		void waitForCompletion() 
		{
			DECLARE_FUNC_LOW();
			if (!m_completed) {
				for (auto& future : m_futures) {
					if (future.valid()) {
						future.wait();
					}
				}
				m_completed = true;
			}
		}

		bool isCompleted() const;

		std::type_index getComponentType() const { return m_componentType; }
		size_t getComponentCount() const { return m_componentCount; }
		size_t getBatchSize() const { return m_futures.size(); }
	};

	// Main JobSystemScheduler class
	class ComponentSystemScheduler {
	private:
		ComponentManager* m_componentManager;
		std::vector<ComponentPhase> m_phases;
		std::unordered_map<std::type_index, size_t> m_typeToPhaseIndex;
		ExecutionPolicy m_executionPolicy;
		bool m_initialized;

		// Registry of component update functions by type
		using ComponentUpdateFunction = std::function<std::unique_ptr<ComponentUpdateBatch>(double)>;
		std::unordered_map<std::type_index, ComponentUpdateFunction> m_componentUpdateRegistry;

		// Get JobSystem from CoreSystem - implementation moved to .cpp file
		JobSystem* getJobSystem() const;

		// Register an update function for a component type
		template<typename T>
		void registerComponentUpdateFunction() {
			static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");

			std::type_index typeIndex(typeid(T));

			// Create a lambda that captures the specific type T and calls updateComponentTypeAsync<T>
			m_componentUpdateRegistry[typeIndex] = [this](double deltaTime) -> std::unique_ptr<ComponentUpdateBatch> {
				return this->updateComponentTypeAsync<T>(deltaTime);
				};
		}

		// Dependency resolution - moved to .cpp
		std::vector<std::vector<std::type_index>> resolveDependencies() const;

		// Sort phases by priority - moved to .cpp
		void sortPhasesByPriority();

		// Update components of a specific type - implementation below class definition
		template<typename T>
		std::unique_ptr<ComponentUpdateBatch> updateComponentTypeAsync(double deltaTime);

		// Generic version using type_index - moved to .cpp
		std::unique_ptr<ComponentUpdateBatch> updateComponentTypeByIndexAsync(std::type_index typeIndex, double deltaTime);

		// Helper method to check if CoreSystem is initialized - implementation in .cpp
		bool isCoreSystemInitialized() const;

		// Private update methods - moved to .cpp
		void updateSequential(double deltaTime);
		void updateParallel(double deltaTime);
		void updateWithDependencies(double deltaTime);

	public:
		ComponentSystemScheduler(ComponentManager* componentManager)
			: m_componentManager(componentManager)
			, m_executionPolicy(ExecutionPolicy::Sequential)
			, m_initialized(false) {

			assert(m_componentManager && "ComponentManager cannot be null");
		}

		~ComponentSystemScheduler() = default;

		// Initialize the scheduler
		bool Initialize() {
			if (m_initialized) return true;

			if (!m_componentManager) {
				return false;
			}

			// Check if CoreSystem and JobSystem are available
			if (!getJobSystem()) {
				return false;
			}

			m_initialized = true;
			return true;
		}

		// Shutdown the scheduler
		void Shutdown() {
			if (!m_initialized) return;

			// Wait for any pending operations
			JobSystem* jobSystem = getJobSystem();
			if (jobSystem) {
				jobSystem->waitForAll();
			}

			m_phases.clear();
			m_typeToPhaseIndex.clear();
			m_componentUpdateRegistry.clear(); // Clear the update registry
			m_initialized = false;
		}

		// Register a component type for async updates
		template<typename T>
		void RegisterComponentType(int priority = 0, const std::string& name = "") {
			static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");

			std::type_index typeIndex(typeid(T));

			// Check if already registered
			if (m_typeToPhaseIndex.find(typeIndex) != m_typeToPhaseIndex.end()) {
				return; // Already registered
			}

			std::string phaseName = name.empty() ? typeid(T).name() : name;
			m_phases.emplace_back(typeIndex, priority, phaseName);

			// Register the update function for runtime dispatch
			registerComponentUpdateFunction<T>();

			sortPhasesByPriority();
		}

		// Add dependency relationship between component types
		template<typename T, typename Dependency>
		void AddDependency() {
			static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");
			static_assert(std::is_base_of_v<Component, Dependency>, "Dependency must derive from Component");

			std::type_index typeIndex(typeid(T));
			std::type_index depIndex(typeid(Dependency));

			auto it = m_typeToPhaseIndex.find(typeIndex);
			if (it != m_typeToPhaseIndex.end()) {
				m_phases[it->second].dependencies.push_back(depIndex);
			}
		}

		// Set execution policy
		void SetExecutionPolicy(ExecutionPolicy policy) {
			m_executionPolicy = policy;
		}

		// Update all registered component types asynchronously
		void UpdateAllAsync(double deltaTime) {
			JobSystem* jobSystem = getJobSystem();
			if (!m_initialized || !m_componentManager || !jobSystem) {
				return;
			}

			switch (m_executionPolicy) {
			case ExecutionPolicy::Sequential:
				updateSequential(deltaTime);
				break;
			case ExecutionPolicy::Parallel:
				updateParallel(deltaTime);
				break;
			case ExecutionPolicy::Custom:
				updateWithDependencies(deltaTime);
				break;
			}
		}

		// Wait for all pending component updates to complete
		void WaitForCompletion() 
		{
			DECLARE_FUNC_LOW();
			JobSystem* jobSystem = getJobSystem();
			if (jobSystem) {
				jobSystem->waitForAll();
			}
		}

		// Wait for completion with timeout
		template<typename Rep, typename Period>
		bool WaitForCompletion(const std::chrono::duration<Rep, Period>& timeout) {
			JobSystem* jobSystem = getJobSystem();
			if (jobSystem) {
				return jobSystem->waitForAll(timeout);
			}
			return false; // Return false if JobSystem is not available
		}

		// Get statistics
		size_t GetRegisteredPhaseCount() const { return m_phases.size(); }
		size_t GetPendingJobCount() const {
			JobSystem* jobSystem = getJobSystem();
			return jobSystem ? jobSystem->getPendingJobCount() : 0;
		}
		size_t GetActiveJobCount() const {
			JobSystem* jobSystem = getJobSystem();
			return jobSystem ? jobSystem->getActiveJobCount() : 0;
		}

		// Get phase information
		const std::vector<ComponentPhase>& GetPhases() const { return m_phases; }
		ExecutionPolicy GetExecutionPolicy() const { return m_executionPolicy; }
	};

	// Template implementation - must be in header for proper instantiation
	template<typename T>
	std::unique_ptr<ComponentUpdateBatch> ComponentSystemScheduler::updateComponentTypeAsync(double deltaTime) {
		static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");

		JobSystem* jobSystem = getJobSystem();
		if (!m_componentManager || !jobSystem) {
			return nullptr;
		}

		auto components = m_componentManager->GetActiveComponentsOfType<T>();
		if (components.empty()) {
			return std::make_unique<ComponentUpdateBatch>(std::type_index(typeid(T)), 0);
		}

		auto batch = std::make_unique<ComponentUpdateBatch>(std::type_index(typeid(T)), components.size());

		// Submit each component as a separate job
		for (T* component : components) {
			if (component && component->IsActiveInHierarchy()) {
				auto future = jobSystem->submitJobWithResult([component, deltaTime]() {
					component->Update(deltaTime);
					});
				batch->addFuture(std::move(future));
			}
		}

		return batch;
	}

} // namespace ComponentSystem