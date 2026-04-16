#pragma once

#include "FileSystemInterface.h"

#include <functional>
#include <memory>
#include <vector>

namespace FileSystem {

/** @brief Composite IFileSystem that tries each registered file system in order until one succeeds. */
class FileSystemManager : public IFileSystem {
private:
    std::vector<std::unique_ptr<IFileSystem>> m_fileSystems;

    // ── Internal helpers ──────────────────────────────────────────────────────

    template<typename T>
    Result<T> TryOperation(std::function<Result<T>(IFileSystem*)> operation)
    {
        if (m_fileSystems.empty())
        {
            return Result<T>("No file systems available");
        }

        std::string lastError;
        for (auto& fs : m_fileSystems)
        {
            auto result = operation(fs.get());
            if (result.IsSuccess()) return result;
            lastError = result.GetError();
        }
        return Result<T>("All file systems failed. Last error: " + lastError);
    }

    Result<void> TryVoidOperation(std::function<Result<void>(IFileSystem*)> operation)
    {
        if (m_fileSystems.empty())
        {
            return Result<void>("No file systems available");
        }

        std::string lastError;
        for (auto& fs : m_fileSystems)
        {
            auto result = operation(fs.get());
            if (result.IsSuccess()) return result;
            lastError = result.GetError();
        }
        return Result<void>("All file systems failed. Last error: " + lastError);
    }

    bool TryBooleanOperation(std::function<bool(IFileSystem*)> operation)
    {
        for (auto& fs : m_fileSystems)
        {
            if (operation(fs.get())) return true;
        }
        return false;
    }

public:
    FileSystemManager() = default;
    ~FileSystemManager() = default;

    // ── Stack management ──────────────────────────────────────────────────────

    /** @brief Insert a file system at the front (highest priority). */
    void PushFileSystem(std::unique_ptr<IFileSystem> fileSystem)
    {
        if (fileSystem)
        {
            m_fileSystems.insert(m_fileSystems.begin(), std::move(fileSystem));
        }
    }

    /** @brief Append a file system at the back (lowest priority). */
    void AddFileSystem(std::unique_ptr<IFileSystem> fileSystem)
    {
        if (fileSystem)
        {
            m_fileSystems.push_back(std::move(fileSystem));
        }
    }

    /** @brief Remove and return the highest-priority file system. */
    std::unique_ptr<IFileSystem> PopFileSystem()
    {
        if (!m_fileSystems.empty())
        {
            auto fs = std::move(m_fileSystems.front());
            m_fileSystems.erase(m_fileSystems.begin());
            return fs;
        }
        return nullptr;
    }

    /** @brief Remove and return the lowest-priority file system. */
    std::unique_ptr<IFileSystem> RemoveFileSystem()
    {
        if (!m_fileSystems.empty())
        {
            auto fs = std::move(m_fileSystems.back());
            m_fileSystems.pop_back();
            return fs;
        }
        return nullptr;
    }

    /** @brief Remove all registered file systems. */
    void ClearFileSystems()
    {
        m_fileSystems.clear();
    }

    /** @brief Return the number of registered file systems. */
    size_t GetFileSystemCount() const
    {
        return m_fileSystems.size();
    }

    /** @brief Return true if no file systems are registered. */
    bool IsEmpty() const
    {
        return m_fileSystems.empty();
    }

    // ── IFileSystem overrides ─────────────────────────────────────────────────

    Result<std::unique_ptr<IFile>> OpenFile(const std::string& path, FileMode mode) override
    {
        return TryOperation<std::unique_ptr<IFile>>([&](IFileSystem* fs)
        {
            return fs->OpenFile(path, mode);
        });
    }

    Result<std::unique_ptr<IFile>> CreateFile(const std::string& path) override
    {
        return TryOperation<std::unique_ptr<IFile>>([&](IFileSystem* fs)
        {
            return fs->CreateFile(path);
        });
    }

    Result<void> DeleteFile(const std::string& path) override
    {
        return TryVoidOperation([&](IFileSystem* fs)
        {
            return fs->DeleteFile(path);
        });
    }

    Result<void> CopyFile(const std::string& sourcePath, const std::string& destinationPath, bool overwrite = false) override
    {
        return TryVoidOperation([&](IFileSystem* fs)
        {
            return fs->CopyFile(sourcePath, destinationPath, overwrite);
        });
    }

    Result<void> MoveFile(const std::string& sourcePath, const std::string& destinationPath) override
    {
        return TryVoidOperation([&](IFileSystem* fs)
        {
            return fs->MoveFile(sourcePath, destinationPath);
        });
    }

    Result<std::unique_ptr<IDirectory>> OpenDirectory(const std::string& path) override
    {
        return TryOperation<std::unique_ptr<IDirectory>>([&](IFileSystem* fs)
        {
            return fs->OpenDirectory(path);
        });
    }

    Result<void> CreateDirectory(const std::string& path) override
    {
        return TryVoidOperation([&](IFileSystem* fs)
        {
            return fs->CreateDirectory(path);
        });
    }

    Result<void> DeleteDirectory(const std::string& path, bool recursive = false) override
    {
        return TryVoidOperation([&](IFileSystem* fs)
        {
            return fs->DeleteDirectory(path, recursive);
        });
    }

    Result<void> CopyDirectory(const std::string& sourcePath, const std::string& destinationPath, bool recursive = true) override
    {
        return TryVoidOperation([&](IFileSystem* fs)
        {
            return fs->CopyDirectory(sourcePath, destinationPath, recursive);
        });
    }

    Result<void> MoveDirectory(const std::string& sourcePath, const std::string& destinationPath) override
    {
        return TryVoidOperation([&](IFileSystem* fs)
        {
            return fs->MoveDirectory(sourcePath, destinationPath);
        });
    }

    bool Exists(const std::string& path) override
    {
        return TryBooleanOperation([&](IFileSystem* fs)
        {
            return fs->Exists(path);
        });
    }

    bool IsFile(const std::string& path) override
    {
        return TryBooleanOperation([&](IFileSystem* fs)
        {
            return fs->IsFile(path);
        });
    }

    bool IsDirectory(const std::string& path) override
    {
        return TryBooleanOperation([&](IFileSystem* fs)
        {
            return fs->IsDirectory(path);
        });
    }

    Result<FileInfo> GetInfo(const std::string& path) override
    {
        return TryOperation<FileInfo>([&](IFileSystem* fs)
        {
            return fs->GetInfo(path);
        });
    }

    // Path manipulation — delegate to first available file system
    std::string GetFileName(const std::string& path) override
    {
        if (!m_fileSystems.empty()) return m_fileSystems[0]->GetFileName(path);
        return "";
    }

    std::string GetFileNameWithoutExtension(const std::string& path) override
    {
        if (!m_fileSystems.empty()) return m_fileSystems[0]->GetFileNameWithoutExtension(path);
        return "";
    }

    std::string GetExtension(const std::string& path) override
    {
        if (!m_fileSystems.empty()) return m_fileSystems[0]->GetExtension(path);
        return "";
    }

    std::string GetDirectoryName(const std::string& path) override
    {
        if (!m_fileSystems.empty()) return m_fileSystems[0]->GetDirectoryName(path);
        return "";
    }

    std::string GetFullPath(const std::string& path) override
    {
        if (!m_fileSystems.empty()) return m_fileSystems[0]->GetFullPath(path);
        return path;
    }

    std::string CombinePath(const std::string& path1, const std::string& path2) override
    {
        if (!m_fileSystems.empty()) return m_fileSystems[0]->CombinePath(path1, path2);
        return path1 + "/" + path2;
    }

    Result<std::string> GetCurrentDirectory() override
    {
        return TryOperation<std::string>([&](IFileSystem* fs)
        {
            return fs->GetCurrentDirectory();
        });
    }

    Result<void> SetCurrentDirectory(const std::string& path) override
    {
        return TryVoidOperation([&](IFileSystem* fs)
        {
            return fs->SetCurrentDirectory(path);
        });
    }

    Result<std::string> GetTempDirectory() override
    {
        return TryOperation<std::string>([&](IFileSystem* fs)
        {
            return fs->GetTempDirectory();
        });
    }

    Result<std::string> GetUserDirectory() override
    {
        return TryOperation<std::string>([&](IFileSystem* fs)
        {
            return fs->GetUserDirectory();
        });
    }

    Result<std::string> GetApplicationDirectory() override
    {
        return TryOperation<std::string>([&](IFileSystem* fs)
        {
            return fs->GetApplicationDirectory();
        });
    }

    Result<std::vector<uint8_t>> ReadAllBytes(const std::string& path) override
    {
        return TryOperation<std::vector<uint8_t>>([&](IFileSystem* fs)
        {
            return fs->ReadAllBytes(path);
        });
    }

    Result<std::string> ReadAllText(const std::string& path) override
    {
        return TryOperation<std::string>([&](IFileSystem* fs)
        {
            return fs->ReadAllText(path);
        });
    }

    Result<void> WriteAllBytes(const std::string& path, const std::vector<uint8_t>& data) override
    {
        return TryVoidOperation([&](IFileSystem* fs)
        {
            return fs->WriteAllBytes(path, data);
        });
    }

    Result<void> WriteAllText(const std::string& path, const std::string& text) override
    {
        return TryVoidOperation([&](IFileSystem* fs)
        {
            return fs->WriteAllText(path, text);
        });
    }
};

} // namespace FileSystem
