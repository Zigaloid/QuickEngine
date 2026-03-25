
#include "CoreSystem/CoreSystem.h"
#include "GLTFRenderComponent.h"
#include "LevelComponent.h"
#include "ManipulatorComponent.h"


bool RegisterRenderComponentTypes()
{
	// Get the component manager from CoreSystem
	auto* componentManager = Core::CoreSystem::GetComponentManager();
	// Register component types with the component manager
	componentManager->RegisterComponentType<Rendering::RenderComponent>(Rendering::RenderComponent::ClassName(), 100, 500);
	componentManager->RegisterComponentType<Rendering::GLTFRenderComponent>(Rendering::GLTFRenderComponent::ClassName(), 100, 500);
	componentManager->RegisterComponentType<Rendering::LevelComponent>(Rendering::LevelComponent::ClassName(), 10, 50);
	componentManager->RegisterComponentType<Rendering::ManipulatorComponent>(Rendering::ManipulatorComponent::ClassName(), 1, 1);	

	// Schedule the component types with the scheduler for async updates.
	auto* scheduler = Core::CoreSystem::GetJobSystemScheduler();
	scheduler->RegisterComponentType<Rendering::RenderComponent>(50, Rendering::RenderComponent::ClassName());
	scheduler->RegisterComponentType<Rendering::GLTFRenderComponent>(50, Rendering::GLTFRenderComponent::ClassName());
	scheduler->RegisterComponentType<Rendering::LevelComponent>(30, Rendering::LevelComponent::ClassName());
	scheduler->RegisterComponentType<Rendering::ManipulatorComponent>(20, Rendering::ManipulatorComponent::ClassName());	
	// Registration logic would go here
	return true;
}