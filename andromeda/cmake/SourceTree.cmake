# Mirrors the folder structure on disk in IDE project generators (Xcode, Visual Studio
# solutions). Without it, those generators put every file of a target flat into the
# default "Source Files" / "Header Files" groups, and headers that no target lists do
# not show up at all. Ninja/Makefile builds are not affected.
#
# Source groups are stored per directory, and an IDE generator only looks at the groups
# of the directory that defines a target. A call from the top-level CMakeLists.txt would
# therefore miss every target in modules/*, so each CMakeLists.txt calls this itself:
#
#   andromeda_group_sources_by_folder()   # last line, after all target_sources()
#
# What it does for every target defined in the calling directory:
#   - IDE generators only: adds the headers in the target's folder that it does not list
#     yet (header-only, nothing extra gets compiled). With several targets in one folder,
#     a header goes to the target that compiles the .cpp of the same name, else to the
#     target named like the folder, else to the first one.
#   - Files in the target's folder are grouped by their path relative to it.
#   - Other files from this repo (e.g. bindings/) are grouped by their path from the root.
#   - Files in the build tree (protobuf output, ...) go to a "Generated" group.
#   - The generated_* metadata headers, which `definitions` hands to every target that
#     links it, go to a "Generated" group as well instead of cluttering "Header Files".

set(_ANDROMEDA_HEADER_GLOBS *.h *.hpp *.hh *.inl *.inc)
set(_ANDROMEDA_COMPILED_TYPES STATIC_LIBRARY SHARED_LIBRARY MODULE_LIBRARY OBJECT_LIBRARY
    INTERFACE_LIBRARY EXECUTABLE)

function(_andromeda_add_unlisted_headers targets)
    set(dir "${CMAKE_CURRENT_SOURCE_DIR}")
    # The top-level folder holds the whole repo (and the build trees under conan*/), so
    # globbing there would pull in everything. Its targets list their files explicitly.
    if(dir STREQUAL CMAKE_SOURCE_DIR)
        return()
    endif()

    set(candidates "")
    foreach(target IN LISTS targets)
        get_target_property(type ${target} TYPE)
        if(type IN_LIST _ANDROMEDA_COMPILED_TYPES)
            list(APPEND candidates ${target})
        endif()
    endforeach()
    if(NOT candidates)
        return()
    endif()

    list(TRANSFORM _ANDROMEDA_HEADER_GLOBS PREPEND "${dir}/" OUTPUT_VARIABLE globs)
    file(GLOB_RECURSE headers CONFIGURE_DEPENDS ${globs})

    # Every source any target here already lists, as absolute paths.
    set(listed "")
    foreach(target IN LISTS candidates)
        get_target_property(sources ${target} SOURCES)
        if(sources)
            foreach(source IN LISTS sources)
                cmake_path(ABSOLUTE_PATH source BASE_DIRECTORY "${dir}" NORMALIZE
                           OUTPUT_VARIABLE file)
                list(APPEND listed "${file}")
                list(APPEND "_sources_of_${target}" "${file}")
            endforeach()
        endif()
    endforeach()

    get_filename_component(dir_name "${dir}" NAME)
    list(GET candidates 0 fallback)
    if(dir_name IN_LIST candidates)
        set(fallback ${dir_name})
    endif()

    foreach(header IN LISTS headers)
        if(header IN_LIST listed)
            continue()
        endif()
        set(owner ${fallback})
        cmake_path(REMOVE_EXTENSION header LAST_ONLY OUTPUT_VARIABLE stem)
        foreach(target IN LISTS candidates)
            foreach(ext cpp cc cxx mm c)
                if("${stem}.${ext}" IN_LIST "_sources_of_${target}")
                    set(owner ${target})
                endif()
            endforeach()
        endforeach()
        # PRIVATE also for INTERFACE libraries (allowed since CMake 3.19): the header
        # shows up in the IDE without being passed on to every consumer.
        target_sources(${owner} PRIVATE "${header}")
    endforeach()
endfunction()

function(andromeda_group_sources_by_folder)
    get_property(targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)

    if(CMAKE_GENERATOR MATCHES "Xcode|Visual Studio")
        _andromeda_add_unlisted_headers("${targets}")
    endif()

    # Regex groups apply to every file in this directory's targets, including the
    # INTERFACE sources other targets pass on - which explicit FILES groups can't reach.
    source_group("Generated" REGULAR_EXPRESSION "/generated_[^/]*\\.(hpp|h|inc)$")

    foreach(target IN LISTS targets)
        get_target_property(sources ${target} SOURCES)
        if(NOT sources)
            continue()
        endif()
        get_target_property(target_dir ${target} SOURCE_DIR)

        set(tree_files "")
        set(repo_files "")
        set(generated_files "")
        foreach(source IN LISTS sources)
            # Generator expressions are only resolved at generate time - skip them.
            if(source MATCHES "\\$<")
                continue()
            endif()
            cmake_path(ABSOLUTE_PATH source BASE_DIRECTORY "${target_dir}" NORMALIZE
                       OUTPUT_VARIABLE file)
            # The build tree can sit inside the source tree (conan/, conan-xcode/), so
            # test it first, otherwise generated files would show up as e.g.
            # conan-xcode/build/... in the tree.
            cmake_path(IS_PREFIX CMAKE_BINARY_DIR "${file}" NORMALIZE in_build_tree)
            cmake_path(IS_PREFIX target_dir "${file}" NORMALIZE in_target_dir)
            cmake_path(IS_PREFIX CMAKE_SOURCE_DIR "${file}" NORMALIZE in_repo)
            if(in_build_tree)
                list(APPEND generated_files "${file}")
            elseif(in_target_dir)
                list(APPEND tree_files "${file}")
            elseif(in_repo)
                list(APPEND repo_files "${file}")
            endif()
        endforeach()

        if(tree_files)
            source_group(TREE "${target_dir}" FILES ${tree_files})
        endif()
        if(repo_files)
            source_group(TREE "${CMAKE_SOURCE_DIR}" FILES ${repo_files})
        endif()
        if(generated_files)
            source_group("Generated" FILES ${generated_files})
        endif()
    endforeach()
endfunction()
