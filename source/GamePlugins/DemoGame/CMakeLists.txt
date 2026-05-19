# source/GamePlugins/DemoGame/CMakeLists.txt
# DemoGame shared library — contains game-specific ViewModels, scenes, and logic.
# Self-registers ViewModels with UGECore's ViewModelRegistry via static initialisers
# (REGISTER_VIEWMODEL). Application bootstrapping explicitly loads DemoGame.dll at
# startup, before UGEApplication::Create() runs.

add_library(DemoGame SHARED
    LuaConsoleViewModel.cpp
    Platformer/PlatformerHudViewModel.cpp
    Platformer/PlatformerNamePlateViewModel.h
    Platformer/PlatformerNamePlateViewModel.cpp
    Platformer/PlatformerCharacter.cpp
    Platformer/TileWorld.h
    Platformer/TileWorld.cpp
    Platformer/PlatformerScene.cpp
    Platformer/PlatformerStateTags.h
    LuaTests/LuaTestsScene.h
    LuaTests/LuaTestsScene.cpp
    LuaTests/LuaTestsViewModel.h
    LuaTests/LuaTestsViewModel.cpp
    StateAssetsDemo/StateAssetsDemoScene.h
    StateAssetsDemo/StateAssetsDemoScene.cpp
    StateAssetsDemo/StateAssetsDemoViewModel.h
    StateAssetsDemo/StateAssetsDemoViewModel.cpp
)

# source/GamePlugins/DemoGame/ is the include root — consumers use #include <LuaConsoleViewModel.h> etc.
target_include_directories(DemoGame
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/Platformer"
        "${CMAKE_CURRENT_SOURCE_DIR}/LuaTests"
        "${CMAKE_CURRENT_SOURCE_DIR}/StateAssetsDemo"
)

# UGECore is PUBLIC so its transitive deps (RmlUi, SDL3, sol2, PhysFS, Lua)
# are also available to DemoGame sources without re-listing them here.
# UIWindows provides the window ViewModel infrastructure.
# Strip the MinGW "lib" prefix so the output is DemoGame.dll, not libDemoGame.dll
set_target_properties(DemoGame PROPERTIES
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

target_link_libraries(DemoGame PUBLIC UGECore UIWindows)


