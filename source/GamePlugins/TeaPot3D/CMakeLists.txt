# source/GamePlugins/TeaPot3D/CMakeLists.txt
# TeaPot3D shared library — hosts the Render3D engine layer and the teapot demo scene/viewmodel.

add_library(TeaPot3D SHARED
    Render3DObjectLayer/Render3DPrimitives.h
    Render3DObjectLayer/PhysFSMaterialReader.hpp
    Render3DObjectLayer/Render3DObjectLayer.h
    Render3DObjectLayer/Render3DObjectLayer.cpp
    TeaPotDemo/TeaPotDemoScene.h
    TeaPotDemo/TeaPotDemoScene.cpp
    TeaPotDemo/TeaPotDemoViewModel.h
    TeaPotDemo/TeaPotDemoViewModel.cpp
)

# Include roots:
#   - plugin root for <Render3DObjectLayer/Render3DObjectLayer.h> etc.
#   - Render3DObjectLayer/ for local primitive/material-reader includes
#   - TeaPotDemo/ for local scene/viewmodel includes
#   - tinyobjloader source for tiny_obj_loader.h (PRIVATE: implementation only)
target_include_directories(TeaPot3D
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/Render3DObjectLayer"
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
        glm::glm        # PRIVATE: only needed by Render3DObjectLayer internals
        tinyobjloader   # PRIVATE: OBJ loading implementation detail
)
