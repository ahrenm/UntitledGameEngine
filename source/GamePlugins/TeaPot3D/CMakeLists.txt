# source/GamePlugins/TeaPot3D/CMakeLists.txt
# TeaPot3D shared library — hosts the Render3D engine layer and the teapot demo scene/viewmodel.

add_library(TeaPot3D SHARED
    Render3DLayer/Render3DLayer.h
    Render3DLayer/Render3DLayer.cpp
    TeaPotDemo/TeaPotDemoScene.h
    TeaPotDemo/TeaPotDemoScene.cpp
    TeaPotDemo/TeaPotDemoViewModel.h
    TeaPotDemo/TeaPotDemoViewModel.cpp
)

# Include roots: plugin root for <TeaPot3D/...> and TeaPotDemo sources for local includes.
target_include_directories(TeaPot3D
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/TeaPotDemo"
    PRIVATE
        "${tinyobjloader_SOURCE_DIR}"
)

# Strip the MinGW "lib" prefix so the output is TeaPot3D.dll, not libTeaPot3D.dll
set_target_properties(TeaPot3D PROPERTIES
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

target_link_libraries(TeaPot3D
    PUBLIC
        UGECore
)

