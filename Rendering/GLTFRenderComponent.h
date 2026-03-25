#pragma once
#include "RenderComponent.h"
#include "GLTFResource.h"
#include "GLTFRenderer.h"
#include "CoreSystem/CoreSystem.h"
#include <memory>
#include <string>

namespace Rendering {

    // GLTF Render Component - renders GLTF resources using the component system
    class GLTFRenderComponent : public RenderComponent {
    public:
        REFL_DECLARE_OBJECT(GLTFRenderComponent, RenderComponent);

    private:
        // GLTF resource and renderer
        std::shared_ptr<ResourceSystem::GLTFResource> m_gltfResource;
        std::shared_ptr<RenderEngine::GLTFRenderer> m_gltfRenderer;
        
        // Resource path
        std::string m_gltfPath;
        
        // Rendering options
        int m_sceneIndex;
        bool m_enabledForRender;
        
        // Cache for bounds calculation
        mutable bool m_boundsCacheDirty;
        mutable BoundingSphere m_cachedBounds;

        // Helper methods
        void calculateBoundsFromResource() const;
        Matrix4f getViewProjectionMatrix() const;
        ResourceSystem::ResourceManager* getResourceManager() const;

    protected:
        // Override virtual methods from RenderComponent
        void onRender(RenderPrimitives* renderPrimitives) const override;
        BoundingSphere calculateLocalBounds() const override;

    public:
        GLTFRenderComponent();
        explicit GLTFRenderComponent(const std::string& gltfPath);
        virtual ~GLTFRenderComponent();

        // Component lifecycle
        bool OnInitialize() override;
        void OnUpdate(double deltaTime) override;
        void OnShutdown() override;

        // GLTF specific methods
        void setGLTFPath(const std::string& path);
        const std::string& getGLTFPath() const { return m_gltfPath; }
        
        void setSceneIndex(int sceneIndex);
        int getSceneIndex() const { return m_sceneIndex; }
        
        void setEnabledForRender(bool enabled) { m_enabledForRender = enabled; }
        bool isEnabledForRender() const { return m_enabledForRender; }

        // Resource status methods
        bool isResourceLoaded() const;
        bool isResourceFinalized() const;
        bool isRendererReady() const;
        bool isReadyToRender() const;

        // Access to underlying resource and renderer
        std::shared_ptr<ResourceSystem::GLTFResource> getGLTFResource() const { return m_gltfResource; }
        std::shared_ptr<RenderEngine::GLTFRenderer> getGLTFRenderer() const { return m_gltfRenderer; }

        // Rendering methods (called by the rendering system)
        void renderGLTF() const;
        void renderGLTFScene(const Matrix4f& viewProjectionMatrix, int sceneIndex) const;
        void renderGLTFWithMatrix(const Matrix4f& viewProjectionMatrix, const Matrix4f& worldMatrix) const;

        // Debug and utility methods
        void printDebugInfo() const;
        size_t getMeshCount() const;
        size_t getNodeCount() const;
        size_t getMaterialCount() const;
    };

} // namespace Rendering