cmake_minimum_required(VERSION 3.10)

if (POLICY CMP0072)
    cmake_policy(SET CMP0072 NEW)
endif()

project(Code128Studio C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CODE128_VERSION "0.5.8")
add_compile_definitions(CODE128_APP_VERSION="${CODE128_VERSION}")

if (MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

set(MINGW_DLLS "/usr/x86_64-w64-mingw32/bin/libstdc++-6.dll" "/usr/x86_64-w64-mingw32/bin/libgcc_s_seh-1.dll")

# 1. External dependencies
find_package(Freetype REQUIRED)
find_package(OpenGL REQUIRED)

find_package(glfw3 QUIET)
if (NOT glfw3_FOUND)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GLFW REQUIRED glfw3)
endif()

# 2. Shared Engine Sources
set(ENGINE_SOURCES
    src/engine/barcode_core.cpp
    lib/img/libattopng.c
)

set(ENGINE_INCLUDES
    src
    src/engine
    src/gui
    src/print
    src/updater
    lib/img
    ${FREETYPE_INCLUDE_DIRS}
)

# 3. Unified Executable: Code128 Studio (GUI + CLI in ONE single binary)
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui")

set(STUDIO_SOURCES
    src/main.cpp
    src/gui/app.cpp
    src/gui/strip_preview.cpp
    src/gui/preset_manager.cpp
    src/print/print_manager.cpp
    src/updater/updater.cpp
    ${ENGINE_SOURCES}
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

add_executable(code128_studio ${STUDIO_SOURCES})

target_include_directories(code128_studio PRIVATE
    ${ENGINE_INCLUDES}
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${OPENGL_INCLUDE_DIR}
)

if (TARGET glfw)
    target_link_libraries(code128_studio PRIVATE glfw)
elseif (glfw3_FOUND)
    target_link_libraries(code128_studio PRIVATE glfw)
else()
    target_include_directories(code128_studio PRIVATE ${GLFW_INCLUDE_DIRS})
    target_link_libraries(code128_studio PRIVATE ${GLFW_LIBRARIES})
endif()

target_link_libraries(code128_studio PRIVATE
    ${FREETYPE_LIBRARIES}
    ${OPENGL_LIBRARIES}
)

if (UNIX)
    target_link_libraries(code128_studio PRIVATE m)
endif()

if (WIN32)
    target_link_libraries(code128_studio PRIVATE winspool ws2_32 gdi32)
endif()

# 4. Standalone Auto-Updater Executable
add_executable(code128_updater
    src/updater/main_updater.cpp
    src/updater/updater.cpp
)

target_include_directories(code128_updater PRIVATE
    src/updater
)

if (WIN32)
    target_link_libraries(code128_updater PRIVATE ws2_32)
endif()

# 5. Installation Rules
install(TARGETS code128_studio code128_updater
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
