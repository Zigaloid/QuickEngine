#pragma once

#include "ComponentSystem\ComponentSystem.h"

// ApplicationComponent - Main application component that manages the application lifecycle
class ApplicationComponent : public ComponentSystem::Component {
public:
	REFL_DECLARE_OBJECT(ApplicationComponent, Component);

private:
	bool applicationRunning_;
	double totalRunTime_;

protected:
	// Override the component lifecycle methods
	bool OnInitialize() override;
	void OnUpdate(double deltaTime) override;
	void OnShutdown() override;

	// Override activation/deactivation methods
	void OnActivate() override;
	void OnDeactivate() override;

public:
	ApplicationComponent();
	virtual ~ApplicationComponent() = default;

	// Application-specific methods
	bool IsApplicationRunning() const { return applicationRunning_; }
	double GetTotalRunTime() const { return totalRunTime_; }

	// Application control methods
	void StartApplication();
	void StopApplication();
	void PauseApplication();
	void ResumeApplication();

	// Virtual methods that can be overridden by derived application components
	virtual bool OnApplicationStart() { return true; }
	virtual void OnApplicationStop() {}
	virtual void OnApplicationPause() {}
	virtual void OnApplicationResume() {}
};