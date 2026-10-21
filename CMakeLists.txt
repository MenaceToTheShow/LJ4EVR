cmake_minimum_required(VERSION 3.25)   # 3.25: FetchContent SYSTEM (LORD_C-14)
include(cmake/version.cmake)                      # VERSION -> LJ_VERSION (design 9.4)
project(LJ4EVR VERSION ${LJ_VERSION} LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(LJ_SDL "Build platform/sdl and the lj4evr app (OFF = tests + headless renderer only)" ON)
option(LJ_WARNINGS_AS_ERRORS "Warnings are errors" ON)
set(LJ_SANITIZE "off" CACHE STRING "off | address,undefined | thread")
set_property(CACHE LJ_SANITIZE PROPERTY STRINGS "off" "address,undefined" "thread")

# --- warning and floating-point flags (design 9.1) -------------------------
# /fp:strict and -ffp-contract=off are a backstop: no SDL-free directory may use
# float at all (tools/test_layering.py), platform/ may.
if(MSVC)
  # Static CRT for every target incl. the fetched SDL3: the installed lj4evr.exe must start on a
  # machine without the VC++ redistributable (ME-42; ci.yml asserts dumpbin shows no vcruntime).
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  set(LJ_WARN /W4 /permissive- /utf-8 /fp:strict)
  if(LJ_WARNINGS_AS_ERRORS)
    list(APPEND LJ_WARN /WX)
  endif()
  string(REGEX REPLACE "/W[0-4]" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")   # no /W3 + /W4 clash
else()
  set(LJ_WARN -Wall -Wextra -Wpedantic -Wshadow -Wvla -ffp-contract=off)
  if(LJ_WARNINGS_AS_ERRORS)
    list(APPEND LJ_WARN -Werror)
  endif()
endif()
if(NOT LJ_SANITIZE STREQUAL "off")
  if(MSVC)
    add_compile_options(/fsanitize=${LJ_SANITIZE})
  else()
    add_compile_options(-fsanitize=${LJ_SANITIZE} -fno-omit-frame-pointer)
    add_link_options(-fsanitize=${LJ_SANITIZE})
  endif()
endif()

# --- generated version.h ------------------------------------------------------
set(LJ_GEN_DIR "${CMAKE_BINARY_DIR}/generated")
lj_write_version_h("${LJ_GEN_DIR}")

# --- SDL3 (only with LJ_SDL) --------------------------------------------------
if(LJ_SDL)
  include(FetchContent)
  # MSVC 19.44 workarounds bisected in spikes/c-engineering (docs/msvc-sdl-build.md):
  # 1. C1001 through SDL's unconditional PCH; SDL_PRECOMPILED_HEADERS is not an option in 3.4.18.
  set(CMAKE_DISABLE_PRECOMPILE_HEADERS ON)
  # 2. MSBuild's one-CL-per-20-files batches die in code generation; one CL.exe per source.
  set(CMAKE_VS_GLOBALS "UseMultiToolTask=true" "EnforceProcessCountAcrossBuilds=true")
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
  set(SDL_TESTS OFF CACHE BOOL "" FORCE)
  set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
  # Subsystem trim (design 9.1): the clone draws everything itself. SDL_AUDIO stays ON for W5.
  foreach(sub JOYSTICK HAPTIC CAMERA GPU DIALOG PROCESS HIDAPI OFFSCREEN RENDER VULKAN SENSOR POWER TRAY OPENGL OPENGLES DIRECTX)
    set(SDL_${sub} OFF CACHE BOOL "" FORCE)
  endforeach()
  set(SDL_AUDIO ON CACHE BOOL "" FORCE)
  if(APPLE)   # Cocoa has no native window framebuffer: SDL_GetWindowSurface is emulated through a renderer (Metal, software fallback)
    set(SDL_RENDER ON CACHE BOOL "" FORCE)
  endif()
  FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-3.4.18
    GIT_SHALLOW    TRUE
    SYSTEM)
  FetchContent_MakeAvailable(SDL3)
endif()

# --- vendored miniz: SYSTEM include, warnings off ------------------------------
add_library(lj_miniz STATIC third_party/miniz/miniz.c)
target_include_directories(lj_miniz SYSTEM PUBLIC third_party/miniz)
if(MSVC)
  target_compile_options(lj_miniz PRIVATE /W0)   # ME-62: /w overrode /W4 with D9025 on every MSVC build
else()
  target_compile_options(lj_miniz PRIVATE -w)
endif()
set_target_properties(lj_miniz PROPERTIES FOLDER third_party)

# --- one static library per src/<dir> (design 2) -------------------------------
# Sources are globbed so a track adds a file without editing this list.
function(lj_layer name)                            # lj_layer(<dir> <dep>...)
  file(GLOB_RECURSE srcs CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/${name}/*.c")
  add_library(lj_${name} STATIC ${srcs})
  target_include_directories(lj_${name} PUBLIC "${CMAKE_SOURCE_DIR}/src" "${LJ_GEN_DIR}")
  target_compile_options(lj_${name} PRIVATE ${LJ_WARN})
  target_link_libraries(lj_${name} PUBLIC ${ARGN})
  set_target_properties(lj_${name} PROPERTIES FOLDER src)
endfunction()

lj_layer(core)
lj_layer(fixlib   lj_core)
lj_layer(model    lj_fixlib lj_core lj_miniz)   # ME-62: miniz is the library backup (.lj4b, model/lj_library.c); the W1 importer stub does not use it
lj_layer(engine   lj_model lj_fixlib lj_core)
lj_layer(io       lj_model lj_fixlib lj_core)
lj_layer(gfx      lj_core)
lj_layer(ui       lj_gfx lj_core)
lj_layer(desktop  lj_ui lj_model lj_engine lj_fixlib lj_io)
lj_layer(controls lj_ui lj_model lj_engine lj_fixlib lj_io)
lj_layer(windows  lj_ui lj_model lj_engine lj_fixlib lj_io)
set(LJ_SDL_FREE_LIBS lj_windows lj_controls lj_desktop lj_ui lj_gfx lj_engine lj_io lj_model lj_fixlib lj_core)

if(WIN32)
  set(LJ_MANIFEST "${CMAKE_SOURCE_DIR}/packaging/windows/lj4evr.manifest")   # activeCodePage UTF-8, design 2.1
else()
  set(LJ_MANIFEST "")
endif()
if(UNIX AND NOT APPLE)
  find_package(Threads REQUIRED)
  target_link_libraries(lj_core PUBLIC Threads::Threads)
endif()

# --- lj4evr_tests: CTest driver, no SDL ----------------------------------------
file(GLOB_RECURSE LJ_TEST_SRCS CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/tests/*.c")
add_executable(lj4evr_tests ${LJ_TEST_SRCS} ${LJ_MANIFEST})
target_include_directories(lj4evr_tests PRIVATE "${CMAKE_SOURCE_DIR}/tests")
target_compile_options(lj4evr_tests PRIVATE ${LJ_WARN})
target_link_libraries(lj4evr_tests PRIVATE ${LJ_SDL_FREE_LIBS})
target_compile_definitions(lj4evr_tests PRIVATE LJ_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

# --- lj4evr_render: headless BMP renderer, no SDL ------------------------------
add_executable(lj4evr_render src/app/lj_render_main.c ${LJ_MANIFEST})
target_compile_options(lj4evr_render PRIVATE ${LJ_WARN})
target_link_libraries(lj4evr_render PRIVATE ${LJ_SDL_FREE_LIBS})

# --- platform/sdl + lj4evr app (only with LJ_SDL) ------------------------------
if(LJ_SDL)
  file(GLOB_RECURSE LJ_PLATFORM_SRCS CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/platform/*.c")
  add_library(lj_platform STATIC ${LJ_PLATFORM_SRCS})
  target_include_directories(lj_platform PUBLIC "${CMAKE_SOURCE_DIR}/src" "${LJ_GEN_DIR}")
  target_compile_options(lj_platform PRIVATE ${LJ_WARN})
  target_link_libraries(lj_platform PUBLIC SDL3::SDL3-static lj_ui lj_gfx lj_engine lj_io lj_core)
  set_target_properties(lj_platform PROPERTIES FOLDER src)

  file(GLOB LJ_APP_SRCS CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/app/lj_*.c")
  list(REMOVE_ITEM LJ_APP_SRCS "${CMAKE_SOURCE_DIR}/src/app/lj_render_main.c")
  add_executable(lj4evr WIN32 MACOSX_BUNDLE src/app/main.c ${LJ_APP_SRCS} ${LJ_MANIFEST})
  target_compile_options(lj4evr PRIVATE ${LJ_WARN})
  target_link_libraries(lj4evr PRIVATE lj_platform ${LJ_SDL_FREE_LIBS})
  set_target_properties(lj4evr PROPERTIES
    OUTPUT_NAME lj4evr
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/packaging/macos/Info.plist.in"
    MACOSX_BUNDLE_BUNDLE_NAME "LJ4EVR"
    MACOSX_BUNDLE_GUI_IDENTIFIER "org.lj4evr.app"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${LJ_VERSION}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${LJ_VERSION}"
    MACOSX_BUNDLE_COPYRIGHT "MIT, 2026 LJ4EVR contributors")
endif()

# --- CTest ----------------------------------------------------------------------
enable_testing()
# Every lj4evr_tests subcommand is one CTest; a track appends its name here when it
# adds a row to the table in tests/lj_tests.c.
set(LJ_TEST_SUBCOMMANDS widget widgets_lists forms_tables draw_goldens bmp text encoding ini textrec lock clock_monotonic trig engine_stub dump-frames render ui_golden font_corpus font_cells art_table widgets_lj widgets_basic window menu formsini hittest)
# W1 rows: each track appends its names here (append-only, docs/w1-contracts.md):
list(APPEND LJ_TEST_SUBCOMMANDS test_model_table)
list(APPEND LJ_TEST_SUBCOMMANDS test_desktop)   # desktop: toolbar geometry, ME-29 menu table, selection rules, F9 enables, glyph ink, MAINWINDOW golden
list(APPEND LJ_TEST_SUBCOMMANDS test_fixlib_profiles)
list(APPEND LJ_TEST_SUBCOMMANDS test_windows_b)
list(APPEND LJ_TEST_SUBCOMMANDS test_engine_fade test_engine_ownership test_engine_perf test_engine_post)
list(APPEND LJ_TEST_SUBCOMMANDS test_uiscript)   # app track
list(APPEND LJ_TEST_SUBCOMMANDS test_controls_a)
list(APPEND LJ_TEST_SUBCOMMANDS test_model_records test_model_library test_model_scene)
list(APPEND LJ_TEST_SUBCOMMANDS test_replay_grammar)   # replay
list(APPEND LJ_TEST_SUBCOMMANDS test_hotkeys)
list(APPEND LJ_TEST_SUBCOMMANDS test_controls_b)
list(APPEND LJ_TEST_SUBCOMMANDS test_windows_a)
list(APPEND LJ_TEST_SUBCOMMANDS test_windows_c)   # windows-c
list(APPEND LJ_TEST_SUBCOMMANDS test_controls_c)   # controls-c
list(APPEND LJ_TEST_SUBCOMMANDS ui_golden_ctl)     # integrator: control-window goldens
foreach(sub ${LJ_TEST_SUBCOMMANDS})
  add_test(NAME ${sub} COMMAND lj4evr_tests ${sub})
endforeach()
# replay_all: every tests/engine/replay/*.lj4r through the replay harness (design 3.7); zero files = pass.
file(GLOB LJ_REPLAY_FILES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/tests/engine/replay/*.lj4r")
add_test(NAME replay_all COMMAND lj4evr_tests replay ${LJ_REPLAY_FILES})
# replay_dump: the frame dump the determinism job compares (replay-frames.bin in the build dir), every scenario with a per-scenario header.
add_test(NAME replay_dump COMMAND lj4evr_tests --dump-frames "${CMAKE_BINARY_DIR}/replay-frames.bin" ${LJ_REPLAY_FILES})
# Developer-only tests (the 48 shipped + 200 sampled .GS2 files, the exe DFMs) read Martin's data from this folder
# and skip when it is empty; it is never set in CI (design 5.3, reference/ is gitignored).
set(LJ4EVR_REFERENCE_DIR "" CACHE PATH "Developer-only: path to the Martin reference folder (D:/lj4evr/reference); empty = those tests skip")
target_compile_definitions(lj4evr_tests PRIVATE LJ4EVR_REFERENCE_DIR="${LJ4EVR_REFERENCE_DIR}")
find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
  add_test(NAME test_layering
           COMMAND "${Python3_EXECUTABLE}" -I "${CMAKE_SOURCE_DIR}/tools/test_layering.py"
                   --src "${CMAKE_SOURCE_DIR}/src" --cc "${CMAKE_C_COMPILER}" --gen "${LJ_GEN_DIR}")
  add_test(NAME gen_vectors_check                   # committed lj_trig_gen.c + tests/vectors.h match tools/ref (design 9.2)
           COMMAND "${Python3_EXECUTABLE}" -I "${CMAKE_SOURCE_DIR}/tools/gen_vectors.py" --check)
  add_test(NAME dfm2c_selftest COMMAND "${Python3_EXECUTABLE}" -I "${CMAKE_SOURCE_DIR}/tools/dfm2c.py" --selftest)   # ME-49: no reference tree needed
  add_test(NAME qxf2c_selftest COMMAND "${Python3_EXECUTABLE}" -I "${CMAKE_SOURCE_DIR}/tools/qxf2c.py" --selftest)   # ME-26: one in-repo QXF
endif()

# --- install + CPack (design 9.5, ME-19 per-user NSIS) ---------------------------
if(LJ_SDL)
  install(TARGETS lj4evr RUNTIME DESTINATION . BUNDLE DESTINATION .)
endif()
install(FILES NOTICE DESTINATION . RENAME THIRD_PARTY_NOTICES.txt)

set(CPACK_PACKAGE_NAME "LJ4EVR")
set(CPACK_PACKAGE_VENDOR "LJ4EVR contributors")
set(CPACK_PACKAGE_VERSION "${LJ_VERSION}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "LJ4EVR")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A clone of Martin LightJockey 2 with sACN output")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_STRIP_FILES ON)
if(WIN32)
  set(CPACK_GENERATOR NSIS)
  set(CPACK_PACKAGE_FILE_NAME "LJ4EVR-${LJ_VERSION}-win64-setup")
  # CPack picks up packaging/nsis/NSIS.template.in (RequestExecutionLevel user,
  # InstallDir $LOCALAPPDATA\Programs\LJ4EVR, SetShellVarContext current) through
  # CPACK_MODULE_PATH; there is no CPACK_NSIS_TEMPLATE variable in CMake.
  set(CPACK_MODULE_PATH "${CMAKE_SOURCE_DIR}/packaging/nsis")
  set(CPACK_NSIS_EXECUTABLES_DIRECTORY ".")
  set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
  set(CPACK_NSIS_DISPLAY_NAME "LJ4EVR ${LJ_VERSION}")
  set(CPACK_NSIS_PACKAGE_NAME "LJ4EVR ${LJ_VERSION}")
  set(CPACK_PACKAGE_EXECUTABLES "lj4evr;LJ4EVR")
  set(CPACK_CREATE_DESKTOP_LINKS "lj4evr")
elseif(APPLE)
  set(CPACK_GENERATOR DragNDrop)
  set(CPACK_PACKAGE_FILE_NAME "LJ4EVR-${LJ_VERSION}-macos")
  set(CPACK_DMG_VOLUME_NAME "LJ4EVR ${LJ_VERSION}")
else()
  set(CPACK_GENERATOR TGZ)
  set(CPACK_PACKAGE_FILE_NAME "LJ4EVR-${LJ_VERSION}-linux")
endif()
include(CPack)

# --- W1 sacn track (append-only): winsock + iphlpapi for io/lj_sacn.c, io/lj_net.c; the four tests/sacn rows ---
if(WIN32)
  target_link_libraries(lj_io PUBLIC ws2_32 iphlpapi)
endif()
foreach(sub test_sacn_packets test_sacn_rate test_sacn_loopback test_sacn_cfg)
  add_test(NAME ${sub} COMMAND lj4evr_tests ${sub})
endforeach()
set_tests_properties(test_sacn_rate test_sacn_loopback PROPERTIES RESOURCE_LOCK sacn_port)   # integrator: both send to 127.0.0.1:5568; ctest -j must not interleave them

# app track: the SDL-free app files tests/ui/test_uiscript.c drives headless (docs/w1-contracts.md, append-only section)
target_sources(lj4evr_tests PRIVATE "${CMAKE_SOURCE_DIR}/src/app/lj_uiscript.c" "${CMAKE_SOURCE_DIR}/src/app/lj_startup.c")
