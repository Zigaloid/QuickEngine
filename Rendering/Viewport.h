#pragma once

#include <gl\glew.h>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include "Camera.h"
#include "Input\MouseInputHandler.h"
#include "math\Vector3f.h"
#include "math\Matrix4f.h"

class Window; // Forward declaration

namespace ComponentSystem {
	class CameraComponent; // Forward declaration
}
namespace Input {
	class MouseManager; // Forward declaration
}
class Viewport {
public:
	struct ViewportConfig {
		Vector3f backgroundColor = Vector3f(0.1f, 0.1f, 0.1f);
		bool showGrid = true;
		bool showAxes = false;
		Vector3f gridColor = Vector3f(0.3f, 0.3f, 0.3f);
		Vector3f axesColor = Vector3f(1.0f, 1.0f, 1.0f);
		float gridSize = 10.0f;
		int gridLines = 20;
		bool enableDepthTest = true;
		bool enableCulling = true;
		bool enableMSAA = true;
	};

	// Render callback type - called during viewport rendering
	using RenderCallback = std::function<void(Viewport& viewport, const Matrix4f& viewMatrix, const Matrix4f& projectionMatrix)>;

private:
	std::string m_name;
	int m_x, m_y, m_width, m_height;
	ViewportConfig m_config;
	ComponentSystem::CameraComponent* m_cameraComponent;
	Window* m_parentWindow;

	// OpenGL resources for grid and axes
	GLuint m_gridVAO, m_gridVBO;
	GLuint m_axesVAO, m_axesVBO;
	GLuint m_shaderProgram;

	Input::MouseManager* m_mouseManager;
	std::shared_ptr<Viewport> m_selfReference; // For registration with MouseManager

	// Render callback
	RenderCallback m_renderCallback;

	// Internal methods
	void initializeOpenGL();
	void createGridGeometry();
	void createAxesGeometry();
	void createShaders();
	void cleanupOpenGL();
	void initializeCameraComponent();
	void RegisterWithMouseManager();
	void UnregisterFromMouseManager();
	// Shader creation helpers
	GLuint createShader(GLenum type, const char* source);
	GLuint createProgram(const char* vertexSource, const char* fragmentSource);

	// Convert screen coordinates to viewport-relative coordinates	
	bool HandleButtonInput(Input::MouseButton button, bool pressed, bool justPressed, bool justReleased, double x, double y);

public:
	Viewport(const std::string& name, int x, int y, int width, int height, Window* parent);
	~Viewport();

	// Basic properties
	const std::string& getName() const { return m_name; }
	void setName(const std::string& name) { m_name = name; }
	void screenToViewport(double screenX, double screenY, double& viewportX, double& viewportY) const;
	// Position and size
	int getX() const { return m_x; }
	int getY() const { return m_y; }
	int getWidth() const { return m_width; }
	int getHeight() const { return m_height; }

	void setPosition(int x, int y);
	void setSize(int width, int height);
	void setBounds(int x, int y, int width, int height);

	// Configuration
	const ViewportConfig& getConfig() const { return m_config; }
	void setConfig(const ViewportConfig& config);
	void setBackgroundColor(const Vector3f& color) { m_config.backgroundColor = color; }
	void setShowGrid(bool show) { m_config.showGrid = show; }
	void setShowAxes(bool show) { m_config.showAxes = show; }

	// Camera access - now through owned camera component
	Camera* getCamera() const;
	ComponentSystem::CameraComponent* getCameraComponent() const { return m_cameraComponent; }

	// Camera activation/deactivation
	void activateCamera();
	void deactivateCamera();
	bool isCameraActive() const;

	// Input handling - delegates to camera component
	bool containsPoint(double x, double y) const;
	void handleKeyboard(int key, int scancode, int action, int mods);

	// Rendering
	void render();
	void setRenderCallback(const RenderCallback& callback) { m_renderCallback = callback; }

	// Coordinate conversion
	Vector3f screenToWorld(double screenX, double screenY, float depth = 0.0f) const;
	void worldToScreen(const Vector3f& worldPos, double& screenX, double& screenY) const;

	// Shader access for render callbacks
	GLuint getShaderProgram() const { return m_shaderProgram; }

	// Debug information
	void printDebugInfo() const;
};