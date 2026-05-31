#pragma once
#include "ResourceManager/ResourceManager.h"

// Forward declaration so consumers that only need the pointer don't have to
// pull in the full Detour header.
struct dtNavMesh;

/// @brief Loads and owns a dtNavMesh from a .Nav.bin file produced by
///        NavMeshBuilder::SaveToFile.
class CNavigationResource : public ResourceSystem::Resource
{
public:
    static std::vector<std::string_view> GetSupportedExtensions()
    {
        return { ".lvl.nav.bin" };
    }

    explicit CNavigationResource(const std::string& path)
        : Resource(path)
    {
    }

    ~CNavigationResource() override;

    /// Read the raw bytes on the worker thread.
    bool Update(FileSystem::FileSystemManager& fileSystem) override;

    /// Deserialise the dtNavMesh on the main thread.
    void Finalize() override;

    const dtNavMesh* GetNavMesh() const { return m_navMesh; }
          dtNavMesh* GetNavMesh()       { return m_navMesh; }

private:
    dtNavMesh* m_navMesh = nullptr;
};

/// @brief Reflected reference to a CNavigationResource asset.
class CNavigationResourceReference : public CTypedResourceReference<CNavigationResource>
{
public:
    REFL_DECLARE_OBJECT(CNavigationResourceReference, CResourceReference);
};
