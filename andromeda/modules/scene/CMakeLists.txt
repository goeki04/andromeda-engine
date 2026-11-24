
add_library(scene STATIC
	scene.cpp
)

target_include_directories(scene PUBLIC .)


target_link_libraries(scene PUBLIC
	core
	resource
	definitions
)

andromeda_group_sources_by_folder()
