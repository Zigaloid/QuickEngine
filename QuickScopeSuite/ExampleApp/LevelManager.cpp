#include "LevelManager.h"
#include "LevelComponent.h"
#include "RenderComponent.h"
#include "CameraComponent.h"
#include "RenderPrimitives.h"
#include "RenderingSystem.h"
#include "CoreSystem/CoreSystem.h"
#include "ComponentSystem/ComponentSystem.h"
#include "Camera.h"
#include "GLTFRenderComponent.h"
// ImGui includes
#include "imgui.h"

#include <algorithm>
#include <sstream>
#include <chrono>
#include <ctime>

DebugChannels::CDebugChannel LevelManagerDebug("LevelManager");

LevelManager::LevelManager()
{
}

LevelManager::~LevelManager()
{
	Shutdown();
}

bool LevelManager::Initialize(ComponentSystem::ComponentManager* componentManager)
{
	DECLARE_FUNC_VLOW();

	if (!componentManager) {
		LevelManagerDebug.printf("Cannot initialize LevelManager: ComponentManager is null\n");
		return false;
	}

	m_componentManager = componentManager;

	LevelManagerDebug.printf("LevelManager initialized successfully\n");
	return true;
}

void LevelManager::Shutdown()
{
	DECLARE_FUNC_VLOW();

	ClearLevel();
	m_componentManager = nullptr;

	LevelManagerDebug.printf("LevelManager shutdown complete\n");
}

bool LevelManager::CreateLevel(const std::string& levelName)
{
	DECLARE_FUNC_VLOW();

	if (!m_componentManager) {
		LevelManagerDebug.printf("Cannot create level: ComponentManager not initialized\n");
		return false;
	}

	// Clear existing level first
	ClearLevel();

	// Create new level component
	m_level = m_componentManager->CreateComponent<Rendering::LevelComponent>();
	if (!m_level) {
		LevelManagerDebug.printf("Failed to create LevelComponent\n");
		return false;
	}

	m_level->SetLevelName(levelName);
	m_level->Initialize();

	// Setup initial level content
	setupInitialLevelContent();

	LevelManagerDebug.printf("Level '%s' created successfully\n", levelName.c_str());
	return true;
}

void LevelManager::ClearLevel()
{
	DECLARE_FUNC_VLOW();

	if (m_level && m_componentManager) {
		// Level component will be cleaned up by ComponentManager
		m_level = nullptr;
		LevelManagerDebug.printf("Level cleared\n");
	}
}

bool LevelManager::SaveCurrentLevel(const std::string& fileName)
{
	DECLARE_FUNC_LOW();

	if (!m_level) {
		LevelManagerDebug.warning("Cannot save level: No level loaded");
		return false;
	}

	if (m_level->SaveLevel(fileName)) {
		LevelManagerDebug.printf("Level '%s' saved to: %s\n",
			m_level->GetLevelName().c_str(), fileName.c_str());
		return true;
	}
	else {
		LevelManagerDebug.warning("Failed to save level to: %s", fileName.c_str());
		return false;
	}
}

bool LevelManager::LoadLevel(const std::string& fileName)
{
	DECLARE_FUNC_LOW();

	if (!m_componentManager) {
		LevelManagerDebug.warning("Cannot load level: ComponentManager not initialized");
		return false;
	}

	// Clear existing level first
	ClearLevel();

	// Create new level component
	m_level = m_componentManager->CreateComponent<Rendering::LevelComponent>();
	if (!m_level) {
		LevelManagerDebug.warning("Failed to create LevelComponent for loading");
		return false;
	}

	// Load level data
	if (m_level->LoadLevel(fileName)) {
		m_level->Initialize();
		LevelManagerDebug.printf("Level loaded from: %s (Name: '%s')\n",
			fileName.c_str(), m_level->GetLevelName().c_str());
		return true;
	}
	else {
		LevelManagerDebug.warning("Failed to load level from: %s", fileName.c_str());
		// Clean up failed level
		m_componentManager->ReleaseComponent(m_level);
		m_level = nullptr;
		return false;
	}
}

void LevelManager::setupInitialLevelContent()
{
	DECLARE_FUNC_VLOW();

	if (!m_level) return;

	// Add a default RenderComponent to the level
}

Rendering::GLTFRenderComponent* LevelManager::createGLTFRenderComponent(const Vector3f& position, const std::string& gltfPath)
{
	DECLARE_FUNC_LOW();
	if (!m_componentManager) {
		LevelManagerDebug.warning("Cannot create GLTF component: ComponentManager not initialized");
		return nullptr;
	}

	// Create the GLTF render component
	auto* gltfComponent = m_componentManager->CreateComponent<Rendering::GLTFRenderComponent>();
	if (!gltfComponent) {
		LevelManagerDebug.warning("Failed to create GLTFRenderComponent");
		return nullptr;
	}

	// Configure the component
	gltfComponent->setPosition(position);
	gltfComponent->setScale(Vector3f(1.0f, 1.0f, 1.0f));

	// Set GLTF path if provided
	if (!gltfPath.empty()) {
		gltfComponent->setGLTFPath(gltfPath);
	}

	// Enable rendering
	gltfComponent->setEnabledForRender(true);

	// Initialize the component
	if (!gltfComponent->Initialize()) {
		LevelManagerDebug.warning("Failed to initialize GLTFRenderComponent");
		m_componentManager->ReleaseComponent(gltfComponent);
		return nullptr;
	}

	LevelManagerDebug.printf("GLTF component created at position (%.2f, %.2f, %.2f) with path: %s\n",
		position.getX(), position.getY(), position.getZ(), gltfPath.c_str());

	return gltfComponent;
}

Rendering::RenderComponent* LevelManager::AddRenderComponentInFrontOfCamera(Viewport* activeViewport)
{
	DECLARE_FUNC_LOW();

	if (!m_level || !activeViewport) {
		LevelManagerDebug.warning("Cannot add RenderComponent: No level or active viewport");
		return nullptr;
	}

	// Get the camera from the active viewport
	Camera* camera = activeViewport->getCamera();
	if (!camera) {
		LevelManagerDebug.warning("Cannot add RenderComponent: No camera in active viewport");
		return nullptr;
	}

	// Calculate position 3 meters in front of the camera
	Vector3f cameraPosition = camera->getPosition();
	Vector3f cameraForward = camera->getForward();
	Vector3f newPosition = cameraPosition - (cameraForward * DEFAULT_SPAWN_DISTANCE);

	// Create a new RenderComponent in the level
	auto* newRenderComponent = m_level->AddRenderComponent();
	if (newRenderComponent) {
		// Set position and initialize
		newRenderComponent->setPosition(newPosition);

		// Give it a random color to distinguish it
		static int colorIndex = 0;
		Vector3f colors[] = {
			Rendering::RenderPrimitives::Red(),
			Rendering::RenderPrimitives::Green(),
			Rendering::RenderPrimitives::Blue(),
			Vector3f(1.0f, 1.0f, 0.0f), // Yellow
			Vector3f(1.0f, 0.0f, 1.0f), // Magenta
			Vector3f(0.0f, 1.0f, 1.0f), // Cyan
			Vector3f(1.0f, 0.5f, 0.0f), // Orange
			Vector3f(0.5f, 0.0f, 1.0f)  // Purple
		};

		newRenderComponent->setColor(colors[colorIndex % 8]);
		colorIndex++;

		newRenderComponent->Initialize();

		// Log success        
		LevelManagerDebug.printf("Added RenderComponent at position (%.2f, %.2f, %.2f)\n",
			newPosition.getX(), newPosition.getY(), newPosition.getZ());

		return newRenderComponent;
	}
	else {
		LevelManagerDebug.warning("Failed to create RenderComponent");
		return nullptr;
	}
}

Rendering::GLTFRenderComponent* LevelManager::AddGLTFRenderComponentInFrontOfCamera(Viewport* activeViewport, const std::string& gltfPath)
{
	DECLARE_FUNC_LOW();

	if (!m_level || !activeViewport) {
		LevelManagerDebug.warning("Cannot add GLTF component: No level or active viewport");
		return nullptr;
	}

	// Get the camera from the active viewport
	Camera* camera = activeViewport->getCamera();
	if (!camera) {
		LevelManagerDebug.warning("Cannot add GLTF component: No camera in active viewport");
		return nullptr;
	}

	// Calculate position 3 meters in front of the camera
	Vector3f cameraPosition = camera->getPosition();
	Vector3f cameraForward = camera->getForward();
	Vector3f newPosition = cameraPosition - (cameraForward * DEFAULT_SPAWN_DISTANCE);

	// Create the GLTF component as a child of the level
	auto* gltfComponent = m_level->AddRenderComponent<Rendering::GLTFRenderComponent>();
	if (gltfComponent) {
		// Configure the component
		gltfComponent->setPosition(newPosition);
		gltfComponent->setScale(Vector3f(1.0f, 1.0f, 1.0f));

		// Set GLTF path if provided
		if (!gltfPath.empty()) {
			gltfComponent->setGLTFPath(gltfPath);
		}

		// Enable rendering
		gltfComponent->setEnabledForRender(true);

		// Initialize the component
		gltfComponent->Initialize();

		LevelManagerDebug.printf("Added GLTFRenderComponent at position (%.2f, %.2f, %.2f) with path: %s\n",
			newPosition.getX(), newPosition.getY(), newPosition.getZ(), gltfPath.c_str());

		return gltfComponent;
	}
	else {
		LevelManagerDebug.warning("Failed to create GLTFRenderComponent as child of level");
		return nullptr;
	}
}

void LevelManager::RemoveAllRenderComponents()
{
	DECLARE_FUNC_LOW();

	if (!m_level) return;

	m_level->RemoveAllRenderComponents();

	// Clear the render queue after removing components
	auto* renderQueue = Rendering::RenderingSystem::GetRenderQueue();
	if (renderQueue) {
		renderQueue->Clear(); // Assuming FunctionQueue has a Clear() method
	}

	LevelManagerDebug.printf("All RenderComponents removed from level\n");
}

void LevelManager::RemoveAllGLTFRenderComponents()
{
	DECLARE_FUNC_LOW();

	if (!m_level) return;

	// Get all GLTF render components that are children of the level
	auto renderComponents = m_level->GetRenderComponents();
	for (auto* component : renderComponents) {
		// Check if this is a GLTFRenderComponent
		auto* gltfComponent = dynamic_cast<Rendering::GLTFRenderComponent*>(component);
		if (gltfComponent) {
			// Remove as child from level
			m_level->RemoveChild(gltfComponent);
		}
	}

	// Clear the render queue after removing components
	auto* renderQueue = Rendering::RenderingSystem::GetRenderQueue();
	if (renderQueue) {
		renderQueue->Clear();
	}

	LevelManagerDebug.printf("All GLTFRenderComponents removed from level\n");
}

size_t LevelManager::GetGLTFRenderComponentCount() const
{
	if (!m_level) return 0;

	size_t count = 0;
	auto renderComponents = m_level->GetRenderComponents();
	for (auto* component : renderComponents) {
		if (dynamic_cast<Rendering::GLTFRenderComponent*>(component)) {
			count++;
		}
	}
	return count;
}

void LevelManager::RenderLevelMenu(Viewport* activeViewport)
{
	DECLARE_FUNC_LOW();

	if (ImGui::BeginMenu("Level"))
	{
		if (ImGui::MenuItem("Add RenderComponent", "Ctrl+R"))
		{
			AddRenderComponentInFrontOfCamera(activeViewport);
		}

		// Add GLTF component options
		if (ImGui::BeginMenu("Add GLTF Component"))
		{
			// Input field for custom path
			static char gltfPathBuffer[512] = "assets/Datsun/datsun.gltf";
			ImGui::InputText("GLTF Path", gltfPathBuffer, sizeof(gltfPathBuffer));

			if (ImGui::Button("Add Custom GLTF"))
			{
				std::string gltfPath(gltfPathBuffer);
				if (!gltfPath.empty()) {
					AddGLTFRenderComponentInFrontOfCamera(activeViewport, gltfPath);
				}
			}

			ImGui::Separator();
			// Some common GLTF file suggestions
			if (ImGui::MenuItem("Cabin")) {
				AddGLTFRenderComponentInFrontOfCamera(activeViewport, "assets/Cabin/scene.gltf");
			}
			if (ImGui::MenuItem("Building")) {
				AddGLTFRenderComponentInFrontOfCamera(activeViewport, "assets/Building/scene.gltf");
			}
			if (ImGui::MenuItem("Datsun")) {
				AddGLTFRenderComponentInFrontOfCamera(activeViewport, "assets/Datsun/datsun.gltf");
			}
			if (ImGui::MenuItem("Sonic")) {
				AddGLTFRenderComponentInFrontOfCamera(activeViewport, "assets/Sonic/scene.gltf");
			}
			if (ImGui::MenuItem("Terrain")) {
				AddGLTFRenderComponentInFrontOfCamera(activeViewport, "assets/Terrain/scene.gltf");
			}

			ImGui::EndMenu();
		}

		ImGui::Separator();

		// Save/Load functionality
		if (ImGui::BeginMenu("File"))
		{
			// File path input
			static char savePathBuffer[512] = "levels/MyLevel.json";
			ImGui::Text("File Operations:");
			ImGui::InputText("Level Path", savePathBuffer, sizeof(savePathBuffer));
			
			// Show current level name for context
			if (m_level) {
				ImGui::Text("Current Level: %s", m_level->GetLevelName().c_str());
			}

			ImGui::Separator();

			// Save options - use enabled parameter instead of BeginDisabled/EndDisabled
			bool canSave = (m_level != nullptr);
			
			if (ImGui::MenuItem("Save Level", "Ctrl+S", false, canSave))
			{
				std::string savePath(savePathBuffer);
				
				// Auto-generate filename if path is just directory
				if (savePath.empty() || savePath.back() == '/') {
					if (savePath.empty()) savePath = "levels/";
					savePath += m_level->GetLevelName() + ".json";
				}
				// Ensure .json extension if not specified
				else if (savePath.find('.') == std::string::npos) {
					savePath += ".json";
				}
				
				if (SaveCurrentLevel(savePath)) {
					LevelManagerDebug.printf("Level saved successfully");
				}
			}

			if (ImGui::MenuItem("Save As...", nullptr, false, canSave))
			{
				// Create unique filename with timestamp
				auto now = std::chrono::system_clock::now();
				auto time_t = std::chrono::system_clock::to_time_t(now);
				
				// Use localtime_s for safer conversion (fixes the warning)
				std::tm tm_buf;
				#ifdef _WIN32
					localtime_s(&tm_buf, &time_t);
				#else
					localtime_r(&time_t, &tm_buf);
				#endif
				
				char timestampBuffer[64];
				std::strftime(timestampBuffer, sizeof(timestampBuffer), "%Y%m%d_%H%M%S", &tm_buf);
				
				std::string savePath = "levels/MyLevel.json" + m_level->GetLevelName() + "_" + timestampBuffer + ".json";
				
				if (SaveCurrentLevel(savePath)) {
					LevelManagerDebug.printf("Level saved as: %s", savePath.c_str());
				}
			}

			ImGui::Separator();

			// Load options
			if (ImGui::MenuItem("Load Level"))
			{
				std::string loadPath(savePathBuffer);
				if (!loadPath.empty()) {
					if (LoadLevel(loadPath)) {
						LevelManagerDebug.printf("Level loaded successfully");
					}
				} else {
					LevelManagerDebug.warning("Please enter a file path to load");
				}
			}

			ImGui::Separator();

			// Quick save/load options
			if (ImGui::MenuItem("Quick Save","", false, canSave)) {
				std::string quickSavePath = "levels/autosave.json";
				if (SaveCurrentLevel(quickSavePath)) {
					LevelManagerDebug.printf("Quick save completed");
				}
			}

			if (ImGui::MenuItem("Quick Load")) {
				std::string quickLoadPath = "levels/autosave.json";
				if (LoadLevel(quickLoadPath)) {
					LevelManagerDebug.printf("Quick load completed");
				}
			}

			ImGui::EndMenu();
		}

		ImGui::Separator();

		// Show level statistics
		if (m_level) {
			size_t renderComponentCount = m_level->GetRenderComponentCount();
			size_t gltfComponentCount = GetGLTFRenderComponentCount();

			ImGui::Text("Level: %s", m_level->GetLevelName().c_str());
			ImGui::Text("RenderComponents: %zu", renderComponentCount);
			ImGui::Text("GLTF Components: %zu", gltfComponentCount);

			ImGui::Separator();

			if (ImGui::MenuItem("New Level")) {
				RemoveAllRenderComponents();
				RemoveAllGLTFRenderComponents();
				LevelManagerDebug.printf("All components removed from level");
			}			
		}
		ImGui::EndMenu();
	}
}