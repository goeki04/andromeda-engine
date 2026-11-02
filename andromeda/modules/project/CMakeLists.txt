# Where the project lives and how its settings file is read: no engine, no GPU - small enough
# for the asset registry and the tests to depend on it.
add_library(project STATIC
    a_ProjectPaths.cpp
    a_ProjectSettingsFile.cpp
)

target_include_directories(project PUBLIC .)
target_link_libraries(project PUBLIC
    filesystem
    definitions
)
