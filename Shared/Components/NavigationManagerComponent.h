#pragma once

#include "ComponentSystem/ComponentSystem.h"
#include "Math/Vector3f.h"
#include "LevelComponent.h"
#include "NavQuery.h"
#include <DetourNavMesh.h>
#include <DetourNavMeshQuery.h>

#include <vector>
#include <memory>
#include <mutex>


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

    // ── Async query registration ────────────────────────────────────────

    /**
     * @brief Register a query to be serviced asynchronously each update.
     *  The manager holds a weak_ptr so it never prolongs the lifetime of the
     *  owning entity.  Safe to call from the main thread at any time.
     */
    void RegisterQuery(std::shared_ptr<CNavQuery> query);

    /**
     * @brief Remove a previously registered query.
     *  After this call the manager will no longer service it.
     */
    void UnregisterQuery(const std::shared_ptr<CNavQuery>& query);

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

    /**
     * @brief Debug render the active NavMesh into the given BGFX view.
     *
     * Renders the triangulated detail mesh as wireframe lines. Safe to call
     * even when the navmesh is not available.
     */
    void DebugRender(bgfx::ViewId viewId) const;

    // ── Level reference ─────────────────────────────────────────────────

    /** Returns the cached CLevelComponent, or nullptr if not yet resolved. */
    CLevelComponent* GetLevelComponent() const { return m_levelComponent.Get(this); }

private:
    // ── Helpers ─────────────────────────────────────────────────────────

    /** Attempts to locate a CLevelComponent in the hierarchy and rebuild the
     *  dtNavMeshQuery if the NavMesh has changed since the last update. */
    void RefreshNavMeshQuery();

    /** Iterates registered queries, submitting any dirty ones to the JobSystem. */
    void DispatchPendingQueries();

    const dtNavMesh* GetNavMesh() const;

    // ── State ────────────────────────────────────────────────────────────
    
    dtNavMeshQuery*   m_navMeshQuery    = nullptr;

    /// NavMesh pointer we last built the query against – used to detect changes.
    const dtNavMesh*  m_lastNavMesh     = nullptr;

    int      m_maxQueryNodes = 2048;
    Vector3f m_searchExtents = { 2.0f, 4.0f, 2.0f };

    /// Registered async queries. Stored as weak_ptr so the manager never
    /// keeps an entity alive.  Access is guarded by m_queriesMutex to allow
    /// RegisterQuery / UnregisterQuery to be called from any thread.
    mutable std::mutex                        m_queriesMutex;
    std::vector<std::weak_ptr<CNavQuery>>     m_queries;
    ComponentSystem::CComponentReference<CLevelComponent> m_levelComponent{ ComponentSystem::FIRST_IN_HIERARCHY };
    static constexpr int k_maxPathPolys = 256;
};