#include "ComponentSchedulerLoadTest.h"
#include "Profiler/Profiler.h"
#include "CoreSystem/CoreSystem.h"
#include <iostream>
#include <iomanip>

using namespace LoadTest;
using namespace ComponentSystem;

// Reflection definitions
REFL_DEFINE_OBJECT(TestComponentBase)
	REFL_DEFINE_FLOAT_MEMBER(TestComponentBase, m_targetUpdateTimeMs),
	REFL_DEFINE_STRING_MEMBER(TestComponentBase, m_componentName),
REFL_DEFINE_END

REFL_DEFINE_OBJECT(AITestComponent)
REFL_DEFINE_END

REFL_DEFINE_OBJECT(PhysicsTestComponent)
REFL_DEFINE_END

REFL_DEFINE_OBJECT(RenderTestComponent)
REFL_DEFINE_END

REFL_DEFINE_OBJECT(AudioTestComponent)
REFL_DEFINE_END

REFL_DEFINE_OBJECT(NetworkTestComponent)
REFL_DEFINE_END

// TestComponentBase implementation
TestComponentBase::TestComponentBase(const std::string& name, float targetUpdateTimeMs)
	: m_componentName(name)
	, m_targetUpdateTimeMs(targetUpdateTimeMs)
	, m_randomGenerator(std::random_device{}())
	, m_workloadVariation(0.8, 1.2) // ±20% variation in workload
{
}

bool TestComponentBase::OnInitialize()
{
	DECLARE_FUNC_VLOW();
	
	std::cout << "TestComponent [" << m_componentName << "] initialized with target update time: " 
			  << m_targetUpdateTimeMs << "ms" << std::endl;
	return true;
}

void TestComponentBase::OnUpdate(double deltaTime)
{
	DECLARE_FUNC_LOW();
	
	// Apply random variation to simulate realistic workload fluctuation
	double actualWorkTime = m_targetUpdateTimeMs * m_workloadVariation(m_randomGenerator);
	
	// Simulate the work
	SimulateWork(actualWorkTime);
}

void TestComponentBase::OnShutdown()
{
	DECLARE_FUNC_VLOW();
	
	std::cout << "TestComponent [" << m_componentName << "] shutdown" << std::endl;
}

void TestComponentBase::SimulateWork(double durationMs)
{
	DECLARE_FUNC_LOW();
	
	// Convert milliseconds to nanoseconds for high precision
	auto targetDuration = std::chrono::nanoseconds(static_cast<long long>(durationMs * 1000000));
	auto startTime = std::chrono::high_resolution_clock::now();
	
	// Busy wait to simulate actual CPU work
	// This is more realistic than sleep() for simulating computation
	volatile uint64_t counter = 0;
	while (true) {
		auto currentTime = std::chrono::high_resolution_clock::now();
		auto elapsed = currentTime - startTime;
		
		if (elapsed >= targetDuration) {
			break;
		}
		
		// Do some meaningless computation to consume CPU cycles
		counter += (counter * 31 + 17) % 1000000;
	}
}

// ComponentSchedulerLoadTest implementation
ComponentSchedulerLoadTest::ComponentSchedulerLoadTest()
	: m_componentManager(nullptr)
	, m_scheduler(nullptr)
	, m_isSetup(false)
	, m_updateCount(0)
{
}

ComponentSchedulerLoadTest::~ComponentSchedulerLoadTest()
{
	Cleanup();
}

bool ComponentSchedulerLoadTest::Setup(size_t aiComponents, size_t physicsComponents, 
									   size_t renderComponents, size_t audioComponents, 
									   size_t networkComponents)
{
	DECLARE_FUNC_VLOW();
	
	std::cout << "Setting up ComponentSchedulerLoadTest..." << std::endl;
	
	// Get managers from CoreSystem
	m_componentManager = Core::CoreSystem::GetComponentManager();
	m_scheduler = Core::CoreSystem::GetJobSystemScheduler();
	
	if (!m_componentManager || !m_scheduler) {
		std::cerr << "Error: Could not get ComponentManager or Scheduler from CoreSystem" << std::endl;
		return false;
	}
	
	// Register component types with the component manager
	m_componentManager->RegisterComponentType<AITestComponent>("AITestComponent", 
		std::max(aiComponents, size_t(1)), std::max(aiComponents * 2, size_t(10)));
	m_componentManager->RegisterComponentType<PhysicsTestComponent>("PhysicsTestComponent", 
		std::max(physicsComponents, size_t(1)), std::max(physicsComponents * 2, size_t(10)));
	m_componentManager->RegisterComponentType<RenderTestComponent>("RenderTestComponent", 
		std::max(renderComponents, size_t(1)), std::max(renderComponents * 2, size_t(10)));
	m_componentManager->RegisterComponentType<AudioTestComponent>("AudioTestComponent", 
		std::max(audioComponents, size_t(1)), std::max(audioComponents * 2, size_t(10)));
	m_componentManager->RegisterComponentType<NetworkTestComponent>("NetworkTestComponent", 
		std::max(networkComponents, size_t(1)), std::max(networkComponents * 2, size_t(10)));
	
	// Register component types with the scheduler with priorities
	// Lower numbers = higher priority
	m_scheduler->RegisterComponentType<PhysicsTestComponent>(10, "PhysicsTest");
	m_scheduler->RegisterComponentType<AITestComponent>(20, "AITest");
	m_scheduler->RegisterComponentType<AudioTestComponent>(30, "AudioTest");
	m_scheduler->RegisterComponentType<NetworkTestComponent>(40, "NetworkTest");
	m_scheduler->RegisterComponentType<RenderTestComponent>(50, "RenderTest"); // Render last
	
	// Create AI components
	for (size_t i = 0; i < aiComponents; ++i) {
		auto* component = m_componentManager->CreateComponent<AITestComponent>();
		if (component && component->Initialize()) {
			m_testComponents.push_back(component);
		}
	}
	
	// Create Physics components
	for (size_t i = 0; i < physicsComponents; ++i) {
		auto* component = m_componentManager->CreateComponent<PhysicsTestComponent>();
		if (component && component->Initialize()) {
			m_testComponents.push_back(component);
		}
	}
	
	// Create Render components
	for (size_t i = 0; i < renderComponents; ++i) {
		auto* component = m_componentManager->CreateComponent<RenderTestComponent>();
		if (component && component->Initialize()) {
			m_testComponents.push_back(component);
		}
	}
	
	// Create Audio components
	for (size_t i = 0; i < audioComponents; ++i) {
		auto* component = m_componentManager->CreateComponent<AudioTestComponent>();
		if (component && component->Initialize()) {
			m_testComponents.push_back(component);
		}
	}
	
	// Create Network components
	for (size_t i = 0; i < networkComponents; ++i) {
		auto* component = m_componentManager->CreateComponent<NetworkTestComponent>();
		if (component && component->Initialize()) {
			m_testComponents.push_back(component);
		}
	}
	
	std::cout << "Load test setup complete:" << std::endl;
	std::cout << "  AI Components: " << aiComponents << std::endl;
	std::cout << "  Physics Components: " << physicsComponents << std::endl;
	std::cout << "  Render Components: " << renderComponents << std::endl;
	std::cout << "  Audio Components: " << audioComponents << std::endl;
	std::cout << "  Network Components: " << networkComponents << std::endl;
	std::cout << "  Total Components: " << m_testComponents.size() << std::endl;
	
	m_isSetup = true;
	return true;
}

void ComponentSchedulerLoadTest::Cleanup()
{
	DECLARE_FUNC_VLOW();
	
	if (!m_isSetup) {
		return;
	}
	
	std::cout << "Cleaning up ComponentSchedulerLoadTest..." << std::endl;
	
	// Check if ComponentManager is still valid before releasing components
	if (m_componentManager && m_componentManager->IsInitialized()) {
		for (auto* component : m_testComponents) {
			if (component) {
				m_componentManager->ReleaseComponent(component);
			}
		}
	}
	
	m_testComponents.clear();
	m_isSetup = false;
	m_updateCount = 0;
	
	std::cout << "Load test cleanup complete." << std::endl;
}

void ComponentSchedulerLoadTest::StartTest()
{
	DECLARE_FUNC_VLOW();
	
	if (!m_isSetup) {
		std::cerr << "Error: Load test not set up. Call Setup() first." << std::endl;
		return;
	}
	
	std::cout << "Starting load test with " << m_testComponents.size() << " components..." << std::endl;
	m_testStartTime = std::chrono::high_resolution_clock::now();
	m_updateCount = 0;
}

void ComponentSchedulerLoadTest::StopTest()
{
	DECLARE_FUNC_VLOW();
	
	if (!m_isSetup) {
		return;
	}
	
	std::cout << "Stopping load test..." << std::endl;
	PrintStatistics();
}

void ComponentSchedulerLoadTest::PrintStatistics() const
{
	DECLARE_FUNC_VLOW();
	
	if (!m_isSetup) {
		std::cout << "Load test not set up." << std::endl;
		return;
	}
	
	double testDuration = GetTestDurationSeconds();
	double avgUpdatesPerSecond = (testDuration > 0.0) ? (m_updateCount / testDuration) : 0.0;
	
	std::cout << std::endl;
	std::cout << "=== ComponentSchedulerLoadTest Statistics ===" << std::endl;
	std::cout << std::fixed << std::setprecision(3);
	std::cout << "Total Components: " << m_testComponents.size() << std::endl;
	std::cout << "Test Duration: " << testDuration << " seconds" << std::endl;
	std::cout << "Update Count: " << m_updateCount << std::endl;
	std::cout << "Average Updates/sec: " << avgUpdatesPerSecond << std::endl;
	
	// Component type breakdown
	size_t aiCount = 0, physicsCount = 0, renderCount = 0, audioCount = 0, networkCount = 0;
	for (const auto* component : m_testComponents) {
		if (dynamic_cast<const AITestComponent*>(component)) aiCount++;
		else if (dynamic_cast<const PhysicsTestComponent*>(component)) physicsCount++;
		else if (dynamic_cast<const RenderTestComponent*>(component)) renderCount++;
		else if (dynamic_cast<const AudioTestComponent*>(component)) audioCount++;
		else if (dynamic_cast<const NetworkTestComponent*>(component)) networkCount++;
	}
	
	std::cout << std::endl << "Component Type Breakdown:" << std::endl;
	std::cout << "  AI: " << aiCount << std::endl;
	std::cout << "  Physics: " << physicsCount << std::endl;
	std::cout << "  Render: " << renderCount << std::endl;
	std::cout << "  Audio: " << audioCount << std::endl;
	std::cout << "  Network: " << networkCount << std::endl;
	
	double estimatedTotalMs = (aiCount * 0.5) + (physicsCount * 1.2) + 
							  (renderCount * 1.8) + (audioCount * 0.3) + (networkCount * 0.8);
	std::cout << "Estimated Total Update Time: " << estimatedTotalMs << "ms per frame" << std::endl;
	std::cout << "=============================================" << std::endl;
}

size_t ComponentSchedulerLoadTest::GetTotalComponentCount() const
{
	return m_testComponents.size();
}

double ComponentSchedulerLoadTest::GetTestDurationSeconds() const
{
	if (!m_isSetup) {
		return 0.0;
	}
	
	auto currentTime = std::chrono::high_resolution_clock::now();
	auto duration = currentTime - m_testStartTime;
	return std::chrono::duration<double>(duration).count();
}