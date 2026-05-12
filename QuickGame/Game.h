#pragma once
#include "app\AppInterface.h"
#include "ImGuiVisualizerManager.h"
#include "Physics/PhysicsManager.h"
#include "GameCameraController.h"
#include "Rendering/BgfxRenderPrimitives.h"
#include "levelComponent.h"
#include "CoreSystem\functionqueue.h"

#include <memory>

class CCameraComponent;

class GameApp : public IApplication
{
public:
    bool Initialize() override;
    void Update(double deltaTime) override;
    void Render(double deltaTime) override;
    void ImguiUpdate() override;
    void ImguiMainMenu() override;
    bool Shutdown() override;
    void RegisterComponents();
    void HandleQuickEditCommand(const std::string& messageBody);

    /** Set the active camera component for rendering. 
     *  If set to nullptr, falls back to default m_camera. */
    void SetActiveCamera(CCameraComponent* camera) { m_activeCamera = camera; }

    /** Get the currently active camera component (may be nullptr). */
    CCameraComponent* GetActiveCamera() const { return m_activeCamera; }

private:
    ImGuiVisualizers::ImGuiVisualizerManager  m_visualizerManager;
    PhysicsManager                            m_physicsManager;

    // Primary game viewport camera — renders directly into bgfx view 0.
    BgfxGameCamera            m_camera;
    std::shared_ptr<GameCameraController>     m_cameraController;    

    // Active camera component (if nullptr, uses m_camera instead)
    CCameraComponent*         m_activeCamera = nullptr;

    CLevelComponent *m_RootLevel;
    Core::FunctionQueue                       m_functionQueue;
    std::string                               m_lastLevelPath;

    void LoadLevel(std::string filePath);
    void ReloadLevel();
    void RegisterConsoleCommands();
};

extern GameApp theApp;