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

add_subdirectory(source/UGECore)

# Standalone target: rebuild assets.pak without recompiling
# Selectable from CLion's build targets dropdown (or: cmake --build . --target pack_assets)
add_custom_target(pack_assets
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_DIR="${CMAKE_SOURCE_DIR}"
        -DOUTPUT_ZIP="${CMAKE_BINARY_DIR}/assets.pak"
        -P "${CMAKE_SOURCE_DIR}/pack_assets.cmake"
    COMMENT "Repacking assets.pak..."
)

