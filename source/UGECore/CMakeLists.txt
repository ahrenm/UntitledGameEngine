# source/UGECore/CMakeLists.txt
# Builds the UGECore shared library — the engine layer consumed by Application.
# All headers under Public/ are exposed to consumers via target_include_directories PUBLIC.

add_library(UGECore SHARED
    # RmlUi SDL backend — must live in UGECore since RmlUILayer.cpp references these symbols
    "${rmlui_SOURCE_DIR}/Backends/RmlUi_Platform_SDL.cpp"
    "${rmlui_SOURCE_DIR}/Backends/RmlUi_Renderer_SDL.cpp"
        Private/UGEApplication.cpp
    Private/ServiceLocator.cpp
    Private/PhysFSFileInterface.cpp
    Private/Layers/AppLayer.cpp
        Private/Layers/UDEDataLayer.cpp
        Private/Layers/DataStores/StoreBase.cpp
        Private/Layers/DataStores/RuntimeStore.cpp
        Private/Layers/DataStores/StoreSerializer.cpp
        Private/Layers/DataStores/DatasetStore.cpp
    Private/Layers/LoggingLayer.cpp
    Private/Layers/LuaLayer.cpp
    Private/Layers/PhysFSLayer.cpp
    Private/Layers/SDLLayer.cpp
    Private/Layers/RmlUILayer.cpp
    Private/SceneRegistry.cpp
    Private/ViewModelRegistry.cpp
    Private/LayerRegistry.cpp

    Public/Render3D/Camera3D.h
    Public/Render3D/Light3D.h
    Public/Render3D/Mesh3D.h

    Private/Sprite/SpriteSheet.cpp
    Private/Sprite/Sprite.cpp
    Public/Sprite/Sprite.h
    Public/Sprite/SpriteSheet.h
    Public/Sprite/AnimatedSprite.h
        Public/toml.hpp
     Public/GameClasses/BoxCollision.h
     Public/GameClasses/BoxCollisionRegistry.h
     Private/BoxCollisionRegistry.cpp
        Public/GameClasses/BoxCollisionGrid.h
        Private/BoxCollisionGrid.cpp
)

# Strip the MinGW "lib" prefix so the output is UGECore.dll, not libUGECore.dll
set_target_properties(UGECore PROPERTIES PREFIX "")

# Public/ is the include root — consumers use #include <Layers/RmlUILayer.h> etc.
# The RmlUi Backends/ path is PUBLIC because RmlUILayer.h includes backend headers directly.
# The RmlUi Include/ path is PUBLIC because public headers expose Rml types (ViewModel.h etc.).
# It is NOT inherited transitively from RmlUi::Core because that target is PRIVATE (see below).
target_include_directories(UGECore
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/Public"
        "${rmlui_SOURCE_DIR}/Include"
        "${rmlui_SOURCE_DIR}/Backends"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/Private"
        "${tinyobjloader_SOURCE_DIR}"
)

target_compile_definitions(UGECore
    PUBLIC
        # Required by RmlUi_Platform_SDL.h, which is included by the public RmlUILayer.h.
        # Must be PUBLIC so any consumer that includes UGECore public headers gets it too.
        RMLUI_SDL_VERSION_MAJOR=3
        # RmlUi is statically linked into UGECore.dll (see PRIVATE RmlUi::Core below).
        # Consumers must compile with RMLUI_STATIC_LIB so RmlUi headers emit plain
        # symbol references instead of __declspec(dllimport) / __imp_-prefixed ones.
        # Without this, consumers generate __imp_Rml::Foo references that can't be
        # satisfied by libUGECore.dll.a's re-exported (non-__imp_) Rml stubs.
        RMLUI_STATIC_LIB
)

# PUBLIC: anything exposed through Public/ headers (consumers need these too).
# PRIVATE: implementation-only deps — statically embedded into UGECore.dll.
#   RmlUi::Core and RmlUi::Debugger are PRIVATE so their static archives (librmlui.a)
#   are NOT propagated as link dependencies to consumers (e.g. DemoGame.dll). Consumers
#   access Rml symbols at runtime via UGECore.dll; re-linking the static archive would
#   cause "multiple definition" errors. Include paths are exposed separately above.
target_link_libraries(UGECore
    PRIVATE
        RmlUi::Core
        RmlUi::Debugger
        tinyobjloader          # PRIVATE: implementation detail; header only included in Render3DLayer.cpp
    PUBLIC
        glm::glm               # PUBLIC: Camera3D.h / Light3D.h / Mesh3D.h expose glm types
        sol2::sol2
        lua_static          # PUBLIC: sol2 public headers include lua.h directly
        SDL3::SDL3          # PUBLIC so SDL3/SDL_main.h is visible in main.cpp
        SDL3_image::SDL3_image
        physfs-static       # PUBLIC: PhysFSLayer.h includes physfs.h directly
)


# Force Rml::FamilyBase::GetNewId into the UGECore.dll link.
#
# GetNewId lives in librmlui.a:Traits.cpp.obj.  UGECore's own code never calls
# it directly (because UGECore does not instantiate Rml::Family<T>::Id()), so the
# linker does not pull Traits.cpp.obj from the archive.  Consumers such as DemoGame.dll
# instantiate Family<T>::Id() via data-model ViewModel code, causing an "undefined
# reference to Rml::FamilyBase::GetNewId()" link error against libUGECore.dll.a.
#
# -Wl,-u,<symbol> marks the symbol as "undefined", causing ld to search the archives
# for a definition and pull in Traits.cpp.obj.  MinGW then exports it (along with
# every other symbol in the DLL) via its default export-all behaviour.
target_link_options(UGECore PRIVATE "-Wl,-u,_ZN3Rml10FamilyBase8GetNewIdEv")

# Windows: generate UGECore_export.h so UGECORE_EXPORT resolves to dllexport/dllimport
include(GenerateExportHeader)
generate_export_header(UGECore
    BASE_NAME        UGECORE
    EXPORT_FILE_NAME "${CMAKE_CURRENT_SOURCE_DIR}/Public/UGECore_export.h"
)

# Copy UGECore.dll next to the executable after each build
add_custom_command(TARGET UGECore POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:UGECore>
        $<TARGET_FILE_DIR:untitled>
    COMMENT "Copying UGECore.dll..."
)

