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
} // namespace Core

namespace ComponentSystem {

	// Forward declaration
	class ComponentSystemScheduler;

	// ── Execution Policy ─────────────────────────────────────────────────────────

	/** @brief Controls how component update phases are dispatched each frame. */
	enum class ExecutionPolicy {
		Sequential,     // Execute component types one after another
		Parallel,       // Execute all component types in parallel
		Custom          // Use custom dependency graph
	};

	// ── ComponentPhase ────────────────────────────────────────────────────────────

	/** @brief Metadata describing a single component type's update phase. */
	struct ComponentPhase {
		std::type_index componentType;
		int priority;               // Lower values execute first
		std::vector<std::type_index> dependencies;  // Types that must complete before this one
		std::string name;           // Human-readable name for debugging

		ComponentPhase(std::type_index type, int prio = 0, std::string phaseName = "")
			: componentType(type), priority(prio), name(std::move(phaseName)) {}
	};

	// ── ComponentUpdateBatch ──────────────────────────────────────────────────────

	/** @brief Tracks the set of async futures for one component type's update pass. */
	class ComponentUpdateBatch {
	private:
		std::vector<std::future<void>> m_futures;
		std::type_index m_componentType;
		size_t m_componentCount;
		bool m_completed;

	public:
		ComponentUpdateBatch(std::type_index type, size_t count)
			: m_componentType(type), m_componentCount(count), m_completed(false)
		{
			m_futures.reserve(count);
		}

		/** @param future A job future to track as part of this batch. */
		void AddFuture(std::future<void>&& future)
		{
			m_futures.push_back(std::move(future));
		}

		/** @brief Blocks until all futures in this batch have completed. */
		void WaitForCompletion()
		{
			DECLARE_FUNC_LOW();
			if (!m_completed)
			{
				for (auto& future : m_futures)
				{
					if (future.valid())
					{
						future.wait();
					}
				}
				m_completed = true;
			}
		}

		bool IsCompleted() const;

		std::type_index GetComponentType() const { return m_componentType; }
		size_t GetComponentCount() const { return m_componentCount; }
		size_t GetBatchSize() const { return m_futures.size(); }
	};

	// ── ComponentSystemScheduler ──────────────────────────────────────────────────

	/** @brief Dispatches per-frame OnUpdate calls for all registered component types
	 *         via the JobSystem, respecting priority order and optional dependencies. */
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

		// Get JobSystem from CoreSystem - implementation in .cpp
		JobSystem* GetJobSystem() const;

		// Register an update function for a component type
		template<typename T>
		void RegisterComponentUpdateFunction()
		{
			static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");

			std::type_index typeIndex(typeid(T));

			m_componentUpdateRegistry[typeIndex] = [this](double deltaTime) -> std::unique_ptr<ComponentUpdateBatch>
			{
				return this->UpdateComponentTypeAsync<T>(deltaTime);
			};
		}

		// Dependency resolution - moved to .cpp
		std::vector<std::vector<std::type_index>> ResolveDependencies() const;

		// Sort phases by priority - moved to .cpp
		void SortPhasesByPriority();

		// Update components of a specific type - implementation below class definition
		template<typename T>
		std::unique_ptr<ComponentUpdateBatch> UpdateComponentTypeAsync(double deltaTime);

		// Generic version using type_index - moved to .cpp
		std::unique_ptr<ComponentUpdateBatch> UpdateComponentTypeByIndexAsync(std::type_index typeIndex, double deltaTime);

		// Helper method to check if CoreSystem is initialized - implementation in .cpp
		bool IsCoreSystemInitialized() const;

		// Private update methods - moved to .cpp
		void UpdateSequential(double deltaTime);
		void UpdateParallel(double deltaTime);
		void UpdateWithDependencies(double deltaTime);

	public:
		explicit ComponentSystemScheduler(ComponentManager* componentManager)
			: m_componentManager(componentManager)
			, m_executionPolicy(ExecutionPolicy::Sequential)
			, m_initialized(false)
		{
			assert(m_componentManager && "ComponentManager cannot be null");
		}

		~ComponentSystemScheduler() = default;

		/** @brief Validates prerequisites and marks the scheduler ready for use. */
		bool Initialize()
		{
			if (m_initialized) return true;

			if (!m_componentManager)
			{
				return false;
			}

			if (!GetJobSystem())
			{
				return false;
			}

			m_initialized = true;
			return true;
		}

		/** @brief Drains pending jobs and releases all registered phase data. */
		void Shutdown()
		{
			if (!m_initialized) return;

			JobSystem* jobSystem = GetJobSystem();
			if (jobSystem)
			{
				jobSystem->WaitForAll();
			}

			m_phases.clear();
			m_typeToPhaseIndex.clear();
			m_componentUpdateRegistry.clear();
			m_initialized = false;
		}

		/** @brief Registers a component type for async updates each frame.
		 *  @param priority Lower values execute first.
		 *  @param name     Optional human-readable name for debugging. */
		template<typename T>
		void RegisterComponentType(int priority = 0, const std::string& name = "")
		{
			static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");

			std::type_index typeIndex(typeid(T));

			if (m_typeToPhaseIndex.find(typeIndex) != m_typeToPhaseIndex.end())
			{
				return; // Already registered
			}

			std::string phaseName = name.empty() ? typeid(T).name() : name;
			m_phases.emplace_back(typeIndex, priority, phaseName);

			RegisterComponentUpdateFunction<T>();

			SortPhasesByPriority();
		}

		/** @brief Declares that T must wait for Dependency to finish before updating. */
		template<typename T, typename Dependency>
		void AddDependency()
		{
			static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");
			static_assert(std::is_base_of_v<Component, Dependency>, "Dependency must derive from Component");

			std::type_index typeIndex(typeid(T));
			std::type_index depIndex(typeid(Dependency));

			auto it = m_typeToPhaseIndex.find(typeIndex);
			if (it != m_typeToPhaseIndex.end())
			{
				m_phases[it->second].dependencies.push_back(depIndex);
			}
		}

		/** @param policy The desired dispatch strategy for this scheduler. */
		void SetExecutionPolicy(ExecutionPolicy policy)
		{
			m_executionPolicy = policy;
		}

		/** @brief Submits update jobs for all registered component types. */
		void UpdateAllAsync(double deltaTime)
		{
			JobSystem* jobSystem = GetJobSystem();
			if (!m_initialized || !m_componentManager || !jobSystem)
			{
				return;
			}

			switch (m_executionPolicy)
			{
			case ExecutionPolicy::Sequential:
				UpdateSequential(deltaTime);
				break;
			case ExecutionPolicy::Parallel:
				UpdateParallel(deltaTime);
				break;
			case ExecutionPolicy::Custom:
				UpdateWithDependencies(deltaTime);
				break;
			}
		}

		/** @brief Blocks until all pending component update jobs have completed. */
		void WaitForCompletion()
		{
			DECLARE_FUNC_LOW();
			JobSystem* jobSystem = GetJobSystem();
			if (jobSystem)
			{
				jobSystem->WaitForAll();
			}
		}

		/** @brief Waits up to the given duration for all pending jobs to complete.
		 *  @param timeout Maximum time to wait.
		 *  @param Returns false if the timeout elapsed before all jobs finished. */
		template<typename Rep, typename Period>
		bool WaitForCompletion(const std::chrono::duration<Rep, Period>& timeout)
		{
			JobSystem* jobSystem = GetJobSystem();
			if (jobSystem)
			{
				return jobSystem->WaitForAll(timeout);
			}
			return false;
		}

		// Getters
		size_t GetRegisteredPhaseCount() const { return m_phases.size(); }
		size_t GetPendingJobCount() const
		{
			JobSystem* jobSystem = GetJobSystem();
			return jobSystem ? jobSystem->GetPendingJobCount() : 0;
		}
		size_t GetActiveJobCount() const
		{
			JobSystem* jobSystem = GetJobSystem();
			return jobSystem ? jobSystem->GetActiveJobCount() : 0;
		}

		const std::vector<ComponentPhase>& GetPhases() const { return m_phases; }
		ExecutionPolicy GetExecutionPolicy() const { return m_executionPolicy; }
	};

	// ── Template implementation ───────────────────────────────────────────────────

	template<typename T>
	std::unique_ptr<ComponentUpdateBatch> ComponentSystemScheduler::UpdateComponentTypeAsync(double deltaTime)
	{
		static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");

		JobSystem* jobSystem = GetJobSystem();
		if (!m_componentManager || !jobSystem)
		{
			return nullptr;
		}

		auto components = m_componentManager->GetActiveComponentsOfType<T>();
		if (components.empty())
		{
			return std::make_unique<ComponentUpdateBatch>(std::type_index(typeid(T)), 0);
		}

		auto batch = std::make_unique<ComponentUpdateBatch>(std::type_index(typeid(T)), components.size());

		for (T* component : components)
		{
			if (component && component->IsActiveInHierarchy())
			{
				auto future = jobSystem->SubmitJobWithResult([component, deltaTime]()
				{
					component->Update(deltaTime);
				});
				batch->AddFuture(std::move(future));
			}
		}

		return batch;
	}

} // namespace ComponentSystem
