find_package(glew REQUIRED)
find_package(SDL3 REQUIRED)
find_package(imgui REQUIRED)
find_package(imguizmo REQUIRED)
# implot is vendored from master (bindings/implot) instead of ConanCenter's 0.17,
# which is too old for imgui 1.92 and fails to link in Release.
add_library(gui STATIC
    Framework/gui_renderer.cpp
    Framework/a_Docking.cpp
    Panels/a_HierarchyPanel.cpp
    Panels/a_Chart.cpp
    Panels/a_BrowserPanel.cpp
    Panels/a_DetailsPanel.cpp
    Panels/a_ConsolePanel.cpp
    Panels/a_ViewportPanel.cpp
    Widgets/a_MainMenuBar.cpp
    Widgets/Console.cpp
    ${CMAKE_SOURCE_DIR}/bindings/imgui_impl_sdl3.cpp
    ${CMAKE_SOURCE_DIR}/bindings/imgui_impl_opengl3.cpp
    # Vendored ImPlot (master)
    ${CMAKE_SOURCE_DIR}/bindings/implot/implot.cpp
    ${CMAKE_SOURCE_DIR}/bindings/implot/implot_items.cpp
    ${CMAKE_SOURCE_DIR}/bindings/implot/implot_demo.cpp
)
target_compile_definitions(gui PRIVATE ASSET_PATH="${CMAKE_SOURCE_DIR}/assets/" ModulePath="${CMAKE_SOURCE_DIR}/modules/")
target_include_directories(gui PUBLIC 
    .
    Components
    Framework
    Panels
    State
    Widgets
    ${CMAKE_SOURCE_DIR}/bindings/
    ${CMAKE_SOURCE_DIR}/bindings/implot/
    ${CMAKE_BINARY_DIR}/generated/
)
target_compile_definitions(gui PRIVATE IMGUI_IMPL_OPENGL_LOADER_CUSTOM)
target_link_libraries(gui PUBLIC
    GLEW::GLEW
    imgui::imgui
    imguizmo::imguizmo
    sdl::sdl

    core
    scene
    serialization
    definitions
    resource
    renderer_rhi
)