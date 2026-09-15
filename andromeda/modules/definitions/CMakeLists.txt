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
#TODO: sort out telemetry metadata generation. Currently, the telemetry metadata is generated from a single file, 
#but it should be generated from all telemetry files in the modules/data/telemetry directory
set(GEN_TELEMETRY_INPUT "${CMAKE_SOURCE_DIR}/modules/data/telemetry/bmv080_telemetry.hpp")
set(GEN_TELEMETRY_OUTPUT "${CMAKE_SOURCE_DIR}/modules/data/telemetry/generated_telemetry_meta.hpp")
# Event metadata: same scanner, restricted to the events marked [[BindableEvent]]. Emits the name list
# the editor's event dropdown reads. --list-name must differ from the one above, otherwise the
# two generated headers declare the same alias in one translation unit.
set(GEN_EVENT_INPUTS
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/a_EventTypes.hpp"
    "${CMAKE_SOURCE_DIR}/modules/data/telemetry/a_sensor_events.hpp"
)
set(GEN_EVENT_OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/Engine/generated_event_meta.hpp")

set(GEN_EVENT_ARGS "")
foreach(EVENT_INPUT ${GEN_EVENT_INPUTS})
    list(APPEND GEN_EVENT_ARGS --input ${EVENT_INPUT})
endforeach()
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

add_custom_command(
    OUTPUT ${GEN_EVENT_OUTPUT}
    COMMAND ${Python3_EXECUTABLE} ${GEN_META_SCRIPT}
            ${GEN_EVENT_ARGS}
            --output ${GEN_EVENT_OUTPUT}
            --list-name ReflectedEvents
            --require-attr BindableEvent
    DEPENDS ${GEN_META_SCRIPT} ${GEN_EVENT_INPUTS}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Scanning the event headers to generate event metadata..."
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN_TELEMETRY_OUTPUT}
    COMMAND ${Python3_EXECUTABLE} ${GEN_META_SCRIPT}
            --input ${GEN_TELEMETRY_INPUT}
            --output ${GEN_TELEMETRY_OUTPUT}
            --list-name ReflectedTelemetry
    DEPENDS ${GEN_META_SCRIPT} ${GEN_TELEMETRY_INPUT}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Scanning the telemetry headers to generate channel metadata..."
    VERBATIM
)


add_custom_target(generate_ecs_metadata DEPENDS ${GEN_OUTPUT} ${GEN_NAME_OUTPUT} ${GEN_META_OUTPUT} ${GEN_EVENT_OUTPUT} ${GEN_TELEMETRY_OUTPUT})

add_library(definitions INTERFACE
		Math/a_glm_json_parser.hpp)
add_dependencies(definitions generate_ecs_metadata)
target_sources(definitions INTERFACE ${GEN_OUTPUT} ${GEN_NAME_OUTPUT} ${GEN_META_OUTPUT} ${GEN_EVENT_OUTPUT} ${GEN_TELEMETRY_OUTPUT})
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