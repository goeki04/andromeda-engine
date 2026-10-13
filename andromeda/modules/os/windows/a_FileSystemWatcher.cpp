#include "a_FileSystemWatcher.hpp"

#ifdef _WIN32

// Before <windows.h>: a_logger.hpp drags in spdlog, and the min/max macros break any
// header that declares those names. The os target sets NOMINMAX for its consumers, but a
// define on the command line is the only thing that is order-independent, so the include
// order here still matters.
#include "a_logger.hpp"

#include <chrono>
#include <thread>
#include <unordered_map>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

namespace Andromeda::OS {

    namespace {

        /// Creation and deletion (FILE_NAME/DIR_NAME) plus actual writes. ATTRIBUTES is
        /// deliberately absent - it fires for things like the read-only flag, which no
        /// asset reload cares about.
        constexpr DWORD kNotifyFilter = FILE_NOTIFY_CHANGE_FILE_NAME |
                                        FILE_NOTIFY_CHANGE_DIR_NAME |
                                        FILE_NOTIFY_CHANGE_SIZE |
                                        FILE_NOTIFY_CHANGE_LAST_WRITE;

        /// 64 KiB. Also the hard maximum for directories on a network share, and large
        /// enough that a batch rebuild of the asset folder does not overflow it.
        constexpr size_t kBufferElements = 16384;

        /// How long a path has to stay quiet before its event is delivered.
        constexpr std::chrono::milliseconds kDebounceDelay{200};

        /// Wake-up interval while an I/O is pending, so debounced events get flushed even
        /// when nothing else happens.
        constexpr DWORD kFlushInterval = 50;

        FileAction toFileAction(DWORD action) {
            switch (action) {
                case FILE_ACTION_ADDED:            return FileAction::Added;
                case FILE_ACTION_REMOVED:          return FileAction::Removed;
                case FILE_ACTION_MODIFIED:         return FileAction::Modified;
                case FILE_ACTION_RENAMED_OLD_NAME: return FileAction::RenamedFrom;
                case FILE_ACTION_RENAMED_NEW_NAME: return FileAction::RenamedTo;
                default:                           return FileAction::Modified;
            }
        }
    }

    struct FileSystemWatcher::Impl {
        std::filesystem::path directory;
        FileChangedCallback callback;
        bool watchSubtree = true;

        HANDLE hDir = INVALID_HANDLE_VALUE;
        HANDLE hChange = nullptr; ///< Signalled by the kernel when the buffer is filled.
        HANDLE hStop = nullptr;   ///< Signalled by the destructor to end the loop.

        std::thread worker;
        std::vector<DWORD> buffer = std::vector<DWORD>(kBufferElements);

        struct PendingEvent {
            FileAction action;
            std::chrono::steady_clock::time_point stamp;
        };
        /// Keyed by path, so repeated writes to one file collapse into a single callback.
        std::unordered_map<std::wstring, PendingEvent> pending;

        void run();
        void parse(DWORD bytesReturned);
        void enqueue(FileAction action, const std::filesystem::path& path);
        void flush(bool force);

        DWORD bufferBytes() const { return static_cast<DWORD>(buffer.size() * sizeof(DWORD)); }
    };

    void FileSystemWatcher::Impl::run() {
        HANDLE waits[2] = {hChange, hStop};
        OVERLAPPED ov{};

        for (;;) {
            // Manual-reset event, so clearing it is this loop's job. The OVERLAPPED is
            // reset as well: its Internal fields carry kernel state from the previous
            // read, and reusing them unzeroed makes the next read fail in ways that are
            // hard to trace back to here.
            ResetEvent(hChange);
            ov = OVERLAPPED{};
            ov.hEvent = hChange;

            if (ReadDirectoryChangesW(hDir, buffer.data(), bufferBytes(),
                                      watchSubtree ? TRUE : FALSE, kNotifyFilter,
                                      nullptr, &ov, nullptr) == FALSE) {
                const DWORD error = GetLastError();
                // FALSE with ERROR_IO_PENDING is the normal case for overlapped I/O - the
                // read was accepted and is still running. Anything else is fatal for this
                // watcher.
                if (error != ERROR_IO_PENDING) {
                    A_ERROR("FileSystemWatcher: ReadDirectoryChangesW failed on '{}' (error {})",
                            directory.string(), error);
                    return;
                }
            }

            // Wait for the read to complete or for the stop signal, flushing debounced
            // events on every timeout. The read stays pending across timeouts, so the
            // flush must not fall out of this inner loop and issue a second read.
            DWORD wait = WAIT_TIMEOUT;
            while ((wait = WaitForMultipleObjects(2, waits, FALSE, kFlushInterval)) == WAIT_TIMEOUT) {
                flush(false);
            }

            if (wait != WAIT_OBJECT_0) {
                if (wait == WAIT_FAILED) {
                    A_ERROR("FileSystemWatcher: WaitForMultipleObjects failed (error {})", GetLastError());
                }
                // Stop requested, or the wait broke. The kernel still holds the addresses
                // of ov and buffer, both of which belong to this frame - cancel the read
                // before they go away.
                CancelIoEx(hDir, &ov);
                break;
            }

            DWORD bytesReturned = 0;
            if (GetOverlappedResult(hDir, &ov, &bytesReturned, FALSE) == FALSE) {
                A_ERROR("FileSystemWatcher: GetOverlappedResult failed (error {})", GetLastError());
                return;
            }

            if (bytesReturned == 0) {
                // The change queue overflowed and Windows dropped the notifications.
                // Nothing in the buffer is valid; all the consumer can do is re-read the
                // directory itself.
                A_WARN("FileSystemWatcher: notification buffer overflowed, '{}' needs a rescan",
                       directory.string());
                enqueue(FileAction::Rescan, directory);
            } else {
                parse(bytesReturned);
            }
        }

        // Deliver whatever is still sitting in the debounce map, so a change immediately
        // before shutdown is not silently lost.
        flush(true);
    }

    void FileSystemWatcher::Impl::parse(DWORD bytesReturned) {
        const BYTE* base = reinterpret_cast<const BYTE*>(buffer.data());
        DWORD offset = 0;

        for (;;) {
            // NextEntryOffset comes from the kernel, but a truncated last record would
            // walk us past the buffer - bail out instead of trusting it blindly.
            if (offset + sizeof(FILE_NOTIFY_INFORMATION) > bytesReturned) {
                break;
            }

            const auto* info = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(base + offset);

            // FileNameLength is in bytes and the name is not null-terminated, so the
            // length has to be passed explicitly everywhere.
            std::wstring name(info->FileName, info->FileNameLength / sizeof(WCHAR));
            enqueue(toFileAction(info->Action), directory / name);

            if (info->NextEntryOffset == 0) {
                break;
            }
            offset += info->NextEntryOffset;
        }
    }

    void FileSystemWatcher::Impl::enqueue(const FileAction action, const std::filesystem::path& path) {
        // Normalised here rather than at the call sites, so every consumer downstream sees
        // one spelling of a path. Appending a name produces a native separator on Windows,
        // which would otherwise leave events looking like "C:/assets/models\thing.fbx" -
        // mixed enough to break string comparisons and to split one file across two
        // entries in the map below.
        std::wstring key = path.lexically_normal().generic_wstring();

        // Overwriting an existing entry is the point: a save that produces MODIFIED three
        // times in 20 ms ends up as one callback, with the timer restarted each time.
        pending[std::move(key)] = PendingEvent{action, std::chrono::steady_clock::now()};
    }

    void FileSystemWatcher::Impl::flush(const bool force) {
        if (pending.empty()) {
            return;
        }

        const auto now = std::chrono::steady_clock::now();

        for (auto it = pending.begin(); it != pending.end();) {
            if (force || now - it->second.stamp >= kDebounceDelay) {
                if (callback) {
                    callback(it->second.action, std::filesystem::path(it->first));
                }
                it = pending.erase(it);
            } else {
                ++it;
            }
        }
    }

    FileSystemWatcher::FileSystemWatcher(std::filesystem::path directory,
                                         FileChangedCallback callback,
                                         bool watchSubtree)
        : m_impl(std::make_unique<Impl>()) {
        // The path is copied into the Impl rather than kept as a pointer: the watch thread
        // outlives the caller's argument.
        m_impl->directory = std::move(directory);
        m_impl->callback = std::move(callback);
        m_impl->watchSubtree = watchSubtree;

        // FILE_LIST_DIRECTORY rather than GENERIC_READ - listing is what
        // ReadDirectoryChangesW needs, and asking for less fails on fewer ACLs.
        // FILE_FLAG_BACKUP_SEMANTICS is the only way to get a handle to a directory at
        // all; FILE_FLAG_OVERLAPPED is what makes the watch cancellable.
        m_impl->hDir = CreateFileW(m_impl->directory.c_str(),
                                   FILE_LIST_DIRECTORY,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                   nullptr,
                                   OPEN_EXISTING,
                                   FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                                   nullptr);

        if (m_impl->hDir == INVALID_HANDLE_VALUE) {
            A_ERROR("FileSystemWatcher: cannot open '{}' (error {})",
                    m_impl->directory.string(), GetLastError());
            return;
        }

        // Both manual-reset: GetOverlappedResult inspects hChange after the wait, and an
        // auto-reset event would already be clear by then.
        m_impl->hChange = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        m_impl->hStop = CreateEventW(nullptr, TRUE, FALSE, nullptr);

        if (m_impl->hChange == nullptr || m_impl->hStop == nullptr) {
            A_ERROR("FileSystemWatcher: CreateEventW failed (error {})", GetLastError());
            if (m_impl->hChange != nullptr) {
                CloseHandle(m_impl->hChange);
                m_impl->hChange = nullptr;
            }
            CloseHandle(m_impl->hDir);
            m_impl->hDir = INVALID_HANDLE_VALUE;
            return;
        }

        // Everything that can fail has happened by now, so the thread starts on a fully
        // built Impl.
        m_impl->worker = std::thread([impl = m_impl.get()] { impl->run(); });
    }

    FileSystemWatcher::~FileSystemWatcher() {
        // Order matters: signal, join, then close. Closing first would leave the thread
        // calling into handles that no longer exist.
        if (m_impl->hStop != nullptr) {
            SetEvent(m_impl->hStop);
        }
        if (m_impl->worker.joinable()) {
            m_impl->worker.join();
        }
        if (m_impl->hChange != nullptr) {
            CloseHandle(m_impl->hChange);
        }
        if (m_impl->hStop != nullptr) {
            CloseHandle(m_impl->hStop);
        }
        if (m_impl->hDir != INVALID_HANDLE_VALUE) {
            CloseHandle(m_impl->hDir);
        }
    }

    bool FileSystemWatcher::isWatching() const {
        return m_impl->worker.joinable();
    }

    const std::filesystem::path& FileSystemWatcher::directory() const {
        return m_impl->directory;
    }
}

#endif
