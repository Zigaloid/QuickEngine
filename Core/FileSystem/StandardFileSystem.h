#pragma once

#include "FileSystemInterface.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>

// Windows.h defines macros that collide with our method names.
// Undefine them so the override signatures match IFileSystem exactly.
#ifdef CreateFile
#undef CreateFile
#endif
#ifdef DeleteFile
#undef DeleteFile
#endif
#ifdef CopyFile
#undef CopyFile
#endif
#ifdef MoveFile
#undef MoveFile
#endif
#ifdef CreateDirectory
#undef CreateDirectory
#endif
#ifdef GetCurrentDirectory
#undef GetCurrentDirectory
#endif
#ifdef SetCurrentDirectory
#undef SetCurrentDirectory
#endif

namespace fs = std::filesystem;

namespace FileSystem {

// ── Free helpers ──────────────────────────────────────────────────────────────

/** @brief Convert a filesystem time point to a system_clock time point. */
std::chrono::system_clock::time_point FileTimeToTimePoint(const fs::file_time_type& ftime);

/** @brief Derive FileAttributes from filesystem metadata for the given path. */
FileAttributes GetFileAttributes(const fs::path& path);

/** @brief Build a FileInfo record from a filesystem path. */
FileInfo CreateFileInfo(const fs::path& path);

// ── StandardFile ──────────────────────────────────────────────────────────────

/** @brief Concrete IFile backed by std::fstream. */
class StandardFile : public IFile {
private:
    std::fstream m_stream;
    std::string  m_path;
    FileMode     m_mode;
    bool         m_isOpen = false;

public:
    StandardFile(const std::string& path, FileMode mode)
        : m_path(path), m_mode(mode), m_isOpen(false)
    {
        std::ios::openmode openMode = std::ios::binary;

        switch (mode)
        {
        case FileMode::Read:
            openMode |= std::ios::in;
            break;
        case FileMode::Write:
            openMode |= std::ios::out | std::ios::trunc;
            break;
        case FileMode::ReadWrite:
            openMode |= std::ios::in | std::ios::out;
            break;
        case FileMode::Append:
            openMode |= std::ios::out | std::ios::app;
            break;
        }

        m_stream.open(path, openMode);
        m_isOpen = m_stream.is_open();
    }

    ~StandardFile()
    {
        if (m_isOpen)
        {
            m_stream.close();
        }
    }

    Result<size_t> Read(void* buffer, size_t bytesToRead) override
    {
        if (!m_isOpen || !CanRead())
        {
            return Result<size_t>("File not open for reading");
        }

        m_stream.read(static_cast<char*>(buffer), bytesToRead);
        size_t bytesRead = static_cast<size_t>(m_stream.gcount());

        if (m_stream.bad())
        {
            return Result<size_t>("Read error occurred");
        }

        return Result<size_t>(std::move(bytesRead));
    }

    Result<size_t> Write(const void* buffer, size_t bytesToWrite) override
    {
        if (!m_isOpen || !CanWrite())
        {
            return Result<size_t>("File not open for writing");
        }

        m_stream.write(static_cast<const char*>(buffer), bytesToWrite);

        if (m_stream.bad())
        {
            return Result<size_t>("Write error occurred");
        }

        return Result<size_t>(std::move(bytesToWrite));
    }

    Result<void> Flush() override
    {
        if (!m_isOpen)
        {
            return Result<void>("File not open");
        }

        m_stream.flush();
        if (m_stream.bad())
        {
            return Result<void>("Flush failed");
        }

        return Result<void>();
    }

    Result<void> Close() override
    {
        if (m_isOpen)
        {
            m_stream.close();
            m_isOpen = false;
        }
        return Result<void>();
    }

    Result<uint64_t> Seek(int64_t offset, SeekOrigin origin) override
    {
        if (!m_isOpen)
        {
            return Result<uint64_t>("File not open");
        }

        std::ios::seekdir dir;
        switch (origin)
        {
        case SeekOrigin::Begin:
            dir = std::ios::beg;
            break;
        case SeekOrigin::Current:
            dir = std::ios::cur;
            break;
        case SeekOrigin::End:
            dir = std::ios::end;
            break;
        default:
            return Result<uint64_t>("Invalid seek origin");
        }

        m_stream.seekg(offset, dir);
        m_stream.seekp(offset, dir);

        if (m_stream.bad())
        {
            return Result<uint64_t>("Seek failed");
        }

        uint64_t pos = static_cast<uint64_t>(m_stream.tellg());
        return Result<uint64_t>(std::move(pos));
    }

    Result<uint64_t> GetPosition() override
    {
        if (!m_isOpen)
        {
            return Result<uint64_t>("File not open");
        }

        uint64_t pos = static_cast<uint64_t>(m_stream.tellg());
        return Result<uint64_t>(std::move(pos));
    }

    Result<uint64_t> GetSize() override
    {
        if (!m_isOpen)
        {
            return Result<uint64_t>("File not open");
        }

        std::error_code ec;
        auto size = fs::file_size(m_path, ec);
        if (ec)
        {
            return Result<uint64_t>("Could not get file size");
        }

        return Result<uint64_t>(std::move(size));
    }

    Result<FileInfo> GetInfo() override
    {
        if (!fs::exists(m_path))
        {
            return Result<FileInfo>("File does not exist");
        }

        FileInfo info = CreateFileInfo(m_path);
        return Result<FileInfo>(std::move(info));
    }

    bool IsOpen() const override { return m_isOpen; }

    bool CanRead() const override
    {
        return m_isOpen && (m_mode == FileMode::Read || m_mode == FileMode::ReadWrite);
    }

    bool CanWrite() const override
    {
        return m_isOpen && (m_mode == FileMode::Write || m_mode == FileMode::ReadWrite || m_mode == FileMode::Append);
    }

    Result<std::vector<uint8_t>> ReadAll() override
    {
        if (!CanRead())
        {
            return Result<std::vector<uint8_t>>("File not open for reading");
        }

        auto currentPos = m_stream.tellg();
        m_stream.seekg(0, std::ios::end);
        auto size = m_stream.tellg();
        m_stream.seekg(currentPos);

        std::vector<uint8_t> data(static_cast<size_t>(size));
        m_stream.read(reinterpret_cast<char*>(data.data()), size);

        if (m_stream.bad())
        {
            return Result<std::vector<uint8_t>>("Read error occurred");
        }

        return Result<std::vector<uint8_t>>(std::move(data));
    }

    Result<std::string> ReadAllText() override
    {
        auto bytesResult = ReadAll();
        if (bytesResult.HasError())
        {
            return Result<std::string>(bytesResult.GetError());
        }

        const auto& bytes = bytesResult.GetValue();
        std::string text(bytes.begin(), bytes.end());
        return Result<std::string>(std::move(text));
    }

    Result<void> WriteAllText(const std::string& text) override
    {
        if (!CanWrite())
        {
            return Result<void>("File not open for writing");
        }

        m_stream.write(text.c_str(), text.length());

        if (m_stream.bad())
        {
            return Result<void>("Write error occurred");
        }

        return Result<void>();
    }
};

// ── StandardDirectory ─────────────────────────────────────────────────────────

/** @brief Concrete IDirectory backed by std::filesystem. */
class StandardDirectory : public IDirectory {
private:
    fs::path m_path;

    bool MatchesPattern(const std::string& filename, const std::string& pattern)
    {
        std::string regexPattern = pattern;
        std::replace(regexPattern.begin(), regexPattern.end(), '*', '.');
        regexPattern = std::regex_replace(regexPattern, std::regex("\\."), ".*");
        regexPattern = std::regex_replace(regexPattern, std::regex("\\?"), ".");

        std::regex re(regexPattern, std::regex_constants::icase);
        return std::regex_match(filename, re);
    }

public:
    explicit StandardDirectory(const std::string& path) : m_path(path) {}

    Result<std::vector<FileInfo>> GetFiles() override
    {
        return GetFiles("*");
    }

    Result<std::vector<FileInfo>> GetDirectories() override
    {
        return GetDirectories("*");
    }

    Result<std::vector<FileInfo>> GetEntries() override
    {
        if (!fs::exists(m_path) || !fs::is_directory(m_path))
        {
            return Result<std::vector<FileInfo>>("Directory does not exist");
        }

        std::vector<FileInfo> entries;
        std::error_code ec;

        for (const auto& entry : fs::directory_iterator(m_path, ec))
        {
            if (ec) continue;
            entries.push_back(CreateFileInfo(entry.path()));
        }

        return Result<std::vector<FileInfo>>(std::move(entries));
    }

    Result<std::vector<FileInfo>> GetFiles(const std::string& pattern) override
    {
        if (!fs::exists(m_path) || !fs::is_directory(m_path))
        {
            return Result<std::vector<FileInfo>>("Directory does not exist");
        }

        std::vector<FileInfo> files;
        std::error_code ec;

        for (const auto& entry : fs::directory_iterator(m_path, ec))
        {
            if (ec) continue;
            if (entry.is_regular_file() && MatchesPattern(entry.path().filename().string(), pattern))
            {
                files.push_back(CreateFileInfo(entry.path()));
            }
        }

        return Result<std::vector<FileInfo>>(std::move(files));
    }

    Result<std::vector<FileInfo>> GetDirectories(const std::string& pattern) override
    {
        if (!fs::exists(m_path) || !fs::is_directory(m_path))
        {
            return Result<std::vector<FileInfo>>("Directory does not exist");
        }

        std::vector<FileInfo> directories;
        std::error_code ec;

        for (const auto& entry : fs::directory_iterator(m_path, ec))
        {
            if (ec) continue;
            if (entry.is_directory() && MatchesPattern(entry.path().filename().string(), pattern))
            {
                directories.push_back(CreateFileInfo(entry.path()));
            }
        }

        return Result<std::vector<FileInfo>>(std::move(directories));
    }

    Result<FileInfo> GetInfo() override
    {
        if (!fs::exists(m_path))
        {
            return Result<FileInfo>("Directory does not exist");
        }

        FileInfo info = CreateFileInfo(m_path);
        return Result<FileInfo>(std::move(info));
    }

    std::string GetPath() const override
    {
        return m_path.string();
    }
};

// ── StandardFileSystem ────────────────────────────────────────────────────────

/** @brief Concrete IFileSystem implementation using std::filesystem. */
class StandardFileSystem : public IFileSystem {
public:
    Result<std::unique_ptr<IFile>> OpenFile(const std::string& path, FileMode mode) override
    {
        auto file = std::make_unique<StandardFile>(path, mode);
        if (!file->IsOpen())
        {
            return Result<std::unique_ptr<IFile>>("Failed to open file: " + path);
        }
        return Result<std::unique_ptr<IFile>>(std::move(file));
    }

    Result<std::unique_ptr<IFile>> CreateFile(const std::string& path) override
    {
        fs::path fsPath(path);
        if (fsPath.has_parent_path())
        {
            std::error_code ec;
            fs::create_directories(fsPath.parent_path(), ec);
        }

        return OpenFile(path, FileMode::Write);
    }

    Result<void> DeleteFile(const std::string& path) override
    {
        std::error_code ec;
        if (!fs::remove(path, ec))
        {
            return Result<void>("Failed to delete file: " + path);
        }
        return Result<void>();
    }

    Result<void> CopyFile(const std::string& sourcePath, const std::string& destinationPath, bool overwrite) override
    {
        std::error_code ec;
        fs::copy_options options = fs::copy_options::none;
        if (overwrite)
        {
            options = fs::copy_options::overwrite_existing;
        }

        fs::copy_file(sourcePath, destinationPath, options, ec);
        if (ec)
        {
            return Result<void>("Failed to copy file: " + ec.message());
        }

        return Result<void>();
    }

    Result<void> MoveFile(const std::string& sourcePath, const std::string& destinationPath) override
    {
        std::error_code ec;
        fs::rename(sourcePath, destinationPath, ec);
        if (ec)
        {
            return Result<void>("Failed to move file: " + ec.message());
        }

        return Result<void>();
    }

    Result<std::unique_ptr<IDirectory>> OpenDirectory(const std::string& path) override
    {
        if (!fs::exists(path) || !fs::is_directory(path))
        {
            return Result<std::unique_ptr<IDirectory>>("Directory does not exist: " + path);
        }

        auto directory = std::make_unique<StandardDirectory>(path);
        return Result<std::unique_ptr<IDirectory>>(std::move(directory));
    }

    Result<void> CreateDirectory(const std::string& path) override
    {
        std::error_code ec;
        fs::create_directories(path, ec);
        if (ec)
        {
            return Result<void>("Failed to create directory: " + ec.message());
        }

        return Result<void>();
    }

    Result<void> DeleteDirectory(const std::string& path, bool recursive) override
    {
        std::error_code ec;
        if (recursive)
        {
            fs::remove_all(path, ec);
        }
        else
        {
            fs::remove(path, ec);
        }

        if (ec)
        {
            return Result<void>("Failed to delete directory: " + ec.message());
        }

        return Result<void>();
    }

    Result<void> CopyDirectory(const std::string& sourcePath, const std::string& destinationPath, bool recursive) override
    {
        std::error_code ec;
        fs::copy_options options = fs::copy_options::overwrite_existing;
        if (recursive)
        {
            options |= fs::copy_options::recursive;
        }

        fs::copy(sourcePath, destinationPath, options, ec);
        if (ec)
        {
            return Result<void>("Failed to copy directory: " + ec.message());
        }

        return Result<void>();
    }

    Result<void> MoveDirectory(const std::string& sourcePath, const std::string& destinationPath) override
    {
        std::error_code ec;
        fs::rename(sourcePath, destinationPath, ec);
        if (ec)
        {
            return Result<void>("Failed to move directory: " + ec.message());
        }

        return Result<void>();
    }

    bool Exists(const std::string& path) override
    {
        return fs::exists(path);
    }

    bool IsFile(const std::string& path) override
    {
        return fs::is_regular_file(path);
    }

    bool IsDirectory(const std::string& path) override
    {
        return fs::is_directory(path);
    }

    Result<FileInfo> GetInfo(const std::string& path) override
    {
        if (!fs::exists(path))
        {
            return Result<FileInfo>("Path does not exist: " + path);
        }

        FileInfo info = CreateFileInfo(path);
        return Result<FileInfo>(std::move(info));
    }

    std::string GetFileName(const std::string& path) override
    {
        return fs::path(path).filename().string();
    }

    std::string GetFileNameWithoutExtension(const std::string& path) override
    {
        return fs::path(path).stem().string();
    }

    std::string GetExtension(const std::string& path) override
    {
        return fs::path(path).extension().string();
    }

    std::string GetDirectoryName(const std::string& path) override
    {
        return NormalizePath(fs::path(path).parent_path().string());
    }

    std::string GetFullPath(const std::string& path) override
    {
        return fs::absolute(path).string();
    }

    std::string CombinePath(const std::string& path1, const std::string& path2) override
    {
        return NormalizePath((fs::path(path1) / path2).string());
    }

    Result<std::string> GetCurrentDirectory() override
    {
        std::error_code ec;
        auto current = fs::current_path(ec);
        if (ec)
        {
            return Result<std::string>("Failed to get current directory");
        }

        std::string path = current.string();
        return Result<std::string>(std::move(path));
    }

    Result<void> SetCurrentDirectory(const std::string& path) override
    {
        std::error_code ec;
        fs::current_path(path, ec);
        if (ec)
        {
            return Result<void>("Failed to set current directory");
        }

        return Result<void>();
    }

    std::string NormalizePath(const std::string& path)
    {
        std::string normalized = path;
        std::replace(normalized.begin(), normalized.end(), '\\', '/');
        return normalized;
    }

    Result<std::string> GetTempDirectory() override
    {
        std::error_code ec;
        auto temp = fs::temp_directory_path(ec);
        if (ec)
        {
            return Result<std::string>("Failed to get temp directory");
        }

        std::string path = temp.string();
        return Result<std::string>(std::move(path));
    }

    Result<std::string> GetUserDirectory() override
    {
        std::string path;
#if defined(_WIN32)
        char* buffer = nullptr;
        size_t len = 0;
        if (_dupenv_s(&buffer, &len, "USERPROFILE") == 0 && buffer && len > 0)
        {
            path = buffer;
            free(buffer);
        }
#elif defined(__unix__) || defined(__APPLE__)
        char* buffer = nullptr;
        size_t len = 0;
        if (_dupenv_s(&buffer, &len, "HOME") == 0 && buffer && len > 0)
        {
            path = buffer;
            free(buffer);
        }
#endif
        if (path.empty())
        {
            return Result<std::string>("Failed to get user directory");
        }
        return Result<std::string>(std::move(path));
    }

    Result<std::string> GetApplicationDirectory() override
    {
        std::error_code ec;
        auto current = fs::current_path(ec);
        if (ec)
        {
            return Result<std::string>("Failed to get application directory");
        }

        std::string path = current.string();
        return Result<std::string>(std::move(path));
    }

    Result<std::vector<uint8_t>> ReadAllBytes(const std::string& path) override
    {
        auto fileResult = OpenFile(path, FileMode::Read);
        if (fileResult.HasError())
        {
            return Result<std::vector<uint8_t>>(fileResult.GetError());
        }

        return fileResult.GetValue()->ReadAll();
    }

    Result<std::string> ReadAllText(const std::string& path) override
    {
        auto fileResult = OpenFile(path, FileMode::Read);
        if (fileResult.HasError())
        {
            return Result<std::string>(fileResult.GetError());
        }

        return fileResult.GetValue()->ReadAllText();
    }

    Result<void> WriteAllBytes(const std::string& path, const std::vector<uint8_t>& data) override
    {
        auto fileResult = CreateFile(path);
        if (fileResult.HasError())
        {
            return Result<void>(fileResult.GetError());
        }

        auto& file = fileResult.GetValue();
        auto writeResult = file->Write(data.data(), data.size());
        if (writeResult.HasError())
        {
            return Result<void>(writeResult.GetError());
        }

        return file->Close();
    }

    Result<void> WriteAllText(const std::string& path, const std::string& text) override
    {
        auto fileResult = CreateFile(path);
        if (fileResult.HasError())
        {
            return Result<void>(fileResult.GetError());
        }

        auto& file = fileResult.GetValue();
        auto writeResult = file->WriteAllText(text);
        if (writeResult.HasError())
        {
            return Result<void>(writeResult.GetError());
        }

        return file->Close();
    }
};

/** @brief Factory function to create a StandardFileSystem instance. */
std::unique_ptr<IFileSystem> CreateStandardFileSystem();

} // namespace FileSystem
