# The asset registry: which files exist, their Guids and asset_db.json. Deliberately free of
# assimp and OpenGL - loading the data behind a record is the ResourceManager's job - so the
# registry can be linked by tools and tests without dragging the renderer along.
add_library(asset STATIC
    a_AssetRegistry.cpp
    a_AssetManager.cpp
)

target_include_directories(asset PUBLIC .)
target_link_libraries(asset PUBLIC
    project
    filesystem
    definitions
)
