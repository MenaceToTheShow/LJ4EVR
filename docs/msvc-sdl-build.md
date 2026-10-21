# Building SDL3 with MSVC 19.44: the two workarounds

The root `CMakeLists.txt` sets, before `FetchContent_MakeAvailable(SDL3)`:

```cmake
set(CMAKE_DISABLE_PRECOMPILE_HEADERS ON)
set(CMAKE_VS_GLOBALS "UseMultiToolTask=true" "EnforceProcessCountAcrossBuilds=true")
```

plus the subsystem trim (`SDL_JOYSTICK HAPTIC CAMERA GPU DIALOG PROCESS HIDAPI
OFFSCREEN RENDER VULKAN SENSOR POWER TRAY OPENGL OPENGLES DIRECTX` off,
`SDL_AUDIO` on for the W5 Audio Analyzer). This page records why, condensed from
`spikes/c-engineering/README.md` and proposal C section 16 (both measured on the
work PC on 2026-10-21: Windows 11 Pro 10.0.26300, VS 2022 Community 17.14,
toolset 14.44.35207, CL 19.44.35213, Windows SDK 10.0.26100, CMake 4.1.0-rc2).

## The bisect

| # | Step | Outcome |
|---|---|---|
| 1 | `cmake -S . -B build -G "Visual Studio 17 2022" -A x64` with SDL3 `release-3.4.18` via FetchContent | configures, SDL fetched as a shallow clone |
| 2 | `cmake --build build --config Release` | **fails**: `MSB6006: "CL.exe" exited with code -1073741819` (access violation) inside `SDL3-static.vcxproj` after "Generating Code..." of a batch; reproducible, different batch on the second run |
| 3 | `--config Debug` | **fails**: `objidl.h(9491,25): fatal error C1001: Internal compiler error` compiling `SDL_windowsmodes.c` through SDL's PCH (`/Yu cmake_pch.h`) |
| 4 | 3-line repro (`windows.h` + `objidl.h` + `shellscalingapi.h`) under `/std:c11`, `c17`, `clatest`, none | all compile: the ICE needs SDL's include context, not the C standard switch |
| 5 | `SDL_windowsmodes.c` alone with the exact Debug command line from the tlog, without `/Yu` | compiles |
| 6 | `-DSDL_PRECOMPILED_HEADERS=OFF` | no effect: SDL3 3.4.18 calls `target_precompile_headers` unconditionally (`CMakeLists.txt` line 4078) |
| 7 | `set(CMAKE_DISABLE_PRECOMPILE_HEADERS ON)` | PCH gone (no `/Yu` in the vcxproj); Release **still fails** with the code-generation access violation on the 20-file batch `SDL_yuv_sw.c ... SDL_blendpoint.c` |
| 8 | each of those 20 files compiled on its own with the exact Release command line from the tlog | **all 20 exit 0**: the crash is specific to MSBuild's single multi-file CL invocation (223 sources in one ClCompile item group, no `/MP`) |
| 9 | Ninja generator with the same `cl` under `vcvars64` | **OK**: Ninja already runs one CL.exe per source, so only the PCH switch mattered there |
| 10 | `CMAKE_VS_GLOBALS "UseMultiToolTask=true" "EnforceProcessCountAcrossBuilds=true"` added, rows 1 and 2 again | **OK**: configure 0, build 0, 0 warnings in our code under `/W4 /WX`; `seqform.exe` 1,835,008 bytes |

`UseMultiToolTask` makes MSBuild run one CL.exe per source file, which is what
the Ninja generator does anyway; `EnforceProcessCountAcrossBuilds` keeps the
parallel count sane when several projects build at once.

## What this does and does not prove

- Both switches are per-machine evidence against this toolset, not an SDL3 bug
  report; the hosted `windows-latest` runner's toolset is checked when CI first
  runs (risk R1 in the design).
- The spike's `shot.ps1` found that `CopyFromScreen` at the window rect returns
  the desktop wallpaper on this four-monitor PC, DPI-aware or not;
  `PrintWindow(PW_RENDERFULLCONTENT)` works. The W0 self-test reads pixels back
  from the renderer's own surface rather than grabbing the screen.
- Judge-engineering re-ran the recipe on this PC and confirmed it as the only
  one that rebuilds here (`[judge-engineering §1.1]`); the engineering judge
  also kept `SDL_AUDIO` on, which the spike had turned off, so the W0 build
  compiles a few more SDL sources than the spike did.

## If it regresses

1. Check `build-msvc/_deps/sdl3-build/SDL3-static.vcxproj` still has no `/Yu`
   and the `UseMultiToolTask` global (CMake writes it into the project).
2. Try the Ninja preset (`cmake --preset dev-ninja-msvc` inside a vcvars64
   shell); if that builds and the VS generator does not, it is the batching again.
3. Bisect the failing batch by compiling each file with the command line from
   `SDL3-static.dir\Release\SDL3-static.tlog\CL.command.1.tlog`.
