#pragma once

/** @brief Interface for the top-level application object managed by the engine loop. */
class IApplication
{
public:
	/** @brief Virtual destructor. */
	virtual ~IApplication() = default;

	/** @param Returns true on successful initialization. */
	virtual bool Initialize() = 0;
	/** @param deltaTime Elapsed time in seconds since last frame. */
	virtual void Update(double deltaTime) = 0;
	/** @param deltaTime Elapsed time in seconds since last frame. */
	virtual void Render(double deltaTime) = 0;
	virtual void ImguiUpdate() = 0;
	virtual void ImguiMainMenu() = 0;
	/** @param Returns true on successful shutdown. */
	virtual bool Shutdown() = 0;
};
