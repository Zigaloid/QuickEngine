#include "NavigationResource.h"
#include "CoreSystem/CoreSystem.h"

#include <DetourNavMesh.h>

#include <cstring>

// Matches the constants in NavMeshSerializer.cpp
static constexpr int NAV_BIN_MAGIC   = 0x4E415642; // 'NAVB'
static constexpr int NAV_BIN_VERSION = 1;

// ?? Reflection ????????????????????????????????????????????????????????????????

REFL_DEFINE_OBJECT(CNavigationResourceReference)
REFL_DEFINE_END

// ?? CNavigationResource ???????????????????????????????????????????????????????

CNavigationResource::~CNavigationResource()
{
    if (m_navMesh)
    {
        dtFreeNavMesh(m_navMesh);
        m_navMesh = nullptr;
    }
}

bool CNavigationResource::Update(FileSystem::FileSystemManager& fileSystem)
{
    if (!Resource::Update(fileSystem))
        return false;

    m_isLoaded = GetLoadedSize() > 0;
    return m_isLoaded;
}

void CNavigationResource::Finalize()
{
    const std::vector<uint8_t>& raw = GetData();
    constexpr size_t kHeaderSize = sizeof(int) * 3;
    if (raw.size() < kHeaderSize)
    {
        m_isFinalized = false;
        return;
    }

    const uint8_t* ptr = raw.data();
    int magic = 0, version = 0, dataSize = 0;
    std::memcpy(&magic,    ptr,              sizeof(int)); ptr += sizeof(int);
    std::memcpy(&version,  ptr,              sizeof(int)); ptr += sizeof(int);
    std::memcpy(&dataSize, ptr,              sizeof(int)); ptr += sizeof(int);

    if (magic != NAV_BIN_MAGIC || version != NAV_BIN_VERSION
        || dataSize <= 0
        || static_cast<size_t>(dataSize) > raw.size() - kHeaderSize)
    {
        m_isFinalized = false;
        return;
    }

    unsigned char* tileData = static_cast<unsigned char*>(dtAlloc(dataSize, DT_ALLOC_PERM));
    if (!tileData) { m_isFinalized = false; return; }
    std::memcpy(tileData, ptr, static_cast<size_t>(dataSize));

    dtNavMesh* nm = dtAllocNavMesh();
    if (!nm)
    {
        dtFree(tileData);
        m_isFinalized = false;
        return;
    }

    const dtMeshHeader* header = reinterpret_cast<const dtMeshHeader*>(tileData);
    dtNavMeshParams params{};
    params.orig[0]    = header->bmin[0];
    params.orig[1]    = header->bmin[1];
    params.orig[2]    = header->bmin[2];
    params.tileWidth  = header->bmax[0] - header->bmin[0];
    params.tileHeight = header->bmax[2] - header->bmin[2];
    params.maxTiles   = 1;
    params.maxPolys   = header->polyCount;

    if (dtStatusFailed(nm->init(&params)))
    {
        dtFreeNavMesh(nm);
        dtFree(tileData);
        m_isFinalized = false;
        return;
    }

    dtTileRef tileRef = 0;
    if (dtStatusFailed(nm->addTile(tileData, dataSize, DT_TILE_FREE_DATA, 0, &tileRef)))
    {
        dtFreeNavMesh(nm);
        m_isFinalized = false;
        return;
    }

    if (m_navMesh)
        dtFreeNavMesh(m_navMesh);

    m_navMesh     = nm;
    m_isFinalized = true;
}
