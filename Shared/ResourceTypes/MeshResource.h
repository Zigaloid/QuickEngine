#pragma once
#include "Reflection/ReflectionBase.h"
#include "ResourceManager/ResourceManager.h"
#include "bgfx\bgfx.h"
#include "bgfx_utils.h"

class CMeshResource : public ResourceSystem::Resource
{
public:
    explicit CMeshResource(const std::string& path)
        : Resource(path)
        , m_mesh(nullptr)
    {
    }

    ~CMeshResource() override
    {
        if (m_mesh)
        {
            meshUnload(m_mesh);
            m_mesh = nullptr;
        }
    }

    // Skip base-class file reading; bgfx meshLoad handles its own I/O.
    bool Update(FileSystem::FileSystemManager& /*fileSystem*/) override
    {
        m_isLoaded = true;
        return true;
    }

    // Finalize runs on the main thread – safe for bgfx resource creation.
    // path_ should be the mesh binary path, e.g. "meshes/bunny.bin".
    void Finalize() override
    {
        m_mesh = meshLoad(m_path.c_str());
        m_isFinalized = (m_mesh != nullptr);
    }

    // Accessor
    Mesh* GetMesh() { return m_mesh; }
    const Mesh* GetMesh() const { return m_mesh; }
private:
    Mesh* m_mesh = nullptr;
};


class CMeshResourceReference : public CTypedResourceReference<CMeshResource>
{
public:
    REFL_DECLARE_OBJECT(CMeshResourceReference, CResourceReference);
};