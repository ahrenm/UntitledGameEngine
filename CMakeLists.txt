cmake_minimum_required(VERSION 4.2)
project(untitled)

set(CMAKE_CXX_STANDARD 23)

# Fetch SDL3 automatically
include(FetchContent)
FetchContent_Declare(
        SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG main   # or pin to a release tag like "release-3.2.10"
)
FetchContent_MakeAvailable(SDL3)

# Fetch SDL3_image
FetchContent_Declare(
        SDL3_image
        GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
        GIT_TAG main
)
FetchContent_MakeAvailable(SDL3_image)

# Fetch PhysicsFS (zlib license - permissive commercial use)
FetchContent_Declare(
        PhysFS
        GIT_REPOSITORY https://github.com/icculus/physfs.git
        GIT_TAG main
)
set(PHYSFS_BUILD_STATIC ON  CACHE BOOL "" FORCE)
set(PHYSFS_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(PHYSFS_BUILD_TEST   OFF CACHE BOOL "" FORCE)
set(PHYSFS_BUILD_DOCS   OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(PhysFS)

# Fetch FreeType (required by RmlUi for font rendering)
# Force static so no freetype.dll is needed at runtime
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
        freetype
        GIT_REPOSITORY https://gitlab.freedesktop.org/freetype/freetype.git
        GIT_TAG VER-2-13-3
)
set(FT_DISABLE_ZLIB      ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2     ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG       ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ  ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI    ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(freetype)

# Write a find_package redirect so RmlUi's find_package(Freetype) resolves
# to our fetched copy instead of searching the system.
file(WRITE "${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}/FreetypeConfig.cmake" [[
if(NOT TARGET Freetype::Freetype)
    add_library(Freetype::Freetype ALIAS freetype)
endif()
set(FREETYPE_FOUND TRUE)
set(FREETYPE_VERSION_STRING "2.13.3")
set(FREETYPE_INCLUDE_DIRS "${freetype_SOURCE_DIR}/include")
set(FREETYPE_LIBRARIES freetype)
]])

# Remove old cache overrides ? the redirect file handles this now
unset(FREETYPE_FOUND        CACHE)
unset(FREETYPE_INCLUDE_DIRS CACHE)
unset(FREETYPE_LIBRARIES    CACHE)

# Fetch Lua 5.4 (MIT license)
FetchContent_Declare(
        lua
        GIT_REPOSITORY https://github.com/walterschell/Lua.git
        GIT_TAG        v5.4.7
)
set(LUA_BUILD_COMPILER OFF CACHE BOOL "" FORCE)
set(LUA_BUILD_SHARED   OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(lua)

# Fetch sol2 ? header-only C++ bindings for Lua (MIT license)
FetchContent_Declare(
        sol2
        GIT_REPOSITORY https://github.com/ThePhD/sol2.git
        GIT_TAG        v3.5.0
)
set(SOL2_BUILD_LUA OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(sol2)

# Fetch GLM ? header-only math library (MIT license)
FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG        master
)
FetchContent_MakeAvailable(glm)

# Fetch tinyobjloader ? single-header OBJ/MTL parser (MIT license)
FetchContent_Declare(
        tinyobjloader
        GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
        GIT_TAG        release
)
set(TINYOBJLOADER_BUILD_TEST_LOADER OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(tinyobjloader)

# Fetch RmlUi (MIT license - permissive commercial use)
FetchContent_Declare(
        RmlUi
        GIT_REPOSITORY https://github.com/mikke89/RmlUi.git
        GIT_TAG master
)
set(RMLUI_SAMPLES_ENABLED OFF CACHE BOOL "" FORCE)
set(RMLUI_TESTS_ENABLED   OFF CACHE BOOL "" FORCE)
set(RMLUI_BACKEND "SDL_SDLrenderer" CACHE STRING "" FORCE)
FetchContent_MakeAvailable(RmlUi)

# Copy RmlUi sample fonts into our assets folder at configure time
file(COPY "${rmlui_SOURCE_DIR}/Samples/assets/"
     DESTINATION "${CMAKE_SOURCE_DIR}/assets/rmlui/")

add_executable(untitled WIN32)

# UGECore must be processed before Application so the UGECore target exists
# when Application's CMakeLists calls target_link_libraries(untitled UGECore).
add_subdirectory(source/UGECore)
add_subdirectory(source/Application)
add_subdirectory(source/GamePlugins)

# Plugin DLLs are loaded dynamically at runtime, so keep explicit build dependencies
# to ensure building untitled also builds the plugin DLLs.
add_dependencies(untitled UIWindows DemoGame TeaPot3D)

# Backends path is needed by untitled.exe's main.cpp transitively (via UGECore PUBLIC).
# UGECore exposes it already, but the explicit include here is harmless insurance.
target_include_directories(untitled PRIVATE "${rmlui_SOURCE_DIR}/Backends")

# SDL3/physfs/lua/sol2 are pulled in transitively through UGECore (PUBLIC deps).
# RmlUi is PRIVATE in UGECore (to prevent duplicate-symbol errors in plugin DLLs), but
# untitled.exe doesn't include RmlUi headers directly so no explicit link is needed.
# Plugin DLLs (e.g. DemoGame.dll) are built separately and loaded explicitly at runtime
# by Application's module bootstrapper before UGEApplication::Create().
target_link_libraries(untitled PRIVATE UGECore)


# Pack the assets folder into assets.zip next to the executable
add_custom_command(TARGET untitled POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_DIR="${CMAKE_SOURCE_DIR}"
        -DOUTPUT_ZIP="$<TARGET_FILE_DIR:untitled>/assets.pak"
        -P "${CMAKE_SOURCE_DIR}/pack_assets.cmake"
    COMMENT "Packing assets into assets.pak..."
)

# Standalone target: rebuild assets.zip without recompiling
# Selectable from CLion's build targets dropdown (or: cmake --build . --target pack_assets)
add_custom_target(pack_assets
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_DIR="${CMAKE_SOURCE_DIR}"
        -DOUTPUT_ZIP="${CMAKE_BINARY_DIR}/assets.pak"
        -P "${CMAKE_SOURCE_DIR}/pack_assets.cmake"
    COMMENT "Repacking assets.pak..."
)

# Copy SDL3.dll next to the executable after each build
add_custom_command(TARGET untitled POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:untitled>
)

# Copy SDL3_image.dll next to the executable after each build
add_custom_command(TARGET untitled POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3_image::SDL3_image>
        $<TARGET_FILE_DIR:untitled>
)

# Copy launch settings TOML next to the executable after each build.
add_custom_command(TARGET untitled POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/UGESettings.toml"
        "$<TARGET_FILE_DIR:untitled>/UGESettings.toml"
    COMMENT "Copying UGESettings.toml..."
)

# Copy MinGW runtime DLLs next to the executable so the app runs outside the IDE.
# CLion augments PATH with the MinGW bin dir at launch; Windows Explorer does not.
# CMAKE_CXX_COMPILER points to g++, so its parent directory is the MinGW bin folder.
if(MINGW)
    get_filename_component(MINGW_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    foreach(DLL
        libstdc++-6.dll
        libgcc_s_seh-1.dll
        libwinpthread-1.dll
    )
        if(EXISTS "${MINGW_BIN_DIR}/${DLL}")
            add_custom_command(TARGET untitled POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${MINGW_BIN_DIR}/${DLL}"
                    "$<TARGET_FILE_DIR:untitled>"
                COMMENT "Copying MinGW runtime: ${DLL}"
            )
        endif()
    endforeach()
endif()
