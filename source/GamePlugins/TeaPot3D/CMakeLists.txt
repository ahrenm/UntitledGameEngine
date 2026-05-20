# source/GamePlugins/TeaPot3D/CMakeLists.txt
# TeaPot3D shared library — hosts the Render3D engine layer and the teapot demo scene/viewmodel.

add_library(TeaPot3D SHARED
    Render3DLayer/Camera3D.h
    Render3DLayer/Light3D.h
    Render3DLayer/Mesh3D.h
    Render3DLayer/Render3DLayer.h
    Render3DLayer/Render3DLayer.cpp
    TeaPotDemo/TeaPotDemoScene.h
    TeaPotDemo/TeaPotDemoScene.cpp
    TeaPotDemo/TeaPotDemoViewModel.h
    TeaPotDemo/TeaPotDemoViewModel.cpp
)

# Include roots:
#   - plugin root for <Render3DLayer/Render3DLayer.h> etc.
#   - Render3DLayer/ for local Camera3D / Light3D / Mesh3D includes
#   - TeaPotDemo/ for local scene/viewmodel includes
#   - tinyobjloader source for tiny_obj_loader.h (PRIVATE: implementation only)
target_include_directories(TeaPot3D
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/Render3DLayer"
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
    PRIVATE
        glm::glm        # PRIVATE: only needed by Render3DLayer internals
        tinyobjloader   # PRIVATE: OBJ loading implementation detail
)

