# Changelog

Format: Keep a Changelog; versions are semver tags `vX.Y.Z` and `VERSION` is the
single source of the number.

## [Unreleased]

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
