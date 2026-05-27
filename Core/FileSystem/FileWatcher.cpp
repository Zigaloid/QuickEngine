#include "FileWatcher.h"

#include <memory>
#include <stdexcept>

namespace FileSystem {

// ── Helpers ───────────────────────────────────────────────────────────────────

static FileChangeEvent ActionToEvent(DWORD action)
{
    switch (action)
    {
        case FILE_ACTION_ADDED:            return FileChangeEvent::Added;
        case FILE_ACTION_REMOVED:          return FileChangeEvent::Removed;
        case FILE_ACTION_MODIFIED:         return FileChangeEvent::Modified;
        case FILE_ACTION_RENAMED_OLD_NAME: return FileChangeEvent::RenamedOld;
        case FILE_ACTION_RENAMED_NEW_NAME: return FileChangeEvent::RenamedNew;
        default:                           return FileChangeEvent::Modified;
    }
}

// ── FileWatcher ───────────────────────────────────────────────────────────────

bool FileWatcher::AddWatch(const std::string& directoryPath, ChangeCallback callback, bool recursive)
{
    std::lock_guard lock(m_mutex);

    if (m_watches.contains(directoryPath))
        return true; // Already watching

    auto uniqueEntry = std::make_unique<WatchEntry>();
    uniqueEntry->directoryPath = directoryPath;
    uniqueEntry->callback      = std::move(callback);
    uniqueEntry->recursive     = recursive;

    uniqueEntry->dirHandle = ::CreateFileA(
        directoryPath.c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        nullptr
    );

    if (uniqueEntry->dirHandle == INVALID_HANDLE_VALUE)
        return false;

    uniqueEntry->stopEvent = ::CreateEventA(nullptr, TRUE, FALSE, nullptr);
    if (uniqueEntry->stopEvent == INVALID_HANDLE_VALUE)
    {
        ::CloseHandle(uniqueEntry->dirHandle);
        return false;
    }

    uniqueEntry->running = true;
    WatchEntry* entryPtr = uniqueEntry.get();
    m_watches.emplace(directoryPath, std::move(uniqueEntry));
    entryPtr->thread = std::thread(WatchThreadProc, entryPtr);

    return true;
}

void FileWatcher::RemoveWatch(const std::string& directoryPath)
{
    std::unique_lock lock(m_mutex);

    auto it = m_watches.find(directoryPath);
    if (it == m_watches.end())
        return;

    WatchEntry* entry = it->second.get();
    entry->running = false;
    ::SetEvent(entry->stopEvent);
    lock.unlock();

    if (entry->thread.joinable())
        entry->thread.join();

    ::CloseHandle(entry->dirHandle);
    ::CloseHandle(entry->stopEvent);

    lock.lock();
    m_watches.erase(it);
}

void FileWatcher::StopAll()
{
    std::unique_lock lock(m_mutex);

    for (auto& [path, entry] : m_watches)
    {
        entry->running = false;
        ::SetEvent(entry->stopEvent);
    }
    lock.unlock();

    for (auto& [path, entry] : m_watches)
    {
        if (entry->thread.joinable())
            entry->thread.join();

        ::CloseHandle(entry->dirHandle);
        ::CloseHandle(entry->stopEvent);
    }

    lock.lock();
    m_watches.clear();
}

// ── Watch thread ──────────────────────────────────────────────────────────────

void FileWatcher::WatchThreadProc(WatchEntry* entry)
{
    constexpr DWORD k_notifyFilter =
        FILE_NOTIFY_CHANGE_FILE_NAME  |
        FILE_NOTIFY_CHANGE_DIR_NAME   |
        FILE_NOTIFY_CHANGE_SIZE       |
        FILE_NOTIFY_CHANGE_LAST_WRITE |
        FILE_NOTIFY_CHANGE_CREATION;

    constexpr size_t k_bufferSize = 64 * 1024; // 64 KB
    std::vector<BYTE> buffer(k_bufferSize);

    OVERLAPPED overlapped{};
    overlapped.hEvent = ::CreateEventA(nullptr, TRUE, FALSE, nullptr);

    while (entry->running)
    {
        ::ResetEvent(overlapped.hEvent);

        DWORD bytesReturned = 0;
        BOOL  queued = ::ReadDirectoryChangesW(
            entry->dirHandle,
            buffer.data(),
            static_cast<DWORD>(buffer.size()),
            entry->recursive ? TRUE : FALSE,
            k_notifyFilter,
            &bytesReturned,
            &overlapped,
            nullptr
        );

        if (!queued)
            break;

        // Wait for either a change notification or the stop event
        HANDLE waitHandles[2] = { overlapped.hEvent, entry->stopEvent };
        DWORD  waitResult = ::WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);

        if (waitResult == WAIT_OBJECT_0 + 1 || !entry->running)
            break; // Stop event signalled

        if (waitResult != WAIT_OBJECT_0)
            break; // Unexpected error

        if (!::GetOverlappedResult(entry->dirHandle, &overlapped, &bytesReturned, FALSE))
            break;

        if (bytesReturned == 0)
            continue; // Buffer overflow — changes occurred but details lost

        // Parse the notification records
        const BYTE* ptr = buffer.data();
        while (true)
        {
            const auto* info = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(ptr);

            // Convert wide filename to narrow string
            int         len      = WideCharToMultiByte(CP_UTF8, 0, info->FileName,
                                       static_cast<int>(info->FileNameLength / sizeof(WCHAR)),
                                       nullptr, 0, nullptr, nullptr);
            std::string fileName(len, '\0');
            WideCharToMultiByte(CP_UTF8, 0, info->FileName,
                static_cast<int>(info->FileNameLength / sizeof(WCHAR)),
                fileName.data(), len, nullptr, nullptr);

            std::string fullPath = entry->directoryPath + "\\" + fileName;

            entry->callback(fullPath, ActionToEvent(info->Action));

            if (info->NextEntryOffset == 0)
                break;

            ptr += info->NextEntryOffset;
        }
    }

    ::CancelIo(entry->dirHandle);
    ::CloseHandle(overlapped.hEvent);
}

} // namespace FileSystem