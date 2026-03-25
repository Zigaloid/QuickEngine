#include "GLTFRenderComponent.h"
#include "RenderPrimitives.h"
#include "RenderingSystem.h"
#include "Profiler/Profiler.h"
#include "CameraComponent.h"
#include <iostream>
#include <algorithm>

namespace Rendering {

    REFL_DEFINE_OBJECT(GLTFRenderComponent)
        REFL_DEFINE_STRING_MEMBER(GLTFRenderComponent, m_gltfPath),
        REFL_DEFINE_INT_MEMBER(GLTFRenderComponent, m_sceneIndex),
        REFL_DEFINE_BOOL_MEMBER(GLTFRenderComponent, m_enabledForRender),
    REFL_DEFINE_END

    GLTFRenderComponent::GLTFRenderComponent()
        : RenderComponent()
        , m_sceneIndex(0)
        , m_enabledForRender(true)
        , m_boundsCacheDirty(true)
        , m_cachedBounds(Vector3f::zero(), 1.0f)
    {
    }

    GLTFRenderComponent::GLTFRenderComponent(const std::string& gltfPath)
        : RenderComponent()
        , m_gltfPath(gltfPath)
        , m_sceneIndex(0)
        , m_enabledForRender(true)
        , m_boundsCacheDirty(true)
        , m_cachedBounds(Vector3f::zero(), 1.0f)
    {
    }

    GLTFRenderComponent::~GLTFRenderComponent() {
        OnShutdown();
    }

    ResourceSystem::ResourceManager* GLTFRenderComponent::getResourceManager() const {
        return Core::CoreSystem::GetResourceManager();
    }

    bool GLTFRenderComponent::OnInitialize() {
        DECLARE_FUNC_MEDIUM();

        if (!RenderComponent::OnInitialize()) {
            return false;
        }

        // Check if CoreSystem is initialized
        if (!Core::CoreSystem::IsInitialized()) {
            std::cerr << "GLTFRenderComponent: CoreSystem is not initialized" << std::endl;
            return false;
        }

        auto* resourceManager = getResourceManager();
        if (!resourceManager) {
            std::cerr << "GLTFRenderComponent: ResourceManager is null" << std::endl;
            return false;
        }

        if (m_gltfPath.empty()) {
            std::cerr << "GLTFRenderComponent: GLTF path is empty" << std::endl;
            return false;
        }

        std::cout << "GLTFRenderComponent " << GetId() << ": Loading GLTF resource: " << m_gltfPath << std::endl;

        // Request the GLTF resource from the resource manager
        m_gltfResource = resourceManager->RequestResource<ResourceSystem::GLTFResource>(m_gltfPath);

        if (!m_gltfResource) {
            std::cerr << "GLTFRenderComponent: Failed to request GLTF resource: " << m_gltfPath << std::endl;
            return false;
        }

        // Set the resource manager for the GLTF resource so it can load textures
        m_gltfResource->setResourceManager(resourceManager);

        // Initialize the renderer if not already provided
        if (!m_gltfRenderer) {
            m_gltfRenderer = std::make_shared<RenderEngine::GLTFRenderer>();
            if (!m_gltfRenderer->Initialize()) {
                std::cerr << "GLTFRenderComponent: Failed to initialize GLTF renderer" << std::endl;
                return false;
            }
        }

        std::cout << "GLTFRenderComponent " << GetId() << ": Successfully initialized" << std::endl;
        return true;
    }

    void GLTFRenderComponent::OnUpdate(double deltaTime) {
        DECLARE_FUNC_MEDIUM();

        // Mark bounds cache as dirty if transform changed
        if (m_transformDirty) {
            m_boundsCacheDirty = true;
        }
		RenderingSystem::GetRenderQueue()->AddFunction([this]()
			{
				this->renderGLTF();
			}, "RenderGLTF");
    }

    void GLTFRenderComponent::OnShutdown() {
        std::cout << "GLTFRenderComponent " << GetId() << ": Shutdown" << std::endl;

        // Reset the resource reference
        m_gltfResource.reset();

        // Shutdown renderer if we own it
        if (m_gltfRenderer) {
            m_gltfRenderer->Shutdown();
            m_gltfRenderer.reset();
        }

        RenderComponent::OnShutdown();
    }

    void GLTFRenderComponent::onRender(RenderPrimitives* renderPrimitives) const {
        DECLARE_FUNC_MEDIUM();

        if (!isReadyToRender() || !renderPrimitives) {
            return;
        }

        // Get view-projection matrix from the rendering system
        Matrix4f viewProjectionMatrix = getViewProjectionMatrix();
        
        // Get our world transform matrix
        const Matrix4f& worldMatrix = getTransformMatrix();

        // Render the GLTF resource
        renderGLTFWithMatrix(viewProjectionMatrix, worldMatrix);
    }

    BoundingSphere GLTFRenderComponent::calculateLocalBounds() const {
        if (!m_boundsCacheDirty && m_cachedBounds.radius > 0.0f) {
            return m_cachedBounds;
        }

        calculateBoundsFromResource();
        return m_cachedBounds;
    }

    void GLTFRenderComponent::calculateBoundsFromResource() const {
        if (!m_gltfResource || !m_gltfResource->IsFinalized()) {
            // Use default bounds if resource not available
            m_cachedBounds = BoundingSphere(Vector3f::zero(), 1.0f);
            m_boundsCacheDirty = false;
            return;
        }

        // Use the convex hull from the resource to calculate bounds
        const auto* convexHull = m_gltfResource->getResourceConvexHull();
        if (convexHull && convexHull->sphereRadius > 0.0f) {
            m_cachedBounds = BoundingSphere(convexHull->sphereCenter, convexHull->sphereRadius);
        } else {
            // Fall back to calculating from vertices if convex hull not available
            const auto& meshes = m_gltfResource->getMeshes();
            if (!meshes.empty()) {
                Vector3f minBounds(FLT_MAX, FLT_MAX, FLT_MAX);
                Vector3f maxBounds(-FLT_MAX, -FLT_MAX, -FLT_MAX);

                for (const auto& mesh : meshes) {
                    auto vertices = mesh.getVertices();
                    for (size_t i = 0; i < vertices.size(); i += 3) {
                        Vector3f vertex(vertices[i], vertices[i + 1], vertices[i + 2]);
                        minBounds = Vector3f(
                            std::min(minBounds.getX(), vertex.getX()),
                            std::min(minBounds.getY(), vertex.getY()),
                            std::min(minBounds.getZ(), vertex.getZ())
                        );
                        maxBounds = Vector3f(
                            std::max(maxBounds.getX(), vertex.getX()),
                            std::max(maxBounds.getY(), vertex.getY()),
                            std::max(maxBounds.getZ(), vertex.getZ())
                        );
                    }
                }

                Vector3f center = (minBounds + maxBounds) * 0.5f;
                Vector3f extents = (maxBounds - minBounds) * 0.5f;
                float radius = extents.magnitude();
                
                m_cachedBounds = BoundingSphere(center, radius);
            } else {
                m_cachedBounds = BoundingSphere(Vector3f::zero(), 1.0f);
            }
        }

        m_boundsCacheDirty = false;
    }

    Matrix4f GLTFRenderComponent::getViewProjectionMatrix() const {
        // Check if RenderingSystem is initialized
        if (!RenderingSystem::IsInitialized()) {
            std::cerr << "GLTFRenderComponent: RenderingSystem is not initialized" << std::endl;
            Matrix4f identity;
            identity.identity();
            return identity;
        }

        // Get the currently rendering viewport from RenderingSystem
        Viewport* currentViewport = RenderingSystem::GetCurrentlyRenderingViewport();
        if (!currentViewport) {
            std::cerr << "GLTFRenderComponent: No currently rendering viewport available" << std::endl;
            Matrix4f identity;
            identity.identity();
            return identity;
        }

        // Get the camera component from the viewport
        ComponentSystem::CameraComponent* cameraComponent = currentViewport->getCameraComponent();
        if (!cameraComponent) {
            std::cerr << "GLTFRenderComponent: No camera component found in current viewport" << std::endl;
            Matrix4f identity;
            identity.identity();
            return identity;
        }

        // Get the view-projection matrix from the camera component
        // Assuming CameraComponent has a method to get the view-projection matrix
        // If not, we can combine view and projection matrices
		Matrix4f viewMatrix = cameraComponent->GetCamera()->getViewMatrix();
		Matrix4f projectionMatrix = cameraComponent->GetCamera()->getProjectionMatrix();
        
        return projectionMatrix * viewMatrix;
    }

    void GLTFRenderComponent::setGLTFPath(const std::string& path) {
        if (m_gltfPath != path) {
            m_gltfPath = path;
            m_boundsCacheDirty = true;
            
            // If component is already initialized, reload the resource
            if (IsInitialized()) {
                auto* resourceManager = getResourceManager();
                if (resourceManager) {
                    m_gltfResource = resourceManager->RequestResource<ResourceSystem::GLTFResource>(m_gltfPath);
                    if (m_gltfResource) {
                        m_gltfResource->setResourceManager(resourceManager);
                    }
                }
            }
        }
    }

    void GLTFRenderComponent::setSceneIndex(int sceneIndex) {
        m_sceneIndex = std::max(0, sceneIndex);
    }

    bool GLTFRenderComponent::isResourceLoaded() const {
        return m_gltfResource && m_gltfResource->IsLoaded();
    }

    bool GLTFRenderComponent::isResourceFinalized() const {
        return m_gltfResource && m_gltfResource->IsFinalized();
    }

    bool GLTFRenderComponent::isRendererReady() const {
        return m_gltfRenderer && m_gltfRenderer->IsReady();
    }

    bool GLTFRenderComponent::isReadyToRender() const {
        return m_enabledForRender && 
               isResourceFinalized() && 
               isRendererReady() && 
               isVisible() && 
               IsActiveInHierarchy();
    }

    void GLTFRenderComponent::renderGLTF() const {
        if (!isReadyToRender()) {
            return;
        }

		const Matrix4f& viewProjectionMatrix = RenderingSystem::GetCurrentlyRenderingViewport()->getCamera()->getViewProjectionMatrix();
        // Use the component's transform as the model matrix
        const Matrix4f& modelMatrix = getTransformMatrix();
        m_gltfRenderer->Render(*m_gltfResource, viewProjectionMatrix, modelMatrix);
    }

    void GLTFRenderComponent::renderGLTFScene(const Matrix4f& viewProjectionMatrix, int sceneIndex) const {
        if (!isReadyToRender()) {
            return;
        }

        const Matrix4f& modelMatrix = getTransformMatrix();
        m_gltfRenderer->RenderScene(*m_gltfResource, sceneIndex, viewProjectionMatrix, modelMatrix);
    }

    void GLTFRenderComponent::renderGLTFWithMatrix(const Matrix4f& viewProjectionMatrix, const Matrix4f& worldMatrix) const {
        if (!isReadyToRender()) {
            return;
        }

        // Combine the world matrix with our local transform
        Matrix4f finalModelMatrix = worldMatrix * getTransformMatrix();
        m_gltfRenderer->Render(*m_gltfResource, viewProjectionMatrix, finalModelMatrix);
    }

    void GLTFRenderComponent::printDebugInfo() const {
        std::cout << "---------------------------------------------------------------" << std::endl;
        std::cout << "GLTFRenderComponent " << GetId() << " Debug Info:" << std::endl;
        std::cout << "  Path: " << m_gltfPath << std::endl;
        std::cout << "  Scene Index: " << m_sceneIndex << std::endl;
        std::cout << "  Enabled for Render: " << m_enabledForRender << std::endl;
        std::cout << "  Visible: " << isVisible() << std::endl;
        std::cout << "  Active in Hierarchy: " << IsActiveInHierarchy() << std::endl;
        std::cout << "  CoreSystem Initialized: " << Core::CoreSystem::IsInitialized() << std::endl;
        std::cout << "  RenderingSystem Initialized: " << RenderingSystem::IsInitialized() << std::endl;
        
        auto* resourceManager = getResourceManager();
        std::cout << "  Resource Manager: " << (resourceManager ? "Valid" : "Null") << std::endl;
        std::cout << "  Resource: " << (m_gltfResource ? "Loaded" : "Null") << std::endl;
        std::cout << "  Renderer: " << (m_gltfRenderer ? "Available" : "Null") << std::endl;

        // Display viewport and camera information
        Viewport* currentViewport = RenderingSystem::GetCurrentlyRenderingViewport();
        std::cout << "  Current Viewport: " << (currentViewport ? "Available" : "Null") << std::endl;
        if (currentViewport) {
            ComponentSystem::CameraComponent* camera = currentViewport->getCameraComponent();
            std::cout << "  Camera Component: " << (camera ? "Available" : "Null") << std::endl;
        }

        if (m_gltfResource) {
            std::cout << "  Resource Status:" << std::endl;
            std::cout << "    Initialized: " << m_gltfResource->IsInitialized() << std::endl;
            std::cout << "    Loaded: " << m_gltfResource->IsLoaded() << std::endl;
            std::cout << "    Finalized: " << m_gltfResource->IsFinalized() << std::endl;

            if (m_gltfResource->IsFinalized()) {
                std::cout << "    Meshes: " << getMeshCount() << std::endl;
                std::cout << "    Nodes: " << getNodeCount() << std::endl;
                std::cout << "    Materials: " << getMaterialCount() << std::endl;
            }
        }

        if (m_gltfRenderer) {
            std::cout << "  Renderer Status:" << std::endl;
            std::cout << "    Ready: " << m_gltfRenderer->IsReady() << std::endl;
        }

        std::cout << "  Ready to Render: " << isReadyToRender() << std::endl;
        
        // Print bounds information
        BoundingSphere bounds = getWorldBounds();
        std::cout << "  World Bounds: Center(" << bounds.center.getX() << ", " 
                  << bounds.center.getY() << ", " << bounds.center.getZ() 
                  << ") Radius(" << bounds.radius << ")" << std::endl;
        
        std::cout << "---------------------------------------------------------------" << std::endl;
    }

    size_t GLTFRenderComponent::getMeshCount() const {
        if (m_gltfResource && m_gltfResource->IsFinalized()) {
            return m_gltfResource->getMeshes().size();
        }
        return 0;
    }

    size_t GLTFRenderComponent::getNodeCount() const {
        if (m_gltfResource && m_gltfResource->IsFinalized()) {
            return m_gltfResource->getNodes().size();
        }
        return 0;
    }

    size_t GLTFRenderComponent::getMaterialCount() const {
        if (m_gltfResource && m_gltfResource->IsFinalized()) {
            return m_gltfResource->getMaterials().size();
        }
        return 0;
    }

} // namespace Rendering