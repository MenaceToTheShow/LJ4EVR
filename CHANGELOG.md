# Changelog

Format: Keep a Changelog; versions are semver tags `vX.Y.Z` and `VERSION` is the
single source of the number.

## [Unreleased]

## [0.1.0] - 2026-10-21 - W1 First light

The program runs a show: fixtures configured, a sequence programmed, a cue
running, DMX on the wire over sACN (design section 10 W1, acceptance 1-10
walked through in `docs/w1-integration.md`).

- Engine (`src/engine/`): the design 3.x pipeline in C11 integers, command
  array + `lj_lock`, runners for the current sequence, 12 cue slots, statics
  and the background cue; snap / fade per channel with 16-bit pairs as one
  value; post stages S1-S9 (masters, blackout, patch, pan/tilt, overwrite,
  freeze / offline); GM Fade; movement and generic macros; 16 replay
  scenarios under `tests/engine/replay/` (`replay_all`), `test_engine_perf`,
  the `determinism` frame dump (sha 5e61478e...).
- Fixture library (`src/fixlib/`): the 201 exe profile rows, tier-1 hand
  profiles (MAC 500 / 918, the generics), tier-2 QLC+ profiles through
  `tools/qxf2c.py` and `qxf_map.tsv`; `lj_layout_derive` builds every
  control-window table from the profile.
- Model + I/O (`src/model/`, `src/io/`): the user library on disk as text
  records (`Config.dat`, sequences, cues, cue lists, `DefaultOut.dat`,
  `forms.ini` / `Main.Ini` positions, hotkey setups), `.lj4b` backup /
  restore; the sACN sender (E1.31-2018, 40 packets/s cap, keep-alive,
  discovery, Stream_Terminated), `Hardware.Ini [SACN]`.
- Windows (`src/windows/`, `src/controls/`, `src/desktop/`): main window with
  the three toolbars, status bar, fixture desktop and Unassigned list;
  Sequence control, Cue control, Save Sequence / Save Cue, List of Cues,
  Master Intensity, Fixture Configuration, Hardware Setup, Startup Options,
  DMX Output Monitor, the warning boxes; the fixture controls Intensity,
  Movement, Colors, Gobos, Beam, Effects, Levels, Lamp and Extended, code-built
  from the profile tables with O/S/F buttons, drag bars, scroll controls and
  palette tiles.
- App (`src/app/`): the UI script driver (`--script`, `docs/ui-script.md`),
  15 acceptance screenshots under `docs/screenshots/w1/`, the startup status
  window, `--selftest` extended with the DMX monitor binding check.
- Tests: 55 CTest rows on three OSes (`ui_golden` 10 forms, `ui_golden_ctl`
  9 control windows, `test_desktop` MAINWINDOW), CI runs `ctest` on the MSVC
  Debug tree too.

### Fix pass r1 (W1 code council, ME-67..ME-75)

- ME-67: the seven code-built control windows were built to CHM captures that
  are 0.8 reductions; every constant is now x1.25 to the 1:1 anchors (O/S/F
  48x24, Intensity 600x122, Beam 257x96, Lamp 166x125, Extended boxes 346x101,
  scroll control 165x41); goldens regenerated.
- ME-69: the engine thread copies the sACN config under the lock.
- ME-70: `test_controls_a` writes `uint8_t` state (a fresh MSVC Debug `ctest`
  is 55/55); `windows-msvc` runs the Debug `ctest`.
- ME-71: the GM Fade runner keeps running while frozen and is not shifted at
  unfreeze (note 11 S9; replay `gm_fade_under_freeze`).
- ME-72: the fixture toolbar's x table measured from `toolandstatusbars.gif`
  (1:1): Lamp at 196 after the 17-px gap, Controls 248, Groups 300, All 330.
- ME-74: `OnMouseWheelDown` / `OnMouseWheelUp` are dispatched; no `unknown
  event` line at the DMX monitor open (`--selftest` asserts it).
- ME-75: the cue grid prints `1:00 | 0:05` (one `lj_fmt_time`) and `Fwd` /
  `Auto`.
- ME-68: `shot` skips a window AutoClose has already closed; every W1 screenshot
  re-taken.

### Known gaps (W1 code council; W2 entry gates unless noted)

- ME-43 full align, ME-44 header pairing, ME-46 dfm2c event-name check,
  ME-47 `Items.Data` from the binary DFMs, ME-51 skip ceiling, ME-53
  widthcorpus, ME-54 shared scrollbar helpers, ME-57 five sans13 glyphs.
- ME-45 non-admin install screenshot (needs the `v0.1.0` installer).
- ME-76 `exe_profiles_check --check` runs nowhere; ME-77 loaders cast before
  they validate; ME-78 `replay-frames.bin` is compared nowhere (the
  `determinism` job compares `dump-frames` only).
- ME-50 design.md sentences (design owner); ME-21 the Mac run (Joey).
- LOW: ME-79 O/S/F focus ring, ME-80 pinned-static unfreeze, ME-81
  `test_lock_hold`, ME-82 clock / log under the lock, ME-83 negative macro
  index, ME-84 UTF-8 name cut, ME-85 dead `LJ_NOOP` table, ME-86 orphan API,
  ME-87 status-bar and monitor PROPOSALs; hotkey dispatch, `lj_ctl_set_confirm`
  and the memory gauge from `docs/w1-integration.md` section 7.

## [0.0.1] - 2026-10-21 - W0 Foundation

The build skeleton: every one of the 208 LJ2 forms renders from its DFM facts
through the in-repo fonts, headless and on screen, byte-identical on three OSes.
Nothing drives a light yet.

- Font pipeline: five clean-room bitmap faces (`art/fonts/`, `fontpack`),
  advance census from the 208 DFMs (`tools/widthcorpus.py`), `font_corpus`
  0 mismatches on 792 of 826 AutoSize-label rows (the 34 rows on nine
  unpacked faces are skipped, ceiling 34).
- `tools/dfm2c.py`: DFM text -> `src/ui/forms/lj_forms_gen.c` (3632 control
  rows, 1081 menu items, 59 widget classes, 0 unknown) with `--check` for CI.
- Widgets (`src/ui/widgets/`), classic frame, menu bar, scrollbars; the
  headless renderer `lj4evr_render` writes all 208 forms plus the main window;
  six reviewed goldens under `tests/ui/golden/` with `REVIEWED.tsv`, gate
  `ui_golden` byte-exact on Windows, macOS and Linux.
- The app `lj4evr`: `Main.Ini` (`[CHROME] Zoom`, `Native`), main window with
  the generated `TLJMainForm` menu, TSEQUENCEFORM + TCURRENTCUE, `forms.ini`
  positions, engine stub thread; `--selftest`, `--save`, `--version`.
- Engine stub frame dump, `determinism` job: 1641600 bytes identical on the
  three OSes.
- Static CRT on Windows (no `VCRUNTIME140.dll` dependency).

- CMake project: C11, one static library per `src/` layer, presets for
  Windows (MSVC, Ninja+cl), macOS, Linux CI and the two release builds;
  SDL3 `release-3.4.18` by FetchContent with the MSVC 19.44 workarounds
  (`docs/msvc-sdl-build.md`).
- W0 contract headers for core, gfx, ui, forms, engine and platform
  (`docs/w0-contracts.md`); `ui/lj_widget.c` with its test.
- `tools/test_layering.py`: include graph, naming, OS-header, float/atomics
  checks and each header compiling alone, under CTest.
- `VERSION` -> `version.h`, CPack version, bundle version.
- Packaging: per-user NSIS template (no admin), macOS `Info.plist`,
  Windows UTF-8 manifest, our own icon set (`packaging/icons/`).
- CI: `docker/Dockerfile` (debian:trixie) and `docker.yml`; `ci.yml` with
  generate-check, linux-gcc-asan, linux-clang, windows-msvc, macos-clang and
  determinism; `release.yml` tag-driven Windows + macOS installers with
  `SHA256SUMS`.
- Docs: building, ci, releasing, conventions, contributing, `w0-integration`.

### What v1.0.0 is not

v1.0.0 tags at the end of W4 and ships without the Audio Analyzer and audio
trig, DMX-in over sACN input and the `.vtr` tables, the RS-232/TCP remote
protocol, External Application string output, the `.LJB` importer, the LJ
MediaPlayer and Art-Net. All seven are W5. Every menu item, cue-list command
and toolbar button that belongs to one of them stays present, greyed, with
the hint `Not available in LJ4EVR`. (design.md section 1, mirrored in the
README.)

### Known gaps (W0 code council, ME-43..ME-66; W1 entry gates unless noted)

- Align pass: DFM rects stand at creation; no per-class `AdjustClientRect`
  insets or alTop/alLeft ordering when the client resizes (ME-43).
- One `.h` per `.c` not enforced; 18 widget files share group headers (ME-44).
- No About form; version is `lj4evr --version` only; NSIS has no
  `VIProductVersion`; the non-admin install is checked once on the tag,
  the screenshot is W1 (ME-45).
- Unknown DFM event names only warn; `Items.Data` (8 list/tree views) is
  not decoded from the binary DFMs; `glyphmap.tsv` matches nothing yet
  (ME-46..ME-48).
- `dfm2c --check` relies on an uninstalled pre-commit hook; `--selftest`
  is not in CI (ME-49).
- 34 corpus rows on nine unpacked faces; five sans13 CP1252 glyphs are
  boxes; the active caption gradient end is unverified (ME-51, ME-57, ME-58).
- `setlocale(LC_ALL, "C")` makes `lj_locale_decimal()` always `'.'` (ME-52).
- Two DFM parsers (`dfm2c.py`, `widthcorpus.py`); scrollbar fitting is
  duplicated five times (ME-53, ME-54).
- Check-box text and group-box caption 1-2 px off one Windows 2000 capture,
  no Win98-era reference (ME-55).
- LOW: signed right shift in the engine stub, `lj_render_main` path bound,
  `lj_miniz` `/w` and no consumer, F20..F24 shortcut text, pref path CP1252
  conversion, three find-form loops (ME-60..ME-66).
- macOS frame, Spaces and Mission Control behaviour unchecked on a real Mac
  (ME-21, open for Joey).
