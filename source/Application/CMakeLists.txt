# source/Application/CMakeLists.txt
# Executable entry point. All engine/UI code lives in the UGECore shared library.
# UGECore's Public/ include path is inherited transitively via target_link_libraries.

target_sources(untitled PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/ConfigLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

target_link_libraries(untitled PRIVATE UGECore)


