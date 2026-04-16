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

// ── Test component types ──────────────────────────────────────────────────────

class MockComponent : public Component {
private:
	std::atomic<int>  m_updateCount     { 0 };
	std::atomic<bool> m_lastUpdateCalled{ false };
	double            m_lastDeltaTime   = 0.0;

public:
	MockComponent() = default;

	int    GetUpdateCount()       const { return m_updateCount.load(); }
	bool   WasLastUpdateCalled()  const { return m_lastUpdateCalled.load(); }
	double GetLastDeltaTime()     const { return m_lastDeltaTime; }

	void ResetCounters()
	{
		m_updateCount      = 0;
		m_lastUpdateCalled = false;
		m_lastDeltaTime    = 0.0;
	}

protected:
	void OnUpdate(double deltaTime) override
	{
		m_updateCount.fetch_add(1);
		m_lastUpdateCalled = true;
		m_lastDeltaTime    = deltaTime;
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
};

class SlowComponent : public Component {
private:
	std::atomic<int>       m_updateCount{ 0 };
	std::chrono::milliseconds m_delay   { 50 };

public:
	SlowComponent() = default;

	int GetUpdateCount() const { return m_updateCount.load(); }

	void ResetCounters()  { m_updateCount = 0; }
	void SetDelay(std::chrono::milliseconds delay) { m_delay = delay; }

protected:
	void OnUpdate(double /*deltaTime*/) override
	{
		m_updateCount.fetch_add(1);
		std::this_thread::sleep_for(m_delay);
	}
};

class FastComponent : public Component {
private:
	std::atomic<int> m_updateCount{ 0 };

public:
	FastComponent() = default;

	int GetUpdateCount() const { return m_updateCount.load(); }
	void ResetCounters()       { m_updateCount = 0; }

protected:
	void OnUpdate(double /*deltaTime*/) override
	{
		m_updateCount.fetch_add(1);
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
};

// ── Test fixture ──────────────────────────────────────────────────────────────

class ComponentSystemSchedulerTest : public ::testing::Test {
protected:
	void SetUp() override
	{
		ASSERT_TRUE(Core::CoreSystem::Initialize());
		componentManager = Core::CoreSystem::GetComponentManager();
		ASSERT_NE(componentManager, nullptr);
		scheduler = std::make_unique<ComponentSystemScheduler>(componentManager);
		ASSERT_NE(scheduler, nullptr);
	}

	void TearDown() override
	{
		if (scheduler)
		{
			scheduler->Shutdown();
			scheduler.reset();
		}
		Core::CoreSystem::Shutdown();
		componentManager = nullptr;
	}

	ComponentManager* componentManager = nullptr;
	std::unique_ptr<ComponentSystemScheduler> scheduler;
};

// ── ComponentUpdateBatch tests ────────────────────────────────────────────────

TEST(ComponentUpdateBatchTest, Construction)
{
	std::type_index testType(typeid(MockComponent));
	ComponentUpdateBatch batch(testType, 5);

	EXPECT_EQ(batch.GetComponentType(), testType);
	EXPECT_EQ(batch.GetComponentCount(), 5);
	EXPECT_EQ(batch.GetBatchSize(), 0);
	EXPECT_FALSE(batch.IsCompleted());
}

TEST(ComponentUpdateBatchTest, FutureManagement)
{
	std::type_index testType(typeid(MockComponent));
	ComponentUpdateBatch batch(testType, 2);

	std::promise<void> promise1, promise2;
	batch.AddFuture(promise1.get_future());
	batch.AddFuture(promise2.get_future());

	EXPECT_EQ(batch.GetBatchSize(), 2);
	EXPECT_FALSE(batch.IsCompleted());

	promise1.set_value();
	promise2.set_value();

	batch.WaitForCompletion();
	EXPECT_TRUE(batch.IsCompleted());
}

// ── Scheduler basic tests ─────────────────────────────────────────────────────

TEST_F(ComponentSystemSchedulerTest, Construction)
{
	EXPECT_EQ(scheduler->GetRegisteredPhaseCount(), 0);
	EXPECT_EQ(scheduler->GetExecutionPolicy(), ExecutionPolicy::Sequential);
}

TEST_F(ComponentSystemSchedulerTest, Initialization)
{
	EXPECT_TRUE(scheduler->Initialize());
	EXPECT_TRUE(scheduler->Initialize()); // Double init should succeed
}

TEST_F(ComponentSystemSchedulerTest, InitializationWithoutCoreSystem)
{
	Core::CoreSystem::Shutdown();

	ComponentSystemScheduler isolatedScheduler(componentManager);
	EXPECT_FALSE(isolatedScheduler.Initialize());

	Core::CoreSystem::Initialize();
}

TEST_F(ComponentSystemSchedulerTest, Shutdown)
{
	EXPECT_TRUE(scheduler->Initialize());
	scheduler->Shutdown();
	EXPECT_EQ(scheduler->GetRegisteredPhaseCount(), 0);
}

// ── Registration tests ────────────────────────────────────────────────────────

TEST_F(ComponentSystemSchedulerTest, RegisterComponentType)
{
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

TEST_F(ComponentSystemSchedulerTest, RegisterMultipleComponentTypes)
{
	EXPECT_TRUE(scheduler->Initialize());

	scheduler->RegisterComponentType<MockComponent>(20, "Mock");
	scheduler->RegisterComponentType<SlowComponent>(10, "Slow");
	scheduler->RegisterComponentType<FastComponent>(30, "Fast");

	EXPECT_EQ(scheduler->GetRegisteredPhaseCount(), 3);

	const auto& phases = scheduler->GetPhases();
	EXPECT_EQ(phases[0].priority, 10); // SlowComponent
	EXPECT_EQ(phases[1].priority, 20); // MockComponent
	EXPECT_EQ(phases[2].priority, 30); // FastComponent
}

TEST_F(ComponentSystemSchedulerTest, DuplicateRegistration)
{
	EXPECT_TRUE(scheduler->Initialize());

	scheduler->RegisterComponentType<MockComponent>(10, "First");
	scheduler->RegisterComponentType<MockComponent>(20, "Second"); // Should be ignored

	EXPECT_EQ(scheduler->GetRegisteredPhaseCount(), 1);

	const auto& phases = scheduler->GetPhases();
	EXPECT_EQ(phases[0].priority, 10);
	EXPECT_EQ(phases[0].name, "First");
}

// ── Dependency tests ──────────────────────────────────────────────────────────

TEST_F(ComponentSystemSchedulerTest, AddDependency)
{
	EXPECT_TRUE(scheduler->Initialize());

	scheduler->RegisterComponentType<MockComponent>(10, "Mock");
	scheduler->RegisterComponentType<SlowComponent>(20, "Slow");
	scheduler->AddDependency<SlowComponent, MockComponent>();

	const auto& phases = scheduler->GetPhases();
	EXPECT_EQ(phases.size(), 2);

	auto slowPhaseIt = std::find_if(phases.begin(), phases.end(),
		[](const ComponentPhase& phase)
		{
			return phase.componentType == std::type_index(typeid(SlowComponent));
		});

	ASSERT_NE(slowPhaseIt, phases.end());
	EXPECT_EQ(slowPhaseIt->dependencies.size(), 1);
	EXPECT_EQ(slowPhaseIt->dependencies[0], std::type_index(typeid(MockComponent)));
}

// ── Execution policy tests ────────────────────────────────────────────────────

TEST_F(ComponentSystemSchedulerTest, SetExecutionPolicy)
{
	EXPECT_EQ(scheduler->GetExecutionPolicy(), ExecutionPolicy::Sequential);

	scheduler->SetExecutionPolicy(ExecutionPolicy::Parallel);
	EXPECT_EQ(scheduler->GetExecutionPolicy(), ExecutionPolicy::Parallel);

	scheduler->SetExecutionPolicy(ExecutionPolicy::Custom);
	EXPECT_EQ(scheduler->GetExecutionPolicy(), ExecutionPolicy::Custom);
}

// ── Update tests ──────────────────────────────────────────────────────────────

TEST_F(ComponentSystemSchedulerTest, UpdateAllAsyncSequential)
{
	EXPECT_TRUE(scheduler->Initialize());

	scheduler->RegisterComponentType<MockComponent>(10, "Mock");
	scheduler->RegisterComponentType<SlowComponent>(20, "Slow");

	MockComponent* mockComp1 = componentManager->CreateComponent<MockComponent>();
	MockComponent* mockComp2 = componentManager->CreateComponent<MockComponent>();
	SlowComponent* slowComp  = componentManager->CreateComponent<SlowComponent>();

	ASSERT_NE(mockComp1, nullptr);
	ASSERT_NE(mockComp2, nullptr);
	ASSERT_NE(slowComp,  nullptr);

	mockComp1->Initialize();
	mockComp2->Initialize();
	slowComp->Initialize();

	scheduler->SetExecutionPolicy(ExecutionPolicy::Sequential);

	auto startTime = std::chrono::high_resolution_clock::now();
	scheduler->UpdateAllAsync(0.016);
	scheduler->WaitForCompletion();
	auto endTime = std::chrono::high_resolution_clock::now();

	EXPECT_EQ(mockComp1->GetUpdateCount(), 1);
	EXPECT_EQ(mockComp2->GetUpdateCount(), 1);
	EXPECT_EQ(slowComp->GetUpdateCount(),  1);

	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
	EXPECT_GT(duration.count(), 40);
}

TEST_F(ComponentSystemSchedulerTest, UpdateAllAsyncParallel)
{
	EXPECT_TRUE(scheduler->Initialize());

	scheduler->RegisterComponentType<MockComponent>(10, "Mock");
	scheduler->RegisterComponentType<SlowComponent>(20, "Slow");

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

	EXPECT_EQ(mockComp->GetUpdateCount(), 1);
	EXPECT_EQ(slowComp->GetUpdateCount(),  1);

	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
	EXPECT_LT(duration.count(), 100);
}

TEST_F(ComponentSystemSchedulerTest, UpdateWithDependencies)
{
	EXPECT_TRUE(scheduler->Initialize());

	scheduler->RegisterComponentType<FastComponent>(10, "Fast");
	scheduler->RegisterComponentType<SlowComponent>(20, "Slow");
	scheduler->AddDependency<SlowComponent, FastComponent>();

	FastComponent* fastComp = componentManager->CreateComponent<FastComponent>();
	SlowComponent* slowComp = componentManager->CreateComponent<SlowComponent>();

	ASSERT_NE(fastComp, nullptr);
	ASSERT_NE(slowComp, nullptr);

	fastComp->Initialize();
	slowComp->Initialize();

	scheduler->SetExecutionPolicy(ExecutionPolicy::Custom);
	scheduler->UpdateAllAsync(0.016);
	scheduler->WaitForCompletion();

	EXPECT_EQ(fastComp->GetUpdateCount(), 1);
	EXPECT_EQ(slowComp->GetUpdateCount(),  1);
}

TEST_F(ComponentSystemSchedulerTest, UpdateWithoutInitialization)
{
	scheduler->RegisterComponentType<MockComponent>();

	MockComponent* comp = componentManager->CreateComponent<MockComponent>();
	ASSERT_NE(comp, nullptr);
	comp->Initialize();

	scheduler->UpdateAllAsync(0.016);
	scheduler->WaitForCompletion();

	EXPECT_EQ(comp->GetUpdateCount(), 0);
}

TEST_F(ComponentSystemSchedulerTest, UpdateInactiveComponents)
{
	EXPECT_TRUE(scheduler->Initialize());
	scheduler->RegisterComponentType<MockComponent>();

	MockComponent* activeComp   = componentManager->CreateComponent<MockComponent>();
	MockComponent* inactiveComp = componentManager->CreateComponent<MockComponent>();

	ASSERT_NE(activeComp,   nullptr);
	ASSERT_NE(inactiveComp, nullptr);

	activeComp->Initialize();
	inactiveComp->Initialize();
	inactiveComp->SetActive(false);

	scheduler->UpdateAllAsync(0.016);
	scheduler->WaitForCompletion();

	EXPECT_EQ(activeComp->GetUpdateCount(),   1);
	EXPECT_EQ(inactiveComp->GetUpdateCount(), 0);
}

// ── Statistics tests ──────────────────────────────────────────────────────────

TEST_F(ComponentSystemSchedulerTest, GetStatistics)
{
	EXPECT_TRUE(scheduler->Initialize());

	scheduler->RegisterComponentType<MockComponent>();
	scheduler->RegisterComponentType<SlowComponent>();

	EXPECT_EQ(scheduler->GetRegisteredPhaseCount(), 2);
	EXPECT_GE(scheduler->GetPendingJobCount(), 0u);
	EXPECT_GE(scheduler->GetActiveJobCount(),  0u);
}

// ── Edge cases ────────────────────────────────────────────────────────────────

TEST_F(ComponentSystemSchedulerTest, NullComponentManager)
{
	EXPECT_DEATH(
	{
		ComponentSystemScheduler nullScheduler(nullptr);
	}, "ComponentManager cannot be null");
}

TEST_F(ComponentSystemSchedulerTest, EmptyUpdate)
{
	EXPECT_TRUE(scheduler->Initialize());
	scheduler->UpdateAllAsync(0.016);
	scheduler->WaitForCompletion();
	EXPECT_EQ(scheduler->GetRegisteredPhaseCount(), 0);
}

TEST_F(ComponentSystemSchedulerTest, UpdateWithNoComponents)
{
	EXPECT_TRUE(scheduler->Initialize());
	scheduler->RegisterComponentType<MockComponent>();
	scheduler->UpdateAllAsync(0.016);
	scheduler->WaitForCompletion();
	EXPECT_EQ(scheduler->GetRegisteredPhaseCount(), 1);
}

// ── Integration test ──────────────────────────────────────────────────────────

TEST_F(ComponentSystemSchedulerTest, CompleteWorkflow)
{
	EXPECT_TRUE(scheduler->Initialize());

	scheduler->RegisterComponentType<FastComponent>(10, "Fast");
	scheduler->RegisterComponentType<MockComponent>(20, "Mock");
	scheduler->RegisterComponentType<SlowComponent>(30, "Slow");

	scheduler->AddDependency<MockComponent, FastComponent>();
	scheduler->AddDependency<SlowComponent, MockComponent>();

	std::vector<FastComponent*> fastComps;
	std::vector<MockComponent*> mockComps;
	std::vector<SlowComponent*> slowComps;

	for (int i = 0; i < 3; ++i)
	{
		fastComps.push_back(componentManager->CreateComponent<FastComponent>());
		mockComps.push_back(componentManager->CreateComponent<MockComponent>());
		slowComps.push_back(componentManager->CreateComponent<SlowComponent>());

		fastComps[i]->Initialize();
		mockComps[i]->Initialize();
		slowComps[i]->Initialize();
	}

	for (auto policy : { ExecutionPolicy::Sequential, ExecutionPolicy::Parallel, ExecutionPolicy::Custom })
	{
		scheduler->SetExecutionPolicy(policy);

		for (auto* comp : fastComps) comp->ResetCounters();
		for (auto* comp : mockComps) comp->ResetCounters();
		for (auto* comp : slowComps) comp->ResetCounters();

		scheduler->UpdateAllAsync(0.016);
		scheduler->WaitForCompletion();

		for (auto* comp : fastComps) { EXPECT_EQ(comp->GetUpdateCount(), 1); }
		for (auto* comp : mockComps) { EXPECT_EQ(comp->GetUpdateCount(), 1); }
		for (auto* comp : slowComps) { EXPECT_EQ(comp->GetUpdateCount(), 1); }
	}

	scheduler->Shutdown();
	EXPECT_EQ(scheduler->GetRegisteredPhaseCount(), 0);
}
