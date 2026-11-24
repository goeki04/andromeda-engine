#include "a_FileSystemWatcher.hpp"

#ifdef __APPLE__

#include "a_logger.hpp"

#include <atomic>
#include <chrono>
#include <string>
#include <system_error>
#include <unordered_map>

// System headers last: CoreServices pulls in a lot of macros, keep them away from ours.
#include <CoreServices/CoreServices.h>
#include <dispatch/dispatch.h>

namespace Andromeda::OS {

    namespace {

        /// How long a path has to stay quiet before its event is delivered (same as Windows).
        constexpr std::chrono::milliseconds kDebounceDelay{200};

        /// How often the debounce map is checked for events that are due.
        constexpr std::chrono::milliseconds kFlushInterval{50};

        /// Seconds FSEvents collects changes before calling us. Small, because the real
        /// debouncing happens per path in Impl::flush.
        constexpr CFTimeInterval kLatency = 0.05;

        /// The kernel or fseventsd dropped events - only a full rescan is reliable now.
        constexpr FSEventStreamEventFlags kDroppedFlags = kFSEventStreamEventFlagMustScanSubDirs |
                                                          kFSEventStreamEventFlagUserDropped |
                                                          kFSEventStreamEventFlagKernelDropped;

        /// Changes an asset reload cares about. Pure metadata changes (xattr, owner,
        /// Finder info) are ignored, like FILE_NOTIFY_CHANGE_ATTRIBUTES on Windows.
        constexpr FSEventStreamEventFlags kContentFlags = kFSEventStreamEventFlagItemCreated |
                                                          kFSEventStreamEventFlagItemRemoved |
                                                          kFSEventStreamEventFlagItemModified |
                                                          kFSEventStreamEventFlagItemRenamed;
    }

    struct FileSystemWatcher::Impl {
        std::filesystem::path directory;          ///< As the caller passed it, used in callbacks.
        std::filesystem::path canonicalDirectory; ///< Resolved, the form FSEvents reports.
        FileChangedCallback callback;
        bool watchSubtree = true;

        FSEventStreamRef stream = nullptr;
        dispatch_queue_t queue = nullptr;       ///< Serial - this is the watcher "thread".
        dispatch_source_t flushTimer = nullptr; ///< Runs flush(false) on the queue.
        std::atomic<bool> watching{false};

        struct PendingEvent {
            FileAction action;
            std::chrono::steady_clock::time_point stamp;
        };
        /// Keyed by path, so repeated writes to one file collapse into a single callback.
        /// Only touched on `queue`, which is serial, so it needs no mutex.
        std::unordered_map<std::string, PendingEvent> pending;

        static void onEvents(ConstFSEventStreamRef streamRef, void* info, size_t count,
                             void* eventPaths, const FSEventStreamEventFlags flags[],
                             const FSEventStreamEventId ids[]);
        static void onFlushTimer(void* info);

        void handle(const char* rawPath, FSEventStreamEventFlags flags);
        void enqueue(FileAction action, const std::filesystem::path& path);
        void flush(bool force);
        void shutdown();
    };

    void FileSystemWatcher::Impl::onEvents(ConstFSEventStreamRef, void* info, const size_t count,
                                           void* eventPaths, const FSEventStreamEventFlags flags[],
                                           const FSEventStreamEventId[]) {
        auto* impl = static_cast<Impl*>(info);
        // Without kFSEventStreamCreateFlagUseCFTypes the paths are plain UTF-8 C strings.
        auto* const* paths = static_cast<char* const*>(eventPaths);
        for (size_t i = 0; i < count; ++i) {
            impl->handle(paths[i], flags[i]);
        }
    }

    void FileSystemWatcher::Impl::onFlushTimer(void* info) {
        static_cast<Impl*>(info)->flush(false);
    }

    void FileSystemWatcher::Impl::handle(const char* rawPath, const FSEventStreamEventFlags flags) {
        if ((flags & kDroppedFlags) != 0) {
            A_WARN("FileSystemWatcher: events were dropped, '{}' needs a rescan", directory.string());
            enqueue(FileAction::Rescan, directory);
            return;
        }

        if ((flags & kFSEventStreamEventFlagRootChanged) != 0) {
            // The watched directory itself was moved or deleted - the contract says that
            // ends the watch.
            A_WARN("FileSystemWatcher: '{}' was moved or deleted, watch ended", directory.string());
            watching = false;
            return;
        }

        if ((flags & kContentFlags) == 0) {
            return;
        }

        // FSEvents reports resolved paths (/private/var/... for /var/...). Map them back onto
        // the directory as the caller spelled it, so callbacks look the same as on Windows.
        const std::filesystem::path path(rawPath);
        const std::filesystem::path relative = path.lexically_relative(canonicalDirectory);
        if (relative.empty() || relative == "." || *relative.begin() == "..") {
            return; // The directory itself, or something outside of it.
        }
        // FSEvents always watches the whole subtree; filter here when only the top level is wanted.
        if (!watchSubtree && relative.has_parent_path()) {
            return;
        }

        // FSEvents coalesces and keeps history: one event can carry Created | Modified |
        // Removed | Renamed for the same path, including flags from earlier changes (a file
        // that was renamed into place still has ItemRenamed when it is deleted later). What
        // is on disk now decides, the flags only narrow it down.
        std::error_code ec;
        const bool exists = std::filesystem::exists(path, ec);

        const bool created = (flags & kFSEventStreamEventFlagItemCreated) != 0;
        const bool removed = (flags & kFSEventStreamEventFlagItemRemoved) != 0;
        const bool modified = (flags & kFSEventStreamEventFlagItemModified) != 0;
        const bool renamed = (flags & kFSEventStreamEventFlagItemRenamed) != 0;

        FileAction action;
        if (!exists) {
            // Gone: deleted, or the old name of a rename.
            action = (renamed && !removed) ? FileAction::RenamedFrom : FileAction::Removed;
        } else if (modified) {
            action = FileAction::Modified;
        } else if (created) {
            action = FileAction::Added;
        } else if (renamed) {
            // The new name of a rename.
            action = FileAction::RenamedTo;
        } else {
            action = FileAction::Modified;
        }

        enqueue(action, directory / relative);
    }

    void FileSystemWatcher::Impl::enqueue(const FileAction action, const std::filesystem::path& path) {
        // One spelling per path, so the map below never splits one file across two entries.
        std::string key = path.lexically_normal().generic_string();

        // Overwriting an existing entry is the point: a save that produces several events
        // in a few milliseconds ends up as one callback, with the timer restarted each time.
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

    void FileSystemWatcher::Impl::shutdown() {
        watching = false;

        // Stop + Invalidate: FSEvents queues no further callbacks after this.
        if (stream != nullptr) {
            FSEventStreamStop(stream);
            FSEventStreamInvalidate(stream);
            FSEventStreamRelease(stream);
            stream = nullptr;
        }
        // After cancel the timer handler is not started again.
        if (flushTimer != nullptr) {
            dispatch_source_cancel(flushTimer);
        }
        if (queue != nullptr) {
            // Runs after everything already queued (a callback may still be in flight), and
            // delivers whatever is left in the debounce map, so a change immediately before
            // shutdown is not silently lost.
            dispatch_sync_f(queue, this, [](void* info) { static_cast<Impl*>(info)->flush(true); });
        }
        // Plain C++ (no ARC), so dispatch objects are released by hand.
        if (flushTimer != nullptr) {
            dispatch_release(flushTimer);
            flushTimer = nullptr;
        }
        if (queue != nullptr) {
            dispatch_release(queue);
            queue = nullptr;
        }
    }

    FileSystemWatcher::FileSystemWatcher(std::filesystem::path directory,
                                         FileChangedCallback callback,
                                         bool watchSubtree)
        : m_impl(std::make_unique<Impl>()) {
        m_impl->directory = std::move(directory);
        m_impl->callback = std::move(callback);
        m_impl->watchSubtree = watchSubtree;

        std::error_code ec;
        if (!std::filesystem::is_directory(m_impl->directory, ec)) {
            A_ERROR("FileSystemWatcher: cannot open '{}' (not a directory)", m_impl->directory.string());
            return;
        }
        m_impl->canonicalDirectory = std::filesystem::canonical(m_impl->directory, ec);
        if (ec) {
            A_ERROR("FileSystemWatcher: cannot resolve '{}' ({})", m_impl->directory.string(), ec.message());
            return;
        }

        CFStringRef cfPath = CFStringCreateWithFileSystemRepresentation(
            kCFAllocatorDefault, m_impl->canonicalDirectory.c_str());
        const void* values[] = {cfPath};
        CFArrayRef pathsToWatch = CFArrayCreate(kCFAllocatorDefault, values, 1, &kCFTypeArrayCallBacks);

        FSEventStreamContext context{};
        context.info = m_impl.get();

        // FileEvents: one event per file instead of per directory.
        // NoDefer: deliver the first event of a burst right away instead of after kLatency.
        // WatchRoot: report when the watched directory itself goes away (RootChanged).
        const FSEventStreamCreateFlags createFlags = kFSEventStreamCreateFlagFileEvents |
                                                     kFSEventStreamCreateFlagNoDefer |
                                                     kFSEventStreamCreateFlagWatchRoot;

        m_impl->stream = FSEventStreamCreate(kCFAllocatorDefault, &Impl::onEvents, &context,
                                             pathsToWatch, kFSEventStreamEventIdSinceNow,
                                             kLatency, createFlags);
        CFRelease(pathsToWatch);
        CFRelease(cfPath);

        if (m_impl->stream == nullptr) {
            A_ERROR("FileSystemWatcher: FSEventStreamCreate failed for '{}'", m_impl->directory.string());
            return;
        }

        m_impl->queue = dispatch_queue_create("andromeda.filesystem-watcher", DISPATCH_QUEUE_SERIAL);
        FSEventStreamSetDispatchQueue(m_impl->stream, m_impl->queue);

        // Periodic flush of the debounce map, on the same serial queue as the events.
        const auto interval = std::chrono::duration_cast<std::chrono::nanoseconds>(kFlushInterval).count();
        m_impl->flushTimer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, m_impl->queue);
        dispatch_source_set_timer(m_impl->flushTimer, dispatch_time(DISPATCH_TIME_NOW, interval),
                                  static_cast<uint64_t>(interval), static_cast<uint64_t>(interval / 10));
        dispatch_set_context(m_impl->flushTimer, m_impl.get());
        dispatch_source_set_event_handler_f(m_impl->flushTimer, &Impl::onFlushTimer);
        dispatch_resume(m_impl->flushTimer);

        if (!FSEventStreamStart(m_impl->stream)) {
            A_ERROR("FileSystemWatcher: FSEventStreamStart failed for '{}'", m_impl->directory.string());
            m_impl->shutdown();
            return;
        }

        m_impl->watching = true;
    }

    FileSystemWatcher::~FileSystemWatcher() {
        m_impl->shutdown();
    }

    bool FileSystemWatcher::isWatching() const {
        return m_impl->watching;
    }

    const std::filesystem::path& FileSystemWatcher::directory() const {
        return m_impl->directory;
    }
}

#endif
