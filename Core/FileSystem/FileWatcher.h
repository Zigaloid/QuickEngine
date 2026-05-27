#pragma once

#include <string>
#include <functional>
#include <unordered_map>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

// Undefine Windows macros that collide with FileSystem method names
#ifdef CopyFile
#undef CopyFile
#endif
#ifdef DeleteFile
#undef DeleteFile
#endif
#ifdef MoveFile
#undef MoveFile
#endif
#ifdef CreateFile
#undef CreateFile
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

namespace FileSystem {

// ── FileChangeEvent ───────────────────────────────────────────────────────────

/** @brief The type of change that occurred on a watched file or directory. */
enum class FileChangeEvent {
    Added,
    Removed,
    Modified,
    RenamedOld,
    RenamedNew
};

// ── FileWatcher ───────────────────────────────────────────────────────────────

/**
 * @brief Watches one or more directory paths for file changes.
 *
 * Add a watch via AddWatch(), supplying a directory path and a callback.
 * The callback is invoked on an internal background thread whenever a
 * file-system change is detected inside that directory.
 *
 * Example:
 * @code
 *   FileSystem::FileWatcher watcher;
 *   watcher.AddWatch("C:/MyProject/Assets",
 *       [](const std::string& path, FileSystem::FileChangeEvent event) {
 *           // Handle change...
 *       });
 * @endcode
 */
class FileWatcher {
public:
    using ChangeCallback = std::function<void(const std::string& filePath, FileChangeEvent event)>;

    FileWatcher()  = default;
    ~FileWatcher() { StopAll(); }

    // Non-copyable, movable
    FileWatcher(const FileWatcher&)            = delete;
    FileWatcher& operator=(const FileWatcher&) = delete;
    FileWatcher(FileWatcher&&)                 = default;
    FileWatcher& operator=(FileWatcher&&)      = default;

    /**
     * @brief Add a directory to watch.
     * @param directoryPath  Absolute or relative path to the directory.
     * @param callback       Invoked on change with the affected file path and event type.
     * @param recursive      If true, subdirectories are also watched.
     * @return               True on success, false if the path could not be opened.
     */
    bool AddWatch(const std::string& directoryPath, ChangeCallback callback, bool recursive = true);

    /**
     * @brief Remove a previously added watch by directory path.
     * @param directoryPath  The same path passed to AddWatch().
     */
    void RemoveWatch(const std::string& directoryPath);

    /** @brief Stop all watches and join background threads. */
    void StopAll();

private:
    struct WatchEntry {
        std::string      directoryPath;
        ChangeCallback   callback;
        bool             recursive    = true;
        HANDLE           dirHandle    = INVALID_HANDLE_VALUE;
        HANDLE           stopEvent    = INVALID_HANDLE_VALUE;
        std::thread      thread;
        std::atomic_bool running{ false };
    };

    std::mutex                                                              m_mutex;
    std::unordered_map<std::string, std::unique_ptr<WatchEntry>>       m_watches;

    static void WatchThreadProc(WatchEntry* entry);
};

} // namespace FileSystem