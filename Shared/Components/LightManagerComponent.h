#pragma once
#include "ComponentSystem/ComponentSystem.h"
#include "LightComponent.h"
#include "Math/Vector4f.h"

/**
 * @brief Singleton-style manager component that gathers all CLightComponents
 *        in the scene and provides the resolved light state to renderers.
 *
 * Place one instance at the level root (or any always-active entity).
 * Mesh components call GetActiveLightDir() / GetActiveLightColor() when
 * setting bgfx uniforms instead of using hardcoded values.
 *
 * Currently supports a single directional (sun) light; additional light
 * types can be layered in later without changing the consumer API.
 */
class CLightManagerComponent : public ComponentSystem::Component
{
public:
    REFL_DECLARE_OBJECT(CLightManagerComponent, ComponentSystem::Component);
    DECLARE_COMPONENT();

    bool OnInitialize() override;
    void OnUpdate(double deltaTime) override;
    void OnShutdown() override;

    // ── Consumer API ─────────────────────────────────────────────────────

    /// Returns the resolved directional light direction (Vec4, w=0) for bgfx.
    Vector4f GetActiveLightDir()   const { return m_resolvedLightDir; }

    /// Returns the resolved light color (Vec4) for bgfx.
    Vector4f GetActiveLightColor() const { return m_resolvedLightColor; }

    /// Returns the resolved ambient color (Vec4) for bgfx.
    Vector4f GetActiveAmbientColor() const { return m_resolvedAmbientColor; }

    /// Returns the first active CLightComponent found, or nullptr if none.
    CLightComponent* GetPrimaryLight() const { return m_primaryLight; }

    // ── Static accessor ───────────────────────────────────────────────────

    /// Returns the first active CLightManagerComponent in the scene, or nullptr.
    static CLightManagerComponent* Get();

private:
    void RefreshFromScene();

    // Cached resolved values — updated every frame from the active CLightComponents.
    Vector4f m_resolvedLightDir     = Vector4f( 0.57735f, -0.57735f, 0.57735f, 0.0f);
    Vector4f m_resolvedLightColor   = Vector4f(1.0f, 1.0f, 1.0f, 1.0f);
    Vector4f m_resolvedAmbientColor = Vector4f(0.15f, 0.15f, 0.15f, 1.0f);

    CLightComponent* m_primaryLight = nullptr;
};