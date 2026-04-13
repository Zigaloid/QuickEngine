#pragma once

#include "Reflection/ReflectionBase.h"
#include "ResourceManager/ResourceManager.h"
#include "Filesystem/FileSystemManager.h"
#include "bgfx_utils.h"
#include "FbxMeshConverter.h"
#include <bx/readerwriter.h>
#include <string>
#include <vector>

/// Resource that loads an FBX file with ufbx, converts it to the bgfx
/// binary mesh format, and creates GPU buffers via the existing Mesh class.
///
/// Usage with the ResourceManager:
///   auto res = resourceMgr.RequestResource<CFbxMeshResource>("models/hero.fbx");
///   // ... later, after UpdateFinalization() ...
///   if (res->IsFinalized()) {
///       meshSubmit(res->GetMesh(), viewId, program, mtx);
///   }
///
class CFbxMeshResource : public ResourceSystem::Resource
{
public:
    CFbxMeshResource(const std::string& path)
        : Resource(path)
    {
    }

    ~CFbxMeshResource() override
    {
        if (m_mesh)
        {
            meshUnload(m_mesh);
            m_mesh = nullptr;
        }
    }

    /// Worker-thread step: convert FBX → bgfx binary in memory.
    bool Update(FileSystem::FileSystemManager& /*fileSystem*/) override
    {
        FbxConvertOptions opts;
        opts.scaleFactor    = m_scaleFactor;
        opts.includeNormals = true;
        opts.includeUVs     = true;
        opts.includeTangents = true;

        if (!ConvertFbxToBgfxBinary(path_.c_str(), opts, m_binaryBlob))
            return false;

        isLoaded_ = true;
        return true;
    }

    /// Main-thread step: feed the binary blob through Mesh::load().
    void Finalize() override
    {
        if (m_binaryBlob.empty())
        {
            isFinalized_ = false;
            return;
        }

        // Wrap the in-memory blob as a bx::MemoryReader.
        bx::MemoryReader reader(m_binaryBlob.data(),
                                static_cast<uint32_t>(m_binaryBlob.size()));

        m_mesh = new Mesh;
        m_mesh->load(&reader, /*_ramcopy=*/false);

        // Release the intermediate blob — GPU buffers own the data now.
        m_binaryBlob.clear();
        m_binaryBlob.shrink_to_fit();

        isFinalized_ = (m_mesh != nullptr && !m_mesh->m_groups.empty());
    }

    // ── Accessors ───────────────────────────────────────────────────

    Mesh*       GetMesh()       { return m_mesh; }
    const Mesh* GetMesh() const { return m_mesh; }

    void SetScaleFactor(float s) { m_scaleFactor = s; }

    /// Optionally cache the binary to disk so future loads skip the
    /// FBX conversion entirely.
    bool SaveBinaryCache(const std::string& binPath,
                         FileSystem::FileSystemManager& fs) const
    {
        // Re-generate if blob was already released after Finalize.
        std::vector<uint8_t> blob;
        FbxConvertOptions opts;
        opts.scaleFactor = m_scaleFactor;
        if (!ConvertFbxToBgfxBinary(path_.c_str(), opts, blob))
            return false;

        auto result = fs.WriteAllBytes(binPath, blob);
        return result.IsSuccess();
    }

private:
    Mesh*                 m_mesh        = nullptr;
    std::vector<uint8_t>  m_binaryBlob;
    float                 m_scaleFactor = 1.0f;
};