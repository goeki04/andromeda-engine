find_package(glm REQUIRED)
find_package(glew REQUIRED)
find_package(SDL3 REQUIRED)
find_package(imgui REQUIRED)
add_library(renderer STATIC
    a_particle.cpp
    renderer.cpp
        a_AssetPreviewRenderer.cpp
        a_FrameConstants.cpp
)

target_include_directories(renderer PUBLIC 
    .
    ${CMAKE_BINARY_DIR}/generated/
)
target_compile_definitions(renderer PRIVATE SHADER_PATH="${CMAKE_SOURCE_DIR}/modules/shader/" SHADER_SPV_PATH="${CMAKE_BINARY_DIR}/shaders/spirv/")
target_link_libraries(renderer PUBLIC 
    glm::glm
    imgui::imgui

    definitions
    scene
    core
    renderer_rhi
    resource
)