# Platform services that can only be built on top of one OS's native API (file system
# watching, and whatever else needs a syscall rather than the C++ standard library).
#
# The layout follows modules/renderer_rhi: consumers include a backend-agnostic header,
# and exactly one platform subfolder provides the implementation, picked here at configure
# time rather than at runtime. Keeping <windows.h> inside windows/*.cpp is the point -
# it defines min/max and a few hundred other macros that break anything including it.

set(OS_PLATFORM_DIR "")
set(OS_PLATFORM_SOURCES "")

# One subfolder per platform, picked by the OS we build for.
if(WIN32)
    set(OS_PLATFORM windows)
elseif(APPLE)
    set(OS_PLATFORM macos)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(OS_PLATFORM linux)
else()
    set(OS_PLATFORM "${CMAKE_SYSTEM_NAME}")
endif()

if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${OS_PLATFORM}")
    set(OS_PLATFORM_DIR ${OS_PLATFORM})
else()
    # Linux is a real build target here (see Dockerfile and build.sh), so a platform
    # without an implementation must not stop the configure the way renderer_rhi's
    # unimplemented graphics backends do. The target still exists and still carries its
    # headers, it just has nothing to compile - consumers link it either way.
    message(STATUS
        "Andromeda: modules/os has no implementation for '${OS_PLATFORM}' yet. Add "
        "modules/os/${OS_PLATFORM}/*.cpp - it is picked up automatically.")
endif()

if(OS_PLATFORM_DIR)
    # CONFIGURE_DEPENDS re-runs the glob on every build, so dropping a .cpp into the
    # platform folder is picked up without a manual cmake re-configure - the same
    # reasoning as the modules/ glob in the top-level CMakeLists.txt.
    # *.mm: Objective-C++ for the macOS backends that need Cocoa (AppKit, Foundation).
    file(GLOB OS_PLATFORM_SOURCES CONFIGURE_DEPENDS
        "${OS_PLATFORM_DIR}/*.cpp"
        "${OS_PLATFORM_DIR}/*.mm"
    )
endif()

# Sources that are the same everywhere - subsystems and helpers that only talk to the
# module through its neutral headers. Listed by hand rather than globbed: this folder also
# holds the neutral headers, so a glob here would be a wider net than intended.
set(OS_SOURCES
    a_os_manager.cpp
)

add_library(os STATIC ${OS_SOURCES} ${OS_PLATFORM_SOURCES})

# IDE projects (Xcode, VS solutions) only show files that belong to a target. Add the
# backends of the other platforms as well, marked HEADER_FILE_ONLY so they are visible
# for reference but never compiled (no <windows.h> on macOS and vice versa).
if(CMAKE_GENERATOR MATCHES "Xcode|Visual Studio")
    file(GLOB OS_OTHER_PLATFORM_FILES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/*/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/*/*.mm"
        "${CMAKE_CURRENT_SOURCE_DIR}/*/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/*/*.hpp"
    )
    if(OS_PLATFORM_DIR)
        list(FILTER OS_OTHER_PLATFORM_FILES EXCLUDE REGEX "/${OS_PLATFORM_DIR}/[^/]*$")
    endif()
    if(OS_OTHER_PLATFORM_FILES)
        set_source_files_properties(${OS_OTHER_PLATFORM_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)
        target_sources(os PRIVATE ${OS_OTHER_PLATFORM_FILES})
    endif()
endif()

set(OS_LINKAGE PUBLIC)

# `.` is for the backend-agnostic headers that belong directly in modules/os; the platform
# folder is exposed as well so its headers are reachable by plain name. Consumers should
# include the neutral header, not the platform one, or they stop compiling on Linux.
target_include_directories(os ${OS_LINKAGE} . ${OS_PLATFORM_DIR})
# ${OS_LINKAGE} rather than INTERFACE: the module compiles its own sources now, and those
# need ASSET_PATH and NOMINMAX just as much as its consumers do. With INTERFACE the
# defines would silently skip every .cpp in this folder.
target_compile_definitions(os ${OS_LINKAGE} ASSET_PATH="${CMAKE_SOURCE_DIR}/assets/")
target_compile_definitions(os ${OS_LINKAGE} NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(os ${OS_LINKAGE}
    definitions
)

if(APPLE)
    # ARC for the Objective-C++ backends, so Cocoa objects need no manual retain/release.
    target_compile_options(os PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
    # CoreServices: FSEvents (FileSystemWatcher). AppKit: NSOpenPanel, NSWorkspace
    # (FolderDialog, RevealInFileBrowser). Foundation: NSFileManager (Trash).
    target_link_libraries(os PRIVATE
        "-framework CoreServices"
        "-framework AppKit"
        "-framework Foundation"
    )
endif()

andromeda_group_sources_by_folder()
