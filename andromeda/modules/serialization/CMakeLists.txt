
find_package(nlohmann_json REQUIRED)

add_library(serialization STATIC
	sceneSerializer.cpp
)

target_include_directories(serialization PUBLIC .)


# No resource here: mesh references go through IMeshLookup, so the serializer stays free of
# assimp and OpenGL. core only provides the header-only ECS registry.
target_link_libraries(serialization PUBLIC
	nlohmann_json::nlohmann_json

	core
	filesystem
	definitions
)