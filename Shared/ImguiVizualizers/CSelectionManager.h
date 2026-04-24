#pragma once

#include "CSelectable.h"
#include "Bgfx3DCamera.h"
#include "BgfxGizmoRenderer.h"
#include "bx/bounds.h"
#include "imgui/imgui.h"
#include <bgfx/bgfx.h>
#include <memory>
#include <vector>

namespace ImGuiVisualizers {

/**
 * @brief Manages a set of selectable objects and performs ray-cast picking
 *        against them using view/projection information from a Bgfx3DCamera.
 *
 * Usage:
 *   1. Call SetViewInfo() each frame before picking.
 *   2. Register objects via AddSelectable() / RemoveSelectable().
 *   3. Call PickAtCursor() to update the current selection (no-op during gizmo drag).
 *   4. Call RenderSelectionGizmo() each frame to draw the manipulator and drive
 *      hover detection and drag-based transform manipulation.
 *   5. Read results with GetSelected() or GetAllSelected().
 *
 * Multi-select:
 *   - Plain click      : replaces the selection with the hit object.
 *   - Shift+click hit  : adds the hit object to the selection.
 *   - Shift+click selected object : removes it from the selection.
 *   - Plain click on empty space  : clears the selection.
 *
 * Gizmo:
 *   - Single selection  : gizmo inherits the object's full world transform.
 *   - Multi-selection   : gizmo is at the centroid, world-aligned.
 *   - Hover             : detected automatically each frame.
 *   - Drag              : left-click + drag on a highlighted axis/plane applies
 *                         the corresponding translate / scale / rotate operation
 *                         to all selected objects in real time.
 */
class CSelectionManager
{
public:
    CSelectionManager() = default;

    // ── View setup ─────────────────────────────────────────────────────

    void SetViewInfo(Bgfx3DCamera& camera,
                     const ImVec2& viewportMin,
                     const ImVec2& viewportSize);

    // ── Selectable registry ────────────────────────────────────────────

    void AddSelectable(std::shared_ptr<CSelectable> selectable);
    void RemoveSelectable(const std::shared_ptr<CSelectable>& selectable);
    void ClearSelectables();

    // ── Picking ────────────────────────────────────────────────────────

    /// Casts a ray through the current mouse position and updates the selection.
    /// Returns nullptr (and does nothing) while a gizmo drag is in progress.
    std::shared_ptr<CSelectable> PickAtCursor();

    // ── Selection state ────────────────────────────────────────────────

    std::shared_ptr<CSelectable>                     GetSelected()    const { return m_lastSelected; }
    const std::vector<std::shared_ptr<CSelectable>>& GetAllSelected() const { return m_selection; }

    bool IsSelected(const std::shared_ptr<CSelectable>& selectable) const;
    void ClearSelection();
    void SetSelected(std::shared_ptr<CSelectable> selectable);

    // ── Gizmo ──────────────────────────────────────────────────────────

    bool InitializeGizmo() { return m_gizmoRenderer.Initialize(); }
    void ShutdownGizmo()   { m_gizmoRenderer.Shutdown(); }

    /// Renders the gizmo, detects hover, and drives click-drag manipulation.
    /// Call once per frame inside your render callback.
    ///
    /// @param viewId  BGFX view to submit into.
    /// @param mode    GizmoMode::Translate, Scale, or Rotate.
    /// @param size    Normalised screen-space size (scales with camera distance).
    void RenderSelectionGizmo(bgfx::ViewId viewId,
                               GizmoMode    mode = GizmoMode::Translate,
                               float        size = 1.0f);

    /// The gizmo axis/plane the mouse is currently over. GizmoAxis::None when idle.
    GizmoAxis GetHoveredGizmoAxis() const { return m_hoveredGizmoAxis; }

    /// True while the user is actively dragging a gizmo handle.
    bool IsGizmoDragging() const { return m_drag.active; }

private:
    // ── Internal helpers ───────────────────────────────────────────────

    bx::Ray   BuildPickRay()   const;
    bool      IsMouseInViewport() const;

    void      AddToSelection(const std::shared_ptr<CSelectable>& selectable);
    void      RemoveFromSelection(const std::shared_ptr<CSelectable>& selectable);

    GizmoAxis HitTestGizmo(GizmoMode mode, const float* gizmoMtx, float effectiveSize) const;

    void      BeginGizmoDrag(GizmoMode mode, const float* gizmoMtx);
    void      ApplyGizmoDrag();
    void      ApplyTranslateDrag(const bx::Ray& ray);
    void      ApplyScaleDrag   (const bx::Ray& ray);
    void      ApplyRotateDrag  (const bx::Ray& ray);

    // ── Drag state ─────────────────────────────────────────────────────

    struct GizmoDragInfo
    {
        bool      active      = false;
        GizmoMode mode        = GizmoMode::Translate;
        GizmoAxis axis        = GizmoAxis::None;

        bx::Vec3  origin      = { 0.0f, 0.0f, 0.0f }; ///< Gizmo world-space origin at drag start.
        bx::Vec3  axisDir     = { 0.0f, 0.0f, 0.0f }; ///< Primary constrained axis (unit).
        bx::Vec3  axisDir2    = { 0.0f, 0.0f, 0.0f }; ///< Secondary axis (plane/rotate tangent).
        bx::Vec3  planeNormal = { 0.0f, 0.0f, 0.0f }; ///< Plane normal (plane translate / rotate ring).

        float     tStart      = 0.0f;                  ///< Axis parameter at drag start (translate/scale).
        bx::Vec3  hitStart    = { 0.0f, 0.0f, 0.0f }; ///< World-space hit point at drag start (plane translate).
        float     angleStart  = 0.0f;                  ///< Angle in ring plane at drag start (rotate).

        std::vector<Matrix4f> startTransforms;          ///< Per-object transform snapshots taken at drag start.
    };

    // ── Data members ───────────────────────────────────────────────────

    Bgfx3DCamera* m_camera       = nullptr;
    ImVec2        m_viewportMin  = { 0.0f, 0.0f };
    ImVec2        m_viewportSize = { 1.0f, 1.0f };

    std::vector<std::shared_ptr<CSelectable>> m_selectables;
    std::vector<std::shared_ptr<CSelectable>> m_selection;
    std::shared_ptr<CSelectable>              m_lastSelected;

    BgfxGizmoRenderer m_gizmoRenderer;
    GizmoAxis         m_hoveredGizmoAxis = GizmoAxis::None;
    GizmoDragInfo     m_drag;
};

} // namespace ImGuiVisualizers