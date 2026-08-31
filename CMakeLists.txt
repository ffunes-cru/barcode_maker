cmake_minimum_required(VERSION 3.10)

project(Code128Studio C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(MINGW_DLLS "/usr/x86_64-w64-mingw32/bin/libstdc++-6.dll" "/usr/x86_64-w64-mingw32/bin/libgcc_s_seh-1.dll")

# 1. External dependencies
find_package(Freetype REQUIRED)
find_package(OpenGL REQUIRED)

find_package(glfw3 QUIET)
if (NOT glfw3_FOUND)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GLFW REQUIRED glfw3)
endif()

# 2. Engine sources (Shared between CLI and GUI)
set(ENGINE_SOURCES
    src/engine/barcode_core.cpp
    lib/img/libattopng.c
)

set(ENGINE_INCLUDES
    src/engine
    lib/img
    ${FREETYPE_INCLUDE_DIRS}
)

# 3. CLI Executable (code128_maker)
add_executable(code128_maker
    src/main_cli.cpp
    ${ENGINE_SOURCES}
)

target_include_directories(code128_maker PRIVATE
    ${ENGINE_INCLUDES}
)

target_link_libraries(code128_maker PRIVATE
    ${FREETYPE_LIBRARIES}
    m
)

# 4. GUI Executable (code128_gui)
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui")

set(GUI_SOURCES
    src/main_gui.cpp
    src/gui/app.cpp
    src/gui/strip_preview.cpp
    src/print/print_manager.cpp
    ${ENGINE_SOURCES}
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

add_executable(code128_gui ${GUI_SOURCES})

target_include_directories(code128_gui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${ENGINE_INCLUDES}
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${OPENGL_INCLUDE_DIR}
)

if (TARGET glfw)
    target_link_libraries(code128_gui PRIVATE glfw)
elseif (glfw3_FOUND)
    target_link_libraries(code128_gui PRIVATE glfw)
else()
    target_include_directories(code128_gui PRIVATE ${GLFW_INCLUDE_DIRS})
    target_link_libraries(code128_gui PRIVATE ${GLFW_LIBRARIES})
endif()

target_link_libraries(code128_gui PRIVATE
    ${FREETYPE_LIBRARIES}
    ${OPENGL_LIBRARIES}
    m
)

if (WIN32)
    target_link_libraries(code128_gui PRIVATE winspool)
endif()

# 5. Installation Rules
install(TARGETS code128_maker code128_gui
    RUNTIME DESTINATION bin
)

set(RESOURCE_FILES
    "code128char.txt"
    "code128int.txt"
    "font.otf"
    "input_rep.txt"
)

install(FILES ${RESOURCE_FILES}
    DESTINATION bin
    COMPONENT Runtime
)

if (EXISTS "${MINGW_DLLS}")
    install(FILES ${MINGW_DLLS}
        DESTINATION bin
    )
endif()
