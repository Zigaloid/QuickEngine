#pragma once

#include "ComponentSystem/ComponentSystem.h"
#include "Math/Vector3f.h"
#include "Math/Vector4f.h"

class CTransformComponent;

/**
 * @brief Defines a directional (sun) light source.
 *
 * The light direction is derived from the forward axis (column 2) of the
 * sibling CTransformComponent when one is present. If no sibling transform
 * exists the serialized m_direction fallback is used instead.
 *
 * Color and ambient intensity are expressed as normalized [0,1] floats.
 */
class CLightComponent : public ComponentSystem::Component
{
public:
    REFL_DECLARE_OBJECT(CLightComponent, ComponentSystem::Component);
    DECLARE_COMPONENT();

    bool OnInitialize() override;
    void OnUpdate(double deltaTime) override;
    void OnShutdown() override;

    // ── Light properties ─────────────────────────────────────────────────

    /// Direction the light travels (towards the scene).
    /// Derived from the sibling CTransformComponent forward axis when available.
    const Vector3f& GetDirection() const { return m_direction; }

    /// Fallback direction used when no sibling transform exists. Normalized automatically.
    void SetDirection(const Vector3f& dir) { m_direction = dir.Normalized(); }

    /// RGB light color, each component in [0, 1].
    const Vector4f& GetColor() const { return m_color; }
    void            SetColor(const Vector4f& color) { m_color = color; }

    /// Ambient fill color, each component in [0, 1].
    const Vector4f& GetAmbientColor() const { return m_ambientColor; }
    void            SetAmbientColor(const Vector4f& color) { m_ambientColor = color; }

    /// Convenience: returns direction as a Vec4 padded to 0 in w (ready for bgfx setUniform).
    Vector4f GetDirectionVec4() const { return Vector4f(m_direction.x, m_direction.y, m_direction.z, 0.0f); }

private:
    void UpdateDirectionFromTransform();

    CTransformComponent* m_transform = nullptr;

    // Default: a sun coming from upper-right, pointing down and into scene.
    // Also used as fallback when no sibling CTransformComponent is found.
    Vector3f m_direction = Vector3f(0.57735f, -0.57735f, 0.57735f);
    Vector4f m_color = Vector4f(1.0f, 1.0f, 1.0f, 1.0f);
    Vector4f m_ambientColor = Vector4f(0.15f, 0.15f, 0.15f, 1.0f);
};