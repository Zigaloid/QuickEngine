#pragma once

#include "Selectable.h"
#include "Bgfx3DCamera.h"
#include "BgfxGizmoRenderer.h"
#include "CommandHistory.h"
#include "imgui/imgui.h"
#include <bgfx/bgfx.h>
#include <functional>
#include <memory>
#include <vector>

// Include the shared math utilities (Ray + helpers)
#include "../Utils/MathUtils.h"

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
	 *   - Single selection  : gizmo inherits the object's world rotation and position (scale stripped).
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

		// ── Undo/redo integration ──────────────────────────────────────────

		/// Provide an external CCommandHistory so gizmo drags (and future
		/// operations) are automatically recorded. Pass nullptr to disable.
		void SetCommandHistory(CCommandHistory* history) { m_commandHistory = history; }

		/// Optional callback invoked at the start of a gizmo drag when the SHIFT
		/// key is held. The callback should duplicate the selected objects and
		/// update the selection to the new copies before the drag proceeds.
		using ShiftDragCallback = std::function<void()>;
		void SetShiftDragCallback(ShiftDragCallback cb) { m_shiftDragCallback = std::move(cb); }

		// ── Selectable registry ────────────────────────────────────────────

		void AddSelectable(std::shared_ptr<CSelectable> selectable);
		void RemoveSelectable(const std::shared_ptr<CSelectable>& selectable);
		void ClearSelectables();

		// ── Picking ────────────────────────────────────────────────────────

		/// Casts a ray through the current mouse position and updates the selection.
		/// Returns nullptr (and does nothing) while a gizmo drag is in progress.
		std::shared_ptr<CSelectable> PickAtCursor();

		// ── Selection state ────────────────────────────────────────────────

		/// Returns all registered selectables (read-only).
		const std::vector<std::shared_ptr<CSelectable>>& GetAllSelectables() const { return m_selectables; }

		/// Adds a selectable to the current selection if not already present.
		void AddToSelection(const std::shared_ptr<CSelectable>& selectable);

		std::shared_ptr<CSelectable>                     GetSelected()    const { return m_lastSelected; }
		const std::vector<std::shared_ptr<CSelectable>>& GetAllSelected() const { return m_selection; }

		bool IsSelected(const std::shared_ptr<CSelectable>& selectable) const;
		void ClearSelection();
		void SetSelected(std::shared_ptr<CSelectable> selectable);

		/// Replaces the entire selection with the given set of selectables.
		void SetAllSelected(const std::vector<std::shared_ptr<CSelectable>>& selectables);

		// ── Gizmo ──────────────────────────────────────────────────────────

		bool InitializeGizmo() { return m_gizmoRenderer.Initialize(); }
		void ShutdownGizmo() { m_gizmoRenderer.Shutdown(); }

		/// Renders the gizmo, detects hover, and drives click-drag manipulation.
		/// Call once per frame inside your render callback, after the scene pass.
		///
		/// @param fbh   Framebuffer the scene was rendered into (shared with the gizmo overlay view).
		/// @param mode  GizmoMode::Translate, Scale, or Rotate.
		/// @param size  Normalised screen-space size (scales with camera distance).
		void RenderSelectionGizmo(bgfx::FrameBufferHandle fbh,
			GizmoMode               mode = GizmoMode::Translate,
			float                   size = 1.0f);

		/// The gizmo axis/plane the mouse is currently over. GizmoAxis::None when idle.
		GizmoAxis GetHoveredGizmoAxis() const { return m_hoveredGizmoAxis; }

		/// True while the user is actively dragging a gizmo handle.
		bool IsGizmoDragging() const { return m_drag.active; }

		// ── Camera focus ───────────────────────────────────────────────────

		/// Moves the camera's focal point to the gizmo's world position and sets
		/// the orbit distance to twice the selection's bounding radius.
		/// No-op when there is no selection or no camera.
		void FocusCameraOnSelection();



	private:
		// ── Internal helpers ───────────────────────────────────────────────

		Ray       BuildPickRay()   const;
		bool      IsMouseInViewport() const;

		void      RemoveFromSelection(const std::shared_ptr<CSelectable>& selectable);
		void      UpdateBoxDragSelect();
		void      RenderBoxDragSelection();

		GizmoAxis HitTestGizmo(GizmoMode mode, const Matrix4f& gizmoMtx, float effectiveSize) const;

		void      BeginGizmoDrag(GizmoMode mode, const Matrix4f& gizmoMtx);
		void      EndGizmoDrag();   // ← new: commits the command
		void      ApplyGizmoDrag();
		void      ApplyTranslateDrag(const Ray& ray);
		void      ApplyScaleDrag(const Ray& ray);
		void      ApplyRotateDrag(const Ray& ray);

		// ── Data members ───────────────────────────────────────────────────

		Bgfx3DCamera* m_camera = nullptr;
		ImVec2        m_viewportMin = { 0.0f, 0.0f };
		ImVec2        m_viewportSize = { 1.0f, 1.0f };

		std::vector<std::shared_ptr<CSelectable>> m_selectables;
		std::vector<std::shared_ptr<CSelectable>> m_selection;
		std::shared_ptr<CSelectable>              m_lastSelected;

		BgfxGizmoRenderer m_gizmoRenderer;
		GizmoAxis         m_hoveredGizmoAxis = GizmoAxis::None;

		struct DragState
		{
			bool                    active = false;
			GizmoMode               mode = GizmoMode::Translate;
			GizmoAxis               axis = GizmoAxis::None;
			Vector3f                origin;
			Vector3f                axisDir;
			Vector3f                axisDir2;
			Vector3f                planeNormal;
			Vector3f                hitStart;
			float                   tStart = 0.0f;
			float                   angleStart = 0.0f;
			std::vector<Matrix4f>   startTransforms;
		};

		DragState        m_drag;
		struct BoxDragState
		{
			bool    active = false;
			ImVec2  start  = { 0.0f, 0.0f };
			ImVec2  current= { 0.0f, 0.0f };
		};
		BoxDragState      m_boxDrag;
    // True if the most recent left-mouse down occurred inside the viewport.
	bool              m_mouseDownStartedInViewport = false;
		CCommandHistory*   m_commandHistory    = nullptr;   // ← new: non-owning
		ShiftDragCallback  m_shiftDragCallback;              ///< Called on SHIFT+drag start.
	};

} // namespace ImGuiVisualizers