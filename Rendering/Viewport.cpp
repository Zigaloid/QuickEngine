#include "Viewport.h"
#include "Profiler/Profiler.h"
#include "CameraComponent.h"
#include "ComponentSystem/ComponentSystem.h"
#include "CoreSystem/CoreSystem.h"
#include "Window.h"
#include <iostream>
#include <cmath>

// Simple vertex and fragment shaders for grid and axes
const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;

uniform mat4 viewProjection;
out vec3 color;

void main() {
    gl_Position = viewProjection * vec4(aPos, 1.0);
    color = aColor;
}
)";

const char* fragmentShaderSource = R"(
#version 330 core
in vec3 color;
out vec4 FragColor;

void main() {
    FragColor = vec4(color, 1.0);
}
)";

DebugChannels::CDebugChannel ViewportDebug("ViewportDebug");

Viewport::Viewport(const std::string& name, int x, int y, int width, int height, Window* parent)
	: m_name(name)
	, m_x(x)
	, m_y(y)
	, m_width(width)
	, m_height(height)
	, m_parentWindow(parent)
	, m_cameraComponent(nullptr)
	, m_gridVAO(0)
	, m_gridVBO(0)
	, m_axesVAO(0)
	, m_axesVBO(0)
	, m_shaderProgram(0)
{
	initializeOpenGL();
	initializeCameraComponent();
	RegisterWithMouseManager();

}

Viewport::~Viewport() {
	// Release the camera component back to the component manager
	auto* componentManager = Core::CoreSystem::GetComponentManager();
	if (componentManager && m_cameraComponent) {
		componentManager->ReleaseComponent(m_cameraComponent);
		m_cameraComponent = nullptr;
	}
	UnregisterFromMouseManager();
	cleanupOpenGL();
}

void Viewport::initializeCameraComponent() {
	auto* componentManager = Core::CoreSystem::GetComponentManager();
	auto* scheduler = Core::CoreSystem::GetJobSystemScheduler();
	
	if (componentManager) {
		// Register camera component type if not already registered
		componentManager->RegisterComponentType<ComponentSystem::CameraComponent>(1, 10);

		// Register CameraComponent with the scheduler for updates (only once globally)
		if (scheduler) {
			// Check if CameraComponent is already registered by trying to register it
			// The scheduler should handle duplicate registrations gracefully
			scheduler->RegisterComponentType<ComponentSystem::CameraComponent>(5, ComponentSystem::CameraComponent::ClassName());
			ViewportDebug.printf("CameraComponent registered with ComponentScheduler for viewport: %s\n", m_name.c_str());
		} else {
			ViewportDebug.warning("ComponentScheduler not available for viewport: %s\n", m_name.c_str());
		}

		// Create and initialize camera component
		m_cameraComponent = componentManager->CreateComponent<ComponentSystem::CameraComponent>();
		if (m_cameraComponent) {
			if (!m_cameraComponent->Initialize()) {
				ViewportDebug.warning("Failed to initialize camera component for viewport: %s\n", m_name.c_str());
				componentManager->ReleaseComponent(m_cameraComponent);
				m_cameraComponent = nullptr;
				return;
			}

			// Configure default camera settings
			ComponentSystem::CameraComponent::CameraSettings cameraSettings;
			cameraSettings.movementSpeed = 10.0f;
			cameraSettings.rotationSpeed = 3.0f;
			cameraSettings.sprintMultiplier = 2.5f;
			cameraSettings.smoothing = 0.15f;
			cameraSettings.invertPitch = false;
			cameraSettings.invertYaw = false;
			cameraSettings.mouseSensitivity = 0.002f;  // Set mouse sensitivity
			m_cameraComponent->SetSettings(cameraSettings);

			// Set initial camera position
			m_cameraComponent->SetPosition(Vector3f(0, 5, 10));
			m_cameraComponent->SetLookAt(Vector3f(0, 0, 0));

			// Update aspect ratio
			if (m_height > 0) {
				m_cameraComponent->UpdateAspectRatio(static_cast<float>(m_width) / m_height);
			}
			ViewportDebug.printf("Camera component created and initialized for viewport: %s\n", m_name.c_str());
		}
		else {
			ViewportDebug.warning("Failed to create camera component for viewport: %s\n", m_name.c_str());
		}
	}
	else {
		ViewportDebug.warning("CoreSystem component manager not available for viewport: %s\n", m_name.c_str());
	}
}

void Viewport::initializeOpenGL() {
	createShaders();
	createGridGeometry();
	createAxesGeometry();
}

GLuint Viewport::createShader(GLenum type, const char* source) {
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, nullptr);
	glCompileShader(shader);

	// Check for compilation errors
	GLint success;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success) {
		char infoLog[512];
		glGetShaderInfoLog(shader, 512, nullptr, infoLog);
		ViewportDebug.warning("Shader compilation failed: %s\n", infoLog);
		glDeleteShader(shader);
		return 0;
	}

	return shader;
}

GLuint Viewport::createProgram(const char* vertexSource, const char* fragmentSource) {
	GLuint vertexShader = createShader(GL_VERTEX_SHADER, vertexSource);
	GLuint fragmentShader = createShader(GL_FRAGMENT_SHADER, fragmentSource);

	if (vertexShader == 0 || fragmentShader == 0) {
		return 0;
	}

	GLuint program = glCreateProgram();
	glAttachShader(program, vertexShader);
	glAttachShader(program, fragmentShader);
	glLinkProgram(program);

	// Check for linking errors
	GLint success;
	glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (!success) {
		char infoLog[512];
		glGetProgramInfoLog(program, 512, nullptr, infoLog);
		ViewportDebug.warning("Program linking failed: %s\n", infoLog);
	}

	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	return program;
}

void Viewport::createShaders() {
	m_shaderProgram = createProgram(vertexShaderSource, fragmentShaderSource);
}

void Viewport::createGridGeometry() {
	std::vector<float> gridVertices;

	float halfSize = m_config.gridSize * 0.5f;
	float step = m_config.gridSize / m_config.gridLines;

	// Generate grid lines
	for (int i = 0; i <= m_config.gridLines; ++i) {
		float pos = -halfSize + i * step;

		// Vertical lines (along X axis)
		gridVertices.insert(gridVertices.end(), {
			pos, 0.0f, -halfSize,  // position
			m_config.gridColor.getX(), m_config.gridColor.getY(), m_config.gridColor.getZ(),  // color
			pos, 0.0f, halfSize,   // position
			m_config.gridColor.getX(), m_config.gridColor.getY(), m_config.gridColor.getZ()   // color
			});

		// Horizontal lines (along Z axis)
		gridVertices.insert(gridVertices.end(), {
			-halfSize, 0.0f, pos,  // position
			m_config.gridColor.getX(), m_config.gridColor.getY(), m_config.gridColor.getZ(),  // color
			halfSize, 0.0f, pos,   // position
			m_config.gridColor.getX(), m_config.gridColor.getY(), m_config.gridColor.getZ()   // color
			});
	}

	glGenVertexArrays(1, &m_gridVAO);
	glGenBuffers(1, &m_gridVBO);

	glBindVertexArray(m_gridVAO);
	glBindBuffer(GL_ARRAY_BUFFER, m_gridVBO);
	glBufferData(GL_ARRAY_BUFFER, gridVertices.size() * sizeof(float),
		gridVertices.data(), GL_STATIC_DRAW);

	// Position attribute
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	// Color attribute
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
		(void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	glBindVertexArray(0);
}

void Viewport::createAxesGeometry() {
	std::vector<float> axesVertices = {
		// X axis (red)
		0.0f, 0.0f, 0.0f,  1.0f, 0.0f, 0.0f,
		2.0f, 0.0f, 0.0f,  1.0f, 0.0f, 0.0f,

		// Y axis (green)
		0.0f, 0.0f, 0.0f,  0.0f, 1.0f, 0.0f,
		0.0f, 2.0f, 0.0f,  0.0f, 1.0f, 0.0f,

		// Z axis (blue)
		0.0f, 0.0f, 0.0f,  0.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 2.0f,  0.0f, 0.0f, 1.0f
	};

	glGenVertexArrays(1, &m_axesVAO);
	glGenBuffers(1, &m_axesVBO);

	glBindVertexArray(m_axesVAO);
	glBindBuffer(GL_ARRAY_BUFFER, m_axesVBO);
	glBufferData(GL_ARRAY_BUFFER, axesVertices.size() * sizeof(float),
		axesVertices.data(), GL_STATIC_DRAW);

	// Position attribute
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	// Color attribute
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
		(void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	glBindVertexArray(0);
}

void Viewport::cleanupOpenGL() {
	if (m_gridVAO) {
		glDeleteVertexArrays(1, &m_gridVAO);
		glDeleteBuffers(1, &m_gridVBO);
		m_gridVAO = 0;
		m_gridVBO = 0;
	}

	if (m_axesVAO) {
		glDeleteVertexArrays(1, &m_axesVAO);
		glDeleteBuffers(1, &m_axesVBO);
		m_axesVAO = 0;
		m_axesVBO = 0;
	}

	if (m_shaderProgram) {
		glDeleteProgram(m_shaderProgram);
		m_shaderProgram = 0;
	}
}

// Position and size methods
void Viewport::setPosition(int x, int y) {
	m_x = x;
	m_y = y;
}

void Viewport::setSize(int width, int height) {
	m_width = width;
	m_height = height;

	// Update camera aspect ratio through camera component
	if (m_cameraComponent && height > 0) {
		m_cameraComponent->UpdateAspectRatio(static_cast<float>(width) / height);
	}
}

void Viewport::setBounds(int x, int y, int width, int height) {
	setPosition(x, y);
	setSize(width, height);
}

void Viewport::setConfig(const ViewportConfig& config) {
	m_config = config;

	// Recreate grid geometry if grid settings changed
	if (m_gridVAO) {
		glDeleteVertexArrays(1, &m_gridVAO);
		glDeleteBuffers(1, &m_gridVBO);
	}
	createGridGeometry();
}

Camera* Viewport::getCamera() const {
	return m_cameraComponent ? m_cameraComponent->GetCamera() : nullptr;
}

// Camera activation/deactivation methods - updated for mouse/keyboard input
void Viewport::activateCamera() {
	if (m_cameraComponent) {
		m_cameraComponent->SetEnabled(true);
		ViewportDebug.printf("Activated camera input handler for viewport: %s\n", m_name.c_str());
	}
}

void Viewport::deactivateCamera() {
	if (m_cameraComponent) {
		m_cameraComponent->SetEnabled(false);		
	}
}

bool Viewport::isCameraActive() const {
	if (m_cameraComponent) {
		return m_cameraComponent->IsEnabled();
	}
	return false;
}

void Viewport::handleKeyboard(int key, int scancode, int action, int mods) {
	if (m_cameraComponent && isCameraActive()) {
		m_cameraComponent->HandleKeyboard(key, scancode, action, mods);		
	}
}
void Viewport::RegisterWithMouseManager() 
{
	m_mouseManager = Core::CoreSystem::GetFirstComponentOfType<Input::MouseManager>();	
	if (m_mouseManager) {
		m_selfReference = std::shared_ptr<Viewport>(this, [](Viewport*) {
			// Custom deleter that does nothing - component lifetime is managed by ComponentManager
			});
		//m_mouseManager->AddInputHandler(m_selfReference);
		std::cout << "CameraComponent registered with MouseManager" << std::endl;
	}
	else {
		std::cout << "Warning: No MouseManager found for CameraComponent registration" << std::endl;
	}
}

void Viewport::UnregisterFromMouseManager()
{
	if (m_mouseManager && m_selfReference) {
		//m_mouseManager->RemoveInputHandler(m_selfReference);
		m_selfReference.reset();
		m_mouseManager = nullptr;
		std::cout << "CameraComponent unregistered from MouseManager" << std::endl;
	}
}

void Viewport::render() 
{
	DECLARE_FUNC_LOW();
	Camera* camera = getCamera();
	if (!camera) {
		ViewportDebug.warning("Viewport '%s' has no camera - skipping render\n", m_name.c_str());
		return;
	}

	// Set viewport
	int windowHeight = m_parentWindow->getHeight();
	glViewport(m_x, windowHeight - m_y - m_height, m_width, m_height);

	// Enable scissor test to clip rendering to this viewport
	glEnable(GL_SCISSOR_TEST);
	glScissor(m_x, windowHeight - m_y - m_height, m_width, m_height);

	// Enable depth testing and face culling if configured
	if (m_config.enableDepthTest) {
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);
	}
	else {
		glDisable(GL_DEPTH_TEST);
	}

	if (m_config.enableCulling) {
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
		glFrontFace(GL_CCW);
	}
	else {
		glDisable(GL_CULL_FACE);
	}

	// Clear viewport with background color
	glClearColor(m_config.backgroundColor.getX(),
		m_config.backgroundColor.getY(),
		m_config.backgroundColor.getZ(), 1.0f);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Get view and projection matrices
	const Matrix4f& viewMatrix = camera->getViewMatrix();
	const Matrix4f& projectionMatrix = camera->getProjectionMatrix();
	Matrix4f viewProjectionMatrix = projectionMatrix * viewMatrix;

	// Render grid and axes if we have shaders
	if (m_shaderProgram) {
		glUseProgram(m_shaderProgram);

		// Set view-projection matrix uniform
		GLint vpLocation = glGetUniformLocation(m_shaderProgram, "viewProjection");
		if (vpLocation == -1) {
			static bool errorReported = false;
			if (!errorReported) {
				ViewportDebug.warning("Could not find 'viewProjection' uniform in shader\n");
				errorReported = true;
			}
		}
		else {
			glUniformMatrix4fv(vpLocation, 1, GL_FALSE, viewProjectionMatrix.data());
		}

		// Render grid
		if (m_config.showGrid && m_gridVAO) {
			glBindVertexArray(m_gridVAO);
			int gridLineCount = (m_config.gridLines + 1) * 4;  // 2 lines per grid line, 2 vertices per line

			glDrawArrays(GL_LINES, 0, gridLineCount);
		}

		// Render axes
		if (m_config.showAxes && m_axesVAO) {
			glLineWidth(3.0f);  // Make axes thicker
			glBindVertexArray(m_axesVAO);
			glDrawArrays(GL_LINES, 0, 6);  // 3 axes, 2 vertices each
			glLineWidth(1.0f);  // Reset line width
		}

		glBindVertexArray(0);
	}
	else {
		static bool errorReported = false;
		if (!errorReported) {
			ViewportDebug.warning("No shader program available for viewport rendering!\n");
			errorReported = true;
		}
	}

	// Call user render callback - this should use the same shader program
	if (m_renderCallback) {
		// Make sure the shader is still active for user content
		if (m_shaderProgram) {
			glUseProgram(m_shaderProgram);
			GLint vpLocation = glGetUniformLocation(m_shaderProgram, "viewProjection");
			glUniformMatrix4fv(vpLocation, 1, GL_FALSE, viewProjectionMatrix.data());
		}

		m_renderCallback(*this, viewMatrix, projectionMatrix);
	}

	// Clean up
	glUseProgram(0);
	glBindVertexArray(0);
	glDisable(GL_SCISSOR_TEST);
}

bool Viewport::containsPoint(double x, double y) const {
	int windowHeight = m_parentWindow->getHeight();
	double viewportY = windowHeight - m_y - m_height;  // Convert to OpenGL coordinates
	y = windowHeight - y;
	return x >= m_x && x < m_x + m_width &&
		y >= viewportY && y < viewportY + m_height;
}

void Viewport::screenToViewport(double screenX, double screenY, double& viewportX, double& viewportY) const {
	int windowHeight = m_parentWindow->getHeight();
	viewportX = screenX - m_x;
	viewportY = (windowHeight - screenY) - (windowHeight - m_y - m_height);
}

Vector3f Viewport::screenToWorld(double screenX, double screenY, float depth) const {
	Camera* camera = getCamera();
	if (!camera) return Vector3f(0, 0, 0);

	// Convert screen coordinates to viewport-relative coordinates
	double viewportX, viewportY;
	screenToViewport(screenX, screenY, viewportX, viewportY);

	// Convert to normalized device coordinates (-1 to 1)
	float ndcX = (2.0f * static_cast<float>(viewportX) / m_width) - 1.0f;
	float ndcY = (2.0f * static_cast<float>(viewportY) / m_height) - 1.0f;
	float ndcZ = 2.0f * depth - 1.0f; // Convert depth from [0,1] to [-1,1]

	// Create point in clip space
	Vector3f clipSpacePoint(ndcX, ndcY, ndcZ);

	// Get the inverse view-projection matrix
	Matrix4f viewMatrix = camera->getViewMatrix();
	Matrix4f projMatrix = camera->getProjectionMatrix();
	Matrix4f viewProjMatrix = projMatrix * viewMatrix;
	Matrix4f inverseViewProj = viewProjMatrix.inverse();

	// Transform from clip space to world space
	// For proper perspective division, we need to work with homogeneous coordinates
	float x = clipSpacePoint.getX();
	float y = clipSpacePoint.getY();
	float z = clipSpacePoint.getZ();
	float w = 1.0f;

	// Apply inverse transformation manually to handle homogeneous coordinates
	const float* invMatrix = inverseViewProj.data();
	float worldX = x * invMatrix[0] + y * invMatrix[4] + z * invMatrix[8] + w * invMatrix[12];
	float worldY = x * invMatrix[1] + y * invMatrix[5] + z * invMatrix[9] + w * invMatrix[13];
	float worldZ = x * invMatrix[2] + y * invMatrix[6] + z * invMatrix[10] + w * invMatrix[14];
	float worldW = x * invMatrix[3] + y * invMatrix[7] + z * invMatrix[11] + w * invMatrix[15];

	// Perform perspective division
	if (std::abs(worldW) > 1e-6f) {
		worldX /= worldW;
		worldY /= worldW;
		worldZ /= worldW;
	}

	return Vector3f(worldX, worldY, worldZ);
}

void Viewport::worldToScreen(const Vector3f& worldPos, double& screenX, double& screenY) const {
	Camera* camera = getCamera();
	if (!camera) {
		screenX = screenY = 0.0;
		return;
	}

	Matrix4f mvp = camera->getViewProjectionMatrix();
	Vector3f clipPos = mvp.transformPoint(worldPos);

	// Convert from clip space to screen space
	screenX = (clipPos.getX() * 0.5f + 0.5f) * m_width + m_x;
	screenY = (1.0f - (clipPos.getY() * 0.5f + 0.5f)) * m_height + m_y;
}

void Viewport::printDebugInfo() const {
	ViewportDebug.printf("\n=== Viewport Debug Info ===\n");
	ViewportDebug.printf("Viewport: %s\n", m_name.c_str());
	ViewportDebug.printf("  Position: (%d, %d)\n", m_x, m_y);
	ViewportDebug.printf("  Size: %dx%d\n", m_width, m_height);
	ViewportDebug.printf("  Shader Program: %u\n", m_shaderProgram);
	ViewportDebug.printf("  Camera Active: %s\n", (isCameraActive() ? "Yes" : "No"));
	ViewportDebug.printf("  Background Color: (%.3f, %.3f, %.3f)\n", 
		m_config.backgroundColor.getX(),
		m_config.backgroundColor.getY(),
		m_config.backgroundColor.getZ());
	ViewportDebug.printf("  Show Grid: %s\n", (m_config.showGrid ? "Yes" : "No"));
	ViewportDebug.printf("  Show Axes: %s\n", (m_config.showAxes ? "Yes" : "No"));

	Camera* camera = getCamera();
	if (camera) {
		const Vector3f& pos = camera->getPosition();
		ViewportDebug.printf("  Camera Position: (%.3f, %.3f, %.3f)\n", pos.getX(), pos.getY(), pos.getZ());

		// Print distance from camera to origin
		float distance = pos.magnitude();
		ViewportDebug.printf("  Distance from origin: %.3f\n", distance);

		// Print camera component settings if available
		if (m_cameraComponent) {
			const auto& settings = m_cameraComponent->GetSettings();
			ViewportDebug.printf("  Camera Component Settings:\n");
			ViewportDebug.printf("    Movement Speed: %.3f\n", settings.movementSpeed);
			ViewportDebug.printf("    Rotation Speed: %.3f\n", settings.rotationSpeed);
			ViewportDebug.printf("    Sprint Multiplier: %.3f\n", settings.sprintMultiplier);
			ViewportDebug.printf("    Smoothing: %.3f\n", settings.smoothing);
			ViewportDebug.printf("    Mouse Sensitivity: %.6f\n", settings.mouseSensitivity);
			ViewportDebug.printf("    Invert Pitch: %s\n", (settings.invertPitch ? "Yes" : "No"));
			ViewportDebug.printf("    Invert Yaw: %s\n", (settings.invertYaw ? "Yes" : "No"));
		}
	}
	else {
		ViewportDebug.printf("  Camera: NULL\n");
	}
	ViewportDebug.printf("=========================\n");
}


bool Viewport::HandleButtonInput(Input::MouseButton button, bool pressed, bool justPressed, bool justReleased, double x, double y)
{
	return false;
}