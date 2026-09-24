
add_library(particle_system STATIC
	a_particle_binding_system.cpp
	a_particle_graph_system.cpp
)

target_include_directories(particle_system PUBLIC .)


target_link_libraries(particle_system PUBLIC
	core
	scene
	data
	definitions
)
