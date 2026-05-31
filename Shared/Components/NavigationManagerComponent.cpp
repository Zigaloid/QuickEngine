#include "NavigationManagerComponent.h"
#include "CoreSystem/CoreSystem.h"
#include "ComponentSystem/ComponentSystem.h"

#include <DetourNavMesh.h>
#include <DetourNavMeshQuery.h>

#include <cstring>

// ── Reflection ─────────────────────────────────────────────────────────────

REGISTER_COMPONENT(CNavigationManagerComponent, "NavigationManager", "NAV");

REFL_DEFINE_OBJECT(CNavigationManagerComponent)
    REFL_DEFINE_INT_MEMBER(CNavigationManagerComponent, m_maxQueryNodes),
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
    // Locate the first CLevelComponent in the hierarchy.
    // Check descendants of the parent first, then walk up.
    m_levelComponent = FindDescendant<CLevelComponent>();

    if (!m_levelComponent)
    {
        auto* parent = GetParent();
        while (parent && !m_levelComponent)
        {
            m_levelComponent = dynamic_cast<CLevelComponent*>(parent);
            if (!m_levelComponent)
                m_levelComponent = parent->FindDescendant<CLevelComponent>();
            parent = parent->GetParent();
        }
    }

    RefreshNavMeshQuery();
    return true;
}

void CNavigationManagerComponent::OnUpdate(double /*deltaTime*/)
{
    RefreshNavMeshQuery();
}

void CNavigationManagerComponent::OnShutdown()
{
    if (m_navMeshQuery)
    {
        dtFreeNavMeshQuery(m_navMeshQuery);
        m_navMeshQuery = nullptr;
    }
    m_lastNavMesh    = nullptr;
    m_levelComponent = nullptr;

    Component::OnShutdown();
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

const dtNavMesh* CNavigationManagerComponent::GetNavMesh() const
{
    return m_levelComponent ? m_levelComponent->GetNavMesh() : nullptr;
}

void CNavigationManagerComponent::RefreshNavMeshQuery()
{
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