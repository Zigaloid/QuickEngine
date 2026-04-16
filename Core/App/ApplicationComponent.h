#pragma once

#include "ComponentSystem\ComponentSystem.h"

/** @brief Main application component that manages the application lifecycle. */
class ApplicationComponent : public ComponentSystem::Component {
public:
	REFL_DECLARE_OBJECT(ApplicationComponent, Component);

private:
	bool m_applicationRunning = false;
	double m_totalRunTime = 0.0;

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
	/** @param Returns whether the application is currently running. */
	bool IsApplicationRunning() const { return m_applicationRunning; }
	/** @param Returns total elapsed run time in seconds. */
	double GetTotalRunTime() const { return m_totalRunTime; }

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
