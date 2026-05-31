#include "NavMeshBuilder.h"

#include <DetourNavMesh.h>

#include <cstdio>
#include <cstring>

// ?? File format ???????????????????????????????????????????????????????????????
// Header: magic (4 bytes) + version (4 bytes) + tileData size (4 bytes)
// Body:   raw Detour tile data blob
// ?????????????????????????????????????????????????????????????????????????????

static constexpr int NAV_BIN_MAGIC   = 0x4E415642; // 'NAVB'
static constexpr int NAV_BIN_VERSION = 1;

namespace NavMesh {

bool NavMeshBuilder::SaveToFile(const std::string& path)
{
    if (!IsBuilt() || !m_navMesh)
    {
        m_lastError = "SaveToFile: no nav mesh built";
        return false;
    }

    const dtNavMesh* nm = m_navMesh.get();
    if (nm->getMaxTiles() == 0 || !nm->getTile(0) || !nm->getTile(0)->data)
    {
        m_lastError = "SaveToFile: nav mesh has no tile data";
        return false;
    }

    const dtMeshTile* tile     = nm->getTile(0);
    const int         dataSize = tile->dataSize;

    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "wb") != 0 || !f)
    {
        m_lastError = "SaveToFile: cannot open '" + path + "' for writing";
        return false;
    }

    fwrite(&NAV_BIN_MAGIC,   sizeof(int), 1, f);
    fwrite(&NAV_BIN_VERSION, sizeof(int), 1, f);
    fwrite(&dataSize,        sizeof(int), 1, f);
    fwrite(tile->data,       1, static_cast<size_t>(dataSize), f);
    fclose(f);
    return true;
}

bool NavMeshBuilder::LoadFromFile(const std::string& path)
{
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "rb") != 0 || !f)
    {
        m_lastError = "LoadFromFile: cannot open '" + path + "'";
        return false;
    }

    int magic = 0, version = 0, dataSize = 0;
    fread(&magic,    sizeof(int), 1, f);
    fread(&version,  sizeof(int), 1, f);
    fread(&dataSize, sizeof(int), 1, f);

    if (magic != NAV_BIN_MAGIC || version != NAV_BIN_VERSION || dataSize <= 0)
    {
        fclose(f);
        m_lastError = "LoadFromFile: bad header in '" + path + "'";
        return false;
    }

    unsigned char* data = static_cast<unsigned char*>(dtAlloc(dataSize, DT_ALLOC_PERM));
    if (!data)
    {
        fclose(f);
        m_lastError = "LoadFromFile: allocation failed";
        return false;
    }

    fread(data, 1, static_cast<size_t>(dataSize), f);
    fclose(f);

    dtNavMesh* nm = dtAllocNavMesh();
    if (!nm)
    {
        dtFree(data);
        m_lastError = "LoadFromFile: dtAllocNavMesh failed";
        return false;
    }

    const dtMeshHeader* header = reinterpret_cast<const dtMeshHeader*>(data);
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
        dtFree(data);
        m_lastError = "LoadFromFile: dtNavMesh::init failed";
        return false;
    }

    dtTileRef tileRef = 0;
    if (dtStatusFailed(nm->addTile(data, dataSize, DT_TILE_FREE_DATA, 0, &tileRef)))
    {
        dtFreeNavMesh(nm);
        // data ownership was passed with DT_TILE_FREE_DATA — already freed by Detour
        m_lastError = "LoadFromFile: addTile failed";
        return false;
    }

    m_navMesh.reset(nm);
    return true;
}

} // namespace NavMesh
