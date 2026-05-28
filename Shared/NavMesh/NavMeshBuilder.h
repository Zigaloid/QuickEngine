#pragma once

#include <memory>
#include <string>
#include <vector>

// Forward declarations – avoid pulling Recast/Detour headers into every TU
// that includes this header.
struct rcContext;
struct rcPolyMesh;
struct rcPolyMeshDetail;
struct dtNavMesh;
class CLevelComponent;

namespace NavMesh {

/// Parameters exposed to the editor UI.  All values match the defaults used
/// in the classic Recast solo-mesh demo.
struct NavMeshConfig
{
    // Rasterisation
    float cellSize          = 0.3f;
    float cellHeight        = 0.2f;

    // Agent
    float agentHeight       = 2.0f;
    float agentRadius       = 0.6f;
    float agentMaxClimb     = 0.9f;
    float agentMaxSlope     = 45.0f;

    // Region
    float regionMinSize     = 8.0f;
    float regionMergeSize   = 20.0f;

    // Edge simplification
    float edgeMaxLen        = 12.0f;
    float edgeMaxError      = 1.3f;

    // Polygon mesh
    int   vertsPerPoly      = 6;

    // Detail mesh
    float detailSampleDist  = 6.0f;
    float detailSampleMaxError = 1.0f;
};

/// Collects geometry from every CHeightFieldPhysicsComponent found under a
/// CLevelComponent and runs the full Recast/Detour solo-mesh pipeline.
///
/// The built rcPolyMesh is retained so NavMeshDebugDraw can visualise it
/// without re-running the pipeline.  Call Build() again to regenerate.
class NavMeshBuilder
{
public:
    NavMeshBuilder();
    ~NavMeshBuilder();

    // Non-copyable, movable
    NavMeshBuilder(const NavMeshBuilder&)            = delete;
    NavMeshBuilder& operator=(const NavMeshBuilder&) = delete;
    NavMeshBuilder(NavMeshBuilder&&)                 = default;
    NavMeshBuilder& operator=(NavMeshBuilder&&)      = default;

    /// Run the full build pipeline.
    /// @returns true on success, false if geometry collection or any Recast
    ///          step fails.  Call GetLastError() for a human-readable message.
    bool Build(CLevelComponent* level, const NavMeshConfig& cfg);

    /// Destroy all built data (does not reset the config).
    void Clear();

    bool IsBuilt()  const { return m_polyMesh != nullptr; }
    int  PolyCount() const;

    /// Access the built polygon mesh for debug rendering.  Valid only when
    /// IsBuilt() returns true.
    const rcPolyMesh*       GetPolyMesh()       const { return m_polyMesh.get(); }
    const rcPolyMeshDetail* GetPolyMeshDetail() const { return m_polyMeshDetail.get(); }
    const dtNavMesh*        GetNavMesh()        const { return m_navMesh.get(); }

    const std::string& GetLastError() const { return m_lastError; }

private:
    /// Walk the level hierarchy and collect world-space triangles from every
    /// CHeightFieldPhysicsComponent.  Returns false when no geometry is found.
    bool CollectGeometry(CLevelComponent* level,
                         std::vector<float>& outVerts,
                         std::vector<int>&   outTris);

    // Recast context (log + timer)
    std::unique_ptr<rcContext> m_ctx;

    // Built data – owning pointers with custom deleters that call rcFree /
    // dtFreeNavMesh so we can forward-declare the types.
    struct RcPolyMeshDeleter      { void operator()(rcPolyMesh* p)       const; };
    struct RcPolyMeshDetailDeleter{ void operator()(rcPolyMeshDetail* p) const; };
    struct DtNavMeshDeleter       { void operator()(dtNavMesh* p)        const; };

    std::unique_ptr<rcPolyMesh,       RcPolyMeshDeleter>       m_polyMesh;
    std::unique_ptr<rcPolyMeshDetail, RcPolyMeshDetailDeleter> m_polyMeshDetail;
    std::unique_ptr<dtNavMesh,        DtNavMeshDeleter>        m_navMesh;

    std::string m_lastError;
};

} // namespace NavMesh
