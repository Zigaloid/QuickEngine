#include <gtest/gtest.h>
#include "CoreSystem/CoreSystem.h"
#include "ComponentSystem/ComponentSystemScheduler.h"
#include "ComponentSystem/ComponentSystem.h"
#include <memory>
#include <thread>
#include <chrono>
#include <atomic>

using namespace ComponentSystem;
using namespace Core;

// Test component classes for testing the scheduler
class MockComponent : public Component {
private:
	std::atomic<int> updateCount_;
	std::atomic<bool> lastUpdateCalled_;
	double lastDeltaTime_;

public:
	MockComponent() : updateCount_(0), lastUpdateCalled_(false), lastDeltaTime_(0.0) {}

	int GetUpdateCount() const { return updateCount_.load(); }
	bool WasLastUpdateCalled() const { return lastUpdateCalled_.load(); }
	double GetLastDeltaTime() const { return lastDeltaTime_; }

	void ResetCounters() {
		updateCount_ = 0;
		lastUpdateCalled_ = false;
		lastDeltaTime_ = 0.0;
	}

protected:
	void OnUpdate(double deltaTime) override {
		updateCount_.fetch_add(1);
		lastUpdateCalled_ = true;
		lastDeltaTime_ = deltaTime;

		// Simulate some work
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
};

class SlowComponent : public Component {
private:
	std::atomic<int> updateCount_;
	std::chrono::milliseconds delay_;

public:
	SlowComponent() : updateCount_(0), delay_(std::chrono::milliseconds(50)) {}

	int GetUpdateCount() const { return updateCount_.load(); }

	void ResetCounters() {
		updateCount_ = 0;
	}

	void SetDelay(std::chrono::milliseconds delay) {
		delay_ = delay;
	}

protected:
	void OnUpdate(double deltaTime) override {
		updateCount_.fetch_add(1);
		std::this_thread::sleep_for(delay_);
	}
};

class FastComponent : public Component {
private:
	std::atomic<int> updateCount_;

public:
	FastComponent() : updateCount_(0) {}

	int GetUpdateCount() const { return updateCount_.load(); }

	void ResetCounters() {
		updateCount_ = 0;
	}

	protected:
	void OnUpdate(double deltaTime) override {
		updateCount_.fetch_add(1);
		// Minimal work
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
};

// ComponentSystemScheduler Test Fixture
class ComponentSystemSchedulerTest : public ::testing::Test {
protected:
	void SetUp() override {
		// Initialize CoreSystem which sets up JobSystem and ComponentManager
		ASSERT_TRUE(Core::CoreSystem::Initialize());

		componentManager = Core::CoreSystem::GetComponentManager();
		ASSERT_NE(componentManager, nullptr);

		scheduler = std::make_unique<ComponentSystemScheduler>(componentManager);
		ASSERT_NE(scheduler, nullptr);
	}

	void TearDown() override {
		if (scheduler) {
			scheduler->Shutdown();
			scheduler.reset();
		}

		Core::CoreSystem::Shutdown();
		componentManager = nullptr;
	}

	ComponentManager* componentManager;
	std::unique_ptr<ComponentSystemScheduler> scheduler;
};

// ComponentUpdateBatch Tests
TEST(ComponentUpdateBatchTest, Construction) {
	std::type_index testType(typeid(MockComponent));
	ComponentUpdateBatch batch(testType, 5);

	EXPECT_EQ(batch.getComponentType(), testType);
	EXPECT_EQ(batch.getComponentCount(), 5);
	EXPECT_EQ(batch.getBatchSize(), 0); // No futures added yet
	EXPECT_FALSE(batch.isCompleted()); // No futures means not completed yet
}

TEST(ComponentUpdateBatchTest, FutureManagement) {
	std::type_index testType(typeid(MockComponent));
	ComponentUpdateBatch batch(testType, 2);

	// Add some futures
	std::promise<void> promise1, promise2;
	batch.addFuture(promise1.get_future());
	batch.addFuture(promise2.get_future());

	EXPECT_EQ(batch.getBatchSize(), 2);
	EXPECT_FALSE(batch.isCompleted());

	// Complete the promises
	promise1.set_value();
	promise2.set_value();

	// Wait for completion
	batch.waitForCompletion();
	EXPECT_TRUE(batch.isCompleted());
}

// ComponentSystemScheduler Basic Tests
TEST_F(ComponentSystemSchedulerTest, Construction) {
	EXPECT_EQ(scheduler->GetRegisteredPhaseCount(), 0);
	EXPECT_EQ(scheduler->GetExecutionPolicy(), ExecutionPolicy::Sequential);
}

TEST_F(ComponentSystemSchedulerTest, Initialization) {
	EXPECT_TRUE(scheduler->Initialize());

	// Double initialization should return true
	EXPECT_TRUE(scheduler->Initialize());
}

TEST_F(ComponentSystemSchedulerTest, InitializationWithoutCoreSystem) {
	Core::CoreSystem::Shutdown();

	ComponentSystemScheduler isolatedScheduler(componentManager);
	EXPECT_FALSE(isolatedScheduler.Initialize()); // Should fail without CoreSystem

	// Reinitialize CoreSystem for cleanup
	Core::CoreSystem::Initialize();
}

TEST_F(ComponentSystemSchedulerTest, Shutdown) {
	EXPECT_TRUE(scheduler->Initialize());

	scheduler->Shutdown();
	EXPECT_EQ(scheduler->GetRegisteredPhaseCount(), 0);
}

// Component Registration Tests
TEST_F(ComponentSystemSchedulerTest, RegisterComponentType) {
	EXPECT_TRUE(scheduler->Initialize());

	scheduler->RegisterComponentType<MockComponent>(10, "MockComponent");

	EXPECT_EQ(scheduler->GetRegisteredPhaseCount(), 1);

	const auto& phases = scheduler->GetPhases();
	EXPECT_EQ(phases.size(), 1);
	EXPECT_EQ(phases[0].componentType, std::type_index(typeid(MockComponent)));
	EXPECT_EQ(phases[0].priority, 10);
	EXPECT_EQ(phases[0].name, "MockComponent");
	EXPECT_TRUE(phases[0].dependencies.empty());
}

TEST_F(ComponentSystemSchedulerTest, RegisterMultipleComponentTypes) {
	EXPECT_TRUE(scheduler->Initialize());

	scheduler->RegisterComponentType<MockComponent>(20, "Mock");
	scheduler->RegisterComponentType<SlowComponent>(10, "Slow");
	scheduler->RegisterComponentType<FastComponent>(30, "Fast");

	EXPECT_EQ(scheduler->GetRegisteredPhaseCount(), 3);

	const auto& phases = scheduler->GetPhases();
	EXPECT_EQ(phases.size(), 3);

	// Should be sorted by priority (lower first)
	EXPECT_EQ(phases[0].priority, 10); // SlowComponent
	EXPECT_EQ(phases[1].priority, 20); // MockComponent
	EXPECT_EQ(phases[2].priority, 30); // FastComponent
}

TEST_F(ComponentSystemSchedulerTest, DuplicateRegistration) {
	EXPECT_TRUE(scheduler->Initialize());

	scheduler->RegisterComponentType<MockComponent>(10, "First");
	scheduler->RegisterComponentType<MockComponent>(20, "Second"); // Should be ignored

	EXPECT_EQ(scheduler->GetRegisteredPhaseCount(), 1);

	const auto& phases = scheduler->GetPhases();
	EXPECT_EQ(phases[0].priority, 10);
	EXPECT_EQ(phases[0].name, "First");
}

// Dependency Tests
TEST_F(ComponentSystemSchedulerTest, AddDependency) {
	EXPECT_TRUE(scheduler->Initialize());

	scheduler->RegisterComponentType<MockComponent>(10, "Mock");
	scheduler->RegisterComponentType<SlowComponent>(20, "Slow");

	scheduler->AddDependency<SlowComponent, MockComponent>();

	const auto& phases = scheduler->GetPhases();
	EXPECT_EQ(phases.size(), 2);

	// Find SlowComponent phase
	auto slowPhaseIt = std::find_if(phases.begin(), phases.end(),
		[](const ComponentPhase& phase) {
			return phase.componentType == std::type_index(typeid(SlowComponent));
		});

	ASSERT_NE(slowPhaseIt, phases.end());
	EXPECT_EQ(slowPhaseIt->dependencies.size(), 1);
	EXPECT_EQ(slowPhaseIt->dependencies[0], std::type_index(typeid(MockComponent)));
}

// Execution Policy Tests
TEST_F(ComponentSystemSchedulerTest, SetExecutionPolicy) {
	EXPECT_EQ(scheduler->GetExecutionPolicy(), ExecutionPolicy::Sequential);

	scheduler->SetExecutionPolicy(ExecutionPolicy::Parallel);
	EXPECT_EQ(scheduler->GetExecutionPolicy(), ExecutionPolicy::Parallel);

	scheduler->SetExecutionPolicy(ExecutionPolicy::Custom);
	EXPECT_EQ(scheduler->GetExecutionPolicy(), ExecutionPolicy::Custom);
}

// Update Tests
TEST_F(ComponentSystemSchedulerTest, UpdateAllAsyncSequential) {
	EXPECT_TRUE(scheduler->Initialize());

	// Register component types
	scheduler->RegisterComponentType<MockComponent>(10, "Mock");
	scheduler->RegisterComponentType<SlowComponent>(20, "Slow");

	// Create some components
	MockComponent* mockComp1 = componentManager->CreateComponent<MockComponent>();
	MockComponent* mockComp2 = componentManager->CreateComponent<MockComponent>();
	SlowComponent* slowComp = componentManager->CreateComponent<SlowComponent>();

	ASSERT_NE(mockComp1, nullptr);
	ASSERT_NE(mockComp2, nullptr);
	ASSERT_NE(slowComp, nullptr);

	// Initialize components
	mockComp1->Initialize();
	mockComp2->Initialize();
	slowComp->Initialize();

	scheduler->SetExecutionPolicy(ExecutionPolicy::Sequential);

	auto startTime = std::chrono::high_resolution_clock::now();
	scheduler->UpdateAllAsync(0.016);
	scheduler->WaitForCompletion();
	auto endTime = std::chrono::high_resolution_clock::now();

	// Verify components were updated
	EXPECT_EQ(mockComp1->GetUpdateCount(), 1);
	EXPECT_EQ(mockComp2->GetUpdateCount(), 1);
	EXPECT_EQ(slowComp->GetUpdateCount(), 1);

	// Sequential should take longer (though this is timing-dependent)
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
	EXPECT_GT(duration.count(), 40); // At least 50ms from SlowComponent
}

TEST_F(ComponentSystemSchedulerTest, UpdateAllAsyncParallel) {
	EXPECT_TRUE(scheduler->Initialize());

	scheduler->RegisterComponentType<MockComponent>(10, "Mock");
	scheduler->RegisterComponentType<SlowComponent>(20, "Slow");

	// Create components
	MockComponent* mockComp = componentManager->CreateComponent<MockComponent>();
	SlowComponent* slowComp = componentManager->CreateComponent<SlowComponent>();

	ASSERT_NE(mockComp, nullptr);
	ASSERT_NE(slowComp, nullptr);

	mockComp->Initialize();
	slowComp->Initialize();

	scheduler->SetExecutionPolicy(ExecutionPolicy::Parallel);

	auto startTime = std::chrono::high_resolution_clock::now();
	scheduler->UpdateAllAsync(0.016);
	scheduler->WaitForCompletion();
	auto endTime = std::chrono::high_resolution_clock::now();

	// Verify components were updated
	EXPECT_EQ(mockComp->GetUpdateCount(), 1);
	EXPECT_EQ(slowComp->GetUpdateCount(), 1);

	// Parallel should be faster than sequential for this case
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
	EXPECT_LT(duration.count(), 100); // Should be much faster than sequential
}

TEST_F(ComponentSystemSchedulerTest, UpdateWithDependencies) {
	EXPECT_TRUE(scheduler->Initialize());

	scheduler->RegisterComponentType<FastComponent>(10, "Fast");
	scheduler->RegisterComponentType<SlowComponent>(20, "Slow");

	// Add dependency: SlowComponent depends on FastComponent
	scheduler->AddDependency<SlowComponent, FastComponent>();

	// Create components
	FastComponent* fastComp = componentManager->CreateComponent<FastComponent>();
	SlowComponent* slowComp = componentManager->CreateComponent<SlowComponent>();

	ASSERT_NE(fastComp, nullptr);
	ASSERT_NE(slowComp, nullptr);

	fastComp->Initialize();
	slowComp->Initialize();

	scheduler->SetExecutionPolicy(ExecutionPolicy::Custom);

	scheduler->UpdateAllAsync(0.016);
	scheduler->WaitForCompletion();

	// Both should be updated
	EXPECT_EQ(fastComp->GetUpdateCount(), 1);
	EXPECT_EQ(slowComp->GetUpdateCount(), 1);
}

TEST_F(ComponentSystemSchedulerTest, UpdateWithoutInitialization) {
	// Don't initialize scheduler
	scheduler->RegisterComponentType<MockComponent>();

	MockComponent* comp = componentManager->CreateComponent<MockComponent>();
	ASSERT_NE(comp, nullptr);
	comp->Initialize();

	// Should handle gracefully
	scheduler->UpdateAllAsync(0.016);
	scheduler->WaitForCompletion();

	// Component should not be updated
	EXPECT_EQ(comp->GetUpdateCount(), 0);
}

TEST_F(ComponentSystemSchedulerTest, UpdateInactiveComponents) {
	EXPECT_TRUE(scheduler->Initialize());

	scheduler->RegisterComponentType<MockComponent>();

	MockComponent* activeComp = componentManager->CreateComponent<MockComponent>();
	MockComponent* inactiveComp = componentManager->CreateComponent<MockComponent>();

	ASSERT_NE(activeComp, nullptr);
	ASSERT_NE(inactiveComp, nullptr);

	activeComp->Initialize();
	inactiveComp->Initialize();
	inactiveComp->SetActive(false); // Deactivate

	scheduler->UpdateAllAsync(0.016);
	scheduler->WaitForCompletion();

	// Only active component should be updated
	EXPECT_EQ(activeComp->GetUpdateCount(), 1);
	EXPECT_EQ(inactiveComp->GetUpdateCount(), 0);
}

// Statistics Tests
TEST_F(ComponentSystemSchedulerTest, GetStatistics) {
	EXPECT_TRUE(scheduler->Initialize());

	scheduler->RegisterComponentType<MockComponent>();
	scheduler->RegisterComponentType<SlowComponent>();

	EXPECT_EQ(scheduler->GetRegisteredPhaseCount(), 2);
	EXPECT_GE(scheduler->GetPendingJobCount(), 0);
	EXPECT_GE(scheduler->GetActiveJobCount(), 0);
}

// Edge Cases and Error Handling
TEST_F(ComponentSystemSchedulerTest, NullComponentManager) {
	EXPECT_DEATH({
		ComponentSystemScheduler nullScheduler(nullptr);
		}, "ComponentManager cannot be null");
}

TEST_F(ComponentSystemSchedulerTest, EmptyUpdate) {
	EXPECT_TRUE(scheduler->Initialize());

	// No registered component types
	scheduler->UpdateAllAsync(0.016);
	scheduler->WaitForCompletion();

	// Should complete without issues
	EXPECT_EQ(scheduler->GetRegisteredPhaseCount(), 0);
}

TEST_F(ComponentSystemSchedulerTest, UpdateWithNoComponents) {
	EXPECT_TRUE(scheduler->Initialize());

	scheduler->RegisterComponentType<MockComponent>();

	// No components created
	scheduler->UpdateAllAsync(0.016);
	scheduler->WaitForCompletion();

	// Should complete without issues
	EXPECT_EQ(scheduler->GetRegisteredPhaseCount(), 1);
}

// Integration Test
TEST_F(ComponentSystemSchedulerTest, CompleteWorkflow) {
	EXPECT_TRUE(scheduler->Initialize());

	// Register types with priorities
	scheduler->RegisterComponentType<FastComponent>(10, "Fast");
	scheduler->RegisterComponentType<MockComponent>(20, "Mock");
	scheduler->RegisterComponentType<SlowComponent>(30, "Slow");

	// Add dependencies
	scheduler->AddDependency<MockComponent, FastComponent>();
	scheduler->AddDependency<SlowComponent, MockComponent>();

	// Create components
	std::vector<FastComponent*> fastComps;
	std::vector<MockComponent*> mockComps;
	std::vector<SlowComponent*> slowComps;

	for (int i = 0; i < 3; ++i) {
		fastComps.push_back(componentManager->CreateComponent<FastComponent>());
		mockComps.push_back(componentManager->CreateComponent<MockComponent>());
		slowComps.push_back(componentManager->CreateComponent<SlowComponent>());

		fastComps[i]->Initialize();
		mockComps[i]->Initialize();
		slowComps[i]->Initialize();
	}

	// Test all execution policies
	for (auto policy : { ExecutionPolicy::Sequential, ExecutionPolicy::Parallel, ExecutionPolicy::Custom }) {
		scheduler->SetExecutionPolicy(policy);

		// Reset counters
		for (auto* comp : fastComps) comp->ResetCounters();
		for (auto* comp : mockComps) comp->ResetCounters();
		for (auto* comp : slowComps) comp->ResetCounters();

		scheduler->UpdateAllAsync(0.016);
		scheduler->WaitForCompletion();

		// Verify all components were updated
		for (auto* comp : fastComps) {
			EXPECT_EQ(comp->GetUpdateCount(), 1);
		}
		for (auto* comp : mockComps) {
			EXPECT_EQ(comp->GetUpdateCount(), 1);
		}
		for (auto* comp : slowComps) {
			EXPECT_EQ(comp->GetUpdateCount(), 1);
		}
	}

	scheduler->Shutdown();
	// Note: Cannot check IsInitialized() as the method doesn't exist
	// We can only verify that Shutdown() clears the registered phases
	EXPECT_EQ(scheduler->GetRegisteredPhaseCount(), 0);
}