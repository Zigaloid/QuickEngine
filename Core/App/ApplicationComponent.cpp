
#include "ApplicationComponent.h"
#include "../Profiler/Profiler.h"
#include "../Profiler/ProfilerAnalyzer.h"
#include <iostream>

REFL_DEFINE_OBJECT(ApplicationComponent)
REFL_DEFINE_END

ApplicationComponent::ApplicationComponent()
	: applicationRunning_(false), totalRunTime_(0.0) {
}

bool ApplicationComponent::OnInitialize() {
	DECLARE_FUNC_VLOW(); 
	
	std::cout << "ApplicationComponent: Initializing application component..." << std::endl;
		
	// Initialize application-specific resources here
	applicationRunning_ = true;
	totalRunTime_ = 0.0;

	// Call the virtual method for derived classes to override
	if (!OnApplicationStart()) {
		std::cerr << "ApplicationComponent: Failed to start application!" << std::endl;
		applicationRunning_ = false;
		return false;
	}

	std::cout << "ApplicationComponent: Application component initialized successfully." << std::endl;
	return true;
}

void ApplicationComponent::OnUpdate(double deltaTime) {
	DECLARE_FUNC_LOW();
	
	if (!applicationRunning_) {
		return;
	}
	
	// Update total runtime
	totalRunTime_ += deltaTime;	
}

void ApplicationComponent::OnShutdown() {		
	std::cout << "ApplicationComponent: Shutting down application component..." << std::endl;
		
	if (applicationRunning_) {
		StopApplication();
	}


	std::cout << "ApplicationComponent: Application component shutdown complete." << std::endl;
}

void ApplicationComponent::OnActivate() {
	DECLARE_FUNC_VLOW();
	
	std::cout << "ApplicationComponent: Application activated." << std::endl;
	if (!applicationRunning_) {
		StartApplication();
	}
}

void ApplicationComponent::OnDeactivate() {
	DECLARE_FUNC_VLOW();
	
	std::cout << "ApplicationComponent: Application deactivated." << std::endl;
	PauseApplication();
}

void ApplicationComponent::StartApplication() {
	DECLARE_FUNC_VLOW();
	
	if (!applicationRunning_) {
		std::cout << "ApplicationComponent: Starting application..." << std::endl;
		applicationRunning_ = true;
		OnApplicationStart();
	}
}

void ApplicationComponent::StopApplication() {	
	DECLARE_FUNC_VLOW();
	if (applicationRunning_) {
		std::cout << "ApplicationComponent: Stopping application..." << std::endl;
		applicationRunning_ = false;
		OnApplicationStop();
	}
}

void ApplicationComponent::PauseApplication() {
	DECLARE_FUNC_VLOW();
	
	if (applicationRunning_) {
		std::cout << "ApplicationComponent: Pausing application..." << std::endl;
		// Note: We don't set applicationRunning_ to false for pause,
		// just call the pause handler
		OnApplicationPause();
	}
}

void ApplicationComponent::ResumeApplication() {
	DECLARE_FUNC_VLOW();
	
	if (applicationRunning_) {
		std::cout << "ApplicationComponent: Resuming application..." << std::endl;
		OnApplicationResume();
	}
}
