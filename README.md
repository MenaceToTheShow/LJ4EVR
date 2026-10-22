# LJ4EVR

A C11 clone of Martin LightJockey 2 (2.209 feature set, 2.100 help as the
behavioural reference) for Windows and macOS, with sACN E1.31 output. One static
executable per OS; SDL3 supplies the window, input, timers and a surface blit,
every pixel is drawn by LJ4EVR itself, so both OSes produce identical images.

Status: **W3 Show control** (version 0.3.0). Cue lists with their runner (PC
time, MTC, manual lines, links, hotkey commands), the Generic Macro Editor with
the 66 shapes, movement macros with Bezier shapes, Fan Out and Auto Delay, Cue
Macro Control, Generic DMX Control, follow spots, sequence fades, `.GS2` / `.Udf`
import and export, the Generic Fixture Profile editor, and the 15/06-15/09 hand
profiles; W2 playback underneath (`CHANGELOG.md`, `docs/w3-integration.md`).
Pre-release until v1.0.0.

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

- `docs/building.md` - configure, build and test on Windows, macOS and Linux
- `docs/ci.md` - the three workflows, the runner security model, the CI image
- `docs/conventions.md` - the coding conventions every file under `src/` follows
- `docs/fidelity.md` - every known deviation from LightJockey 2.209 and the decision behind it
- `docs/file-formats.md` - the user library on disk (text records, INI files, `.lj4b`)
- `docs/fixtures.md` - the fixture library: exe rows, hand profiles, QLC+ tier 2, `qxf_map.tsv`
- `docs/fonts.md` - the clean-room bitmap faces and the width corpus
- `docs/generated-files.md` - every committed generated file, its tool and its check
- `docs/hotkeys.md` - the hotkey function table and the `.Hot` setups
- `docs/midi.md` - MIDI-IN: the backends, the map, Learn and the UI-thread dispatch
- `docs/msvc-sdl-build.md` - why the CMake file carries two MSVC workarounds
- `docs/releasing.md` - tagging, `release.yml`, the installers
- `docs/replay-grammar.md` - the `.lj4r` engine replay scenarios
- `docs/running.md` - running the app, `Main.Ini`, `--selftest` and the other flags
- `docs/sacn.md` - the sACN sender, `Hardware.Ini [SACN]`, the LAN capture
- `docs/screenshots/` - the W0, W1, W2 and W3 acceptance captures
- `docs/ui-script.md` - the `--script` driver behind the acceptance walkthrough
- `docs/w0-contracts.md` - map of the W0 headers and the three rules for tracks
- `docs/w0-integration.md` - the W0 integration log
- `docs/w1-contracts.md` - the W1 seams between tracks
- `docs/w1-gates.md` - the W1 gate table with the council's status column
- `docs/w1-integration.md` - the W1 integration log, CI rounds, acceptance walkthrough and fix pass
- `docs/w2-contracts.md` - the W2 seams between the eleven tracks
- `docs/w2-gates.md` - the W2 gate table with the council's status column
- `docs/w2-integration.md` - the W2 integration log, CI rounds, acceptance walkthrough and fix pass
- `docs/w3-contracts.md` - the W3 seams between the ten tracks
- `docs/w3-gates.md` - the W3 gate table with the council's status column
- `docs/w3-integration.md` - the W3 integration log, CI rounds, acceptance walkthrough and fix pass

Licence: MIT (`LICENSE`). Third-party notices: `NOTICE`. LightJockey is a
trademark of Martin Professional; LJ4EVR is not affiliated.
