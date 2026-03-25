#include "RenderComponent.h"
#include "Profiler/Profiler.h"
#include "RenderPrimitives.h"

#include <cmath>

namespace Rendering {

REFL_DEFINE_OBJECT(RenderComponent)
    REFL_DEFINE_VECTOR3_MEMBER(RenderComponent, m_position),
    REFL_DEFINE_VECTOR3_MEMBER(RenderComponent, m_rotation),
    REFL_DEFINE_VECTOR3_MEMBER(RenderComponent, m_scale),
    REFL_DEFINE_VECTOR3_MEMBER(RenderComponent, m_color),
    REFL_DEFINE_BOOL_MEMBER(RenderComponent, m_visible),
	REFL_DEFINE_BOOL_MEMBER(RenderComponent, m_wireframe),
REFL_DEFINE_END

    RenderComponent::RenderComponent() 
        : m_position(Vector3f::zero())
        , m_rotation(Vector3f::zero())
        , m_scale(Vector3f::one())
        , m_color(Vector3f::one())
        , m_visible(true)
        , m_wireframe(false)
        , m_transformDirty(true)
        , m_localBounds(Vector3f::zero(), 0.866f) // Default unit cube has sphere radius of sqrt(3)/2 ≈ 0.866
    {
        m_transformMatrix.identity();
    }

    RenderPrimitives* RenderComponent::getRenderPrimitives() const {
        return RenderingSystem::GetRenderPrimitives();
    }

    bool RenderComponent::OnInitialize() {
        // Update local bounds when component initializes
        m_transformDirty = true;
        m_localBounds = calculateLocalBounds();
        return true;
    }

    void RenderComponent::OnUpdate(double deltaTime) {
        // Base implementation does nothing
        // Derived classes can override for animation, etc.
        updateTransformMatrix();
        RenderingSystem::GetRenderQueue()->AddFunction([this]() 
        {
            this->render();
		}, "RenderComponentRender");
    }

    void RenderComponent::OnShutdown() {
        // Base implementation does nothing
    }

    void RenderComponent::setPosition(const Vector3f& position) {
        m_position = position;
        m_transformDirty = true;
    }

    void RenderComponent::setRotation(const Vector3f& rotation) {
        m_rotation = rotation;
        m_transformDirty = true;
    }

    void RenderComponent::setScale(const Vector3f& scale) {
        m_scale = scale;
        m_transformDirty = true;
    }

    void RenderComponent::setTransform(const Vector3f& position, const Vector3f& rotation, const Vector3f& scale) {
        m_position = position;
        m_rotation = rotation;
        m_scale = scale;
        m_transformDirty = true;
    }

    void RenderComponent::updateTransformMatrix() const {
        if (!m_transformDirty) return;

        m_transformMatrix.identity();
		
        // Apply scale first
		m_transformMatrix = Matrix4f::scale(m_scale.getX(), m_scale.getY(), m_scale.getZ()) * m_transformMatrix;

        // Apply rotations (Z * Y * X order for Euler angles)
        if (m_rotation.getX() != 0.0f) {
            float radX = m_rotation.getX() * (float)M_PI / 180.0f;
            m_transformMatrix = Matrix4f::rotationX(radX) * m_transformMatrix;
        }
        if (m_rotation.getY() != 0.0f) {
            float radY = m_rotation.getY() * (float)M_PI / 180.0f;
            m_transformMatrix = Matrix4f::rotationY(radY) * m_transformMatrix;
        }
        if (m_rotation.getZ() != 0.0f) {
            float radZ = m_rotation.getZ() * (float)M_PI / 180.0f;
            m_transformMatrix = Matrix4f::rotationZ(radZ) * m_transformMatrix;
        }

        // Apply translation last
        m_transformMatrix = Matrix4f::translation(m_position.getX(), m_position.getY(), m_position.getZ()) * m_transformMatrix;


        m_transformDirty = false;
    }

    const Matrix4f& RenderComponent::getTransformMatrix() const {
        updateTransformMatrix();
        return m_transformMatrix;
    }

    BoundingSphere RenderComponent::calculateLocalBounds() const {
        // Default implementation returns a sphere that encompasses a unit cube
        // For a cube from -0.5 to 0.5, the sphere radius is sqrt(3)/2 ≈ 0.866
        return BoundingSphere(Vector3f::zero(), 0.866f);
    }

    BoundingSphere RenderComponent::getWorldBounds() const {
        return m_localBounds.transform(getTransformMatrix());
    }

    void RenderComponent::onRender(RenderPrimitives* renderPrimitives) const {
        if (!renderPrimitives) return;

        // Default implementation renders a unit cube
        Vector3f size(1.0f, 1.0f, 1.0f);
        Vector3f center = m_localBounds.center;

        if (m_wireframe) {
            renderPrimitives->RenderWireBox(center, size, m_color, Vector3f::zero());
        } else {
            renderPrimitives->RenderBox(center, size, m_color, Vector3f::zero());
        }
    }

    void RenderComponent::render() const 
    {
        DECLARE_FUNC_MEDIUM();
		// SOLUTION: Add safety check at the beginning of render
		if (!IsInitialized() || !m_visible || !IsActiveInHierarchy()) {
			return;
		}

        RenderPrimitives* renderPrimitives = getRenderPrimitives();
        if (!renderPrimitives) return;

        // Set up transform using RenderPrimitives matrix stack
        renderPrimitives->PushMatrix();
        renderPrimitives->SetTransform(m_position, m_rotation, m_scale);

        // Call the virtual render method
        onRender(renderPrimitives);

        // Restore matrix stack
        renderPrimitives->PopMatrix();
    }

    void RenderComponent::lookAt(const Vector3f& target, const Vector3f& up) {
        Vector3f forward = (target - m_position).normalized();
        Vector3f right = forward.cross(up).normalized();
        Vector3f realUp = right.cross(forward).normalized();

        // Convert to Euler angles (simplified approach)
        // This assumes Y-up coordinate system
        float yaw = std::atan2(forward.getX(), forward.getZ()) * 180.0f / (float)M_PI;
        float pitch = std::asin(-forward.getY()) * 180.0f / (float)M_PI;

        setRotation(Vector3f(pitch, yaw, 0.0f));
    }

    Vector3f RenderComponent::transformPoint(const Vector3f& localPoint) const {
        return getTransformMatrix().transformPoint(localPoint);
    }

    Vector3f RenderComponent::transformVector(const Vector3f& localVector) const {
        return getTransformMatrix().transformVector(localVector);
    }

    Vector3f RenderComponent::inverseTransformPoint(const Vector3f& worldPoint) const {
        return getTransformMatrix().inverse().transformPoint(worldPoint);
    }

    Vector3f RenderComponent::inverseTransformVector(const Vector3f& worldVector) const {
        return getTransformMatrix().inverse().transformVector(worldVector);
    }

    bool RenderComponent::containsPoint(const Vector3f& worldPoint) const {
        return getWorldBounds().contains(worldPoint);
    }

    bool RenderComponent::intersectsWith(const RenderComponent& other) const {
        return getWorldBounds().intersects(other.getWorldBounds());
    }

    float RenderComponent::distanceTo(const RenderComponent& other) const {
        BoundingSphere myBounds = getWorldBounds();
        BoundingSphere otherBounds = other.getWorldBounds();
        
        float centerDistance = (otherBounds.center - myBounds.center).magnitude();
        float surfaceDistance = centerDistance - myBounds.radius - otherBounds.radius;
        
        return std::max(0.0f, surfaceDistance); // Return 0 if spheres are overlapping
    }

} // namespace Rendering