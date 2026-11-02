# The undo history on its own: it only needs the ECS registry and the generated undo commands,
# not the editor's window, GUI and renderer - so it can be tested without any of them.
add_library(undo STATIC
    "a_command_dispatcher.cpp"
    "a_undo_buffer.cpp"
)
target_include_directories(undo PUBLIC .)
target_link_libraries(undo PUBLIC
    core
    definitions
)

add_library(editor STATIC
    "editor.cpp"
)

target_include_directories(editor PUBLIC .)
target_include_directories(editor PUBLIC ${CMAKE_SOURCE_DIR}/bindings/)
target_link_libraries(editor PUBLIC
    undo
    core
    scene
    renderer_rhi
    input
    renderer
    gui
    resource
)
