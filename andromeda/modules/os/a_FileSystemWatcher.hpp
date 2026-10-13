#pragma once

/**
 * @file a_FileSystemWatcher.hpp
 * @brief Backend-agnostic directory watcher.
 *
 * @details Include this header, never the platform one - the platform header pulls in
 *          <windows.h> and stops compiling everywhere else. Exactly one
 *          modules/os/<platform>/*.cpp provides the implementation, picked at configure
 *          time (see CMakeLists.txt in this folder).
 */

#include <filesystem>
#include <functional>
#include <memory>

#include "a_file_event.hpp"

namespace Andromeda::OS {

    // FileAction and FileEvent are declared in definitions/a_file_event.hpp: the engine
    // event in a_EventTypes.hpp carries them too, and definitions must not depend on os.
    // Pulling the names in here keeps OS::FileAction working at the call sites.
    using Andromeda::FileAction;
    using Andromeda::FileEvent;

    /**
     * @brief Called once per change, with the full path of the affected entry.
     *
     * @warning Runs on the watcher's own thread, not the thread that created the watcher.
     *          Anything touching renderer or editor state has to be queued across.
     */
    using FileChangedCallback = std::function<void(FileAction action, const std::filesystem::path& path)>;

    /**
     * @class FileSystemWatcher
     * @brief Watches a directory and reports changes until it is destroyed.
     *
     * @details The watcher owns a thread that sleeps until the OS reports a change, so an
     *          idle watcher costs no CPU. Events are debounced: editors write a file in
     *          several steps (temp file, rename, write), and without debouncing a single
     *          save produces three or four callbacks for the same path.
     *
     *          Construction never throws. A watcher whose directory could not be opened
     *          logs the reason and reports isWatching() == false.
     */
    class FileSystemWatcher {
    public:
        /**
         * @param directory    Existing directory to watch. Must stay valid; deleting it ends the watch.
         * @param callback     Invoked per change on the watcher thread.
         * @param watchSubtree Also report changes in nested directories.
         */
        FileSystemWatcher(std::filesystem::path directory,
                          FileChangedCallback callback,
                          bool watchSubtree = true);

        /** @brief Signals the thread to stop, waits for it, then releases the OS handles. */
        ~FileSystemWatcher();

        // One watcher owns one set of OS handles and one thread. Copying would close the
        // same handles twice in the second destructor - a bug that only shows up
        // sporadically, so it is forbidden outright rather than left to discipline.
        FileSystemWatcher(const FileSystemWatcher&) = delete;
        FileSystemWatcher& operator=(const FileSystemWatcher&) = delete;
        FileSystemWatcher(FileSystemWatcher&&) = delete;
        FileSystemWatcher& operator=(FileSystemWatcher&&) = delete;

        /** @brief False if the directory could not be opened, or the watch thread has ended. */
        bool isWatching() const;

        /** @brief The directory this watcher was constructed with. */
        const std::filesystem::path& directory() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}
