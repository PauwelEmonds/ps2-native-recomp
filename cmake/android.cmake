# The native part of the Android app (android/, docs/android.md), included by CMakeLists.txt when the Android NDK builds it
# (Gradle runs CMake on this source tree with -DPS2_GENERATED=generated/<game>). It makes libmain.so: the window runner of
# host/ (the same main.cpp, window.cpp, ... as ps2run, started by SDL's Java activity through host/android_main.cpp), the
# translated game (ps2game) and the runtime, on SDL2 and wgpu-native's Vulkan backend.

include(FetchContent)
# SDL2 from its release; the Java side of the same release is in android/app/src/main/java/org/libsdl/app
FetchContent_Declare(SDL2
    URL https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/SDL2-2.32.10.tar.gz
    URL_HASH SHA256=5f5993c530f084535c65a6879e9b26ad441169b3e25d789d83287040a9ca5165)
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL2_DISABLE_SDL2MAIN ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(SDL2)

add_library(main SHARED
    host/android_main.cpp
    host/main.cpp
    host/crash.cpp
    host/window.cpp
    host/keymap.cpp
    host/quickmenu.cpp
    host/stretch.cpp)
target_compile_definitions(main PRIVATE PS2_WINDOW)
# SDLActivity calls SDL_main (host/android_main.cpp), which calls the runner's own main under another name
set_source_files_properties(host/main.cpp PROPERTIES COMPILE_DEFINITIONS main=ps2_host_main)
target_link_libraries(main PRIVATE ps2game ps2runtime SDL2::SDL2 android log)
