#include "LevelComponent.h"

namespace Rendering {

    REFL_DEFINE_OBJECT(LevelComponent)
        REFL_DEFINE_STRING_MEMBER(LevelComponent, m_levelName),
    REFL_DEFINE_END

    LevelComponent::LevelComponent()
        : m_levelName("Untitled Level") {
    }

    bool LevelComponent::OnInitialize() {
        return true;
    }

    void LevelComponent::OnUpdate(double deltaTime) {
        // Basic update - ComponentManager handles individual component updates
    }

    void LevelComponent::OnShutdown() {
        RemoveAllRenderComponents();
    }

    RenderComponent* LevelComponent::AddRenderComponent() {
        return CreateChild<RenderComponent>();
    }

    std::vector<RenderComponent*> LevelComponent::GetRenderComponents() const {
        return FindChildren<RenderComponent>();
    }

    size_t LevelComponent::GetRenderComponentCount() const {
        return FindChildren<RenderComponent>().size();
    }

    void LevelComponent::RemoveAllRenderComponents() {
        auto renderComponents = GetRenderComponents();
        for (auto* renderComponent : renderComponents) {
            if (renderComponent) {
                RemoveChild(renderComponent);
            }
        }
    }

    // Add serialization support methods
    bool LevelComponent::SaveLevel(const std::string& fileName) {
        auto result = SafeWrite(fileName);
        if (result.IsSuccess()) {
            return result.GetValue();
        } else {
            const auto& error = result.GetError();
            // Log error - you can use your debug system here
            return false;
        }
    }

    bool LevelComponent::LoadLevel(const std::string& fileName) {
        auto result = SafeRead(fileName);
        if (result.IsSuccess()) {
            return result.GetValue();
        } else {
            const auto& error = result.GetError();
            // Log error - you can use your debug system here
            return false;
        }
    }

} // namespace Rendering