#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <chrono>

namespace FileSystem {

// ── Forward declarations ──────────────────────────────────────────────────────

class IFile;
class IDirectory;

// ── Enumerations ──────────────────────────────────────────────────────────────

/** @brief Modes for opening a file. */
enum class FileMode {
    Read,
    Write,
    ReadWrite,
    Append
};

/** @brief Origin for seek operations. */
enum class SeekOrigin {
    Begin,
    Current,
    End
};

/** @brief Attribute flags for files and directories. */
enum class FileAttributes {
    None       = 0,
    ReadOnly   = 1 << 0,
    Hidden     = 1 << 1,
    System     = 1 << 2,
    Directory  = 1 << 3,
    Archive    = 1 << 4,
    Compressed = 1 << 5,
    Encrypted  = 1 << 6
};

inline FileAttributes operator|(FileAttributes a, FileAttributes b)
{
    return static_cast<FileAttributes>(static_cast<int>(a) | static_cast<int>(b));
}

inline FileAttributes operator&(FileAttributes a, FileAttributes b)
{
    return static_cast<FileAttributes>(static_cast<int>(a) & static_cast<int>(b));
}

inline bool HasAttribute(FileAttributes attrs, FileAttributes check)
{
    return (attrs & check) == check;
}

// ── FileInfo ──────────────────────────────────────────────────────────────────

/** @brief Metadata describing a file or directory entry. */
struct FileInfo {
    std::string                          name;
    std::string                          fullPath;
    uint64_t                             size           = 0;
    FileAttributes                       attributes     = FileAttributes::None;
    std::chrono::system_clock::time_point creationTime;
    std::chrono::system_clock::time_point lastWriteTime;
    std::chrono::system_clock::time_point lastAccessTime;
    bool                                 isDirectory    = false;
};

// ── Result ────────────────────────────────────────────────────────────────────

/** @brief Result wrapper carrying either a value or an error message. */
template<typename T>
class Result {
public:
    explicit Result(T&& value) : m_value(std::move(value)), m_hasError(false) {}
    explicit Result(const std::string& error) : m_errorMessage(error), m_hasError(true) {}

    bool IsSuccess() const { return !m_hasError; }
    bool HasError() const { return m_hasError; }
    const std::string& GetError() const { return m_errorMessage; }
    const T& GetValue() const { return m_value; }
    T& GetValue() { return m_value; }

private:
    T           m_value;
    std::string m_errorMessage;
    bool        m_hasError = false;
};

/** @brief Specialization of Result for void operations. */
template<>
class Result<void> {
public:
    Result() : m_hasError(false) {}
    explicit Result(const std::string& error) : m_errorMessage(error), m_hasError(true) {}

    bool IsSuccess() const { return !m_hasError; }
    bool HasError() const { return m_hasError; }
    const std::string& GetError() const { return m_errorMessage; }

private:
    std::string m_errorMessage;
    bool        m_hasError = false;
};

// ── IFile ─────────────────────────────────────────────────────────────────────

/** @brief Abstract interface for file I/O operations. */
class IFile {
public:
    virtual ~IFile() = default;

    virtual Result<size_t>               Read(void* buffer, size_t bytesToRead) = 0;
    virtual Result<size_t>               Write(const void* buffer, size_t bytesToWrite) = 0;
    virtual Result<void>                 Flush() = 0;
    virtual Result<void>                 Close() = 0;

    virtual Result<uint64_t>             Seek(int64_t offset, SeekOrigin origin) = 0;
    virtual Result<uint64_t>             GetPosition() = 0;
    virtual Result<uint64_t>             GetSize() = 0;

    virtual Result<FileInfo>             GetInfo() = 0;
    virtual bool                         IsOpen() const = 0;
    virtual bool                         CanRead() const = 0;
    virtual bool                         CanWrite() const = 0;

    virtual Result<std::vector<uint8_t>> ReadAll() = 0;
    virtual Result<std::string>          ReadAllText() = 0;
    virtual Result<void>                 WriteAllText(const std::string& text) = 0;
};

// ── IDirectory ────────────────────────────────────────────────────────────────

/** @brief Abstract interface for directory enumeration. */
class IDirectory {
public:
    virtual ~IDirectory() = default;

    virtual Result<std::vector<FileInfo>> GetFiles() = 0;
    virtual Result<std::vector<FileInfo>> GetDirectories() = 0;
    virtual Result<std::vector<FileInfo>> GetEntries() = 0;

    virtual Result<std::vector<FileInfo>> GetFiles(const std::string& pattern) = 0;
    virtual Result<std::vector<FileInfo>> GetDirectories(const std::string& pattern) = 0;

    virtual Result<FileInfo>              GetInfo() = 0;
    virtual std::string                   GetPath() const = 0;
};

// ── IFileSystem ───────────────────────────────────────────────────────────────

/** @brief Abstract interface for a complete file system implementation. */
class IFileSystem {
public:
    virtual ~IFileSystem() = default;

    virtual Result<std::unique_ptr<IFile>>      OpenFile(const std::string& path, FileMode mode) = 0;
    virtual Result<std::unique_ptr<IFile>>      CreateFile(const std::string& path) = 0;
    virtual Result<void>                        DeleteFile(const std::string& path) = 0;
    virtual Result<void>                        CopyFile(const std::string& sourcePath, const std::string& destinationPath, bool overwrite = false) = 0;
    virtual Result<void>                        MoveFile(const std::string& sourcePath, const std::string& destinationPath) = 0;

    virtual Result<std::unique_ptr<IDirectory>> OpenDirectory(const std::string& path) = 0;
    virtual Result<void>                        CreateDirectory(const std::string& path) = 0;
    virtual Result<void>                        DeleteDirectory(const std::string& path, bool recursive = false) = 0;
    virtual Result<void>                        CopyDirectory(const std::string& sourcePath, const std::string& destinationPath, bool recursive = true) = 0;
    virtual Result<void>                        MoveDirectory(const std::string& sourcePath, const std::string& destinationPath) = 0;

    virtual bool                                Exists(const std::string& path) = 0;
    virtual bool                                IsFile(const std::string& path) = 0;
    virtual bool                                IsDirectory(const std::string& path) = 0;
    virtual Result<FileInfo>                    GetInfo(const std::string& path) = 0;

    virtual std::string                         GetFileName(const std::string& path) = 0;
    virtual std::string                         GetFileNameWithoutExtension(const std::string& path) = 0;
    virtual std::string                         GetExtension(const std::string& path) = 0;
    virtual std::string                         GetDirectoryName(const std::string& path) = 0;
    virtual std::string                         GetFullPath(const std::string& path) = 0;
    virtual std::string                         CombinePath(const std::string& path1, const std::string& path2) = 0;

    virtual Result<std::string>                 GetCurrentDirectory() = 0;
    virtual Result<void>                        SetCurrentDirectory(const std::string& path) = 0;

    virtual Result<std::string>                 GetTempDirectory() = 0;
    virtual Result<std::string>                 GetUserDirectory() = 0;
    virtual Result<std::string>                 GetApplicationDirectory() = 0;

    virtual Result<std::vector<uint8_t>>        ReadAllBytes(const std::string& path) = 0;
    virtual Result<std::string>                 ReadAllText(const std::string& path) = 0;
    virtual Result<void>                        WriteAllBytes(const std::string& path, const std::vector<uint8_t>& data) = 0;
    virtual Result<void>                        WriteAllText(const std::string& path, const std::string& text) = 0;
};

} // namespace FileSystem
