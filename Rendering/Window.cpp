#define NOMINMAX
#include "Window.h"
#include "math/Vector3f.h"
#include "math/Quaternion.h"
#include <iostream>
#include <algorithm>
#include <windows.h>
#include <sstream>
#include "Profiler/Profiler.h"
#include "CameraComponent.h"
#include "RenderingSystem.h"

// Static GLFW callback implementations
void Window::glfwErrorCallback(int error, const char* description) {
	std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

void Window::glfwKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	Window* windowInstance = static_cast<Window*>(glfwGetWindowUserPointer(window));
	if (windowInstance) {
		// Handle built-in keys
		if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
			glfwSetWindowShouldClose(window, GLFW_TRUE);
			return;
		}

		// Forward keyboard input to active viewport's camera
		if (windowInstance->m_activeViewport) {
			windowInstance->m_activeViewport->handleKeyboard(key, scancode, action, mods);
		}

		// Call user callback
		if (windowInstance->m_keyCallback) {
			windowInstance->m_keyCallback(key, scancode, action, mods);
		}
	}
}

void Window::glfwMouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
	Window* windowInstance = static_cast<Window*>(glfwGetWindowUserPointer(window));
	if (windowInstance) {
		double xpos, ypos;
		glfwGetCursorPos(window, &xpos, &ypos);

		// Find which viewport was clicked and set it as active
		if (action == GLFW_PRESS) {
			Viewport* clickedViewport = windowInstance->getViewportAtPosition(xpos, ypos);
			if (clickedViewport) {
				windowInstance->setActiveViewport(clickedViewport);
			}
		}		

		// Call user callback
		if (windowInstance->m_mouseButtonCallback) {
			windowInstance->m_mouseButtonCallback(button, action, mods);
		}
	}
}

void Window::glfwCursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
	Window* windowInstance = static_cast<Window*>(glfwGetWindowUserPointer(window));
	if (!windowInstance) {
		return; // Safety check
	}
	
	if (windowInstance->m_firstMouse) {
		windowInstance->m_lastMouseX = xpos;
		windowInstance->m_lastMouseY = ypos;
		windowInstance->m_firstMouse = false;
	}

	// Call user callback - with improved null check
	if (windowInstance->m_mouseMoveCallback) {
		windowInstance->m_mouseMoveCallback(xpos, ypos);
	}

	windowInstance->m_lastMouseX = xpos;
	windowInstance->m_lastMouseY = ypos;
}

void Window::glfwScrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
	Window* windowInstance = static_cast<Window*>(glfwGetWindowUserPointer(window));
	if (windowInstance) {

		// Call user callback
		if (windowInstance->m_mouseScrollCallback) {
			windowInstance->m_mouseScrollCallback(xoffset, yoffset);
		}
	}
}

void Window::glfwFramebufferSizeCallback(GLFWwindow* window, int width, int height) {
	Window* windowInstance = static_cast<Window*>(glfwGetWindowUserPointer(window));
	if (windowInstance) {
		windowInstance->m_config.width = width;
		windowInstance->m_config.height = height;
		windowInstance->updateViewportSizes();

		// Call user callback
		if (windowInstance->m_windowResizeCallback) {
			windowInstance->m_windowResizeCallback(width, height);
		}
	}
}

// Constructor and Destructor
Window::Window(const WindowConfig& config)
	: m_window(nullptr)
	, m_config(config)
	, m_lastMouseX(0.0)
	, m_lastMouseY(0.0)
	, m_firstMouse(true)
	, m_activeViewport(nullptr)
	, m_keyCallback(nullptr)           // Initialize callback pointers to null
	, m_mouseButtonCallback(nullptr)   // Initialize callback pointers to null
	, m_mouseMoveCallback(nullptr)     // Initialize callback pointers to null
	, m_mouseScrollCallback(nullptr)   // Initialize callback pointers to null
	, m_windowResizeCallback(nullptr)  // Initialize callback pointers to null
{
}

Window::~Window() {
	shutdown();
}

#ifdef _WIN32
// Registry support methods
std::string Window::getRegistryKeyPath() const {
	return "SOFTWARE\\Multi-Viewport3DWindow";
}

bool writeRegistryValue(HKEY hKey, const std::string& valueName, DWORD value) {
	LSTATUS result = RegSetValueExA(hKey, valueName.c_str(), 0, REG_DWORD, 
		reinterpret_cast<const BYTE*>(&value), sizeof(DWORD));
	return result == ERROR_SUCCESS;
}

bool readRegistryValue(HKEY hKey, const std::string& valueName, DWORD& value) {
	DWORD dataSize = sizeof(DWORD);
	DWORD dataType = REG_DWORD;
	LSTATUS result = RegQueryValueExA(hKey, valueName.c_str(), nullptr, &dataType, 
		reinterpret_cast<BYTE*>(&value), &dataSize);
	return result == ERROR_SUCCESS && dataType == REG_DWORD;
}

void Window::saveWindowStateToRegistry() {
	if (!m_config.saveWindowState || !m_window) {
		return;
	}

	HKEY hKey;
	std::string keyPath = getRegistryKeyPath();
	
	// Create or open the registry key
	LSTATUS result = RegCreateKeyExA(HKEY_CURRENT_USER, keyPath.c_str(), 0, nullptr,
		REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr);
	
	if (result != ERROR_SUCCESS) {
		std::cerr << "Failed to create/open registry key: " << keyPath << std::endl;
		return;
	}

	// Get current window position and size
	int x, y, width, height;
	glfwGetWindowPos(m_window, &x, &y);
	glfwGetWindowSize(m_window, &width, &height);

	// Save values to registry
	bool success = true;
	success &= writeRegistryValue(hKey, "WindowX", static_cast<DWORD>(x));
	success &= writeRegistryValue(hKey, "WindowY", static_cast<DWORD>(y));
	success &= writeRegistryValue(hKey, "WindowWidth", static_cast<DWORD>(width));
	success &= writeRegistryValue(hKey, "WindowHeight", static_cast<DWORD>(height));

	// Check if window is maximized
	if (glfwGetWindowAttrib(m_window, GLFW_MAXIMIZED)) {
		writeRegistryValue(hKey, "WindowMaximized", 1);
	} else {
		writeRegistryValue(hKey, "WindowMaximized", 0);
	}

	RegCloseKey(hKey);

	if (success) {
		std::cout << "Window state saved to registry: " << x << "x" << y << " " << width << "x" << height << std::endl;
	} else {
		std::cerr << "Failed to save some window state values to registry" << std::endl;
	}
}

void Window::loadWindowStateFromRegistry() {
	if (!m_config.saveWindowState) {
		return;
	}

	HKEY hKey;
	std::string keyPath = getRegistryKeyPath();
	
	// Open the registry key
	LSTATUS result = RegOpenKeyExA(HKEY_CURRENT_USER, keyPath.c_str(), 0, KEY_READ, &hKey);
	
	if (result != ERROR_SUCCESS) {
		std::cout << "No saved window state found in registry" << std::endl;
		return;
	}

	// Read values from registry
	DWORD x, y, width, height, maximized;
	bool hasValidData = true;
	
	hasValidData &= readRegistryValue(hKey, "WindowX", x);
	hasValidData &= readRegistryValue(hKey, "WindowY", y);
	hasValidData &= readRegistryValue(hKey, "WindowWidth", width);
	hasValidData &= readRegistryValue(hKey, "WindowHeight", height);
	
	// Maximized state is optional
	if (!readRegistryValue(hKey, "WindowMaximized", maximized)) {
		maximized = 0;
	}

	RegCloseKey(hKey);

	if (hasValidData) {
		// Validate the values are reasonable
		if (width >= 100 && height >= 100 && width <= 10000 && height <= 10000) {
			m_config.width = static_cast<int>(width);
			m_config.height = static_cast<int>(height);
			
			std::cout << "Restored window state from registry: " << x << "x" << y << " " 
					  << width << "x" << height << (maximized ? " (maximized)" : "") << std::endl;
		} else {
			std::cerr << "Invalid window dimensions in registry, using defaults" << std::endl;
		}
	} else {
		std::cout << "Incomplete window state data in registry, using defaults" << std::endl;
	}
}
#endif

// Window management
bool Window::initialize() {
	if (!initializeGLFW()) {
		return false;
	}

#ifdef _WIN32
	// Load window state from registry before creating the window
	loadWindowStateFromRegistry();
#endif

	// Set OpenGL version and profile
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_RESIZABLE, m_config.resizable ? GLFW_TRUE : GLFW_FALSE);
	glfwWindowHint(GLFW_SAMPLES, m_config.samples);

#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

	// Create window
	m_window = glfwCreateWindow(m_config.width, m_config.height,
		m_config.title.c_str(), nullptr, nullptr);
	if (!m_window) {
		std::cerr << "Failed to create GLFW window" << std::endl;
		return false;
	}

#ifdef _WIN32
	// Restore window position if we have saved state
	if (m_config.saveWindowState) {
		HKEY hKey;
		std::string keyPath = getRegistryKeyPath();
		
		if (RegOpenKeyExA(HKEY_CURRENT_USER, keyPath.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
			DWORD x, y, maximized;
			
			if (readRegistryValue(hKey, "WindowX", x) && readRegistryValue(hKey, "WindowY", y)) {
				int posX = static_cast<int>(x);
				int posY = static_cast<int>(y);
				
				// Validate that the restored position is visible on a connected monitor
				bool positionValid = false;
				int monitorCount;
				GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
				for (int i = 0; i < monitorCount; ++i) {
					int monitorX, monitorY;
					glfwGetMonitorPos(monitors[i], &monitorX, &monitorY);
					const GLFWvidmode* mode = glfwGetVideoMode(monitors[i]);
					// Check if the window's top-left corner is at least partially on this monitor
					if (posX >= monitorX - m_config.width + 100 &&
						posX < monitorX + mode->width - 100 &&
						posY >= monitorY &&
						posY < monitorY + mode->height - 100) {
						positionValid = true;
						break;
					}
				}
				
				if (positionValid) {
					glfwSetWindowPos(m_window, posX, posY);
				} else {
					std::cout << "Saved window position (" << posX << ", " << posY 
							  << ") is off-screen, using default position" << std::endl;
				}
			}
			
			if (readRegistryValue(hKey, "WindowMaximized", maximized) && maximized == 1) {
				glfwMaximizeWindow(m_window);
			}
			
			RegCloseKey(hKey);
		}
	}
#endif

	makeContextCurrent();

	// Initialize OpenGL
	if (!initializeGL()) {
		return false;
	}

	setupCallbacks();
	setVSync(m_config.vsync);

	// Create a default full-window viewport
	createViewport("main", 0, 0, m_config.width, m_config.height);

	return true;
}

void Window::shutdown() {
#ifdef _WIN32
	// Save window state before shutting down
	saveWindowStateToRegistry();
#endif

	clearViewports();

	if (m_window) {
		glfwDestroyWindow(m_window);
		m_window = nullptr;
	}
}

bool Window::initializeGL() {
	// Initialize GLEW
	if (glewInit() != GLEW_OK) {
		std::cerr << "Failed to initialize GLEW" << std::endl;
		return false;
	}

	// Print OpenGL version info
	std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
	std::cout << "GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;

	// Enable depth testing
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	// Enable multisampling if available
	if (m_config.samples > 1) {
		glEnable(GL_MULTISAMPLE);
	}

	return true;
}

void Window::setupCallbacks() {
	glfwSetWindowUserPointer(m_window, this);
	glfwSetErrorCallback(glfwErrorCallback);
	glfwSetKeyCallback(m_window, glfwKeyCallback);
	glfwSetMouseButtonCallback(m_window, glfwMouseButtonCallback);
	glfwSetCursorPosCallback(m_window, glfwCursorPosCallback);
	glfwSetScrollCallback(m_window, glfwScrollCallback);
	glfwSetFramebufferSizeCallback(m_window, glfwFramebufferSizeCallback);
}

bool Window::shouldClose() const {
	return m_window ? glfwWindowShouldClose(m_window) : true;
}

void Window::swapBuffers() 
{
	DECLARE_FUNC_LOW();
	if (m_window) 
	{
		glfwSwapBuffers(m_window);
	}
}

void Window::pollEvents() 
{
	DECLARE_FUNC_LOW();
	glfwPollEvents();
}

// Window properties
int Window::getWidth() const {
	int width;
	glfwGetFramebufferSize(m_window, &width, nullptr);
	return width;
}

int Window::getHeight() const {
	int height;
	glfwGetFramebufferSize(m_window, nullptr, &height);
	return height;
}

void Window::getPosition(int& x, int& y) const {
	if (m_window) {
		glfwGetWindowPos(m_window, &x, &y);
	}
}

void Window::setPosition(int x, int y) {
	if (m_window) {
		glfwSetWindowPos(m_window, x, y);
	}
}

void Window::setSize(int width, int height) {
	if (m_window) {
		glfwSetWindowSize(m_window, width, height);
		m_config.width = width;
		m_config.height = height;
	}
}

void Window::setTitle(const std::string& title) {
	m_config.title = title;
	if (m_window) {
		glfwSetWindowTitle(m_window, title.c_str());
	}
}

void Window::setVSync(bool enabled) {
	m_config.vsync = enabled;
	glfwSwapInterval(enabled ? 1 : 0);
}

// Registry state management
void Window::saveWindowState() {
#ifdef _WIN32
	saveWindowStateToRegistry();
#endif
}

void Window::restoreWindowState() {
#ifdef _WIN32
	loadWindowStateFromRegistry();
#endif
}

// Cache management methods
Window::CachedCameraSettings convertToCachedSettings(const ComponentSystem::CameraComponent::CameraSettings& settings, const Camera* camera) {
	Window::CachedCameraSettings cached;
	
	// Copy camera settings
	cached.movementSpeed = settings.movementSpeed;
	cached.rotationSpeed = settings.rotationSpeed;
	cached.sprintMultiplier = settings.sprintMultiplier;
	cached.smoothing = settings.smoothing;
	cached.invertPitch = settings.invertPitch;
	cached.invertYaw = settings.invertYaw;
	cached.mouseSensitivity = settings.mouseSensitivity;
	
	// Copy camera transform if camera is available
	if (camera) {
		const Vector3f& position = camera->getPosition();
		const Quaternion& orientation = camera->getOrientation();
		
		cached.transform.posX = position.getX();
		cached.transform.posY = position.getY();
		cached.transform.posZ = position.getZ();
		
		cached.transform.orientW = orientation.getW();
		cached.transform.orientX = orientation.getX();
		cached.transform.orientY = orientation.getY();
		cached.transform.orientZ = orientation.getZ();
	}
	
	return cached;
}

ComponentSystem::CameraComponent::CameraSettings convertFromCachedSettings(const Window::CachedCameraSettings& cached) {
	ComponentSystem::CameraComponent::CameraSettings settings;
	settings.movementSpeed = cached.movementSpeed;
	settings.rotationSpeed = cached.rotationSpeed;
	settings.sprintMultiplier = cached.sprintMultiplier;
	settings.smoothing = cached.smoothing;
	settings.invertPitch = cached.invertPitch;
	settings.invertYaw = cached.invertYaw;
	settings.mouseSensitivity = cached.mouseSensitivity;
	return settings;
}

void applyCachedTransform(Camera* camera, const Window::CachedCameraTransform& transform) {
	if (!camera) return;
	
	Vector3f position(transform.posX, transform.posY, transform.posZ);
	Quaternion orientation(transform.orientW, transform.orientX, transform.orientY, transform.orientZ);
	
	camera->setPosition(position);
	camera->setOrientation(orientation);
}

void Window::cacheViewportSettings(const std::string& name, const Viewport* viewport) {
	if (!viewport) return;

	ViewportCacheEntry entry;
	entry.config = viewport->getConfig();
	
	// Cache camera settings and transform if available
	auto* cameraComponent = viewport->getCameraComponent();
	if (cameraComponent) {
		Camera* camera = cameraComponent->GetCamera();
		entry.cameraSettings = convertToCachedSettings(cameraComponent->GetSettings(), camera);
		entry.hasCameraSettings = true;
		std::cout << "Cached camera settings and transform for viewport: " << name << std::endl;
	} else {
		entry.hasCameraSettings = false;
	}
	
	// Note: We can't easily copy the render callback as std::function is not copyable by default
	// The render callback will need to be set manually after loading from cache
	
	m_viewportCache[name] = entry;
	std::cout << "Cached viewport settings for: " << name << std::endl;
}

bool Window::loadCachedViewportSettings(const std::string& name, Viewport* viewport) {
	if (!viewport) return false;

	auto it = m_viewportCache.find(name);
	if (it != m_viewportCache.end()) {
		// Load viewport configuration
		viewport->setConfig(it->second.config);
		
		// Load camera settings and transform if available
		if (it->second.hasCameraSettings) {
			auto* cameraComponent = viewport->getCameraComponent();
			if (cameraComponent) {
				// Apply camera settings
				cameraComponent->SetSettings(convertFromCachedSettings(it->second.cameraSettings));
				
				// Apply camera transform
				Camera* camera = cameraComponent->GetCamera();
				if (camera) {
					applyCachedTransform(camera, it->second.cameraSettings.transform);
					std::cout << "Loaded cached camera settings and transform for viewport: " << name << std::endl;
				} else {
					std::cout << "Warning: Cached camera transform found but no camera for viewport: " << name << std::endl;
				}
			} else {
				std::cout << "Warning: Cached camera settings found but no camera component for viewport: " << name << std::endl;
			}
		}
		
		// Note: Render callback needs to be set manually after this call
		std::cout << "Loaded cached viewport settings for: " << name << std::endl;
		return true;
	}
	
	return false;
}

// Add this new method to explicitly cache a viewport's current settings
void Window::cacheCurrentViewportSettings(const std::string& name) {
	Viewport* viewport = getViewport(name);
	if (viewport) {
		cacheViewportSettings(name, viewport);
		std::cout << "Explicitly cached current settings for viewport: " << name << std::endl;
	} else {
		std::cout << "Warning: Cannot cache settings - viewport not found: " << name << std::endl;
	}
}

// Viewport management
Viewport* Window::createViewport(const std::string& name, int x, int y, int width, int height, bool useCache) {
	// Cache existing viewport settings before removal if it exists and we're using cache
	if (useCache) {
		Viewport* existingViewport = getViewport(name);
		if (existingViewport) {
			cacheViewportSettings(name, existingViewport);
		}
	}

	// Remove existing viewport with same name
	removeViewport(name);

	auto viewport = std::make_unique<Viewport>(name, x, y, width, height, this);
	Viewport* ptr = viewport.get();
	
	m_viewports.push_back(std::move(viewport));

	// Load cached settings if requested and available
	if (useCache) {
		bool loadedFromCache = loadCachedViewportSettings(name, ptr);
		if (!loadedFromCache) {
			std::cout << "No cached settings found for viewport: " << name << " (using defaults)" << std::endl;
			if(ptr->getCameraComponent()) ptr->getCameraComponent()->ResetCamera();
		}		
	}

	// Set as active if it's the first viewport
	if (m_viewports.size() == 1) {
		setActiveViewport(ptr);
	}

	return ptr;
}

Viewport* Window::getViewport(const std::string& name) {
	auto it = std::find_if(m_viewports.begin(), m_viewports.end(),
		[&name](const std::unique_ptr<Viewport>& vp) {
			return vp->getName() == name;
		});
	return (it != m_viewports.end()) ? it->get() : nullptr;
}

Viewport* Window::getViewport(size_t index) {
	return (index < m_viewports.size()) ? m_viewports[index].get() : nullptr;
}

void Window::removeViewport(const std::string& name) {
	auto it = std::find_if(m_viewports.begin(), m_viewports.end(),
		[&name](const std::unique_ptr<Viewport>& vp) {
			return vp->getName() == name;
		});

	if (it != m_viewports.end()) {
		// Cache settings before removal
		cacheViewportSettings(name, it->get());
		
		if (m_activeViewport == it->get()) {
			m_activeViewport = nullptr;
		}
		m_viewports.erase(it);

		// Set new active viewport if needed
		if (!m_activeViewport && !m_viewports.empty()) {
			setActiveViewport(m_viewports[0].get());
		}
	}
}

void Window::removeViewport(size_t index) {
	if (index < m_viewports.size()) {
		if (m_activeViewport == m_viewports[index].get()) {
			m_activeViewport = nullptr;
		}
		m_viewports.erase(m_viewports.begin() + index);

		// Set new active viewport if needed
		if (!m_activeViewport && !m_viewports.empty()) {
			setActiveViewport(m_viewports[0].get());
		}
	}
}

void Window::clearViewports() {
	m_viewports.clear();
	m_activeViewport = nullptr;
}

// Viewport cache management
void Window::clearViewportCache() {
	m_viewportCache.clear();
	std::cout << "Viewport cache cleared" << std::endl;
}

bool Window::hasViewportInCache(const std::string& name) const {
	return m_viewportCache.find(name) != m_viewportCache.end();
}

void Window::removeViewportFromCache(const std::string& name) {
	auto it = m_viewportCache.find(name);
	if (it != m_viewportCache.end()) {
		m_viewportCache.erase(it);
		std::cout << "Removed viewport from cache: " << name << std::endl;
	}
}

Viewport* Window::getViewportAtPosition(double x, double y) {
	for (auto& viewport : m_viewports) {
		if (viewport->containsPoint(x, y)) {
			return viewport.get();
		}
	}
	return nullptr;
}

void Window::updateViewportSizes() {
	if (m_viewports.empty()) {
		return;
	}

	// Get current window dimensions
	int newWidth = getWidth();
	int newHeight = getHeight();

	// Detect current viewport layout and regenerate it proportionally
	if (m_viewports.size() == 1) {
		// Single viewport - resize to full window
		Viewport* viewport = m_viewports[0].get();
		viewport->setBounds(0, 0, newWidth, newHeight);

		// Update camera aspect ratio if available
		auto* cameraComponent = viewport->getCameraComponent();
		if (cameraComponent) {
			float aspectRatio = static_cast<float>(newWidth) / static_cast<float>(newHeight);
			cameraComponent->UpdateAspectRatio(aspectRatio);
		}

		std::cout << "Updated single viewport to: " << newWidth << "x" << newHeight << std::endl;
	}
	else if (m_viewports.size() == 2) {
		// Determine if it's horizontal or vertical split based on current layout
		Viewport* first = m_viewports[0].get();
		Viewport* second = m_viewports[1].get();

		// Check if it's a horizontal split (left/right)
		if (first->getX() == 0 && second->getX() > 0 &&
			first->getY() == 0 && second->getY() == 0 &&
			first->getHeight() == second->getHeight()) {

			// Horizontal split - calculate split ratio from current layout
			float splitRatio = static_cast<float>(first->getWidth()) /
				static_cast<float>(first->getWidth() + second->getWidth());

			int splitPos = static_cast<int>(newWidth * splitRatio);

			first->setBounds(0, 0, splitPos, newHeight);
			second->setBounds(splitPos, 0, newWidth - splitPos, newHeight);

			// Update camera aspect ratios
			updateViewportCameraAspectRatio(first);
			updateViewportCameraAspectRatio(second);

			std::cout << "Updated horizontal split viewports with ratio: " << splitRatio << std::endl;
		}
		// Check if it's a vertical split (top/bottom)
		else if (first->getX() == 0 && second->getX() == 0 &&
			first->getY() == 0 && second->getY() > 0 &&
			first->getWidth() == second->getWidth()) {

			// Vertical split - calculate split ratio from current layout
			float splitRatio = static_cast<float>(first->getHeight()) /
				static_cast<float>(first->getHeight() + second->getHeight());

			int splitPos = static_cast<int>(newHeight * splitRatio);

			first->setBounds(0, 0, newWidth, splitPos);
			second->setBounds(0, splitPos, newWidth, newHeight - splitPos);

			// Update camera aspect ratios
			updateViewportCameraAspectRatio(first);
			updateViewportCameraAspectRatio(second);

			std::cout << "Updated vertical split viewports with ratio: " << splitRatio << std::endl;
		}
		else {
			// Unknown 2-viewport layout, resize proportionally
			resizeViewportsProportionally(newWidth, newHeight);
		}
	}
	else if (m_viewports.size() == 4) {
		// Check if it's a quad layout
		bool isQuadLayout = true;

		// Look for viewport names that suggest quad layout
		Viewport* topLeft = getViewport("top-left");
		Viewport* topRight = getViewport("top-right");
		Viewport* bottomLeft = getViewport("bottom-left");
		Viewport* bottomRight = getViewport("bottom-right");

		if (topLeft && topRight && bottomLeft && bottomRight) {
			// Regenerate quad layout
			int halfWidth = newWidth / 2;
			int halfHeight = newHeight / 2;

			topLeft->setBounds(0, 0, halfWidth, halfHeight);
			topRight->setBounds(halfWidth, 0, halfWidth, halfHeight);
			bottomLeft->setBounds(0, halfHeight, halfWidth, halfHeight);
			bottomRight->setBounds(halfWidth, halfHeight, halfWidth, halfHeight);

			// Update camera aspect ratios for all viewports
			updateViewportCameraAspectRatio(topLeft);
			updateViewportCameraAspectRatio(topRight);
			updateViewportCameraAspectRatio(bottomLeft);
			updateViewportCameraAspectRatio(bottomRight);

			std::cout << "Updated quad layout viewports" << std::endl;
		}
		else {
			// Unknown 4-viewport layout, resize proportionally
			resizeViewportsProportionally(newWidth, newHeight);
		}
	}
	else {
		// Multiple viewports with unknown layout - resize proportionally
		resizeViewportsProportionally(newWidth, newHeight);
	}
}

// Active viewport management with camera activation/deactivation
void Window::setActiveViewport(Viewport* viewport) {
	if (m_activeViewport == viewport) {
		return; // Already active, no change needed
	}

	// Deactivate the current active viewport's camera
	if (m_activeViewport) {
		m_activeViewport->deactivateCamera();
		std::cout << "Deactivated camera for viewport: " << m_activeViewport->getName() << std::endl;
	}

	// Set the new active viewport
	m_activeViewport = viewport;
	Rendering::RenderingSystem::SetActiveViewport(m_activeViewport);

	// Activate the new viewport's camera
	if (m_activeViewport) {
		m_activeViewport->activateCamera();
		std::cout << "Activated camera for viewport: " << m_activeViewport->getName() << std::endl;
	}
}

// Layout helpers
void Window::setHorizontalSplit(float splitRatio) {
	// Cache all current viewports before clearing
	for (const auto& viewport : m_viewports) {
		cacheViewportSettings(viewport->getName(), viewport.get());
	}
	
	clearViewports();
	int width = getWidth();
	int height = getHeight();
	int splitPos = static_cast<int>(width * splitRatio);

	createViewport("left", 0, 0, splitPos, height);
	createViewport("right", splitPos, 0, width - splitPos, height);
}

void Window::setVerticalSplit(float splitRatio) {
	// Cache all current viewports before clearing
	for (const auto& viewport : m_viewports) {
		cacheViewportSettings(viewport->getName(), viewport.get());
	}
	
	clearViewports();
	int width = getWidth();
	int height = getHeight();
	int splitPos = static_cast<int>(height * splitRatio);

	createViewport("top", 0, 0, width, splitPos);
	createViewport("bottom", 0, splitPos, width, height - splitPos);
}

void Window::setQuadLayout() {
	// Cache all current viewports before clearing
	for (const auto& viewport : m_viewports) {
		cacheViewportSettings(viewport->getName(), viewport.get());
	}
	
	clearViewports();
	int width = getWidth();
	int height = getHeight();
	int halfWidth = width / 2;
	int halfHeight = height / 2;

	createViewport("top-left", 0, 0, halfWidth, halfHeight);
	createViewport("top-right", halfWidth, 0, halfWidth, halfHeight);
	createViewport("bottom-left", 0, halfHeight, halfWidth, halfHeight);
	createViewport("bottom-right", halfWidth, halfHeight, halfWidth, halfHeight);
}

void Window::setSingleViewport() {
	// Cache all current viewports before clearing
	for (const auto& viewport : m_viewports) {
		cacheViewportSettings(viewport->getName(), viewport.get());
	}
	
	clearViewports();
	createViewport("main", 0, 0, getWidth(), getHeight());
}

// Rendering
void Window::render() 
{
	DECLARE_FUNC_LOW();
	makeContextCurrent();

	ClearGLViewport();


	// Render each viewport
	for (auto& viewport : m_viewports) {
		viewport->render();
	}
	{
		Rendering::RenderingSystem::GetRenderQueue()->Clear();
	}
}

void Window::ClearGLViewport()
{
	DECLARE_FUNC_LOW();
    // Clear the entire window
    glViewport(0, 0, getWidth(), getHeight());
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Window::clear(float r, float g, float b, float a) {
	glClearColor(r, g, b, a);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

double Window::getTime() const {
	return glfwGetTime();
}

void Window::makeContextCurrent() {
	if (m_window) {
		glfwMakeContextCurrent(m_window);
	}
}

// Static utility methods
bool Window::initializeGLFW() {
	static bool initialized = false;
	if (!initialized) {
		if (!glfwInit()) {
			std::cerr << "Failed to initialize GLFW" << std::endl;
			return false;
		}
		initialized = true;
	}
	return true;
}

void Window::terminateGLFW() {
	glfwTerminate();
}


// Helper method to update a single viewport's camera aspect ratio
void Window::updateViewportCameraAspectRatio(Viewport* viewport) {
	if (!viewport) return;

	auto* cameraComponent = viewport->getCameraComponent();
	if (cameraComponent && viewport->getWidth() > 0 && viewport->getHeight() > 0) {
		float aspectRatio = static_cast<float>(viewport->getWidth()) /
		static_cast<float>(viewport->getHeight());
		cameraComponent->UpdateAspectRatio(aspectRatio);
	}
}

// Helper method to resize all viewports proportionally when layout is unknown
void Window::resizeViewportsProportionally(int newWidth, int newHeight) {
	// Calculate scale factors based on window size change
	// This assumes we can determine the original window size from viewport bounds

	// Find the bounding box of all current viewports to determine original size
	int maxRight = 0;
	int maxBottom = 0;

	for (const auto& viewport : m_viewports) {
		maxRight = std::max(maxRight, viewport->getX() + viewport->getWidth());
		maxBottom = std::max(maxBottom, viewport->getY() + viewport->getHeight());
	}

	// Avoid division by zero
	if (maxRight == 0 || maxBottom == 0) {
		std::cerr << "Cannot determine original viewport dimensions for proportional resize" << std::endl;
		return;
	}

	// Calculate scale factors
	float scaleX = static_cast<float>(newWidth) / static_cast<float>(maxRight);
	float scaleY = static_cast<float>(newHeight) / static_cast<float>(maxBottom);

	// Resize each viewport proportionally
	for (const auto& viewport : m_viewports) {
		int newX = static_cast<int>(viewport->getX() * scaleX);
		int newY = static_cast<int>(viewport->getY() * scaleY);
		int newViewportWidth = static_cast<int>(viewport->getWidth() * scaleX);
		int newViewportHeight = static_cast<int>(viewport->getHeight() * scaleY);

		viewport->setBounds(newX, newY, newViewportWidth, newViewportHeight);
		updateViewportCameraAspectRatio(viewport.get());
	}

	std::cout << "Resized " << m_viewports.size() << " viewports proportionally (scale: "
		<< scaleX << "x" << scaleY << ")" << std::endl;
}