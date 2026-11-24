find_package(SDL3 REQUIRED)
add_library(input STATIC
    input_manager.cpp
)

target_include_directories(input PUBLIC .)

target_link_libraries(input PUBLIC 
    sdl::sdl
    definitions
    core
)

andromeda_group_sources_by_folder()
