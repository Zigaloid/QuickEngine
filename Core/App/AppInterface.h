#pragma once

class IApplication
{
public:
	// Override the component lifecycle methods
	virtual bool Initialize() = 0;
	virtual void Update(double deltaTime) = 0;
	virtual void Render(double deltaTime) = 0;
	virtual void ImguiUpdate() = 0;
	virtual void ImguiMainMenu() = 0;
	virtual bool Shutdown() = 0;
};