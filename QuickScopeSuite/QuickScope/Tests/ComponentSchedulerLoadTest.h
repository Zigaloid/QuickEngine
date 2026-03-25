#pragma once

#include "ComponentSystem\ComponentSystem.h"
#include "ComponentSystem\ComponentSystemScheduler.h"

#include <chrono>
#include <thread>
#include <random>
using namespace ComponentSystem;
namespace LoadTest {

	// Base class for test components that simulate different workloads
	class TestComponentBase : public ComponentSystem::Component {
	public:
		REFL_DECLARE_OBJECT(TestComponentBase, ComponentSystem::Component);
		TestComponentBase() {}
	protected:
		float m_targetUpdateTimeMs;
		std::string m_componentName;
		std::mt19937 m_randomGenerator;
		std::uniform_real_distribution<double> m_workloadVariation;

		// Simulate CPU work for specified duration
		void SimulateWork(double durationMs);

	public:
		TestComponentBase(const std::string& name, float targetUpdateTimeMs);
		virtual ~TestComponentBase() = default;

		// Component lifecycle
		bool OnInitialize() override;
		void OnUpdate(double deltaTime) override;
		void OnShutdown() override;

		// Getters
		const std::string& GetComponentName() const { return m_componentName; }
		double GetTargetUpdateTime() const { return m_targetUpdateTimeMs; }
	};

	// Test component for AI/Logic simulation
	class AITestComponent : public TestComponentBase {
	public:
		REFL_DECLARE_OBJECT(AITestComponent, TestComponentBase);

		AITestComponent() : TestComponentBase("AIComponent", 0.5f) {}
	};

	// Test component for Physics simulation
	class PhysicsTestComponent : public TestComponentBase {
	public:
		REFL_DECLARE_OBJECT(PhysicsTestComponent, TestComponentBase);

		PhysicsTestComponent() : TestComponentBase("PhysicsComponent", 0.2f) {}
	};

	// Test component for Rendering simulation
	class RenderTestComponent : public TestComponentBase {
	public:
		REFL_DECLARE_OBJECT(RenderTestComponent, TestComponentBase);

		RenderTestComponent() : TestComponentBase("RenderComponent", 0.3f) {}
	};

	// Test component for Audio simulation
	class AudioTestComponent : public TestComponentBase {
	public:
		REFL_DECLARE_OBJECT(AudioTestComponent, TestComponentBase);

		AudioTestComponent() : TestComponentBase("AudioComponent", 0.4f) {}
	};

	// Test component for Network simulation
	class NetworkTestComponent : public TestComponentBase {
	public:
		REFL_DECLARE_OBJECT(NetworkTestComponent, TestComponentBase);

		NetworkTestComponent() : TestComponentBase("NetworkComponent", 0.6f) {}
	};

	// Load test manager class
	class ComponentSchedulerLoadTest {
	private:
		static const size_t DEFAULT_COMPONENT_COUNT = 10;
		
		ComponentSystem::ComponentManager* m_componentManager;
		ComponentSystem::ComponentSystemScheduler* m_scheduler;
		
		std::vector<TestComponentBase*> m_testComponents;
		bool m_isSetup;

		// Statistics tracking
		std::chrono::high_resolution_clock::time_point m_testStartTime;
		size_t m_updateCount;

	public:
		ComponentSchedulerLoadTest();
		~ComponentSchedulerLoadTest();

		// Setup the load test with specified number of each component type
		bool Setup(size_t aiComponents = DEFAULT_COMPONENT_COUNT,
				   size_t physicsComponents = DEFAULT_COMPONENT_COUNT,
				   size_t renderComponents = DEFAULT_COMPONENT_COUNT,
				   size_t audioComponents = DEFAULT_COMPONENT_COUNT,
				   size_t networkComponents = DEFAULT_COMPONENT_COUNT);

		// Cleanup the load test
		void Cleanup();

		// Start/stop the test
		void StartTest();
		void StopTest();

		// Statistics
		void PrintStatistics() const;
		size_t GetTotalComponentCount() const;
		double GetTestDurationSeconds() const;
		size_t GetUpdateCount() const { return m_updateCount; }

		// Check if test is set up
		bool IsSetup() const { return m_isSetup; }
	};

} // namespace LoadTest