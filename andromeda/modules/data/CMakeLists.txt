
find_package(nlohmann_json REQUIRED)

add_library(data INTERFACE)

target_include_directories(data INTERFACE 
	.
	./telemetry
)

target_link_libraries(data INTERFACE
	nlohmann_json::nlohmann_json
	definitions
)

andromeda_group_sources_by_folder()
