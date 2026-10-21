# LJ4EVR

A C11 clone of Martin LightJockey 2 (2.209 feature set, 2.100 help as the
behavioural reference) for Windows and macOS, with sACN E1.31 output. One static
executable per OS; SDL3 supplies the window, input, timers and a surface blit,
every pixel is drawn by LJ4EVR itself, so both OSes produce identical images.

Status: **W0 Foundation** (version 0.0.1). Build skeleton, contracts (headers),
widget tree, layering test. Nothing drives a light yet.

## Build

Windows (MSVC 2022, CMake >= 3.24):

    cmake --preset dev-msvc
    cmake --build --preset dev-msvc
    ctest --preset dev-msvc

Without SDL (what the Linux CI container builds; tests and the headless renderer only):

    cmake -S . -B build-nosdl -DLJ_SDL=OFF
    cmake --build build-nosdl
    ctest --test-dir build-nosdl

Presets: `dev-msvc`, `dev-ninja-msvc`, `dev-mac`, `ci-linux-asan`, `ci-linux-clang`,
`release-win`, `release-mac` (`CMakePresets.json`). Build directories are `build-*/`
under the repo and are ignored by git.

## What v1.0.0 is not

v1.0.0 tags at the end of W4 and ships without the Audio Analyzer and audio trig
(inventory §5 "Keep, built in"), DMX-in over sACN input and the `.vtr` tables
(inventory §5 "Keep over sACN/Art-Net input"), the RS-232/TCP remote protocol,
External Application string output, the `.LJB` importer, the LJ MediaPlayer
(inventory §5 "Keep (cross-platform player)"; section 8.5 states what its
cue-list rows and transport buttons do until then) and Art-Net `[D14]`. All
seven are W5 (section 10). Every menu item, cue-list command and toolbar button
that belongs to one of them is still present, greyed, with the hint
`Not available in LJ4EVR`, so the chrome matches and the gap is visible. Joey
may pull any of them forward before W3; the default is the list above.

## Docs

- `docs/conventions.md` - the coding conventions every file under `src/` follows
- `docs/w0-contracts.md` - map of the W0 headers and the three rules for tracks
- `docs/msvc-sdl-build.md` - why the CMake file carries two MSVC workarounds

Licence: MIT (`LICENSE`). Third-party notices: `NOTICE`. LightJockey is a
trademark of Martin Professional; LJ4EVR is not affiliated.
