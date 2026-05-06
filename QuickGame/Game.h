#pragma once
#include "app\AppInterface.h"
#include "ImGuiVisualizerManager.h"
#include "Physics/PhysicsManager.h"
#include "GameCameraController.h"
#include "Rendering/BgfxRenderPrimitives.h"
#include "levelComponent.h"
#include "CoreSystem\functionqueue.h"

#include <memory>

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

private:
    ImGuiVisualizers::ImGuiVisualizerManager  m_visualizerManager;
    PhysicsManager                            m_physicsManager;

    // Primary game viewport camera — renders directly into bgfx view 0.
    BgfxGameCamera            m_camera;
    std::shared_ptr<GameCameraController>     m_cameraController;    
    CLevelComponent *m_RootLevel;
    Core::FunctionQueue                       m_functionQueue;
};