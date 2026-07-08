#pragma once
#include "ComponentSystem/ComponentSystem.h"
#include "ResourceManager/ResourceManager.h"
#include "TextureResource.h"
#include "Math/Matrix4f.h"
#include "Math/Vector4f.h"
#include "TransformComponent.h"
#include "Rendering/BgfxUIView.h"
#include "MeshComponent.h"

#include <bgfx/bgfx.h>

/**
 * @brief Vertex layout for UI quads: position (NDC/depth-driving Z) + texcoord.
 */
struct UIElementVertex
{
    float    x, y, z;
    float    u, v;

    static void Init()
    {
        ms_layout
            .begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .end();
    }

    static bgfx::VertexLayout ms_layout;
};

/**
 * @brief Base render component for 2D UI elements drawn into the dedicated
 *        UI bgfx view (see Rendering::BgfxUIView).
 *
 * The UI view uses identity view and identity projection matrices, so the
 * component's model matrix alone carries local-space vertices into clip-space
 * NDC. The Z coordinate of the transformed position is written to the depth
 * buffer, so multiple overlapping UI elements resolve draw order via depth
 * testing rather than submission order.
 *
 * Derived classes override Render() to bind their own textures/uniforms and
 * then call SubmitQuad() to draw the shared unit quad with the component's
 * model matrix.
 *
 * The shared shader program, sampler uniform, tint uniform, and unit quad
 * vertex/index buffers are owned statically by this class and must be
 * initialised once via InitializeUIRendering() (after bgfx::init) and
 * released via ShutdownUIRendering() (before bgfx::shutdown).
 */
class CUIElementComponent : public CRenderComponent
{
public:
    REFL_DECLARE_OBJECT(CUIElementComponent, CRenderComponent);
    DECLARE_COMPONENT();

    CUIElementComponent() = default;
    ~CUIElementComponent() override = default;

    // ── IComponent lifecycle ────────────────────────────────────────────

    bool OnInitialize() override;
    void OnUpdate(double deltaTime) override;
    void OnShutdown() override;

    // ── Public API ──────────────────────────────────────────────────────

    /** @brief Submit the shared unit quad into the UI view using the
     *         component's model matrix. Derive state/texture bindings before
     *         calling this from a subclass Render() override. */
    void Render(bgfx::ViewId viewId) override;

    /** @brief Multiplicative tint applied to the bound texture (RGBA,
     *         default opaque white). */
    const Vector4f& GetTint() const { return m_tint; }
    void SetTint(const Vector4f& tint) { m_tint = tint; }

    // ── Shared UI rendering init/shutdown ──────────────────────────────

    /** @brief Create the shared UI shader program, sampler/tint uniforms and
     *         the unit quad vertex/index buffers. Call once after bgfx::init().
     * Idempotent; returns true if shaders and geometry were created
     * successfully (or were already initialised). */
    static bool InitializeUIRendering();

    /** @brief Release the shared UI GPU resources. Call before bgfx::shutdown().
     *  Safe to call multiple times. */
    static void ShutdownUIRendering();

    static bool IsUIRenderingInitialised() { return s_uiInitialised; }

protected:
    /** @brief Bind uniforms/state shared by all UI elements (tint, transform)
     *         and submit the unit quad with the shared program.
     *  Subclasses call this after binding their own texture(s) to slot 0. */
    void SubmitQuad(bgfx::ViewId viewId);

    Vector4f m_tint = { 1.0f, 1.0f, 1.0f, 1.0f };

    // ── Shared static state ────────────────────────────────────────────
    static bgfx::ProgramHandle   s_uiProgram;
    static bgfx::UniformHandle  s_tintUniform;
    static bgfx::UniformHandle  s_texSampler;
    static bgfx::VertexBufferHandle s_quadVbh;
    static bgfx::IndexBufferHandle  s_quadIbh;
    static bool                 s_uiInitialised;
};

/**
 * @brief Renders a textured quad as a UI element. The texture is supplied
 *        through a CTextureResourceReference and bound to sampler slot 0 on
 *        the shared UI shader program. The component's model matrix (sibling
 *        CTransformComponent via CRenderComponent::GetModelMatrix()) drives
 *        the on-screen position, rotation, scale and depth of the quad.
 */
class CUIImageComponent : public CUIElementComponent
{
public:
    REFL_DECLARE_OBJECT(CUIImageComponent, CUIElementComponent);
    DECLARE_COMPONENT();

    CUIImageComponent() = default;
    ~CUIImageComponent() override = default;

    // ── IComponent lifecycle ────────────────────────────────────────────

    bool OnInitialize() override;
    void OnShutdown() override;

    // ── Public API ──────────────────────────────────────────────────────

    void Render(bgfx::ViewId viewId) override;

    bool IsLoaded() const;

    CTextureResourceReference&       GetTextureResource()       { return m_textureResource; }
    const CTextureResourceReference& GetTextureResource() const { return m_textureResource; }

    void SetTextureResourceFileName(const std::string& fileName)
    {
        m_textureResource.SetResourceFileName(fileName);
    }

private:
    CTextureResourceReference m_textureResource;
};