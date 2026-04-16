#include "ComponentSystemScheduler.h"
#include "CoreSystem/CoreSystem.h"

#include <iostream>
#include <cassert>
#include <chrono>

namespace ComponentSystem {

	// ── ComponentUpdateBatch ──────────────────────────────────────────────────────

	bool ComponentUpdateBatch::IsCompleted() const
	{
		if (m_completed) return true;

		// A batch with no futures is not considered completed
		if (m_futures.empty()) return false;

		return std::all_of(m_futures.begin(), m_futures.end(),
			[](const std::future<void>& future)
			{
				return !future.valid() ||
					future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
			});
	}

	// ── ComponentSystemScheduler ──────────────────────────────────────────────────

	JobSystem* ComponentSystemScheduler::GetJobSystem() const
	{
		return Core::CoreSystem::GetJobSystem();
	}

	bool ComponentSystemScheduler::IsCoreSystemInitialized() const
	{
		return Core::CoreSystem::IsInitialized();
	}

	std::vector<std::vector<std::type_index>> ComponentSystemScheduler::ResolveDependencies() const
	{
		std::vector<std::vector<std::type_index>> batches;
		std::unordered_set<std::type_index> processed;

		while (processed.size() < m_phases.size())
		{
			std::vector<std::type_index> currentBatch;

			for (const auto& phase : m_phases)
			{
				if (processed.find(phase.componentType) != processed.end())
				{
					continue;
				}

				bool canExecute = true;
				for (const auto& dep : phase.dependencies)
				{
					if (processed.find(dep) == processed.end())
					{
						canExecute = false;
						break;
					}
				}

				if (canExecute)
				{
					currentBatch.push_back(phase.componentType);
				}
			}

			if (currentBatch.empty())
			{
				// Circular dependency detected
				assert(false && "Circular dependency detected in component phases");
				break;
			}

			for (const auto& type : currentBatch)
			{
				processed.insert(type);
			}

			batches.push_back(std::move(currentBatch));
		}

		return batches;
	}

	void ComponentSystemScheduler::SortPhasesByPriority()
	{
		std::sort(m_phases.begin(), m_phases.end(),
			[](const ComponentPhase& a, const ComponentPhase& b)
			{
				return a.priority < b.priority;
			});

		m_typeToPhaseIndex.clear();
		for (size_t i = 0; i < m_phases.size(); ++i)
		{
			m_typeToPhaseIndex[m_phases[i].componentType] = i;
		}
	}

	std::unique_ptr<ComponentUpdateBatch> ComponentSystemScheduler::UpdateComponentTypeByIndexAsync(
		std::type_index typeIndex, double deltaTime)
	{
		JobSystem* jobSystem = GetJobSystem();
		if (!m_componentManager || !jobSystem)
		{
			return std::make_unique<ComponentUpdateBatch>(typeIndex, 0);
		}

		auto updateFuncIt = m_componentUpdateRegistry.find(typeIndex);
		if (updateFuncIt == m_componentUpdateRegistry.end())
		{
			return std::make_unique<ComponentUpdateBatch>(typeIndex, 0);
		}

		return updateFuncIt->second(deltaTime);
	}

	void ComponentSystemScheduler::UpdateSequential(double deltaTime)
	{
		for (const auto& phase : m_phases)
		{
			auto batch = UpdateComponentTypeByIndexAsync(phase.componentType, deltaTime);
			if (batch)
			{
				batch->WaitForCompletion();
			}
		}
	}

	void ComponentSystemScheduler::UpdateParallel(double deltaTime)
	{
		std::vector<std::unique_ptr<ComponentUpdateBatch>> batches;
		batches.reserve(m_phases.size());

		for (const auto& phase : m_phases)
		{
			auto batch = UpdateComponentTypeByIndexAsync(phase.componentType, deltaTime);
			if (batch)
			{
				batches.push_back(std::move(batch));
			}
		}

		for (auto& batch : batches)
		{
			batch->WaitForCompletion();
		}
	}

	void ComponentSystemScheduler::UpdateWithDependencies(double deltaTime)
	{
		auto dependencyBatches = ResolveDependencies();

		for (const auto& batch : dependencyBatches)
		{
			std::vector<std::unique_ptr<ComponentUpdateBatch>> currentBatches;
			currentBatches.reserve(batch.size());

			for (const auto& typeIndex : batch)
			{
				auto updateBatch = UpdateComponentTypeByIndexAsync(typeIndex, deltaTime);
				if (updateBatch)
				{
					currentBatches.push_back(std::move(updateBatch));
				}
			}

			for (auto& updateBatch : currentBatches)
			{
				updateBatch->WaitForCompletion();
			}
		}
	}

	// ── SchedulerUtils ────────────────────────────────────────────────────────────

	namespace SchedulerUtils {

		template<typename... ComponentTypes>
		void RegisterComponentTypes(ComponentSystemScheduler* scheduler, int basePriority = 0)
		{
			static_assert(sizeof...(ComponentTypes) > 0, "Must provide at least one component type");

			int currentPriority = basePriority;
			auto registerType = [&]<typename T>()
			{
				scheduler->RegisterComponentType<T>(currentPriority++);
			};

			(registerType.template operator()<ComponentTypes>(), ...);
		}

		void PrintSchedulerStats(const ComponentSystemScheduler* scheduler)
		{
			if (!scheduler) return;

			std::cout << "ComponentSystemScheduler Statistics:\n";
			std::cout << "  Registered Phases: " << scheduler->GetRegisteredPhaseCount() << "\n";
			std::cout << "  Pending Jobs: " << scheduler->GetPendingJobCount() << "\n";
			std::cout << "  Active Jobs: " << scheduler->GetActiveJobCount() << "\n";
			std::cout << "  Execution Policy: ";

			switch (scheduler->GetExecutionPolicy())
			{
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
			for (const auto& phase : scheduler->GetPhases())
			{
				std::cout << "    - " << phase.name << " (Priority: " << phase.priority;
				if (!phase.dependencies.empty())
				{
					std::cout << ", Dependencies: " << phase.dependencies.size();
				}
				std::cout << ")\n";
			}
		}

	} // namespace SchedulerUtils

} // namespace ComponentSystem
