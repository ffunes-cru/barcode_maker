# 1. Minimum CMake version required for the project
cmake_minimum_required(VERSION 3.10)

set(MINGW_DLLS "/usr/x86_64-w64-mingw32/bin/libstdc++-6.dll" "/usr/x86_64-w64-mingw32/bin/libgcc_s_seh-1.dll")

# 2. Name the project
project(Code128Maker C)

# 3. Find the required external library (FreeType)
find_package(Freetype REQUIRED)

# 4. Define the executable (code128_maker) and list all source files
add_executable(code128_maker
    main.c
    ./lib/img/libattopng.c
)

# 5. Specify include directories
target_include_directories(code128_maker PRIVATE
    ${FREETYPE_INCLUDE_DIRS}
)

# 6. Specify libraries to link
target_link_libraries(code128_maker PRIVATE
     ${FREETYPE_LIBRARIES}
)


# --- FIXES FOR INSTALLATION AND RESOURCE COPYING ---

# 7. Explicitly install the executable to the 'bin' directory.
# This fixes the issue of the executable not being found or not ending up in 'bin'.
install(TARGETS code128_maker
    RUNTIME DESTINATION bin # Executable goes into the 'bin' folder
)

# 8. Resource Files: Ensure they go into the SAME 'bin' directory as the executable.
# The previous DESTINATION . might have put them in the install root, separating them from the executable.
set(RESOURCE_FILES
    "code128char.txt"
    "code128int.txt"
    "font.otf"
)

install(FILES ${RESOURCE_FILES}
    DESTINATION bin       # <--- CHANGED from DESTINATION . to DESTINATION bin
    COMPONENT Runtime
)

install(PROGRAMS ${FREETYPE_LIBRARIES}
    DESTINATION bin
    COMPONENT Runtime
)

# Copy the MinGW/GCC runtime DLLs
install(FILES ${MINGW_DLLS}
    DESTINATION bin
)
