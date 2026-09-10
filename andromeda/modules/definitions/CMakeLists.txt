find_package(glm REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(spdlog REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(GEN_SCRIPT "${CMAKE_SOURCE_DIR}/metaData/gen_components.py")
set(GEN_OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/ECS/generated_components.hpp")
set(GEN_NAME_SCRIPT "${CMAKE_SOURCE_DIR}/metaData/gen_components_name.py")
set(GEN_NAME_OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/ECS/generated_component_names.hpp")
set(COMPONENTS
    ECS/a_components.hpp
)

# Struct metadata (reflection): scans a_particle_group.hpp and emits
# StructInfo<> specializations for the editor UI / serializer / undo system.
set(GEN_META_SCRIPT "${CMAKE_SOURCE_DIR}/metaData/gen_struct_meta.py")
set(GEN_META_INPUTS
    "${CMAKE_CURRENT_SOURCE_DIR}/renderer/a_particle_group.hpp"
)
set(GEN_META_OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/renderer/generated_particle_group_meta.hpp")
add_custom_command(
    OUTPUT ${GEN_OUTPUT}
    COMMAND ${Python3_EXECUTABLE} ${GEN_SCRIPT}
    DEPENDS ${GEN_SCRIPT} ${COMPONENTS}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Scanning modules for a_components.hpp to sync Tuple..."
    VERBATIM
)
add_custom_command(
    OUTPUT ${GEN_NAME_OUTPUT}
    COMMAND ${Python3_EXECUTABLE} ${GEN_NAME_SCRIPT}
    DEPENDS ${GEN_NAME_SCRIPT} ${GEN_OUTPUT}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    VERBATIM
)
set(GEN_META_ARGS "")
foreach(META_INPUT ${GEN_META_INPUTS})
    list(APPEND GEN_META_ARGS --input ${META_INPUT})
endforeach()

add_custom_command(
    OUTPUT ${GEN_META_OUTPUT}
    COMMAND ${Python3_EXECUTABLE} ${GEN_META_SCRIPT} ${GEN_META_ARGS} --output ${GEN_META_OUTPUT}
    DEPENDS ${GEN_META_SCRIPT} ${GEN_META_INPUTS}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Scanning a_particle_group.hpp to generate struct metadata..."
    VERBATIM
)

add_custom_target(generate_ecs_metadata DEPENDS ${GEN_OUTPUT} ${GEN_NAME_OUTPUT} ${GEN_META_OUTPUT})

add_library(definitions INTERFACE
		Math/a_glm_json_parser.hpp)
add_dependencies(definitions generate_ecs_metadata)
target_sources(definitions INTERFACE ${GEN_OUTPUT} ${GEN_NAME_OUTPUT} ${GEN_META_OUTPUT})
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

if(WIN32)
    target_compile_definitions(definitions INTERFACE _WIN32_WINNT=0x0A00 WINVER=0x0A00)
endif()

target_compile_definitions(definitions INTERFACE CPPHTTPLIB_OPENSSL_SUPPORT)