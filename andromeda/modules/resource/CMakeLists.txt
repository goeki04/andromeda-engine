find_package(assimp REQUIRED)

add_library(resource STATIC
    resource_manager.cpp
)

target_compile_definitions(resource PRIVATE ASSET_PATH="${CMAKE_SOURCE_DIR}/assets/")
target_compile_definitions(resource PRIVATE SHADER_PATH="${CMAKE_SOURCE_DIR}/modules/shader/")
target_compile_definitions(resource PRIVATE CUBEMAP_PATH="${CMAKE_SOURCE_DIR}/assets/cubemaps/")
target_include_directories(resource PUBLIC .)

target_link_libraries(resource PUBLIC
    assimp::assimp

    definitions
    core
    renderer_rhi
    filesystem
    asset
    project
    os
)