# source/GamePlugins/UIWindows/CMakeLists.txt
# UIWindows shared library — reusable RmlUi window ViewModels and base class.
# Provides WindowViewModel base class and concrete implementations like LuaConsoleViewModel.

add_library(UIWindows SHARED
    WindowViewModel.cpp
    TestWindow/TestWindowViewModel.cpp
)

# source/GamePlugins/UIWindows/ is the include root
target_include_directories(UIWindows
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/TestWindow"
)

set_target_properties(UIWindows PROPERTIES
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

target_link_libraries(UIWindows PUBLIC UGECore)

