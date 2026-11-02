# ==============================================================================
# Andromeda tests
# ==============================================================================
#   AndromedaTests         unit tests: offline, no window, no GPU, well under two seconds.
#                          ctest label "unit". Must stay green.
#   AndromedaNetworkTests  the network clients against local mock servers (TLS WebSocket
#                          server, HTTP CONNECT proxy) on 127.0.0.1 - no internet needed.
#                          ctest label "network". Must stay green as well.
#   AndromedaGlTests       boots the real subsystems; needs a window and a GL context.
#                          Opt-in: -DANDROMEDA_BUILD_GL_TESTS=ON, local runs only.
#
# A new test is a new .cpp file in unit/ or network/ - the globs below pick it up.
include(GoogleTest)

# ------------------------------------------------------------------------------
# Helpers shared by every test executable: temp folders, log capture, event cleanup
# ------------------------------------------------------------------------------
add_library(andromeda_test_support STATIC
    support/a_TempDirectory.cpp
    support/a_LogCapture.cpp
)
target_include_directories(andromeda_test_support PUBLIC support)
target_link_libraries(andromeda_test_support PUBLIC
    GTest::gtest
    filesystem
    definitions
)

# ------------------------------------------------------------------------------
# AndromedaTests
# ------------------------------------------------------------------------------
file(GLOB_RECURSE ANDROMEDA_UNIT_TEST_SOURCES CONFIGURE_DEPENDS "unit/*.cpp")
add_executable(AndromedaTests ${ANDROMEDA_UNIT_TEST_SOURCES})
target_link_libraries(AndromedaTests PRIVATE
    GTest::gtest_main
    andromeda_test_support
    definitions
    filesystem
    project
    asset
    serialization
    undo
)
# Header-only code that sits next to modules the unit tests must not link - resource pulls in
# assimp and OpenGL, renderer_rhi a GL backend. Only their include paths are needed.
target_include_directories(AndromedaTests PRIVATE
    ${CMAKE_SOURCE_DIR}/modules/resource
    ${CMAKE_SOURCE_DIR}/modules/renderer_rhi
)
gtest_discover_tests(AndromedaTests
    PROPERTIES LABELS unit TIMEOUT 10
    DISCOVERY_TIMEOUT 30
)

# ------------------------------------------------------------------------------
# AndromedaNetworkTests
# ------------------------------------------------------------------------------
add_library(andromeda_network_test_support STATIC
    support/network/a_TestCertificates.cpp
    support/network/a_MockWebsocketServer.cpp
    support/network/a_MockHttpProxy.cpp
)
target_include_directories(andromeda_network_test_support PUBLIC support/network)
target_link_libraries(andromeda_network_test_support PUBLIC
    andromeda_test_support
    network
)

file(GLOB ANDROMEDA_NETWORK_TEST_SOURCES CONFIGURE_DEPENDS "network/*.cpp")
add_executable(AndromedaNetworkTests ${ANDROMEDA_NETWORK_TEST_SOURCES})
target_link_libraries(AndromedaNetworkTests PRIVATE
    GTest::gtest_main
    andromeda_network_test_support
)
if(MSVC)
    # Boost.Beast's coroutine machinery exceeds the default section limit, like in modules/network.
    target_compile_options(andromeda_network_test_support PRIVATE /bigobj)
    target_compile_options(AndromedaNetworkTests PRIVATE /bigobj)
endif()
gtest_discover_tests(AndromedaNetworkTests
    PROPERTIES LABELS network TIMEOUT 30
    DISCOVERY_TIMEOUT 30
)

# ------------------------------------------------------------------------------
# AndromedaGlTests (opt-in)
# ------------------------------------------------------------------------------
# test_system_start.cpp boots the real subsystems (WindowManager, Renderer, Editor), so it needs
# a window and a live GL context. That cannot run on a headless CI runner, which is why it is
# not part of AndromedaTests. Run it locally on a machine with a display:
#
#   cmake --preset conan-release -DANDROMEDA_BUILD_GL_TESTS=ON
option(ANDROMEDA_BUILD_GL_TESTS "Build subsystem tests that require a window and GL context" OFF)

if(ANDROMEDA_BUILD_GL_TESTS)
    add_executable(AndromedaGlTests gl/test_system_start.cpp)
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
    gtest_discover_tests(AndromedaGlTests PROPERTIES LABELS gl)
endif()

# ------------------------------------------------------------------------------
# Reflector tests (opt-in)
# ------------------------------------------------------------------------------
# reflection/test_reflector.cpp exercises the libclang-based reflector: it includes
# a_Reflector.hpp and links a `reflection` target, neither of which exists in the tree yet
# (there is no modules/reflection/). Flip this ON once modules/reflection is added.
option(ANDROMEDA_BUILD_REFLECTION_TESTS "Build reflection/test_reflector.cpp (requires the reflection target)" OFF)

if(ANDROMEDA_BUILD_REFLECTION_TESTS)
    if(NOT TARGET reflection)
        message(FATAL_ERROR
            "ANDROMEDA_BUILD_REFLECTION_TESTS=ON but no `reflection` target exists. "
            "Add modules/reflection (providing a_Reflector.hpp) and an add_subdirectory() for it.")
    endif()
    add_executable(AndromedaReflectionTests reflection/test_reflector.cpp)
    target_link_libraries(AndromedaReflectionTests PRIVATE GTest::gtest_main reflection)
    gtest_discover_tests(AndromedaReflectionTests PROPERTIES LABELS reflection)
endif()
