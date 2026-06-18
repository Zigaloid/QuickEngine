#include "NavigationManagerComponent.h"
#include "CoreSystem/CoreSystem.h"
#include "ComponentSystem/ComponentSystem.h"
#include "JobSystem/JobSystem.h"
#include "CoreSystem/FunctionCallManager.h"
#include <DetourNavMesh.h>
#include <DetourNavMeshQuery.h>

#include "Rendering/BgfxRenderPrimitives.h"
#include <bgfx/bgfx.h>

#include <algorithm>
#include <cstring>

CONSOLE_VARIABLE_BOOL(g_debugRenderNavMesh, false);

// ── Reflection ─────────────────────────────────────────────────────────────

REGISTER_COMPONENT(CNavigationManagerComponent, "NavigationManager", "NAV");

REFL_DEFINE_OBJECT(CNavigationManagerComponent)
    REFL_DEFINE_INT_MEMBER(CNavigationManagerComponent, m_maxQueryNodes)
REFL_DEFINE_END

// ── CNavigationManagerComponent ────────────────────────────────────────────

CNavigationManagerComponent::~CNavigationManagerComponent()
{
    if (m_navMeshQuery)
    {
        dtFreeNavMeshQuery(m_navMeshQuery);
        m_navMeshQuery = nullptr;
    }
}
 
bool CNavigationManagerComponent::OnInitialize()
{
    DECLARE_FUNC_VLOW();
    // Locate the first CLevelComponent in the hierarchy.
    // Check descendants of the parent first, then walk up.
    RefreshNavMeshQuery();
    return true;
}

void CNavigationManagerComponent::OnUpdate(double /*deltaTime*/)
{
    DECLARE_FUNC_MEDIUM();
    RefreshNavMeshQuery();
    DispatchPendingQueries();


    if (g_debugRenderNavMesh.Get())
    {
        auto* renderFunctionQueue = Core::CoreSystem::GetRenderFunctionQueue();
        if (renderFunctionQueue)
        {
            renderFunctionQueue->AddFunction([this]()
                {
                    DebugRender(0);
                }, "CNavigationManagerComponent::DebugRender");
        }
    }
}

void CNavigationManagerComponent::OnShutdown()
{
    DECLARE_FUNC_VLOW();
    {
        std::lock_guard<std::mutex> lock(m_queriesMutex);
        m_queries.clear();
    }

    if (m_navMeshQuery)
    {
        dtFreeNavMeshQuery(m_navMeshQuery);
        m_navMeshQuery = nullptr;
    }
    m_lastNavMesh    = nullptr;
    m_levelComponent.Reset();

    Component::OnShutdown();
}

// ── Async query registration ────────────────────────────────────────────────

void CNavigationManagerComponent::RegisterQuery(std::shared_ptr<CNavQuery> query)
{
    if (!query)
        return;

    std::lock_guard<std::mutex> lock(m_queriesMutex);
    m_queries.emplace_back(query);
}

void CNavigationManagerComponent::UnregisterQuery(const std::shared_ptr<CNavQuery>& query)
{
    if (!query)
        return;

    std::lock_guard<std::mutex> lock(m_queriesMutex);
    m_queries.erase(
        std::remove_if(m_queries.begin(), m_queries.end(),
            [&query](const std::weak_ptr<CNavQuery>& weak)
            {
                auto locked = weak.lock();
                return !locked || locked == query;
            }),
        m_queries.end());
}

// ── Queries ────────────────────────────────────────────────────────────────

bool CNavigationManagerComponent::IsReady() const
{
    return m_navMeshQuery != nullptr && GetNavMesh() != nullptr;
}

bool CNavigationManagerComponent::FindNearestPoint(const Vector3f& position,
                                                    Vector3f&       outNearest) const
{
    if (!IsReady())
        return false;

    const float pos[3]  = { position.x, position.y, position.z };
    const float ext[3]  = { m_searchExtents.x, m_searchExtents.y, m_searchExtents.z };

    dtQueryFilter filter;
    dtPolyRef     polyRef = 0;
    float         nearest[3]{};

    const dtStatus status = m_navMeshQuery->findNearestPoly(pos, ext, &filter, &polyRef, nearest);
    if (dtStatusFailed(status) || polyRef == 0)
        return false;

    outNearest = { nearest[0], nearest[1], nearest[2] };
    return true;
}

bool CNavigationManagerComponent::FindPath(const Vector3f&        start,
                                            const Vector3f&        end,
                                            std::vector<Vector3f>& outPath) const
{
    if (!IsReady())
        return false;

    const float startPos[3] = { start.x, start.y, start.z };
    const float endPos[3]   = { end.x,   end.y,   end.z   };
    const float ext[3]      = { m_searchExtents.x, m_searchExtents.y, m_searchExtents.z };

    dtQueryFilter filter;

    // Snap start and end to the NavMesh.
    dtPolyRef startRef = 0, endRef = 0;
    float     nearestStart[3]{}, nearestEnd[3]{};

    if (dtStatusFailed(m_navMeshQuery->findNearestPoly(startPos, ext, &filter,
                                                        &startRef, nearestStart))
        || startRef == 0)
        return false;

    if (dtStatusFailed(m_navMeshQuery->findNearestPoly(endPos, ext, &filter,
                                                        &endRef, nearestEnd))
        || endRef == 0)
        return false;

    // Find the polygon corridor.
    dtPolyRef polyPath[k_maxPathPolys]{};
    int       pathCount = 0;

    const dtStatus pathStatus = m_navMeshQuery->findPath(
        startRef, endRef,
        nearestStart, nearestEnd,
        &filter,
        polyPath, &pathCount, k_maxPathPolys);

    if (dtStatusFailed(pathStatus) || pathCount == 0)
        return false;

    // Straighten the path into world-space waypoints.
    float     straightPath[k_maxPathPolys * 3]{};
    uint8_t   straightPathFlags[k_maxPathPolys]{};
    dtPolyRef straightPathRefs[k_maxPathPolys]{};
    int       straightPathCount = 0;

    const dtStatus straightStatus = m_navMeshQuery->findStraightPath(
        nearestStart, nearestEnd,
        polyPath, pathCount,
        straightPath, straightPathFlags, straightPathRefs,
        &straightPathCount, k_maxPathPolys);

    if (dtStatusFailed(straightStatus) || straightPathCount == 0)
        return false;

    outPath.clear();
    outPath.reserve(static_cast<size_t>(straightPathCount));

    for (int i = 0; i < straightPathCount; ++i)
    {
        const float* p = &straightPath[i * 3];
        outPath.emplace_back(p[0], p[1], p[2]);
    }

    return true;
}

// ── Private helpers ────────────────────────────────────────────────────────

void CNavigationManagerComponent::DispatchPendingQueries()
{
    if (!IsReady())
        return;

    JobSystem* jobSystem = Core::CoreSystem::GetJobSystem();
    if (!jobSystem || jobSystem->IsShuttingDown())
        return;

    const dtNavMesh* navMesh  = GetNavMesh();
    const int        maxNodes = m_maxQueryNodes;
    const Vector3f   extents  = m_searchExtents;

    // Collect live queries and prune expired weak_ptrs under the lock.
    std::vector<std::shared_ptr<CNavQuery>> live;
    {
        std::lock_guard<std::mutex> lock(m_queriesMutex);
        m_queries.erase(
            std::remove_if(m_queries.begin(), m_queries.end(),
                [](const std::weak_ptr<CNavQuery>& w) { return w.expired(); }),
            m_queries.end());

        live.reserve(m_queries.size());
        for (const auto& weak : m_queries)
        {
            if (auto q = weak.lock())
                live.push_back(q);
        }
    }

    for (const auto& query : live)
    {
        // Skip if not dirty or already running.
        if (!query->IsDirty())
            continue;
        if (query->GetState() == CNavQuery::State::InProgress)
            continue;

        // Snapshot inputs then atomically mark in-progress before the job runs.
        const Vector3f start = query->GetStart();
        const Vector3f dest  = query->GetDestination();
        query->MarkInProgress();

        // Each job allocates its own dtNavMeshQuery so workers are fully parallel.
        jobSystem->SubmitJob([query, navMesh, maxNodes, extents, start, dest]()
        {
            dtNavMeshQuery* localQuery = dtAllocNavMeshQuery();
            if (!localQuery)
            {
                query->SetFailed();
                return;
            }

            if (dtStatusFailed(localQuery->init(navMesh, maxNodes)))
            {
                dtFreeNavMeshQuery(localQuery);
                query->SetFailed();
                return;
            }

            const float startPos[3] = { start.x, start.y, start.z };
            const float endPos[3]   = { dest.x,  dest.y,  dest.z  };
            const float ext[3]      = { extents.x, extents.y, extents.z };

            dtQueryFilter filter;
            dtPolyRef     startRef = 0, endRef = 0;
            float         nearestStart[3]{}, nearestEnd[3]{};

            if (dtStatusFailed(localQuery->findNearestPoly(startPos, ext, &filter, &startRef, nearestStart))
                || startRef == 0)
            {
                dtFreeNavMeshQuery(localQuery);
                query->SetFailed();
                return;
            }

            if (dtStatusFailed(localQuery->findNearestPoly(endPos, ext, &filter, &endRef, nearestEnd))
                || endRef == 0)
            {
                dtFreeNavMeshQuery(localQuery);
                query->SetFailed();
                return;
            }

            dtPolyRef polyPath[k_maxPathPolys]{};
            int       pathCount = 0;

            if (dtStatusFailed(localQuery->findPath(
                    startRef, endRef, nearestStart, nearestEnd,
                    &filter, polyPath, &pathCount, k_maxPathPolys))
                || pathCount == 0)
            {
                dtFreeNavMeshQuery(localQuery);
                query->SetFailed();
                return;
            }

            float     straightPath[k_maxPathPolys * 3]{};
            uint8_t   straightPathFlags[k_maxPathPolys]{};
            dtPolyRef straightPathRefs[k_maxPathPolys]{};
            int       straightPathCount = 0;

            if (dtStatusFailed(localQuery->findStraightPath(
                    nearestStart, nearestEnd,
                    polyPath, pathCount,
                    straightPath, straightPathFlags, straightPathRefs,
                    &straightPathCount, k_maxPathPolys))
                || straightPathCount == 0)
            {
                dtFreeNavMeshQuery(localQuery);
                query->SetFailed();
                return;
            }

            dtFreeNavMeshQuery(localQuery);

            std::vector<Vector3f> path;
            path.reserve(static_cast<size_t>(straightPathCount));
            for (int i = 0; i < straightPathCount; ++i)
            {
                const float* p = &straightPath[i * 3];
                path.emplace_back(p[0], p[1], p[2]);
            }

            query->SetResult(std::move(path));
        });
    }
}

const dtNavMesh* CNavigationManagerComponent::GetNavMesh() const
{
    CLevelComponent* level = m_levelComponent.Get();
    return level ? level->GetNavMesh() : nullptr;
}

void CNavigationManagerComponent::RefreshNavMeshQuery()
{
    // CComponentReference resolves and caches automatically on Get(this).
    // Calling it every update handles the case where CLevelComponent
    // initialises after this component.
    m_levelComponent.Get(this);

    const dtNavMesh* currentNavMesh = GetNavMesh();

    if (currentNavMesh == m_lastNavMesh)
        return;

    // NavMesh changed (or became available for the first time) – rebuild.
    if (m_navMeshQuery)
    {
        dtFreeNavMeshQuery(m_navMeshQuery);
        m_navMeshQuery = nullptr;
    }

    m_lastNavMesh = currentNavMesh;

    if (!currentNavMesh)
        return;

    dtNavMeshQuery* query = dtAllocNavMeshQuery();
    if (!query)
        return;

    if (dtStatusFailed(query->init(currentNavMesh, m_maxQueryNodes)))
    {
        dtFreeNavMeshQuery(query);
        return;
    }

    m_navMeshQuery = query;
}

void CNavigationManagerComponent::DebugRender(bgfx::ViewId viewId) const
{
    const dtNavMesh* nav = GetNavMesh();
    if (!nav)
        return;

    Rendering::BgfxRenderPrimitives& prims = Rendering::BgfxRenderPrimitives::Instance();
    const uint32_t color = 0xff00ff00;

    const int maxTiles = nav->getMaxTiles();
    for (int ti = 0; ti < maxTiles; ++ti)
    {
        const dtMeshTile* tile = nav->getTile(ti);
        if (!tile || !tile->header || tile->header->polyCount == 0)
            continue;

        for (int p = 0; p < tile->header->polyCount; ++p)
        {
            const dtPoly* poly = &tile->polys[p];
            if (poly->getType() == DT_POLYTYPE_OFFMESH_CONNECTION)
                continue;

            const dtPolyDetail* pd = tile->detailMeshes ? &tile->detailMeshes[p] : nullptr;
            if (!pd)
                continue;

            for (int t = 0; t < pd->triCount; ++t)
            {
                const unsigned char* tri = &tile->detailTris[(pd->triBase + t) * 4];

                float v[3][3];
                for (int vi = 0; vi < 3; ++vi)
                {
                    if (tri[vi] < poly->vertCount)
                    {
                        const float* src = &tile->verts[poly->verts[tri[vi]] * 3];
                        v[vi][0] = src[0];
                        v[vi][1] = src[1];
                        v[vi][2] = src[2];
                    }
                    else
                    {
                        const float* src = &tile->detailVerts[(pd->vertBase + tri[vi] - poly->vertCount) * 3];
                        v[vi][0] = src[0];
                        v[vi][1] = src[1];
                        v[vi][2] = src[2];
                    }
                }

                prims.RenderLine(viewId, v[0][0], v[0][1], v[0][2], v[1][0], v[1][1], v[1][2], color);
                prims.RenderLine(viewId, v[1][0], v[1][1], v[1][2], v[2][0], v[2][1], v[2][2], color);
                prims.RenderLine(viewId, v[2][0], v[2][1], v[2][2], v[0][0], v[0][1], v[0][2], color);
            }
        }
    }
}
