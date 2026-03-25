#pragma once

#include "ComponentSystem/ComponentSystem.h"
#include "RenderingSystem.h"
#include "math/Vector3f.h"
#include "math/Matrix4f.h"

namespace Rendering {

    // Bounding sphere structure
    struct BoundingSphere {
        Vector3f center;
        float radius;

        BoundingSphere() : center(Vector3f::zero()), radius(0.0f) {}
        BoundingSphere(const Vector3f& centerPoint, float sphereRadius) : center(centerPoint), radius(sphereRadius) {}

        // Check if point is inside the bounding sphere
        bool contains(const Vector3f& point) const {
            return (point - center).magnitude() <= radius;
        }

        // Check if another sphere intersects with this one
        bool intersects(const BoundingSphere& other) const {
            float distance = (other.center - center).magnitude();
            return distance <= (radius + other.radius);
        }

        // Transform bounding sphere by a matrix
        BoundingSphere transform(const Matrix4f& matrix) const {
            // Transform the center point
            Vector3f transformedCenter = matrix.transformPoint(center);
            
            // Calculate the maximum scale factor to determine new radius
            // We need to check how much the matrix scales vectors
            Vector3f xAxis = matrix.transformVector(Vector3f(1, 0, 0));
            Vector3f yAxis = matrix.transformVector(Vector3f(0, 1, 0));
            Vector3f zAxis = matrix.transformVector(Vector3f(0, 0, 1));
            
            float maxScale = std::max({xAxis.magnitude(), yAxis.magnitude(), zAxis.magnitude()});
            float transformedRadius = radius * maxScale;
            
            return BoundingSphere(transformedCenter, transformedRadius);
        }

        // Get the diameter of the sphere
        float getDiameter() const {
            return radius * 2.0f;
        }

        // Check if sphere is valid (has positive radius)
        bool isValid() const {
            return radius > 0.0f;
        }
    };

    // Base render component class
    class RenderComponent : public ComponentSystem::Component {
    public:
        REFL_DECLARE_OBJECT(RenderComponent, ComponentSystem::Component);

    protected:
        // Transform properties
        Vector3f m_position;
        Vector3f m_rotation;  // Euler angles in degrees
        Vector3f m_scale;
        
        // Rendering properties
        Vector3f m_color;
        bool m_visible;
        bool m_wireframe;
        
        // Transform matrix (cached)
        mutable Matrix4f m_transformMatrix;
        mutable bool m_transformDirty;

        // Local bounding sphere (before transformation)
        BoundingSphere m_localBounds;

        // Helper method to get RenderPrimitives
        RenderPrimitives* getRenderPrimitives() const;

        // Update the cached transform matrix
        void updateTransformMatrix() const;

        // Virtual method for rendering - can be overridden by derived classes
        virtual void onRender(RenderPrimitives* renderPrimitives) const;

        // Virtual method to define local bounds - should be overridden by derived classes
        virtual BoundingSphere calculateLocalBounds() const;

    public:
        RenderComponent();
        virtual ~RenderComponent() = default;

        // Component lifecycle
        bool OnInitialize() override;
        void OnUpdate(double deltaTime) override;
        void OnShutdown() override;

        // Transform setters
        void setPosition(const Vector3f& position);
        void setRotation(const Vector3f& rotation);
        void setScale(const Vector3f& scale);
        void setTransform(const Vector3f& position, const Vector3f& rotation, const Vector3f& scale);

        // Transform getters
        const Vector3f& getPosition() const { return m_position; }
        const Vector3f& getRotation() const { return m_rotation; }
        const Vector3f& getScale() const { return m_scale; }

        // Get the complete transform matrix
        const Matrix4f& getTransformMatrix() const;

        // Rendering properties
        void setColor(const Vector3f& color) { m_color = color; }
        const Vector3f& getColor() const { return m_color; }
        
        void setVisible(bool visible) { m_visible = visible; }
        bool isVisible() const { return m_visible; }
        
        void setWireframe(bool wireframe) { m_wireframe = wireframe; }
        bool isWireframe() const { return m_wireframe; }

        // Bounding sphere methods
        BoundingSphere getLocalBounds() const { return m_localBounds; }
        BoundingSphere getWorldBounds() const;

        // Main render method - called by rendering system
        void render() const;

        // Utility methods
        void lookAt(const Vector3f& target, const Vector3f& up = Vector3f(0, 1, 0));
        
        // Transform points/vectors using this component's transform
        Vector3f transformPoint(const Vector3f& localPoint) const;
        Vector3f transformVector(const Vector3f& localVector) const;
        Vector3f inverseTransformPoint(const Vector3f& worldPoint) const;
        Vector3f inverseTransformVector(const Vector3f& worldVector) const;

        // Convenience methods for bounds testing
        bool containsPoint(const Vector3f& worldPoint) const;
        bool intersectsWith(const RenderComponent& other) const;
        float distanceTo(const RenderComponent& other) const;
    };

} // namespace Rendering