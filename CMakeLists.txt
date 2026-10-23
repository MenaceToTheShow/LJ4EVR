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
# desktop, controls and windows are siblings that call each other (W2: desktop's default actions open windows): a static
# cycle CMake repeats on the link line, which GNU ld needs (it reads archives once, in order). ld64 and link.exe resolve
# across archives and ld64 warns about the repeats, so only Linux gets the cycle.
if(UNIX AND NOT APPLE)
  target_link_libraries(lj_desktop  PUBLIC lj_windows lj_controls)
  target_link_libraries(lj_controls PUBLIC lj_windows lj_desktop)
  target_link_libraries(lj_windows  PUBLIC lj_controls lj_desktop)
endif()
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
list(APPEND LJ_TEST_SUBCOMMANDS test_windows_b2)   # windows-b W2: Background Cue, Statics 2, Smoke Control
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

# ---- W2 contracts (docs/w2-contracts.md), append-only ----
# MIDI backend: io/ is globbed, so the two backends that do not belong to this OS are compiled out (HEADER_FILE_ONLY)
# and the OS libraries linked (design 8.3; tools/test_layering.py OS_ALLOW names the two files). lj_midi_null.c is
# always in the tree and is the backend everywhere else (Linux CI, the tests on every OS run through lj_midi_inject).
if(WIN32)
  set(LJ_MIDI_SKIP "${CMAKE_SOURCE_DIR}/src/io/lj_midi_null.c" "${CMAKE_SOURCE_DIR}/src/io/lj_midi_coremidi.c")
  target_link_libraries(lj_io PUBLIC winmm)
elseif(APPLE)
  set(LJ_MIDI_SKIP "${CMAKE_SOURCE_DIR}/src/io/lj_midi_null.c" "${CMAKE_SOURCE_DIR}/src/io/lj_midi_winmm.c")
  target_link_libraries(lj_io PUBLIC "-framework CoreMIDI" "-framework CoreFoundation")
else()
  set(LJ_MIDI_SKIP "${CMAKE_SOURCE_DIR}/src/io/lj_midi_winmm.c" "${CMAKE_SOURCE_DIR}/src/io/lj_midi_coremidi.c")
endif()
set_source_files_properties(${LJ_MIDI_SKIP} PROPERTIES HEADER_FILE_ONLY TRUE)
foreach(sub test_model_w2_range test_model_w2_cuepages test_runner_loop test_intensity_rule test_statics test_midi_map test_prefs)
  add_test(NAME ${sub} COMMAND lj4evr_tests ${sub})
endforeach()
# profiles_ids: every exe id of the W2 tier-1 families 15/02-15/05 has a hand file (design 10 W2 acceptance 5, ME-76).
# Red until the W2 profiles track lands the hand files; the message names each missing id.
if(Python3_FOUND)
  add_test(NAME profiles_ids COMMAND "${Python3_EXECUTABLE}" -I "${CMAKE_SOURCE_DIR}/tools/check_profiles.py" --tier 1 2 3 4 5)
endif()
# (the contracts' DISABLED property on profiles_ids is removed: the W2 profiles track landed the 15/02-15/05 hand files)

# W2 midi track (append-only): hotkey setups 1..8 (tests/io/test_hotkey_setups.c)
add_test(NAME test_hotkey_setups COMMAND lj4evr_tests test_hotkey_setups)
# W2 desktop track (append-only): F8 order, LED view, ME-29 gates, W2 menu leaves, hotkey setups, memory gauge
add_test(NAME test_desktop_w2 COMMAND lj4evr_tests test_desktop_w2)
# W2 replay track (append-only): the W2 replay line forms, and the 10-minute two-slot run headless (design 10 W2 acceptance 3;
# the acceptance integrator repeats the same file on the wire). The file sits outside the replay_all / replay_dump glob.
add_test(NAME test_replay_w2 COMMAND lj4evr_tests test_replay_w2)
add_test(NAME replay_wire_drift COMMAND lj4evr_tests replay "${CMAKE_SOURCE_DIR}/tests/engine/replay/wire/drift_600s_two_slots_wire.lj4r")
# W2 model track (append-only): the W2 record kinds and the library dialogs (tests/model/test_model_w2_*.c)
foreach(sub test_model_w2_records test_model_w2_library)
  add_test(NAME ${sub} COMMAND lj4evr_tests ${sub})
endforeach()

# W2 windows-a (append-only): Loop Options, Cue Time Control, cue pages, transparent / flash (tests/ui/test_windows_a.c)
add_test(NAME test_windows_a_w2 COMMAND lj4evr_tests test_windows_a_w2)

# W2 app track (append-only): the SDL-free app seam (pending loads, MIDI dispatch, prefs, snapshot) the tests drive headless
target_sources(lj4evr_tests PRIVATE "${CMAKE_SOURCE_DIR}/src/app/lj_app_seam.c")
add_test(NAME test_uiscript_w2 COMMAND lj4evr_tests test_uiscript_w2)
# W2 engine track (append-only): chase timer, lock hold (ME-81), the W2 kinds through the tick
foreach(sub test_chase test_lock_hold test_engine_w2)
  add_test(NAME ${sub} COMMAND lj4evr_tests ${sub})
endforeach()
set_tests_properties(test_lock_hold PROPERTIES RUN_SERIAL TRUE)   # ME-96: a wall-clock bound; ctest -j must not load the CPU beside it

# W2 windows-d (append-only): System Preferences, About, Delete Files, User Libraries, Backup/Restore, Notes, Striking Lamps, Edit Hotkeys, MIDI-IN
add_test(NAME test_windows_d COMMAND lj4evr_tests test_windows_d)

# W2 profiles track (append-only): ME-76 exe_profiles_check beside qxf2c_selftest (binding column vs hand/*.c + qxf_map.tsv);
# test_profiles_w2 = the 15/02-15/05 hand profiles by id (footprints, 16-bit pairs, palettes, ME-25/27 states)
if(Python3_FOUND)
  add_test(NAME exe_profiles_check COMMAND "${Python3_EXECUTABLE}" -I "${CMAKE_SOURCE_DIR}/tools/exe_profiles_check.py" --check)
endif()
add_test(NAME test_profiles_w2 COMMAND lj4evr_tests test_profiles_w2)

# ---- W3 contracts (docs/w3-contracts.md section 7), append-only ----
# fixlib inflates .Udf stream 0 with miniz tinfl (design 5.3); the cue list runner is SDL-free and the tests drive it.
target_link_libraries(lj_fixlib PUBLIC lj_miniz)
target_sources(lj4evr_tests PRIVATE "${CMAKE_SOURCE_DIR}/src/app/lj_cuelist_run.c")
set(LJ_W3_TESTS test_cuelist test_cuelist_run test_genmacro test_movemacro test_gdc_grammar test_gs2 test_udf_crc test_profile_editor test_followspot)
foreach(sub ${LJ_W3_TESTS})
  add_test(NAME ${sub} COMMAND lj4evr_tests ${sub})
endforeach()
# Expected-pending rows (the W2 profiles_ids precedent): each prints PENDING and fails until its track lands the body;
# the track removes its name from this list in the same commit. test_cuelist and test_profile_editor carry real rows now.
# (test_genmacro: the genmacro track landed its body; the DISABLED row is gone)
if(Python3_FOUND)
  # profiles_ids extended to the W3 tier-1 families 15/06-15/09 (design 10 W3 acceptance 8): red until the profiles track lands them
  add_test(NAME profiles_ids_w3 COMMAND "${Python3_EXECUTABLE}" -I "${CMAKE_SOURCE_DIR}/tools/check_profiles.py" --tier 1 2 3 4 5 6 7 8 9)
  # (the DISABLED property on profiles_ids_w3 is removed: the W3 profiles track landed the 15/06-15/09 hand files)
  # lj_shapes_gen.c is the committed output of gen_vectors.py --shapes (design 9.2); the genmacro track writes the generator
  add_test(NAME gen_vectors_shapes_check COMMAND "${Python3_EXECUTABLE}" -I "${CMAKE_SOURCE_DIR}/tools/gen_vectors.py" --shapes --check)
  # genmacro track: the check is live; the --mutate row flips one sample of the committed file and must fail
  add_test(NAME gen_vectors_shapes_mutated COMMAND "${Python3_EXECUTABLE}" -I "${CMAKE_SOURCE_DIR}/tools/gen_vectors.py" --shapes --check --mutate)
  set_tests_properties(gen_vectors_shapes_mutated PROPERTIES WILL_FAIL TRUE)
endif()

# W3 gs2 track (append-only): the 48 shipped .GS2 + the .Udf sample against tools/ref/16-gs2_ref.py, field by field and
# round-trip byte-exact (developer PC only: prints SKIP and passes when LJ4EVR_REFERENCE_DIR is empty).
if(Python3_FOUND)
  add_test(NAME gs2_check COMMAND "${Python3_EXECUTABLE}" -I "${CMAKE_SOURCE_DIR}/tools/gs2_check.py" --tests "$<TARGET_FILE:lj4evr_tests>" --ref "${LJ4EVR_REFERENCE_DIR}")
endif()
# LORD_C-17: two libFuzzer targets over the importer boundary (clang only; ci.yml linux-clang runs each 60 s, integrator).
option(LJ_FUZZ "Build the libFuzzer targets fuzz_gs2 / fuzz_udf (clang)" OFF)
if(LJ_FUZZ)
  foreach(fz fuzz_gs2 fuzz_udf)
    add_executable(${fz} "${CMAKE_SOURCE_DIR}/tools/fuzz/${fz}.c")
    target_include_directories(${fz} PRIVATE "${CMAKE_SOURCE_DIR}/src")
    target_link_libraries(${fz} PRIVATE lj_fixlib lj_core)
    target_compile_options(${fz} PRIVATE -fsanitize=fuzzer,address,undefined)
    target_link_options(${fz} PRIVATE -fsanitize=fuzzer,address,undefined)
  endforeach()
endif()

# W3 cuelist-window (append-only): TCuelist2Form enable table, greyed CD / DA / MP rows, ListPop order, red load-error line, golden
add_test(NAME test_cuelist_form COMMAND lj4evr_tests test_cuelist_form)

# W3 profiles track (append-only): test_profiles_w3 = the 15/06-15/09 hand profiles by id (footprints, Atomic classes, MAC 550 16-bit pairs,
# RoboColor palette counts, ME-25/27 states, every window built headless)
add_test(NAME test_profiles_w3 COMMAND lj4evr_tests test_profiles_w3)

# W3 desktop-app (append-only): the W3 app seam (cue-list host, reloads, follow spot, W3 actions) and its script verbs headless
target_sources(lj4evr_tests PRIVATE "${CMAKE_SOURCE_DIR}/src/app/lj_app_w3.c")
add_test(NAME test_uiscript_w3 COMMAND lj4evr_tests test_uiscript_w3)

# W3 gdc-fades-followspot (append-only): the Generic DMX Control / Set Fade Status / follow spot windows headless + W3GDC_* goldens
add_test(NAME test_gdc_fades_fs COMMAND lj4evr_tests test_gdc_fades_fs)

# W3 profile-editor (append-only): the three windows headless (tests/ui/test_profile_editor_form.c)
add_test(NAME test_profile_editor_form COMMAND lj4evr_tests test_profile_editor_form)

# W3 movement track (append-only): the five movement windows headless (ME-16, LORD_FIDELITY-12, goldens W3MM_*)
add_test(NAME test_movement_w3 COMMAND lj4evr_tests test_movement_w3)

# W3 genmacro (append-only): the Generic Macro Editor, Save Macro and Select Macro headless (tests/ui/test_genmacro_editor.c, golden W3GM_EDITOR)
add_test(NAME test_genmacro_editor COMMAND lj4evr_tests test_genmacro_editor)
# W4 track 0 (append-only): ME-43 the full align pass reproduces every DFM at its design client (tests/ui/test_align.c)
add_test(NAME test_align COMMAND lj4evr_tests test_align)
# W4 contracts (append-only): W4-CAP the caption diff tool's planted-bad-caption self check (tools/caption_diff.py)
if(Python3_FOUND)
  add_test(NAME caption_diff_selftest COMMAND "${Python3_EXECUTABLE}" -I "${CMAKE_SOURCE_DIR}/tools/caption_diff.py" --selftest)
endif()
# W4 entry (append-only): ME-129 one current-scene getter (tests/ui/test_ctx_scene.c)
add_test(NAME test_ctx_scene COMMAND lj4evr_tests test_ctx_scene)
