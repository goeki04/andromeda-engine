
find_package(LLVM REQUIRED)

add_library(definitions INTERFACE
		Math/a_glm_json_parser.hpp)
add_dependencies(definitions generate_ecs_metadata)
target_sources(definitions INTERFACE ${GEN_OUTPUT} ${GEN_NAME_OUTPUT})
target_include_directories(definitions INTERFACE 
   .   
   Math
   Engine
   renderer
   ECS
   network
)

target_link_libraries(definitions INTERFACE
	glm::glm
	nlohmann_json::nlohmann_json
	spdlog::spdlog
)
