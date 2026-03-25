#pragma once

#include "gl/glew.h"
#include "glfw/glfw3.h"
#include <vector>
#include <memory>
#include <string>
#include <functional>
#include <unordered_map>
#include "Viewport.h"

// Forward declarations
namespace ComponentSystem {
	class CameraComponent;
}

class Window {
public:
	struct WindowConfig {
		int width = 1200;
		int height = 800;
		std::string title = "Multi-Viewport 3D Window";
		bool resizable = true;
		bool vsync = false;
		int samples = 4; // MSAA samples
		bool saveWindowState = true; // Save/restore window position and size
	};

	// Camera position and orientation for caching
	struct CachedCameraTransform {
		// Position
		float posX = 0.0f;
		float posY = 5.0f;
		float posZ = 10.0f;

		// Orientation (quaternion)
		float orientW = 1.0f;
		float orientX = 0.0f;
		float orientY = 0.0f;
		float orientZ = 0.0f;
	};

	// Forward declare the nested CameraSettings struct
	struct CachedCameraSettings {
		// Camera settings
		float movementSpeed = 5.0f;
		float rotationSpeed = 2.0f;
		float sprintMultiplier = 2.5f;
		float smoothing = 0.1f;
		bool invertPitch = false;
		bool invertYaw = false;
		float mouseSensitivity = 0.005f;

		// Camera position and orientation
		CachedCameraTransform transform;
	};

	// Viewport cache entry to store viewport settings
	struct ViewportCacheEntry {
		Viewport::ViewportConfig config;
		Viewport::RenderCallback renderCallback;
		// Store camera settings using our own struct to avoid forward declaration issues
		CachedCameraSettings cameraSettings;
		bool hasCameraSettings = false; // Flag to track if camera settings are valid
	};

	// Input callback types
	using KeyCallback = std::function<void(int key, int scancode, int action, int mods)>;
	using MouseButtonCallback = std::function<void(int button, int action, int mods)>;
	using MouseMoveCallback = std::function<void(double xpos, double ypos)>;
	using MouseScrollCallback = std::function<void(double xoffset, double yoffset)>;
	using WindowResizeCallback = std::function<void(int width, int height)>;

private:
	GLFWwindow* m_window;
	WindowConfig m_config;
	std::vector<std::unique_ptr<Viewport>> m_viewports;

	// Viewport cache for storing viewport configurations by name
	std::unordered_map<std::string, ViewportCacheEntry> m_viewportCache;

	// Input state
	double m_lastMouseX, m_lastMouseY;
	bool m_firstMouse;
	Viewport* m_activeViewport;

	// Input callbacks
	KeyCallback m_keyCallback;
	MouseButtonCallback m_mouseButtonCallback;
	MouseMoveCallback m_mouseMoveCallback;
	MouseScrollCallback m_mouseScrollCallback;
	WindowResizeCallback m_windowResizeCallback;

	// Static callback functions for GLFW
	static void glfwKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
	static void glfwMouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
	static void glfwCursorPosCallback(GLFWwindow* window, double xpos, double ypos);
	static void glfwScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
	static void glfwFramebufferSizeCallback(GLFWwindow* window, int width, int height);
	static void glfwErrorCallback(int error, const char* description);

	// Internal methods
	bool initializeGL();
	void setupCallbacks();
	Viewport* getViewportAtPosition(double x, double y);
	void updateViewportSizes();

	// Cache management methods
	void cacheViewportSettings(const std::string& name, const Viewport* viewport);
	bool loadCachedViewportSettings(const std::string& name, Viewport* viewport);
	void updateViewportCameraAspectRatio(Viewport* viewport);
	void resizeViewportsProportionally(int newWidth, int newHeight);
	// Registry support methods
#ifdef _WIN32
	void saveWindowStateToRegistry();
	void loadWindowStateFromRegistry();
	std::string getRegistryKeyPath() const;
#endif

public:
	Window(const WindowConfig& config = WindowConfig());
	~Window();

	// Window management
	bool initialize();
	void shutdown();
	bool shouldClose() const;
	void swapBuffers();
	void pollEvents();

	// Window properties
	GLFWwindow* getGLFWWindow() const { return m_window; }
	int getWidth() const;
	int getHeight() const;
	void setTitle(const std::string& title);
	void setVSync(bool enabled);

	// Window position and size
	void getPosition(int& x, int& y) const;
	void setPosition(int x, int y);
	void setSize(int width, int height);

	// Registry state management
	void saveWindowState();
	void restoreWindowState();

	// Viewport management
	Viewport* createViewport(const std::string& name, int x, int y, int width, int height, bool useCache = true);
	Viewport* getViewport(const std::string& name);
	Viewport* getViewport(size_t index);
	size_t getViewportCount() const { return m_viewports.size(); }
	void removeViewport(const std::string& name);
	void removeViewport(size_t index);
	void clearViewports();

	// Viewport cache management
	void clearViewportCache();
	bool hasViewportInCache(const std::string& name) const;
	void removeViewportFromCache(const std::string& name);
	size_t getViewportCacheSize() const { return m_viewportCache.size(); }
	void cacheCurrentViewportSettings(const std::string& name);

	// Active viewport - now with camera activation/deactivation
	Viewport* getActiveViewport() const { return m_activeViewport; }
	void setActiveViewport(Viewport* viewport);

	// Layout helpers
	void setHorizontalSplit(float splitRatio = 0.5f);
	void setVerticalSplit(float splitRatio = 0.5f);
	void setQuadLayout();
	void setSingleViewport();

	// Input callback setters
	void setKeyCallback(const KeyCallback& callback) { m_keyCallback = callback; }
	void setMouseButtonCallback(const MouseButtonCallback& callback) { m_mouseButtonCallback = callback; }
	void setMouseMoveCallback(const MouseMoveCallback& callback) { m_mouseMoveCallback = callback; }
	void setMouseScrollCallback(const MouseScrollCallback& callback) { m_mouseScrollCallback = callback; }
	void setWindowResizeCallback(const WindowResizeCallback& callback) { m_windowResizeCallback = callback; }

	// Rendering
	void render();

	void ClearGLViewport();

	void clear(float r = 0.2f, float g = 0.2f, float b = 0.2f, float a = 1.0f);

	// Utility
	double getTime() const;
	void makeContextCurrent();

	// Static utility methods
	static bool initializeGLFW();
	static void terminateGLFW();
};