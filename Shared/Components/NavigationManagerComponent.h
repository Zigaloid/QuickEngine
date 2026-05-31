#pragma once

#include "ComponentSystem/ComponentSystem.h"
#include "Math/Vector3f.h"
#include "LevelComponent.h"
#include <DetourNavMesh.h>
#include <DetourNavMeshQuery.h>

#include <vector>


/**
 * @brief Manages navigation queries against the active level's NavMesh.
 *
 * Attach as a sibling or ancestor of a CLevelComponent. On initialization
 * the component searches its own hierarchy (descendants first, then parent
 * hierarchy) for the first CLevelComponent and caches a pointer to it.
 *
 * A dtNavMeshQuery object is allocated once and reused for all queries.
 * All query methods are safe to call every frame; they return false /
 * empty results when the NavMesh is not yet available.
 *
 * ### Example
 * @code
 *  auto* nav = rootComponent->CreateChild<CNavigationManagerComponent>();
 *
 *  std::vector<Vector3f> path;
 *  if (nav->FindPath(startPos, endPos, path))
 *  {
 *      // follow path...
 *  }
 * @endcode
 */
class CNavigationManagerComponent : public ComponentSystem::Component
{
public:
    REFL_DECLARE_OBJECT(CNavigationManagerComponent, Component);
    DECLARE_COMPONENT();

    CNavigationManagerComponent() = default;
    ~CNavigationManagerComponent() override;

    // ── IComponent lifecycle ────────────────────────────────────────────

    bool OnInitialize() override;
    void OnUpdate(double deltaTime) override;
    void OnShutdown() override;

    // ── Configuration ───────────────────────────────────────────────────

    /** Maximum number of polygons considered during a single pathfinding query. */
    void SetMaxQueryNodes(int maxNodes) { m_maxQueryNodes = maxNodes; }
    int  GetMaxQueryNodes() const       { return m_maxQueryNodes; }

    /** Half-extents used when snapping world positions to the nearest poly. */
    void       SetSearchExtents(const Vector3f& extents) { m_searchExtents = extents; }
    Vector3f   GetSearchExtents() const                  { return m_searchExtents; }

    // ── Queries ─────────────────────────────────────────────────────────

    /**
     * @brief Finds a walkable path between two world-space positions.
     * @param start   World-space start position.
     * @param end     World-space end position.
     * @param outPath Receives the sequence of waypoints (including start and end).
     * @return true if a path (even partial) was found.
     */
    bool FindPath(const Vector3f& start, const Vector3f& end,
                  std::vector<Vector3f>& outPath) const;

    /**
     * @brief Snaps a world-space position to the nearest point on the NavMesh.
     * @param position  World-space position to project.
     * @param outNearest Receives the nearest valid NavMesh position.
     * @return true if a nearby polygon was found.
     */
    bool FindNearestPoint(const Vector3f& position, Vector3f& outNearest) const;

    /**
     * @brief Returns true if the NavMesh is loaded and the query object is ready.
     */
    bool IsReady() const;

    // ── Level reference ─────────────────────────────────────────────────

    /** Returns the cached CLevelComponent, or nullptr if not yet resolved. */
    CLevelComponent* GetLevelComponent() const { return m_levelComponent; }

private:
    // ── Helpers ─────────────────────────────────────────────────────────

    /** Attempts to locate a CLevelComponent in the hierarchy and rebuild the
     *  dtNavMeshQuery if the NavMesh has changed since the last update. */
    void RefreshNavMeshQuery();

    const dtNavMesh* GetNavMesh() const;

    // ── State ────────────────────────────────────────────────────────────

    CLevelComponent*  m_levelComponent  = nullptr;
    dtNavMeshQuery*   m_navMeshQuery    = nullptr;

    /// NavMesh pointer we last built the query against – used to detect changes.
    const dtNavMesh*  m_lastNavMesh     = nullptr;

    int      m_maxQueryNodes = 2048;
    Vector3f m_searchExtents = { 2.0f, 4.0f, 2.0f };

    static constexpr int k_maxPathPolys = 256;
};