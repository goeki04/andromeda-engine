# Platform services that can only be built on top of one OS's native API (file system
# watching, and whatever else needs a syscall rather than the C++ standard library).
#
# The layout follows modules/renderer_rhi: consumers include a backend-agnostic header,
# and exactly one platform subfolder provides the implementation, picked here at configure
# time rather than at runtime. Keeping <windows.h> inside windows/*.cpp is the point -
# it defines min/max and a few hundred other macros that break anything including it.

set(OS_PLATFORM_DIR "")
set(OS_PLATFORM_SOURCES "")

if(WIN32)
    set(OS_PLATFORM_DIR windows)
else()
    # Linux is a real build target here (see Dockerfile and build.sh), so a platform
    # without an implementation must not stop the configure the way renderer_rhi's
    # unimplemented graphics backends do. The target still exists and still carries its
    # headers, it just has nothing to compile - consumers link it either way.
    message(STATUS
        "Andromeda: modules/os has no implementation for this platform yet "
        "(only modules/os/windows exists). Add modules/os/<platform>/*.cpp and set "
        "OS_PLATFORM_DIR for it.")
endif()

if(OS_PLATFORM_DIR)
    # CONFIGURE_DEPENDS re-runs the glob on every build, so dropping a .cpp into the
    # platform folder is picked up without a manual cmake re-configure - the same
    # reasoning as the modules/ glob in the top-level CMakeLists.txt.
    file(GLOB OS_PLATFORM_SOURCES CONFIGURE_DEPENDS "${OS_PLATFORM_DIR}/*.cpp")
endif()

# The module is header-only until the first platform .cpp appears, and an empty STATIC
# library is a configure error in CMake ("no source files"). Selecting the library type
# from the glob means nothing here has to change when FileSystemWatcher.cpp is added.
if(OS_PLATFORM_SOURCES)
    add_library(os STATIC ${OS_PLATFORM_SOURCES})
    set(OS_LINKAGE PUBLIC)
else()
    add_library(os INTERFACE)
    set(OS_LINKAGE INTERFACE)
endif()

# `.` is for the backend-agnostic headers that belong directly in modules/os; the platform
# folder is exposed as well so its headers are reachable by plain name. Consumers should
# include the neutral header, not the platform one, or they stop compiling on Linux.
target_include_directories(os ${OS_LINKAGE} . ${OS_PLATFORM_DIR})

target_link_libraries(os ${OS_LINKAGE}
    definitions
)
