cmake_minimum_required(VERSION 3.24)
project(lj4evr_spike_a_fidelity C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Stack under test: SDL3 for the native window/event/blit only; every pixel is drawn by our code.
# Pinned to a release tag; SDL3 is zlib-licensed. Static link so the spike is one exe.
include(FetchContent)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
# Only video + events are needed (the clone draws everything itself); the rest is off to shorten the build
# and to sidestep an MSVC 14.44 crash seen while compiling SDL's hidapi/joystick/gpu sources on this PC.
foreach(sub JOYSTICK HAPTIC CAMERA GPU DIALOG PROCESS HIDAPI OFFSCREEN RENDER VULKAN SENSOR POWER TRAY OPENGL OPENGLES DIRECTX)
    set(SDL_${sub} OFF CACHE BOOL "" FORCE)
endforeach()
FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-3.4.18
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(SDL3)

add_executable(spike_sequence main.c)
target_link_libraries(spike_sequence PRIVATE SDL3::SDL3-static)
if(WIN32)
    target_link_libraries(spike_sequence PRIVATE gdi32)   # system "MS Sans Serif" glyph source
endif()
