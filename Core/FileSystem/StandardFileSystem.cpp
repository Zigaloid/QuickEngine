

#include "StandardFileSystem.h"

namespace fs = std::filesystem;

namespace FileSystem 
{
    // Factory function to create the standard filesystem
    std::unique_ptr<IFileSystem> CreateStandardFileSystem() {
        return std::make_unique<StandardFileSystem>();
    }
    // Helper function to convert filesystem time to chrono time
    std::chrono::system_clock::time_point FileTimeToTimePoint(const fs::file_time_type& ftime) {
        // Convert filesystem time to system clock time
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
        return sctp;
    }

    // Helper function to convert filesystem permissions to FileAttributes
    FileAttributes GetFileAttributes(const fs::path& path) {
        FileAttributes attrs = FileAttributes::None;

        if (fs::is_directory(path)) {
            attrs = attrs | FileAttributes::Directory;
        }

        // Check if file is read-only (no write permissions)
        auto perms = fs::status(path).permissions();
        if ((perms & fs::perms::owner_write) == fs::perms::none &&
            (perms & fs::perms::group_write) == fs::perms::none &&
            (perms & fs::perms::others_write) == fs::perms::none) {
            attrs = attrs | FileAttributes::ReadOnly;
        }

        // On Windows, we could check for hidden/system attributes
        // For cross-platform compatibility, we'll use filename conventions
        std::string filename = path.filename().string();
        if (!filename.empty() && filename[0] == '.') {
            attrs = attrs | FileAttributes::Hidden;
        }

        return attrs;
    }

    // Helper function to create FileInfo from filesystem path
    FileInfo CreateFileInfo(const fs::path& path) {
        FileInfo info;
        info.name = path.filename().string();
        info.fullPath = fs::absolute(path).string();
        info.isDirectory = fs::is_directory(path);
        info.attributes = GetFileAttributes(path);

        if (!info.isDirectory) {
            std::error_code ec;
            info.size = fs::file_size(path, ec);
            if (ec) info.size = 0;
        }
        else {
            info.size = 0;
        }

        // Get file times
        std::error_code ec;
        auto lwt = fs::last_write_time(path, ec);
        if (!ec) {
            info.lastWriteTime = FileTimeToTimePoint(lwt);
            info.lastAccessTime = info.lastWriteTime; // Filesystem doesn't always track access time
            info.creationTime = info.lastWriteTime;   // Use write time as approximation
        }

        return info;
    }

} // namespace FileSystem