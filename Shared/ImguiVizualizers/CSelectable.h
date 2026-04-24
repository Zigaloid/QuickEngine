#pragma once

#include "Math/Matrix4f.h"
#include "Math/Vector4f.h"
#include <memory>

namespace ImGuiVisualizers {

/**
 * @brief Represents any object that can participate in the generalized
 *        selection system managed by CSelectionManager.
 *
 * Holds shared pointers to a world-space transform (Matrix4f) and a
 * bounding sphere (Vector4f: xyz = local centre, w = local radius).
 * When either pointer is null the corresponding accessor returns a
 * safe default (identity matrix / zero vector).
 */
class CSelectable
{
public:
    CSelectable() = default;

    explicit CSelectable(std::shared_ptr<Matrix4f> transform,
                         std::shared_ptr<Vector4f> boundingSphere)
        : m_Transform(std::move(transform))
        , m_BoundingSphere(std::move(boundingSphere))
    {}

    virtual ~CSelectable() = default;

    // --- Accessors ----------------------------------------------------

    /// Returns the column-major world-space transform of the object (read-only).
    /// Falls back to an identity matrix when the pointer is null.
    const Matrix4f& GetTransform() const
    {
        if (m_Transform)
            return *m_Transform;

        static const Matrix4f s_Identity = Matrix4f::GetIdentity();
        return s_Identity;
    }

    /// Returns a raw mutable pointer to the shared transform matrix, or nullptr
    /// if no transform has been set. Use this to apply in-place modifications
    /// (e.g. from a gizmo drag) so all shared owners see the change immediately.
    Matrix4f* GetTransformMutable() { return m_Transform.get(); }

    /// Returns the bounding sphere in local space (xyz = centre, w = radius).
    /// Falls back to a zero vector when the pointer is null.
    Vector4f GetBoundingSphere() const
    {
        if (m_BoundingSphere)
            return *m_BoundingSphere;

        return Vector4f(0.0f, 0.0f, 0.0f, 0.0f);
    }

    // --- Mutators -----------------------------------------------------

    void SetTransform(std::shared_ptr<Matrix4f> transform)
    {
        m_Transform = std::move(transform);
    }

    void SetBoundingSphere(std::shared_ptr<Vector4f> boundingSphere)
    {
        m_BoundingSphere = std::move(boundingSphere);
    }

    // --- Pointer access -----------------------------------------------

    const std::shared_ptr<Matrix4f>&  GetTransformPtr()      const { return m_Transform; }
    const std::shared_ptr<Vector4f>&  GetBoundingSpherePtr() const { return m_BoundingSphere; }

private:
    std::shared_ptr<Matrix4f>  m_Transform;
    std::shared_ptr<Vector4f>  m_BoundingSphere;
};

} // namespace ImGuiVisualizers