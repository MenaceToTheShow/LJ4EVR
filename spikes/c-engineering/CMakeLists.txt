cmake_minimum_required(VERSION 3.24)
project(lj4evr_spike_c_engineering VERSION 0.0.1 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# SDL3 pinned to a release tag (zlib licence). Static link so the spike is one exe.
include(FetchContent)
# MSVC 19.44.35213 (VS 17.14, toolset 14.44.35207) with Windows SDK 10.0.26100 crashes
# (C1001 in objidl.h, and an access violation in code generation) when SDL3's
# sources are compiled through the precompiled header SDL3 turns on unconditionally.
# Measured on this PC 2026-10-21; the same files compile cleanly without /Yu.
set(CMAKE_DISABLE_PRECOMPILE_HEADERS ON)
# Second finding: MSBuild batches all 223 SDL sources into one CL.exe call and that
# call dies with an access violation during code generation, while every file
# compiles cleanly on its own (bisect 2026-10-21). UseMultiToolTask makes MSBuild
# run one CL.exe per source file, which is also what the Ninja generator does.
set(CMAKE_VS_GLOBALS "UseMultiToolTask=true" "EnforceProcessCountAcrossBuilds=true")
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
# Subsystems the clone will never need: keep the build short.
set(SDL_AUDIO OFF CACHE BOOL "" FORCE)
set(SDL_CAMERA OFF CACHE BOOL "" FORCE)
set(SDL_HAPTIC OFF CACHE BOOL "" FORCE)
set(SDL_JOYSTICK OFF CACHE BOOL "" FORCE)
set(SDL_SENSOR OFF CACHE BOOL "" FORCE)
set(SDL_POWER OFF CACHE BOOL "" FORCE)
set(SDL_GPU OFF CACHE BOOL "" FORCE)
set(SDL_RENDER_GPU OFF CACHE BOOL "" FORCE)
set(SDL_VULKAN OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG        release-3.4.18
  GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(SDL3)

add_executable(seqform WIN32 main.c)
target_link_libraries(seqform PRIVATE SDL3::SDL3-static)
if(MSVC)
  target_compile_options(seqform PRIVATE /W4 /WX /utf-8)
else()
  target_compile_options(seqform PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
