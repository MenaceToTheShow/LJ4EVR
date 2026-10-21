# LJ4EVR spike b-lazy: one SDL3 window drawn as TSequenceForm (Windows-classic chrome).
# Configure: cmake -S . -B build -G "Visual Studio 17 2022" -A x64
# Build:     cmake --build build --config Release
cmake_minimum_required(VERSION 3.24)
project(lj4evr_spike_b_lazy C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(FetchContent)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)        # static: one exe, nothing to copy
set(SDL_STATIC ON  CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
# MSVC 19.44.35213 crashes (0xC0000005) compiling src/render/software/SDL_blendfillrect.c
# at /O2 when SDL's precompiled header is in use; the same file builds standalone.  Seen
# 2026-10-21 on this PC with CMake 4.1 + VS 17.14.  Turning the PCH off avoids it.
set(SDL_PRECOMPILED_HEADERS OFF CACHE BOOL "" FORCE)
# Renderer backends the clone will never use (we draw rects with SDL_Renderer on D3D11/software).
set(SDL_RENDER_GPU OFF CACHE BOOL "" FORCE)
set(SDL_RENDER_VULKAN OFF CACHE BOOL "" FORCE)
set(SDL_RENDER_D3D12 OFF CACHE BOOL "" FORCE)
set(SDL_VULKAN OFF CACHE BOOL "" FORCE)
set(SDL_GPU OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG        release-3.2.16          # pinned release tag (zlib licence)
  GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(SDL3)

add_executable(seqwin main.c)
target_link_libraries(seqwin PRIVATE SDL3::SDL3)
if(MSVC)
  target_compile_options(seqwin PRIVATE /W4)
endif()
