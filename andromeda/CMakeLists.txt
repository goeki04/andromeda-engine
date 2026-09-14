cmake_minimum_required(VERSION 3.23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(Andromeda LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==============================================================================
# Compiler cache (sccache)
# ==============================================================================
# Opt-out switch; when sccache is not installed this block does nothing at all, so the
# build stays identical for anyone without it. Must sit before the first add_subdirectory()
# because CMAKE_CXX_COMPILER_LAUNCHER is read when a target is created, not when it builds.
option(ANDROMEDA_USE_SCCACHE "Route compilation through sccache when it is available" ON)

if(ANDROMEDA_USE_SCCACHE)
    find_program(SCCACHE_PROGRAM sccache)

    if(NOT SCCACHE_PROGRAM)
        message(STATUS "Andromeda: sccache not found - building without a compiler cache.")
    elseif(NOT CMAKE_GENERATOR MATCHES "Ninja|Makefiles")
        # Compiler launchers are silently ignored by the Visual Studio generator. Say so
        # rather than letting someone believe caching is active. The conan presets use Ninja.
        message(STATUS
            "Andromeda: sccache found but generator '${CMAKE_GENERATOR}' ignores compiler "
            "launchers - caching stays off. Use the Ninja-based conan presets to benefit.")
    else()
        set(CMAKE_CXX_COMPILER_LAUNCHER "${SCCACHE_PROGRAM}")

        if(MSVC)
            # MSVC defaults to /Zi, which writes debug info into a shared .pdb owned by a
            # separate mspdbsrv process. sccache cannot cache those objects. /Z7 embeds the
            # same debug info in the .obj instead - identical debugging experience, and
            # cacheable. CMP0141 (CMake 3.25) would offer a cleaner switch, but it has to be
            # set before project(), so patch the flags directly.
            foreach(config DEBUG RELWITHDEBINFO)
                string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_${config} "${CMAKE_CXX_FLAGS_${config}}")
                string(REPLACE "/ZI" "/Z7" CMAKE_CXX_FLAGS_${config} "${CMAKE_CXX_FLAGS_${config}}")
            endforeach()
        endif()

        message(STATUS "Andromeda: sccache enabled (${SCCACHE_PROGRAM}).")
    endif()
endif()

if(WIN32)
    add_compile_definitions(ANDROMEDA_OS="Windows")
elseif(UNIX)
    add_compile_definitions(ANDROMEDA_OS="Linux")
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    add_compile_definitions(ANDROMEDA_ARCH="x64")
else()
    add_compile_definitions(ANDROMEDA_ARCH="x86")
endif()
# Graphics backend implementing the renderer_rhi module (modules/renderer_rhi/CMakeLists.txt
# picks the matching *.cpp sources and dependencies for this). Only OpenGL has an actual
# implementation right now; Vulkan/DirectX are reserved names for future backends.
# Set via: cmake -DANDROMEDA_GRAPHICS_API=OpenGL ..
set(ANDROMEDA_GRAPHICS_API "OpenGL" CACHE STRING "Graphics backend used by modules/renderer_rhi (OpenGL, Vulkan, DirectX)")
set_property(CACHE ANDROMEDA_GRAPHICS_API PROPERTY STRINGS OpenGL Vulkan DirectX)

# NOTE: the `renderer` and `gui` modules still talk to OpenGL directly in places (raw gl*
# calls, OpenGLContext hardcoded in Renderer::initRenderer). This switch only governs which
# backend modules/renderer_rhi is built with; it does not yet make the rest of the engine
# backend-agnostic.
find_package(SDL3 REQUIRED)
find_package(protobuf REQUIRED)
find_package(glew REQUIRED)
find_package(glslang REQUIRED)
find_package(GTest REQUIRED)
find_package(httplib REQUIRED)
find_package(spdlog REQUIRED)
find_package(spirv-reflect-static REQUIRED)
# CONFIGURE_DEPENDS re-runs the glob on every build, so adding a file under
# modules/ is picked up without a manual `cmake` re-configure.
file(GLOB SOURCES CONFIGURE_DEPENDS "modules/*.cpp")
file(GLOB HEADERS CONFIGURE_DEPENDS "modules/*.h")


get_filename_component(PROTO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/proto" ABSOLUTE)
set(PROTO_FILES 
    proto/api_options.proto 
    proto/api.proto
)
set(PROTOBUF_GENERATE_CPP_APPEND_PATH TRUE)
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

add_subdirectory(modules/network)
add_subdirectory(modules/core)
add_subdirectory(modules/definitions)
add_subdirectory(modules/renderer)
add_subdirectory(modules/scene)
add_subdirectory(modules/gui)
add_subdirectory(modules/editor)
add_subdirectory(modules/input)
add_subdirectory(modules/resource)
add_subdirectory(modules/filesystem)
add_subdirectory(modules/serialization)
add_subdirectory(modules/renderer_rhi)
add_subdirectory(modules/data)
add_subdirectory(modules/particle_system)

add_executable(AndromedaReflector tools/shader_reflector/main.cpp)
target_link_libraries(AndromedaReflector PRIVATE spirv-reflect-static)
target_link_libraries(AndromedaReflector PRIVATE definitions)
target_compile_definitions(AndromedaReflector PRIVATE SHADER_PATH="${CMAKE_SOURCE_DIR}/modules/shader/")


add_executable(Andromeda 
    ${SOURCES} 
    ${HEADERS}
    ${BINDING_SOURCES} 
    ${BINDING_HEADERS} 
    ${SHADERS}
    ${PROTO_SRCS}
    ${PROTO_HDRS}
    modules/definitions/Math/a_glm_json_parser.hpp
)
add_dependencies(Andromeda GenerateShaderHeaders)
target_include_directories(Andromeda PUBLIC ${CMAKE_BINARY_DIR}/generated)

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/generated)
file(GLOB_RECURSE ALL_GLSL_SHADERS
    CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/modules/shader/*.vert"
    "${CMAKE_SOURCE_DIR}/modules/shader/*.frag"
    # particle.comp (GLSL) is intentionally excluded: compute shaders are now
    # HLSL-only (see compile_hlsl_shader() below). particle.comp is still on
    # disk but must not be compiled - its "EmitterSettings" UBO shares a name
    # with the HLSL cbuffer of the same name, and the reflector would collide
    # on the two same-named-but-different-layout structs.
)
set(ALL_SPV_FILES "")
foreach(SHADER ${ALL_GLSL_SHADERS})
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    list(APPEND ALL_SPV_FILES "${CMAKE_BINARY_DIR}/shaders/spirv/${SHADER_NAME}.spv")
endforeach()
# HLSL kernels are auto-discovered (not glob-derived) inside compile_hlsl_shader(),
# relayed here via PARENT_SCOPE from modules/renderer_rhi/CMakeLists.txt.
list(APPEND ALL_SPV_FILES ${RHI_HLSL_SPV_FILES})
set(GENERATED_HEADER ${CMAKE_BINARY_DIR}/generated/a_shader_generated.hpp)
set(GENERATED_COMPUTE_HEADER ${CMAKE_BINARY_DIR}/generated/a_shader_generated_compute.hpp)
if(ALL_SPV_FILES)
    add_custom_command(
        OUTPUT ${GENERATED_HEADER} ${GENERATED_COMPUTE_HEADER}
        COMMAND AndromedaReflector ${GENERATED_HEADER} ${GENERATED_COMPUTE_HEADER} ${ALL_SPV_FILES}
        DEPENDS AndromedaReflector ${ALL_SPV_FILES}
        COMMENT "Andromeda Engine: Generating Reflected Shader Structs for all shaders..."
    )
else()
    add_custom_command(
        OUTPUT ${GENERATED_HEADER} ${GENERATED_COMPUTE_HEADER}
        COMMAND ${CMAKE_COMMAND} -E echo "#pragma once" > ${GENERATED_HEADER}
        COMMAND ${CMAKE_COMMAND} -E echo "namespace Andromeda::Generated {}" >> ${GENERATED_HEADER}
        COMMAND ${CMAKE_COMMAND} -E echo "#pragma once" > ${GENERATED_COMPUTE_HEADER}
        COMMAND ${CMAKE_COMMAND} -E echo "namespace Andromeda::Generated::Compute {}" >> ${GENERATED_COMPUTE_HEADER}
        COMMENT "Andromeda Engine: No SPIR-V files found, creating empty fallback headers."
    )
endif()

add_custom_target(GenerateShaderHeaders ALL DEPENDS ${GENERATED_HEADER} ${GENERATED_COMPUTE_HEADER})
add_dependencies(GenerateShaderHeaders renderer_rhi_shaders)

# These static libs include the generated header (a_shader_generated.hpp), so
# they must wait for GenerateShaderHeaders. Without this, a clean parallel build
# can compile them before the header exists (fatal C1083) - see CI failure.
add_dependencies(renderer GenerateShaderHeaders)
add_dependencies(gui GenerateShaderHeaders)
target_include_directories(Andromeda PRIVATE ${CMAKE_CURRENT_BINARY_DIR} ${PROTOBUF_INCLUDE_DIRS})
target_include_directories(Andromeda PRIVATE 
    ${CMAKE_CURRENT_BINARY_DIR}/proto 
)
target_link_libraries(Andromeda PRIVATE
    #External dependencies
    sdl::sdl
    GLEW::GLEW
    protobuf::libprotobuf
    glslang::_glslang-do-not-use
    spdlog::spdlog
    #Engine modules
    network
    core
    renderer
    scene
    gui
    editor
    input
    renderer_rhi
    particle_system
    serialization
    filesystem
    resource
    definitions
)

enable_testing()
include(GoogleTest)

set(ANDROMEDA_TEST_SOURCES
    "tests/test_ecs.cpp"
    "tests/test_math.cpp"
    "tests/test_resource.cpp"
    "tests/test_rhi.cpp"
    "tests/test_boost_websocket_client.cpp"
)

# tests/test_reflector.cpp exercises the libclang-based reflector: it includes
# a_Reflector.hpp and links a `reflection` target, neither of which exists in the tree
# yet (there is no modules/reflection/). Leaving them unconditional makes CMake fail at
# generate time with "AndromedaTests links to target reflection but the target was not
# found", which takes down the whole configure - not just the tests. Flip this ON once
# modules/reflection is added.
option(ANDROMEDA_BUILD_REFLECTION_TESTS "Build tests/test_reflector.cpp (requires the reflection target)" OFF)

if(ANDROMEDA_BUILD_REFLECTION_TESTS)
    if(NOT TARGET reflection)
        message(FATAL_ERROR
            "ANDROMEDA_BUILD_REFLECTION_TESTS=ON but no `reflection` target exists. "
            "Add modules/reflection (providing a_Reflector.hpp) and an add_subdirectory() for it.")
    endif()
    list(APPEND ANDROMEDA_TEST_SOURCES "tests/test_reflector.cpp")
endif()

add_executable(AndromedaTests ${ANDROMEDA_TEST_SOURCES})
target_compile_definitions(resource PRIVATE SHADER_PATH="${CMAKE_CURRENT_SOURCE_DIR}/modules/shader/")

# Headers under test are header-only (ECS, math, geometry, primitive generator,
# RHI types), so we only need their include directories plus glm/nlohmann (via
# the definitions interface target). No GL context is required at runtime.
target_include_directories(AndromedaTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/modules/core
    ${CMAKE_CURRENT_SOURCE_DIR}/modules/resource
    ${CMAKE_CURRENT_SOURCE_DIR}/modules/renderer_rhi
)
target_link_libraries(AndromedaTests PRIVATE
    GTest::gtest_main
    definitions
    network
)

if(ANDROMEDA_BUILD_REFLECTION_TESTS)
    target_link_libraries(AndromedaTests PRIVATE reflection)
endif()

gtest_discover_tests(AndromedaTests)

# tests/test_system_start.cpp boots the real subsystems (WindowManager, Renderer,
# Editor), so it needs a window and a live GL context. That cannot run on a
# headless CI runner, which is why it is not part of AndromedaTests. It is kept
# buildable behind an opt-in switch instead of rotting as an unreferenced file:
#
#   cmake --preset conan-release -DANDROMEDA_BUILD_GL_TESTS=ON
#
# Run it locally on a machine with a display.
option(ANDROMEDA_BUILD_GL_TESTS "Build subsystem tests that require a window and GL context" OFF)

if(ANDROMEDA_BUILD_GL_TESTS)
    add_executable(AndromedaGlTests "tests/test_system_start.cpp")
    target_link_libraries(AndromedaGlTests PRIVATE
        GTest::gtest_main
        sdl::sdl
        GLEW::GLEW
        spdlog::spdlog
        core
        renderer
        scene
        gui
        editor
        input
        renderer_rhi
        resource
        definitions
    )
    add_dependencies(AndromedaGlTests GenerateShaderHeaders)
    target_include_directories(AndromedaGlTests PRIVATE ${CMAKE_BINARY_DIR}/generated)
    gtest_discover_tests(AndromedaGlTests)
endif()

# ==============================================================================
# Dedicated TCP Server Executable
# ==============================================================================

add_executable(AndromedaMockServer
    ${CMAKE_CURRENT_SOURCE_DIR}/mock/bmv080_tcp_ip_server.cpp
)

find_package(nlohmann_json REQUIRED)

target_link_libraries(AndromedaMockServer PRIVATE
    nlohmann_json::nlohmann_json
    spdlog::spdlog

    data
)

if(WIN32)
    target_link_libraries(AndromedaMockServer PRIVATE ws2_32)
endif()

