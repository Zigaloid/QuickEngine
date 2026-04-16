#include "StandardFileSystem.h"

#include <filesystem>

namespace fs = std::filesystem;

namespace FileSystem {

// ── Factory ───────────────────────────────────────────────────────────────────

std::unique_ptr<IFileSystem> CreateStandardFileSystem()
{
    return std::make_unique<StandardFileSystem>();
}

// ── Free helpers ──────────────────────────────────────────────────────────────

std::chrono::system_clock::time_point FileTimeToTimePoint(const fs::file_time_type& ftime)
{
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
    return sctp;
}

FileAttributes GetFileAttributes(const fs::path& path)
{
    FileAttributes attrs = FileAttributes::None;

    if (fs::is_directory(path))
    {
        attrs = attrs | FileAttributes::Directory;
    }

    auto perms = fs::status(path).permissions();
    if ((perms & fs::perms::owner_write) == fs::perms::none &&
        (perms & fs::perms::group_write) == fs::perms::none &&
        (perms & fs::perms::others_write) == fs::perms::none)
    {
        attrs = attrs | FileAttributes::ReadOnly;
    }

    std::string filename = path.filename().string();
    if (!filename.empty() && filename[0] == '.')
    {
        attrs = attrs | FileAttributes::Hidden;
    }

    return attrs;
}

FileInfo CreateFileInfo(const fs::path& path)
{
    FileInfo info;
    info.name        = path.filename().string();
    info.fullPath    = fs::absolute(path).string();
    info.isDirectory = fs::is_directory(path);
    info.attributes  = GetFileAttributes(path);

    if (!info.isDirectory)
    {
        std::error_code ec;
        info.size = fs::file_size(path, ec);
        if (ec) info.size = 0;
    }
    else
    {
        info.size = 0;
    }

    std::error_code ec;
    auto lwt = fs::last_write_time(path, ec);
    if (!ec)
    {
        info.lastWriteTime  = FileTimeToTimePoint(lwt);
        info.lastAccessTime = info.lastWriteTime;
        info.creationTime   = info.lastWriteTime;
    }

    return info;
}

} // namespace FileSystem
