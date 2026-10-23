# W4 integration log

## Integrator 1: build and tests

Session of 2026-10-22, on the work PC: MSVC 19.44, CMake 4.1, VS 17 2022 x64, Ninja. Nine tracks wrote disjoint
parts against `docs/w4-contracts.md`. This pass builds the landed tree in every configuration, resolves the blockers
the tracks returned (or records why one stays open), brings `REVIEWED.tsv` to all 208 forms, and gets ctest and CI
green. Where a track and design.md disagree, design.md wins. Each fix is the smallest change that works. Commands and
their output are recorded as run.

### 1. Pre-integration commit

    git status                -> 656 paths from the nine tracks; reference/ and build-*/ are ignored
    staged-file check         -> no path under reference/; no .GS2 / .Udf / .Cl2 / .Mac / .rtf / .chm / .ico staged
    hash check                -> sha1 of all 639 new or changed files against all 18,923 files under reference/: 0 equal
    new binaries              -> docs/screenshots/w4/{about,help-viewer-index,help-viewer-files,statics2-popup}.png
                                 (captures of LJ4EVR's own windows; the provenance.md row "Screenshots") and the track
                                 goldens (our renderer; the provenance.md row "Golden images"). Everything else is
                                 text: art/**/*.txt, our CC0 pixel art (the provenance.md row "Glyphs, icons ...")
    git commit "W4: tracks landed (pre-integration)" -> 1142e62, pushed (ee2e04a..1142e62, which also carries 97fe7b4
                                 track 0 and 4667c80 contracts, the two local commits of the contracts seat)

### 2. Builds

    cmake --build build-w4         (Ninja + cl, Release, LJ_SDL=ON, LJ4EVR_REFERENCE_DIR)    0 warnings, 0 errors
    cmake --build build-w4-nosdl   (Ninja + cl, Release, LJ_SDL=OFF)                         0 warnings, 0 errors
    cmake --preset debug-win && cmake --build build-debug-win   (Debug, LJ_SDL=ON)           0 warnings, 0 errors
    cmake --preset dev-msvc && cmake --build build-msvc --config Release / --config Debug    0 warnings, 0 errors

The landed tree compiled in every tree on the first build: the half-edited files the tracks reported against each
other (`lj_actions.h:316`, `lj_library.c:431`, `lj_app_w3.c:59`, `lj_sdl_srcimg.c:37`, the art table externs, the
`lj_show_dlg_*` declarations) had all been finished by the time the tree was committed. The first ctest run was 108 of
117. The failures were two stale assertions and 21 goldens rendered before the art landed (rows 1, 2 and 13 below).

### 3. Resolutions

| # | Blocker (track) | Resolution |
|---|---|---|
| 1 | `test_cuelist_form.c:281` asserts the old ME-120 greyed state (dlg-show) | the line now expects the three items alive and Import greyed (`ME-120 (W4)`) |
| 2 | `test_uiscript.c:173` expects an unknown `U:<folder>` to fall back to Default (dlg-files, ME-130) | expects `NoSuchLib` (created by the `lj_library_create` step), then deletes it |
| 3 | Save Anyway writes a Config.dat with F41 warning states that the next start throws away (dlg-config) | `model/lj_show.c`: `config_check(show, warn_ok)`. `lj_config_validate` stays strict (replay, tests). `lj_show_load` refuses only hard errors and logs the three warning states (no DMX address, past channel 512, a shared channel) with `(F41 warning, saved with Save Anyway; kept)`. A fixture without an address is unpatched: `lj_show_fixture_base` returns -1 for it, and `lj_engine_rebuild` skips it. Before this, an address-0 fixture from Add Fixture(s) indexed `fixture_of[-1]` on link 1 in the engine rebuild. `test_model_records` gains the row: address 0 and a shared channel both load and keep both fixtures; link 5 is still refused. |
| 4 | Patch Sub Masters entry point (dlg-config) | `lj_master_intensity.c`: `InternalPatch1Click` opens `LJ_WIN_INTENSITY_PATCH` (it logged `W2`) |
| 5 | RGB Palette entry point; the unit captions (dlg-config) | `lj_ctl_common.c` `menu_click`: the RGB pack window's `Palette` bar item opens `LJ_WIN_RGB_PALETTE`. `lj_ctl_rgbpack.c`: a unit panel shows the master fixture's `ch_caption[3 * u]` when it is set (RGBPack configuration), else `Unit n` |
| 6 | Movement Options > Position Options (dlg-show) | `lj_ctl_movement.c`: `M_PosOptions` enabled; `on_menu` opens `LJ_WIN_MOVEMENT_OPTIONS` |
| 7 | `LJ_WIN_SAVE_PRESET` and `LJ_WIN_DEFAULT_OUTPUT` to be deleted (dlg-show, dlg-desktop; W4-WIN) | both ids are gone from `lj_windows.h` and from the class, name, family and dialog tables in `lj_windows.c`, and from both families and their tests. `test_desktop_dialogs` asserts that no id is named `default_output` or `save_preset`. `docs/ui-script.md` lists the names without them. Fidelity rows: TSavePresetForm unused by design (Position Presets save into the highlighted or first free number); Default DMX Output is two menu leaves that act at once (`lj_default_output.h`) |
| 8 | Startup Options Select Startup Cuelist pick-back (dlg-desktop, dlg-show) | `lj_prefs.c`: `ClSelectClick` calls `lj_show_dlg_select_cuelist(cl_picked)`, which hands the number to `lj_startup_options_set_cl` |
| 9 | Global Patch sequential patch (dlg-show) | `lj_global_patch.c`: P_OutSeq (`Set DMX Out Sequence..`) opens TSetSequenceForm through `lj_show_dlg_set_sequence(1, ...)`, and OK sets the selected channel's DMX out to `Start Sequence at :`. Ponytail: the list is single-select, so one channel at a time. P_InSeq and the DMX-in enable switch stay greyed (I7) |
| 10 | Delete Sequences > Check Used (dlg-show) | `lj_delete_files.c` `check_used`: `lj_show_dlg_seq_used` per cue and BG cue, `lj_win_close(LJ_WIN_SEQ_USED)` at the end |
| 11 | Save Sequence `Add to Cue in Slot` (dlg-show) | `lj_save_dialogs.c`: `lj_cue_form_add_sequence_at(number, slot)`, where slot 0 = Default = first free. The cue form raises both [12 Q9] cue-slot prompts, so the old local message is gone |
| 12 | Fan Out saved custom selections (dlg-desktop) | `lj_movemacro_editor.c`: `sbSave` (no DFM handler) opens `lj_desktop_dlg_save_selection(order, n, cb1 entry)`. `cb1` lists Main.Ini `[CUSTOMSEL]` and is rebuilt when the count changes. `cb1Change` loads the saved order as the custom order. The fidelity row "Fan Out, saved custom selections" can now be marked superseded |
| 13 | Startup Status countdown never shown; no selftest line (dlg-files, W4-WIN) | `lj_app.c`: `lj_win_open(LJ_WIN_PLEASE_WAIT)` right after the load-time status window closes. `--selftest` gains `selftest Startup Status: "This window will selfdestruct in 10 seconds (click anywhere in list to keep open)": OK`, read from the window's `Re1` memo |
| 14 | `Place all Fixtures on this tab ?` not raised (dlg-config, W4-Q9) | `desktop/lj_unassigned.c`: Auto Place All asks it (exe 0x38A0EC), and Unassign All now asks `Unassign All Fixtures ?` (exe 0x389D2C), which W1 had skipped (PROPOSAL) |
| 15 | w1/w2/w3 acceptance scripts click `bAdd` and expect an immediate add (dlg-config) | each `click TLinkConfigForm3.bAdd` is followed by `click TAddFixForm2.bOk`, and each `bSave` by `click confirm.BOk` for `Clear data for new fixtures ?`. These are app-run scripts and not CTest rows; they were not re-run in this pass |
| 16 | The icon-popup entries for Scanner 1 / Colour Scroller / RGB pack configuration (hw-dmxin's question) | already wired: the popup's `Define Fixture Profile` item (`lj_profile_editor_form.c:850-852`) opens all three. Nothing to add |
| 17 | `lj_midi.h` append (da2532-fingers) | ratified as is: an append-only block in the header of the `.c` that needs it |
| 18 | D11 vs the brief: TStaticsControlForm / TSaveStaticForm / TStaticsReplaceForm (dlg-show) | design wins: D11 kept. View > Statics Control opens Statics 2. `save_static` and `statics_replace` open nothing and log a fidelity line. The captions script leaves them out |
| 19 | 21 goldens differ once the art landed (dlg-config, dlg-show, dlg-files, hw-dmxin, da2532-fingers, lightcorder-debug) | re-rendered with `LJ_GOLDEN_UPDATE=1`. Each one was diffed against its old golden (old / new / mask, viewed); every change is the landed button glyphs or tile lists. One REVIEWED row each with the pixel count and the bbox (section 5) |

### 4. Checks (as run, all four Windows trees unless named)

    ctest build-w4 (Release, SDL)          100% tests passed, 0 tests failed out of 117
    ctest build-w4-nosdl (Release)         100% tests passed, 0 tests failed out of 116
    ctest build-debug-win (Debug, SDL)     100% tests passed, 0 tests failed out of 117
    ctest build-msvc -C Release / Debug    100% tests passed, 0 tests failed out of 117 (each)
    test_layering                          428 file(s) checked, 205 header(s) compiled alone, noop handlers 0, ini ternaries 0,
                                           scene getters 0, set-text/lib/ctx copies 0, 0 finding(s)
    font_corpus                            826 rows, 34 skipped (face not packed), 0 mismatches
    gen_vectors --check                    17-position_ref.py: all checks passed; lj_trig_gen.c and tests/vectors.h up to date
    replay_all                             36 scenario(s), 0 failed
    profiles_ids, profiles_ids_w3          passed
    art_census                             210 LJ2 Glyphs names (210), 58 icons (58), 108 gobo tiles, 133 gel rows;
                                           204 profile rows -> icons, 439 named palette entries, 0 unresolved
    help_check                             16 texts against 200060 reference 8-word runs, 15 topics, passed
    determinism                            dump-frames twice (Release) and once (Debug): cmp equal, 1,641,600 bytes,
                                           sha256 5b13f41930362b75a7aa6f9c53f77db2d315a4039ea5a18da664ea1263114b50
                                           (= W3: no W4 change reaches the engine's frames)
    replay-frames.bin (replay_dump)        129,664,080 bytes, sha256 b14d4b292708b88227b5fedd319537b05944c6a9947755475ac935b86468d05c
                                           in build-w4, build-w4-nosdl and build-debug-win (= W3)
    build-debug-win lj4evr --selftest      rc 0, six OK lines (startup, TDMXOUTFORM, MIDI-IN, Startup Status, cue-list runner,
                                           TSEQUENCEFORM); 0 "no binding" / "unknown event" / "has no behaviour file" lines
    ownership greps (ME-2, ME-23)          lj_engine_ / lj_sacn_ / lj_table_ in src/io/lj_midi*.c: one comment only;
                                           recvfrom / recv( / bind( / IP_ADD_MEMBERSHIP / WSARecv / listen( in src: none (W4-DMX)

`%APPDATA%\LJ4EVR` was copied aside before every app run and put back afterwards.

### 5. Goldens (W4-GOLD, acceptance 3)

`ui_golden` (`tests/ui/test_golden.c`) now walks `lj_forms_all()`. It renders every one of the 208 generated forms
through lj4evr_render's path (DFM only, no behaviour file) against `tests/ui/golden/<FORM>.bmp`, and fails a form that
has no `REVIEWED.tsv` row. `LJ_GOLDEN_UPDATE=1` rewrites them. `ui_golden --census` prints, per form, the surface,
the DFM size, the client size, the border style, whether there is a menu, and the ME-17 stub widgets.

    ui_golden            208 of 208 form(s) have a REVIEWED.tsv row; 208 form(s), 0 failure(s)
    goldens              359 BMPs (161 before this pass + 198 new DFM-only goldens); every BMP has a row; 427 rows
    the 10 W0-W3 class goldens   byte-unchanged by the new walk

Numbers checked for every form, computed from the census:
- 98 forms store Width x Height: the surface equals it in all 98.
- 110 forms store ClientWidth x ClientHeight. The surface minus the client is constant per border style: sizeable
  8 x 27 (24 forms), with a menu 8 x 46 (52), sizeable tool window 8 x 23 (10) or 8 x 42 with a menu (12), bsSingle
  and bsDialog 6 x 25 (24) or 6 x 44 with a menu (39), tool window 6 x 21 (33) or 6 x 40 with a menu (12), bsNone
  0 x 0 (2).
- Stub widgets (ME-17): 0 in 206 forms. TExportTableForm has 2 (TDriveComboBox, TDirectoryListBox); it opens only
  by id, because its `.vtr` export is W5. TLibraryRestoreForm has 3, which the wired Restore window covers with
  code-built lists (dlg-files, W4-DLG-1).

Each new golden was viewed at 1:1 in one of 19 contact sheets: title, frame, menu bar, controls in DFM order, and the
DFM captions. None of note 07 s14's 1:1 GIFs is a DFM form. The runtime state of each wired window is reviewed in that
window's own golden row (the track rows), so the new rows say this.

Seen while reviewing, and left as they are:
- TAboutForm2's DFM-only render shows the DFM memo text (Martin's credits and forum URL). It is caption data that
  has been in `lj_forms_gen.c` since W0 (D2). The runtime About replaces all of it (captions exclusion row).
- Some speed buttons are blank in the DFM-only renders: the Cue Builder's sbAuto / sbManual / sbMusic, and Fan Out's
  sbSave / sbNew / sbDelete. Their DFM objects have no `Glyph.Data` (checked), so LJ2 assigns those glyphs at
  runtime. The art census reports 0 unresolved names. Whether to draw runtime glyphs for them is the art owner's call.

### 6. Captions (W4-CAP, acceptance 1)

`tests/ui/scripts/w4_captions.lj4s` now also opens every W4 family id that opens from the script without a context
(88 more). Thirteen need a selected fixture, a running record or a prompt answer, so they are not opened by the script
and are covered by their track's headless test instead: auto_address, cleanup, clone, color_scroller_defs, copy_from,
da2532_text, fingers (behind its activation prompt), fixture_info, fixture_notes, fixture_special, rgbpack_defs,
save_lightcorder, scanner1_defs. `tools/caption_diff_excl.tsv` (new, 113 rows) names each runtime difference with
its reason:
- F19 window captions.
- F55 runtime fills of DFM placeholders (exe format strings or live values).
- ME-11 hints.
- D2 / ME-37, the About text.
- Five research-table parse errors: the DFM's unescaped `Don't`, which `13-dfm-captions.tsv` cuts to `Don`.
- One research-table encoding error: `Pan 360` + 0xBA is stored as UTF-8 in the TSV.

    lj4evr --script w4_captions.lj4s U:W4Captions (Release; scratch library, deleted after)   rc 0, 0 ME-16 lines
    caption_diff.py --dump caption_dump.tsv --check   145 implemented form(s) of 207, 0 difference(s),
                                                      139 excluded by a fidelity row, 84 absent at runtime

Absent rows (84) are counted, not gated: TTabSheet captions that `dump_captions` does not cover, and hints behind
ShowHint = False.

### 7. Census (ME-131)

Of the 208 forms, 185 are referenced outside `src/ui/forms/`. The 23 that are not need a `docs/fidelity.md` row
each (fidelity-docs):
- The 19 that never open, named in the contracts.
- TSavePresetForm, whose id is deleted.
- TSaveStaticForm, TStaticsControlForm and TStaticsReplaceForm (D11).

Debug `--selftest` and the captions run print no `has no behaviour file yet`.

### 8. Left open by this pass (not build or test blockers)

- ME-128 (rank-6 16-bit delta, ENTRY). Not done here. It changes `replay-frames.bin`, which then has to be
  re-recorded once.
- LORD_ENGINE-8 / -9, LORD_C-7 / -9 / -10, W2 LORD_C-4 / -8, LORD_FIXTURES-8, LORD_C-8 Execute File, LORD_FIDELITY-19,
  W2 integration s7 / s9, ME-66, the caret row, LORD_C-14 / WIZARD_STYLES-11, WIZARD_STYLES-12: integrator LOW and
  RELEASE rows. Not touched.
- `docs/fidelity.md` rows from every track (their return texts), the F41 Config.dat note (row 3 makes Save Anyway
  safe), the census rows above, and the superseded rows (Fan Out custom selections, P_Clone / P_Clear, BG Edit
  Sequence, the ME-120 "ponytail, W4" rows): fidelity-docs.
- The RGB pack control's colour squares, Calculate addresses / Clear for one fixture at a time, and the statics' scenes
  under Copy / Clear Misc Data: ponytail deferrals that the dlg-config track recorded.
- `TExportTableForm` still paints two ME-17 stubs. It has no entry point (W5).

### 9. CI portability pass (gcc 14 / clang 19 on Linux, clang on macOS)

The first CI run on `893123d` was green on windows-msvc. The four other jobs stopped on compile errors that MSVC does
not report. Track 0's ME-43 `client` member on `lj_widget_vt` was never pushed before this wave, so CI first saw it
here. Locally, `llvm-mingw` clang 23 and `w64devkit` gcc 16 (scratch tools, not in the repo) ran with the CI flags
(`-std=c11 -Wall -Wextra -Wpedantic -Wshadow -Wvla -ffp-contract=off -Werror`) over every `src`, `tests` and
`tools/fuzz` file, and `test_layering.py --cc gcc` ran on the headers.

| Error | Fix |
|---|---|
| `missing field 'client' initializer` (58 positional `lj_widget_vt` initializers, 33 files) | `, NULL` appended to each |
| `lj_art_gen.c`: `NULL` undeclared (macOS), cascading into `missing initializer for field 'rgb' of 'lj_gel'` (gcc) | `tools/artpack.py` emits `#include <stddef.h>`; regenerated |
| `lj_help_topics_gen.h:1`: `"/*" within comment` (gcc header check) | `tools/help_check.py` comment text reworded; regenerated. The same text in `lj_help_viewer.c:3` reworded |
| `lj_backup_restore.c:219` unsequenced `end[n++] = n == 0 ? ...` (clang) | split into the assignment and `n++` |
| `lj_fingers.c:680` positional `lj_ctl_desc` missing `tag` (clang) | `.tag = 0` |
| `lj_fingers.c:349`, `test_desktop_dialogs.c:110` misleading indentation (gcc) | the trailing statement moved to its own line |
| format truncation (gcc 16; gcc 14 may report it too) in `lj_config_dialogs.c:298`, `lj_da2532.c:428`, `lj_debug_form.c:210`, `lj_desktop_dialogs.c:243/342/369`, `lj_show_dialogs.c:335`, `lj_statics2_form.c:414` | the buffers widened to fit the largest value |

The local sweeps now show nothing, except two items that are not CI errors and were left alone:
- `test_window.c:22` `-Wunused-but-set-global`. This is new in clang 23 and the file is unchanged since W0.
- `lj_sacn.c:221/251` `format-truncation`. gcc 16 reports it, but the file is unchanged since W3, when gcc 14 passed it.

After the fixes, `test_layering --cc gcc`: 0 finding(s). The generator checks are current: qxf2c, fontpack, artpack,
gen_vectors, exe_profiles_check, help_check, dfm2c. The four Windows trees are still 0 warnings, and ctest is still
117 / 116 / 117 of the same.

Second CI run (`5c4efd7`): windows-msvc and generate-check were green. Three failures remained, fixed here:
- gcc 14 `format-truncation` at `lj_config_dialogs.c:953` (`rg_save`, `v[80]` -> `v[128]`).
- `W4C_FILE_EXISTS` differed on Linux and macOS: the memo showed the library path, whose separators differ per OS.
  It now shows the file name only (PROPOSAL rig). The golden was re-rendered (797 px, the memo line) with a REVIEWED row.
- `test_restore_data:214` on Linux: `lj_library_dir_list` cut at `max` before sorting, so readdir order chose the
  entries. It now keeps the first `max` names in sorted order as it reads them.

`test_lock_hold` (`lock hold 2359 us >= 2000 us` on linux-clang) is the W1 timing row on a loaded shared runner.
Nothing in W4 touches `core/lj_lock`, so it is left as is.

Third CI run (`d734b71`): green on windows-msvc, macos-clang, linux-clang and generate-check. linux-gcc-asan failed
with gcc 14 `format-truncation` in `lj_config_dialogs.c` `rg_save` (`"Entry%d"` into `key[16]`). Both RGBPalette.Ini
key buffers are now `key[24]`. The mingw builds of gcc 14.2 and 16.2 do not reproduce this class: mingw's `snprintf`
is an inline wrapper, not the builtin. A blanket widening of 41 integer `snprintf` buffers was tried in `13ee343`, and
CI showed that it moved gcc's range analysis onto the `%s` concatenations that read those buffers
(`lj_fixture_config.c:166`, `lj_profile_editor_form.c:254`). It was reverted, so each remaining gcc 14 report is fixed
where CI names it.
The local gcc check now matches CI: w64devkit gcc 14.2 with `-fsanitize=address,undefined` and a forced
`#define snprintf __builtin_snprintf`. It reproduces both of CI's reports on the old files, and the sweep of every
SDL-free `.c` file is clean.

### 10. CI record

    https://github.com/UnbannableGT/LJ4EVR/actions/runs/39887131384 (7d0b9c4): all six jobs green: windows-msvc,
    linux-clang (with the 60 s fuzz_gs2 / fuzz_udf step), linux-gcc-asan, macos-clang, generate-check, determinism (the three
    frame dumps byte-equal)

## Acceptance (W4 integrator 2, acceptance walkthrough)

Session of 2026-10-22 on the work PC (MSVC 19.44, four monitors: DISPLAY1 0,0 2560x1440 primary, DISPLAY2 0,1440
2560x1440, two 1080x1920 portrait screens). Design 10 W4 acceptance, item by item, plus the brief's five checks. Every app
run used `build-w4\lj4evr.exe` (Release, Ninja + cl) in scratch libraries under `%APPDATA%\LJ4EVR` (backed up first and put
back afterwards). Where the walkthrough found a window that did not open from its entry point, the app was fixed, never
the script (section "Fixes" below).

| # | Item | Result |
|---|---|---|
| 1 | Prompt and caption diff against `13-dfm-captions.tsv` = 0 for every implemented form | tick: 154 implemented forms, 0 differences |
| 2 | Every window of inventory section 2 opens from its documented entry point | tick: 155 PASS lines + 5 (update dialogs); the exceptions are named below |
| 3 | All 208 goldens reviewed | tick: 208 of 208 forms, 359 BMPs, 0 without a row |
| 4 | `docs/fidelity.md` complete | tick: F1-F60 all present, each a pair or "deferred: needs rig" |
| 5 | Restart persistence for every W4 window | tick: 21 of 21 |
| 6 | Help viewer opens topics | tick |
| 7 | LightCorder writes an `.LLF` of our frames and plays it back | tick |
| 8 | DMX-in windows open with input greyed | tick |
| 9 | Debug window counters move | tick |

### Item 1: captions (F55)

    lj4evr U:W4Captions --script tests/ui/scripts/w4_captions.lj4s          rc 0, 0 ME-16 lines
    caption_diff.py --dump caption_dump.tsv --check
        caption_diff: 145 implemented form(s) of 207, 0 difference(s), 139 excluded by a fidelity row, 84 absent at runtime
    w4_entry_points.lj4s also dumps the 15 windows that need a context (a selected fixture, a running take, a laser asking)
    into caption_dump_entry.tsv; both dumps together:
    caption_diff.py --dump caption_dump.tsv+caption_dump_entry.tsv --check
        caption_diff: 154 implemented form(s) of 207, 0 difference(s), 147 excluded by a fidelity row, 91 absent at runtime

The entry run found 9 new differences, all F55 runtime fills of DFM placeholders, now rows of `tools/caption_diff_excl.tsv`:
TCleanUpForm `lAction` (`Clearing Fixture #2`), TFixtureInfoForm `LDmxAdr` / `LFixtureType` / `LLink` / `lIntG`, TRGBPackDefForm
`lDMXadr` / `lFixType` / `lUsername`, and TLaserWarningForm `Label1` (exe `%s emitting!`, one laser named; the DFM text is the
several-lasers case). Measured once on integrator 1's run and again here: the captions script alone gives 144 or 145 forms
because TLaserWarningForm closes itself at the next 100 ms poll when no laser is patched (it dumps only if the dump wins the
race); the entry run opens it from a real laser, so the combined count does not depend on that race.

### Item 2: entry points (`tests/ui/scripts/w4_entry_points.lj4s`, `w4_entry_update.lj4s`)

Prepared by `tests/ui/scripts/w4_entry_prep.sh <userdir>` (our own text: Main.Ini `[DEBUG] DebugWindow=1`, Fingers.Ini
`[SETUP] Mode=1`, fixtures 3-7 RGB Pack x4 / Generic Scanner 1 / Color Scroller 1 / Generic Fixture 2 / Mac 250 Laser, a spare
library W4Aux, and for W4Update one format-0 record of each kind the update dialogs name). The walkthrough patches fixtures
1-2 itself (MAC 500 at 1:001, DMX Dimmer at 1:017), then opens every section-2 window by its menu path, toolbar button, form
button, popup item or the automatic condition; one PASS line and one shot (`docs/screenshots/w4/entry/<name>-<Class>.png`,
229 files; the context windows of each shot were pruned to the target) per window. Both scripts parse in CTest
(`test_uiscript_w3`, appended).

    lj4evr U:W4Entry --script tests/ui/scripts/w4_entry_points.lj4s     rc 0, 155 PASS lines, 0 SCRIPT FAIL (six runs in a row)
    lj4evr U:W4Update --script tests/ui/scripts/w4_entry_update.lj4s    rc 0, 5 PASS lines

Per-window PASS lines (w4_entry_points.lj4s, in run order):

    PASS Main desktop (TLJMainForm) <- startup
    PASS Startup Status (TStartupStatusWindow) <- startup (selfdestruct in 10 s)
    PASS Fixture Configuration (TLinkConfigForm3) <- Setup > Fixture Configuration
    PASS Add Fixture(s) (TAddFixForm2) <- Fixture Configuration > Add Fixture (s)
    PASS Prompt Please Confirm.. (TConfirmForm) <- Fixture Configuration > Save
    PASS Unassigned Fixtures (TUnassignedForm) <- automatic after Fixture Configuration
    PASS Prompt Place all Fixtures on this tab ? <- Unassigned Fixtures > Auto Place All
    PASS Sequence control (TSequenceForm) <- Seq toolbar Seq button
    PASS Sequence control (TSequenceForm) <- View > Controls > Sequence Control
    PASS Cue control (TCurrentCue) <- Cue toolbar Cue button
    PASS Cue list control (TCuelist2Form) <- C-L toolbar CueList button
    PASS Background Cue (TBGCueForm) <- C-L toolbar Background Cue LED
    PASS Statics Control / Statics paged (TStatics2Form, D11) <- C-L toolbar Statics LED
    PASS Statics Control (TStatics2Form, D11) <- View > Controls > Statics Control
    PASS List of sequences (TSeqListForm) <- Seq toolbar List of Sequences
    PASS List of cues (TAdvancedCueForm) <- Cue toolbar List of Cues
    PASS Select Cuelist (TSelectCuelist2Form) <- C-L toolbar List of Cue Lists
    PASS Select Background Cue (TSelectBGCueForm2) <- Background Cue > Load
    PASS Save BG Cue (TSaveBGForm) <- Background Cue > Save
    PASS Save Sequence (TSaveSeqForm) <- Seq toolbar Save
    PASS Save Cue (TSaveCueForm) <- Cue toolbar Save
    PASS Save Cuelist (TSaveCuelist2Form) <- C-L toolbar Save
    PASS Cue Builder (TCueStackForm) <- Cue control Options > Cue-Builder
    PASS Cue Time Control (TCueTimeForm) <- Cue control Cue Time Control button
    PASS Sequence slot loop options (TLoopToolsForm) <- Cue control Cue Loop Control button
    PASS Cue macro control (TCueMacroForm) <- Cue control Cue Macro Scale button
    PASS Select Macro (TLoadMacroList) <- Cue control Macro List button
    PASS Select Background Cue (TSelectBGCueForm) <- Cue control Select BG Cue button
    PASS Sequence Slot Labels (TNameSlotsForm) <- Cue control Preferences > Sequence Slot Labels...
    PASS Generic Macro Editor (TCueMacroControlForm) <- Cue control Macro Editor button
    PASS Save Macro (TSaveMacroDialog) <- Generic Macro Editor Save Macro
    PASS Select Macro (TLoadMacroList) <- Generic Macro Editor Load Macro
    PASS Intensity control <- Fixture toolbar button 1
    PASS Movement 16 Bit control <- Fixture toolbar button 2
    PASS Colors control <- Fixture toolbar button 3
    PASS Gobos control <- Fixture toolbar button 4
    PASS Beam control <- Fixture toolbar button 5
    PASS Effects control <- Fixture toolbar button 6
    PASS Extended control <- Fixture toolbar button 7
    PASS Lamp Control control <- Fixture toolbar button 8
    PASS Position Presets (TPresetForm) <- Movement > Presets
    PASS Fan Out (TFanOutForm) <- Movement > Options > Fan Out
    PASS Position Options (TMovementOptionsForm) <- Movement > Options > Position Options
    PASS Sequence Fades (TDefault2Form) <- Movement > Fades
    PASS Movement Macro (TMovementMacroForm) <- Movement > Macro
    PASS Pan/Tilt Macro Auto Delay (TAutoDelayForm) <- Movement Macro > Auto delay
    PASS Bezier editor (in TMovementMacroForm) <- Movement Macro > Bezier Shapes > New Shape
    PASS Delete Macros (TDeleteMacrosForm) <- Movement Macro > Bezier Shapes > Delete Shapes
    PASS User Color Palette (TUserPaletteForm) <- Colors > UserPalette
    PASS Sequence Fades (TDefault2Form) <- Colors > Fades
    PASS User palette (TUserPaletteForm) <- Intensity > UserPalette
    PASS Snapshot from DMX Out (TSnapshotSceneForm) <- Sequence > Snapshot
    PASS Generic DMX Control (TGenericMegaForm) <- main menu Generics
    PASS Master Intensity (TMasterIntensityForm) <- Fixture toolbar Master Intensity
    PASS Intensity Groups (TIntensityGroupForm) <- Master Intensity > Preferences > Groups
    PASS Patch Sub Masters (TInternalIntensityPatchForm) <- Master Intensity > Options > Patch Submasters
    PASS Smoke Control (TSmokeForm) <- Fixture toolbar Smoke Control
    PASS Fixture Groups (TGroupForm) <- Fixture toolbar Fixture Groups
    PASS Next/Previous Fixture Selection (TNextPrevSelectionForm) <- right-click Previous Fixture
    PASS Custom Fixture Selection Order (TCustomOrderForm) <- Next/Previous > Set Custom Order
    PASS Edit FollowSpot Settings (TFollowSpotForm) <- Setup > Preferences > Edit Followspot settings
    PASS Select Followspot Setting (TFollowListForm) <- Fixture toolbar Follow Spot
    PASS Find profile.. (TFindProfileForm) <- Fixture Configuration > Find
    PASS Calculate addresses (TAutoAddressForm2) <- Fixture Configuration > Calculate Address
    PASS Copy From Fixture (TCopyFromForm) <- Fixture Configuration popup > Copy From Fixture
    PASS Clear Fixture Data (TCleanUpForm) <- Fixture Configuration popup > Clear Fixture Data
    PASS Import Profile (TFixtureImportForm) <- Fixture Configuration > Import > Import Profile
    PASS Export (TExportConfigForm) <- Fixture Configuration > Print > Export to text file
    PASS Prompt Please Confirm.. (change the DMX address of a saved fixture) <- Fixture Configuration address edit
    PASS Configuration Warnings (TConfigWarnForm) <- Fixture Configuration > Save with a conflict
    PASS Fixture Info (TFixtureInfoForm) <- icon popup > Fixture Info
    PASS Fixture Notes (TFixtureNotesForm) <- icon popup > Fixture Notes
    PASS DMX Output Monitor (TDMXOutForm) <- icon popup > DMX Output
    PASS Fixture Configuration (TLinkConfigForm3) <- icon popup > Fixture Configuration
    PASS Global Patch (TGlobalPatchForm) <- icon popup > Global Patch
    PASS Customize Fixture (TFixtureSetupForm) <- icon popup (MAC 500) > Customize Fixture
    PASS Channel Configuration (TChannelDefForm) <- icon popup (DMX Dimmer) > Define Fixture Profile
    PASS RGB pack configuration (TRGBPackDefForm) <- icon popup (RGB Pack) > Define Fixture Profile
    PASS Define Generic Scanner1 profile (TScanner1DefsForm) <- icon popup (Generic Scanner 1) > Define Fixture Profile
    PASS Define Color Scroller profile (TColorScrolDefForm) <- icon popup (Color Scroller 1) > Define Fixture Profile
    PASS Generic Fixture Profile (TScanner2DefsForm) <- icon popup (Generic Fixture 2) > Define Fixture Profile
    PASS Select Bitmap (TBitMapPickerForm2) <- Customize Fixture > Gobo/Effects Bitmaps
    PASS RGB pack Colors (TRgb) <- Fixture toolbar Colors (RGB pack master)
    PASS RGB Palette (TRGBPaletteForm) <- RGB pack Colors > Palette
    PASS Unassigned Fixtures (TUnassignedForm) <- View > Unassigned Fixtures
    PASS Notes (TNoteForm) <- View > Notes
    PASS DMX Output Monitor (TDMXOutForm) <- View > Dmx Output
    PASS System Memory (TMemoryViewForm) <- View > Windows System Memory
    PASS Cue control (TCurrentCue) <- View > Controls > Cue Control
    PASS Cue list control (TCuelist2Form) <- View > Controls > Cuelist Control
    PASS Background Cue (TBGCueForm) <- View > Controls > Background Cue
    PASS Debug (TDebugForm) <- View > De&bugWindow
    PASS Debug counters enabled <- Debug > Enable Performance Counters
    PASS Debug counters after 1.5 s <- Debug window
    PASS Hardware Setup (THardwareSetupForm2) <- Setup > Hardware Setup ...
    PASS Global Patch (TGlobalPatchForm) <- Setup > Global Patch ...
    PASS System Preferences (TSystemPrefsForm) <- Setup > Preferences > System ...
    PASS Creating Auto Restore Data (TCreateDataForm) <- System Preferences > Data Restore > Create Restore Data
    PASS Enter New Password (TNewPasswordForm) <- System Preferences > Data Restore > New Password
    PASS Startup Options (TStartupForm) <- Setup > Preferences > Start Up ...
    PASS Select Startup Cuelist (TSelectCuelist2ModalForm) <- Startup Options > Select
    PASS Edit Hotkeys (TFunctionsForm) <- Setup > Hotkeys > Edit Hotkeys
    PASS Notification ! (TShowHotKeyWarningForm) <- automatic when Edit Hotkeys opens (Main.Ini [MISC] ShowHotkeyWarning)
    PASS Next/Previous Fixture Selection (TNextPrevSelectionForm) <- Setup > Preferences > Previous/Next Fixture Selection
    PASS Desktop tabs (TLayerManagerForm) <- desktop popup > Desktop > Tabs..
    PASS Desktop Grid (TGridForm) <- desktop popup > Desktop > Grid..
    PASS Desktop Image (TImageSetupForm) <- desktop popup > Desktop > Image..
    PASS Select Icon Style (TIconStyleForm) <- desktop popup > Icons > Style..
    PASS Auto-Arrange Icons (TAutoArrangeForm) <- desktop popup > Icons > Auto Arrange..
    PASS Fixture Groups (TGroupForm) <- desktop popup > Fixture Groups
    PASS User Libraries (TUserLibForm) <- System > User Libraries...
    PASS Enter User Library Name (TGetUserLibNameForm) <- User Libraries > Create New Library
    PASS Rename File (TRenameFileForm) <- User Libraries > Rename Library
    PASS Backup User Library (TLibraryBackupForm) <- User Libraries > Backup Library
    PASS Restore User Library (TLibraryRestoreForm) <- User Libraries > Restore Library
    PASS Delete Sequences (TDeleteSequencesForm) <- System > Delete Files > Sequences
    PASS Delete Cues (TDeleteCuesForm) <- System > Delete Files > Cues
    PASS Delete Cuelists (TDeleteCuelistsForm) <- System > Delete Files > Cuelists
    PASS Delete Background Cues (TDeleteBGCuesForm) <- System > Delete Files > Background Cues
    PASS Delete Generic Macros (TDeleteGenericMacrosForm) <- System > Delete Files > Generic Macros
    PASS Restore Data (TRestoreDataForm) <- System > Restore Program Data
    PASS Striking Lamps (TAutoLampForm) <- System > Auto Strike Lamps
    PASS Dmx In (TDmxInForm) <- C-L toolbar DMX In, input greyed
    PASS DMX-In translation (TDMXInValueTranslationForm) <- Dmx In > Preferences > Value-translations
    PASS Translation editor (TTranslationEditForm) <- DMX-In translation > Create new table
    PASS LightJockey MIDI-IN (TMidiInForm) <- C-L toolbar MIDI In
    PASS 2532 Direct Access (TDa2532Form_3) <- C-L toolbar 2532
    PASS 2532 Function List (TButtonFunctions2532Form) <- 2532 > Options > Function-List
    PASS 2532 Color Scheme (TColorScheme2532Form) <- 2532 > Font Properties > Color Scheme
    PASS 2532 Tab Colors (TDA2532_TabColorsForm) <- 2532 > Tab Options > Tab Colors > Set colors...
    PASS Set Tab Size (TDa2532TabHeight) <- 2532 > Tab Options > Set Tab Size...
    PASS 2532 Page Names (TName2532PagesForm) <- 2532 > Tab Options > Name Page Tabs...
    PASS C-L toolbar button 7 (CD / MC-X / 2518) greyed, hint Not available in LJ4EVR (dropped hardware)
    PASS C-L toolbar button 8 (CD / MC-X / 2518) greyed, hint Not available in LJ4EVR (dropped hardware)
    PASS C-L toolbar button 9 (CD / MC-X / 2518) greyed, hint Not available in LJ4EVR (dropped hardware)
    PASS Fingers status panel (TFingersStatus) <- Fingers > Status panel
    PASS Virtual Fingers (TLJFingersMainForm) <- Fingers > Virtual Fingers
    PASS Fingers Setup (TFingersSetupDialog) <- Fingers > Setup
    PASS About (TAboutForm2) <- Help > About
    PASS Help viewer (THelpViewer) <- Help > Help
    PASS Help viewer at the Cues topic <- Cue control > Help
    PASS Help viewer <- Intensity > Help
    PASS Statics debug (TStaticsDebugForm) <- by id (no LJ2 entry point, 13 s1.2)
    PASS WM_CPOPYDATA DEBUG (TMsgDebugForm) <- by id (no LJ2 entry point, 13 s1.2)
    PASS Fingers Setup (TFingersSetupDialog) <- Fingers > Setup (again, for its helpers)
    PASS Select Skin (TFingersSkinSelectForm) <- Fingers Setup > skin folder button
    PASS Warning BLACKOUT ACTIVE (TStatusWarningForm) <- automatic on Blackout
    PASS Warning Frozen ! (TFreezeWarnForm) <- automatic on Freeze
    PASS Warning DMX Output is Offline (TOfflineWarningForm) <- automatic on Offline
    PASS Enable laser output ? (TUnblockLasersForm) <- automatic when a laser channel goes non-zero
    PASS Warning! Laser(s) emitting! (TLaserWarningForm) <- automatic while the laser emits
    PASS Record to LightCorder file (TLightCorderDialog) <- Cuelist > Tools > Record to LightCorder file
    PASS Save Lightcorder File (TSaveLightCorderFileDialog) <- Record to LightCorder file > Cancel Recording
    PASS LightCorder wrote LightCorderFiles/LC0001.LLF
    PASS Close Lightjockey ? (TCloseOrSaveForm) <- main window close

w4_entry_update.lj4s:

    PASS Datafile format update required (TUpdateCl2Form) <- automatic: an older .Cl2 in the library
    PASS File Format Update Needed (TUpdateBGCuesForm) <- automatic: an older BG cue file
    PASS Update to Patch Configuration (TPatchUpdateForm) <- automatic: an older patch file
    PASS Dataformat has Changed (TUpdateForm) <- automatic: any other older record
    PASS the three updated records carry format 1

Section-2 rows with no PASS line of their own, and why:
- Martin Audio Analyzer (`LJSound.exe`): W5 (`&Audio` never shown, row of fidelity.md).
- MC-X Setup, 2518-RS232, CD-ROM (Audio), LJ MediaPlayer: their C-L buttons are greyed with the hint (PASS lines for 7-9);
  Capture Status, SandNet status, LJ-Manager preferences, 3D Visualizer menu: never shown or every leaf greyed (D12,
  inventory s5). The forms are in the fidelity.md "never open" row.
- `WARNING !` USB Interface Disconnected: no USB interface exists (TUSBWarningForm, same row).
- `Notification !` no transmitter hardware: raised at start only when no link is mapped to a universe; this PC maps link 1.
  Driven by `w4_desktop_dialogs.lj4s` and `test_desktop_dialogs` (fidelity.md row).
- Statics debug / WM_CPOPYDATA DEBUG: no entry point exists in any LJ2 source (13 s1.2); opened by id, said in the PASS line.
- 2532 and Virtual Fingers on keyboard / MIDI (D16): opened here from their menu and toolbar; the MIDI rows are
  `w4_da2532_fingers.lj4s` and `test_da2532_fingers`.

### Item 3: goldens

    lj4evr_tests ui_golden        208 of 208 form(s) have a REVIEWED.tsv row; 208 form(s), 0 failure(s)
    tests/ui/golden               359 BMPs, 432 REVIEWED rows (359 distinct), 0 BMP without a row
    lj4evr_tests ui_golden_ctl    0 failure(s) after five re-renders (fix 3): CTL_MAC500_MOVEMENT 157 px (bbox 125,26-153,37),
                                  COLORS / GOBOS / BEAM 598 px (9,26-207,37), EFFECTS 598 px (9,26-138,37): the menu-bar text
                                  only (UserPalette / Fades / Help no longer greyed), old | new viewed at 2x, one row each

### Item 4: docs/fidelity.md

New section "Fidelity contract F1-F60, complete": 60 rows, each an LJ4EVR capture beside the CHM reference it was compared
with, or "deferred: needs rig" with the rig item (F15 Q2, F19 EDIT CUE, F20 14 sB.8, F24 M12-M15, F25 R2, F32 M21-M22, F37 /
F38 17 s8, F45, F47, F57 M1, F58 M2-M6, F59 M16-M20). Every capture it names exists (checked by script). Plus the rows
integrator 1 left open: the 19 forms that never open, TSavePresetForm, the three D11 forms, and the rows this pass found.
Not done here: the track return texts' own rows (they are not in the repo; the fidelity-docs owner holds them).

### Item 5: restart persistence (`w4_positions_1.lj4s`, `w4_positions_2.lj4s`)

Every W4 window whose DFM Position is poDesigned / poDefaultPosOnly (lj_formsini_section persists exactly those): cue_stack,
da2532, dmx_in, dmxin_translation, find_profile, fingers, fingers_status, the five Fingers function lists, hidden_fixture,
movement_options, rgb_palette, select_cuelist_modal, debug, statics_debug, msg_debug, fixture_info, rgbpack_defs. Part 1
opens, moves (DISPLAY2) and closes each; part 2, after a restart, asserts the position.

    lj4evr U:W4Entry --script tests/ui/scripts/w4_positions_1.lj4s     rc 0
    lj4evr U:W4Entry --script tests/ui/scripts/w4_positions_2.lj4s     rc 0, 21 PASS lines (three runs in a row; before fix 11,
                                                                        one window per run came back centred, at random)

The help viewer is poScreenCenter (ours) and is not persisted; the prompt boxes (laser pair, update dialogs) are centred.

### Items 6-9

    Help      Help > Help opens THelpViewer at the index; Cue control > Help opens it at "Cues" (help-cues-THelpViewer.png);
              Intensity > Help at the index. 59 Help items now reach the viewer (fix 6); test_help_viewer unchanged
    LightCorder  dimmer 1:017 up (assert_frame 1:17=200 +-55) and the laser emitting, Cuelist > Tools > Record to LightCorder
              file, Start, 1.5 s, Cancel Recording, Save File:
              lightcorder: stopped after 61 frames (573 of 4194304 card bytes)
              lightcorder: 61 frames saved to ...\User\W4Entry\LightCorderFiles\LC0001.LLF
              lj4evr_tests test_llf <that file>  (new playback mode of test_llf: decode and print every frame)
              llf ...LC0001.LLF: link 1, 512 channels, 40 Hz, 61 frame(s), card 4194304 bytes, compress 1
              frame 0: 17:227 121:227   ...   frame 60: 17:227 121:227
              61 of 61 frame(s) carry a non-zero slot                                        rc 0
    DMX-in    Dmx In from the C-L button: assert_enabled TDmxInForm.bAssign 0; Clear Assigned / Assign Function / Save greyed,
              the grid's input columns empty (dmx-in-TDmxInForm.png); translation window and editor open, inputs greyed
    Debug     View > De&bugWindow (Main.Ini [DEBUG] DebugWindow=1), Enable Performance Counters, two shots 1.5 s apart:
              Tick 323 -> 381 (58 ticks = 40 Hz), tick time 30 -> 31 us (max 105), process time 0.030 -> 0.031 ms,
              Link 1-4 1 packets/s (idle keep-alive)

### Fixes (the walkthrough's real bugs; each now has a PASS line or a check)

| # | Found | Fix |
|---|---|---|
| 1 | Cue control reopened from its toolbar button kept the DFM caption `Cue` (F19): the refresh compared against a caption cached from the closed window | `lj_cue_form.c` close clears the cache |
| 2 | Cue control Cue Macro Scale, Macro Editor and Macro List buttons only logged "is W2" | `lj_cue_form.c` `sb_up`: open Cue Macro Control, the Generic Macro Editor, Select Macro |
| 3 | Every control window's UserPalette / Fades / Help were greyed or a no-op ("not in W1") and Movement's Fades greyed: User palettes and Sequence Fades had no entry point from the controls (inventory s2) | `lj_ctl_menu_run` in `lj_ctl_common.c` (palette fn of the kind, Sequence Fades, Help viewer) used by the common, colour-family and Movement windows; 5 goldens re-rendered; `test_controls_c` expects UserPalette enabled |
| 4 | Master Intensity Preferences > Groups logged "W2" | `lj_master_intensity.c`: opens Intensity Groups |
| 5 | Sequence > Snapshot was a stub ("Snapshot is W2") | `lj_seq_form.c`: opens Snapshot from DMX Out |
| 6 | 46 `Help1Click` / `Help2Click` / `LocalHelpClick` bindings were `lj_handler_noop` and 12 more logged "Help is W4" or "no help file"; `lj_help_viewer_click` existed and was used nowhere (design 13 item 12) | 21 + 11 window files bind it with a topic (`cues`, `fixtures`, `controls` ...); the controls through fix 3 |
| 7 | TListView never fired OnChange, so every `lv*Change` handler was dead: a click in Fixture Configuration's list selected nothing (Calculate Address, Copy From, Clear Fixture Data, Remove stayed unreachable), User Libraries rows likewise | `lj_w_listview.c` `select_row` emits `LJ_WEV_CHANGE` (VCL ctState); `widgets_lists` asserts it |
| 8 | An RGB pack master greyed the Colors button and the function toggled the absent Colors kind: RGB pack Colors had no entry point | `lj_toolbars.c` enables it for `LJ_CTL_RGBPACK`; `lj_app_seam.c` routes Colors to the RGB pack window |
| 9 | BLACKOUT ACTIVE (F14) never stayed up: the GM fade-out / fade-in calls of the same pass share its box and destroyed it right after it was made (W1's acceptance7-blackout shot has no box) | `lj_app.c` `lj_app_refresh`: one call per pass for the shared box |
| 10 | Crash (access violation in `bound_dispatch`, 2 of 3 runs, found with lldb on a /Z7 Release build): the Debug family kept the wrapped root of a closed Statics debug window; WM_CPOPYDATA DEBUG allocated at the same address matched it and dispatched into freed bindings | `lj_debug_form.c` close clears the wrap entry; 6 clean runs after |
| 11 | forms.ini positions lost at random (one window per run came back centred on the main window): windows were created visible at SDL's default spot and moved after | `lj_app.c` `lj_app_host_open`: create hidden, place, then show; 3 clean persistence runs after |
| 12 | Script driver: firing a popup item left the popup open, and it ate the next click on that window (fire() closes it) | `lj_uiscript.c` closes the menu first; `close main` raises `Close Lightjockey ?` (docs/ui-script.md W4 verbs) |

Left as found (not windows, outside this acceptance, fidelity.md rows): the Cue control's BG-cue / generic-macro clear,
disable and drop buttons, Master Intensity Sync / Bump (both log only). The Fixture Configuration popup's Fixture Notes shows
the profile notes in the pane (PROPOSAL row).

### Builds and tests after the fixes

    cmake --build build-w4 / build-w4-nosdl / build-debug-win, build-msvc --config Release / Debug    0 warnings, 0 errors
    ctest build-w4 (Release, SDL)            100% tests passed, 0 tests failed out of 117
    ctest build-w4-nosdl                     100% tests passed, 0 tests failed out of 116
    ctest build-debug-win (Debug, SDL)       100% tests passed, 0 tests failed out of 117
    ctest build-msvc -C Release / -C Debug   100% tests passed, 0 tests failed out of 117 (each)
    test_layering                            428 file(s) checked, 205 header(s) compiled alone, noop handlers 0, ini ternaries 0,
                                             scene getters 0, set-text/lib/ctx copies 0, 0 finding(s)
    lj4evr --selftest (Debug and Release)    rc 0, six OK lines, 0 "no binding" / "unknown event" / "has no behaviour file"
    gcc 14.2 (w64devkit) and clang 23 (llvm-mingw), CI flags, -fsyntax-only over the 42 changed SDL-free .c files: clean
    No engine, model or generator input changed: the frame dumps and replay-frames.bin of integrator 1 stand.

The two scratch libraries were deleted and `%APPDATA%\LJ4EVR` put back after the runs.

Pre-commit: 292 staged paths, none under `reference/`; sha1 of each against the 18,923 files under `reference/` (13,637
distinct): 0 equal. The new binaries are our own renders (229 entry-point captures, 5 control goldens).

## Fix pass A r1

Session of 2026-10-22/23 on the work PC (MSVC 19.44, Ninja + cl). Input: `delegations/W4_CODE_MASTER_ELDER_VERDICT.md`
(ruling HOLD) section 5 Part A, items 1-23, each run to its *Verify* line. Joey's rulings of 2026-10-22 (Q1 rewrite, Q2 MIT,
Q3 the `LJ4EVR (User lib: %s)` fallback, Q4 hosted Linux runners, Q5 ship with `Native=1`) are Part B and not applied here:
no tag, no `VERSION` bump, no visibility change. App runs used a neutral data folder, `LJ4EVR_USERDATA=C:\ProgramData\LJ4EVR\`
(new, `docs/running.md`), so `%APPDATA%\LJ4EVR` was not touched.

### Builds and tests

    build-w4 (Release, SDL, Ninja + cl)            0 warnings, 0 errors   ctest 100% passed, 0 failed out of 119
    build-w4-nosdl (Release, LJ_SDL=OFF)           0 warnings, 0 errors   ctest 100% passed, 0 failed out of 118
    build-debug-win (Debug, SDL)                   0 warnings, 0 errors   ctest 100% passed, 0 failed out of 119
    build-fixa-asan (Debug, LJ_SDL=OFF, MSVC /fsanitize=address)  0 warnings  ctest 100% passed, 0 failed out of 118
    build-debug-win lj4evr --selftest              six OK lines; 0 "no binding" / "unknown event" / "has no behaviour file"
    test_layering                                  428 file(s) checked, 205 header(s) compiled alone, noop handlers 0, ini ternaries 0,
                                                   scene getters 0, set-text/lib/ctx copies 0, noop Help handlers 0, 0 finding(s)
    replay_all                                     36 scenario(s), 0 failed
    ui_golden                                      208 of 208 form(s) have a REVIEWED.tsv row; 208 form(s), 0 failure(s)
    dfm2c --check                                  OK          gen_vectors --check / --shapes --check: up to date
    fidelity_check (new CTest row)                 73 F/I id(s) with a row of 73, sacrifice paragraph equal, 0 finding(s)
    caption_diff (w4_captions.lj4s, U:W4Captions)  rc 0, 0 ME-16 lines; 145 implemented form(s) of 207, 0 difference(s),
                                                   139 excluded by a fidelity row, 84 absent at runtime
    determinism (dump-frames, Release / Debug / no-SDL)  equal, 1,641,600 bytes, sha256 5b13f41930362b75a7aa6f9c53f77db2d315a4039ea5a18da664ea1263114b50
                                                   (unchanged: the 400-tick stream has no generic macro)
    replay-frames.bin (all four trees)             129,664,080 bytes, sha256 c46fee4ceff60245c9b504a6d7b2028fca0d0ba6008e1f541af7c8279545039d
                                                   (re-recorded once for ME-128; with ME-132 alone it is still b14d4b29...86468d05c, measured)

New CTest rows: `fidelity_check`, `fidelity_check_selftest`; new rows inside existing subcommands: `test_engine_w2`
(address guards, four cases), `test_restore_data` (ME-139), `test_uiscript_w3` (ME-142 prompt), `test_da2532_fingers`
(ME-143), `test_debug_form` (ME-144). New UI scripts (app runs, not CTest): `w4_wire_record.lj4s`, `w4_help_fades.lj4s`.

### Part A, item by item

| # | ME | Done | Verify (as run) |
|---|---|---|---|
| 1 | ME-132 | `lj_fx_code(fx, ofs)` in `model/lj_model.h`: the address code of an offset, or -1 for address < 1, a link outside 1..4 or an offset past channel 512 of the link. Used by the rebuild (stops at the link end), s7 (every pair read and write), the movement-macro overlay and `pan_winner`. `lj_post_profile` returns NULL for a missing profile, no address, or `ofs` outside `0..footprint-1`. `lj_post_pair_lsb` keeps the LSB inside the link. | `test_engine_w2` `lj_t_address_guards`: (1) 16-ch profile at 1:505, dimmer at 2:001 = 255 in the scene, GM 0: `out[1][0] == 0`; (2) address-0 pt16 head on link 2 with Invert Pan beside a dimmer at 1:512 = 10: `out[0][511] == 10`, `out[1][0] == 0`; (3) the same head on link 1 with movement macro 1 running; (4) a heap profile bound at 4:512, moved to 1:001 with no rebind. Green on build-w4 and on the MSVC ASan no-SDL tree; with the three engine files at HEAD the same binary stops with `AddressSanitizer: heap-buffer-overflow` (measured). Replay sha unchanged by ME-132 alone (above); determinism unchanged. |
| 2 | ME-133 | `lj_lock` around every live write in Fixture Configuration: `lj_fixture_config_set_address` (eAdr, Lnk, up/down, Calculate addresses OK all route through it), `_add`, `_put`, remove, Clear settings and the six flag boxes. Copy From and Clear Fixture Data already locked. The rebind stays at Save; ME-132 (c) makes a stale bind safe. `test_uiscript_w3.c` rebinds under the lock after its memset. | `test_uiscript_w3` passes on the MSVC ASan tree (red at HEAD per the verdict). LORD_C's stale-bind row = case (4) of item 1: 4:512 and 1:001 both 0, ASan clean. |
| 3 | ME-134 (ME-128) | `lj_genmacro_step` on a declared pair adds `round(s * amp * 256 / 32640)` (integer, half away from 0; `/* PROPOSAL rig 14 sA.6 */`) instead of the 8-bit delta x 256. 8 of the 11 `expect_pair` rows of `genmacro_sine_amp186` re-recorded from the formula computed independently in Python. | pair 1:111 = 40668 / 50856 / 58368 / 50884 at ticks 2 / 5 / 10 / 15 (`replay_all` green); `replay-frames.bin` re-recorded once (above); `determinism` locally equal; CI below. |
| 4 | ME-135 | `lj_w_image.c` paints `desc->picture` through `lj_glyph_get` (Source-Images aware): Center, Stretch (integer nearest neighbour) and Transparent (bottom-left key). The runtime laser warning hides Image2 (Timer1's second frame at 0,-8, PROPOSAL rig). | Re-shot `E/laser-warning-TLaserWarningForm.png`: the art box at client (4,13)-(114,112), 111x100, 3,283 yellow sign pixels. `E/about-TAboutForm2.png`: 6,722 non-black pixels inside the logo rect (55,3) 250x113. `E/laser-unblock-TUnblockLasersForm.png` re-shot. REVIEWED rows for every changed golden (below). |
| 5 | ME-136 | `lj_w_bitbtn_kind_glyph`: a TBitBtn with a Kind and no Glyph draws the kind's art stem (`ok`, `cancel`, `help`, `close`, `yes`, `no`, `retry`, `ignore`, `abort`, `all`) through `lj_w_glyph_caption`. | Goldens re-rendered: 159 BMPs changed in all (kind glyphs, plus items 4, 6 and 14); every one diffed against HEAD (old / new / mask contact sheets and old-new crop sheets, viewed). `ui_golden` green; one REVIEWED row per changed golden, 159 rows. Re-shot `E/startup-options-TStartupForm.png` (Setup > Preferences > Start Up ...): the check on `Ok` and the cross on `Cancel`; its golden `TSTARTUPFORM` changed 322 px, the two faces. |
| 6 | ME-137 | `tools/dfm2c.py` `ITEMS_EXCLUDED`: `TABOUTFORM2.RichEdit1` Lines emitted empty, recorded `dropped:D2: authored credits, not a caption` in `dfm2c_blobs.txt`; `caption_diff_excl.tsv` row; `TABOUTFORM2.bmp` re-rendered once with item 4. | `git grep -i -l "<credit-name>\|<credit-word>\|<credit-signer>"` = 0 files (whole tree); `grep -c <credit-word> build-w4/lj4evr.exe build-debug-win/lj4evr.exe` = 0 / 0; `caption_diff` 0 differences. |
| 7 | ME-138 | `docs/fidelity.md` section "Fix pass A r1": the sacrifice paragraph (byte-equal to README's), I1 and I6-I13, the rows the code promises (vsIcon, Fixture Special OK, Patch Sub Masters Source, Debug n/a, sampled loads, `CLUBLITE_EXTERNALMAP`, the by-id forms, TPrintWaitForm), the two Q9 literals, ME-145's other buttons, ME-163's Date column with F50 reworded, ME-160 with F17 corrected, the seven W4-FRAME details, and six superseding rows (`&Fingers`, P_Clone / P_Clear, Apply to all, `eChannelName` 6 bytes, Fan Out selections, BG Edit Sequence). `tools/fidelity_check.py` + CTest rows. | `fidelity_check`: 73 F/I id(s) with a row of 73, sacrifice paragraph equal, 0 finding(s); `--selftest` (a blank F17 cell, a changed paragraph, a missing capture) fails all three. Acceptance item 4 re-ticked with that output (`w4-gates.md`). |
| 8 | ME-139 | `lj_fs_replace(tmp, path)` in `core/lj_fsinfo`: remove, rename, and on a failed rename keep `tmp` and log its name. `lj_library_update_file` (F56) and `lj_restore_data_create` both use it. | `test_restore_data`: `ME-139: forced rename failure -> -1, the .tmp copy survives` (a non-empty folder stands where the restore data goes). |
| 9 | ME-140 | `-fno-sanitize-recover=all` beside `-fsanitize` (non-MSVC) and in the fuzz `CMAKE_C_FLAGS`. | linux-gcc-asan green with the flag: CI below. |
| 10 | ME-141 | The five Help leaves bound to `lj_help_viewer_click` (`sequences`, `fixtures` x3, `files`). `test_layering.py` check 10: a `*elp*Click` / `Heklp1Click` handler, or the OnClick of a form's bkHelp TBitBtn, bound to `lj_handler_noop` is a finding. | On the HEAD copies of `lj_gdc_form.c` / `lj_config_dialogs.c` the check reports 4 findings (`cfm_b Heklp1Click`, `im_b BitBtn4Click`, `fe_b BitBtn1Click`, `lj_sf_bindings help1Click`); on the tree 0. `w4_help_fades.lj4s`: `PASS Help viewer (THelpViewer) <- Colors > Fades > Help (ME-141)`. |
| 11 | ME-142 | `ask("Clear All Inverted Channels ?", 3)` (exe 0x1BE380). Fidelity rows for `Do you wish to restart LightJockey now ?` and `Sequence-file format changed`. | `test_uiscript_w3`: `open global_patch` / `click TGlobalPatchForm.ClearAllInverted1` / `assert_prompt "Clear All Inverted Channels ?"` passes. Q9 grep (every backticked literal of note 12 Q9 against `src/`): 58 literals, 48 verbatim; the 10 others are 7 INI keys or file names, `Close LightJockey ?` (the DFM's `Close Lightjockey ?` wins, D13) and the two with fidelity rows. |
| 12 | ME-143 | `held_bump[LJ_FG_FADERS]`: a bump's release runs the function that fired. | `test_da2532_fingers`: press Bump 1 on page A, `lj_fingers_set_page(2)`, release: `BUMP_SUB_FLASH_1` edge 0. |
| 13 | ME-144 | `lj_engine_note_overrun` (caller holds the lock), called by the engine thread beside `lj_log_overrun`. | `test_debug_form`: one late tick reads `Overruns: 1`. |
| 14 | ME-145 | `tools/glyphmap.tsv` rows: Cue Builder `sbAuto` / `sbManual` / `sbMusic` = `autotrig` / `manualtrig` / `music`, Select Bitmap `bGobos` / `bColors` = `gobos` / `colors`; `dfm2c` regenerated. Fan Out `sbNew` / `sbDelete` / `sbSave` and User Libraries `bSelect`: fidelity rows. | Re-shot `E/cue-builder-TCueStackForm.png` (the three trig faces) and `E/bitmap-picker-TBitMapPickerForm2.png` (the gobo and colour faces) from their entry points: non-blank (viewed); goldens `TCUESTACKFORM` / `W4SD_CUE_STACK` 257 px, `TBITMAPPICKERFORM2` / `W4C_BITMAP_PICKER` 264 px. |
| 15 | ME-146 | `RestoreData.lj4d` (constant, `file-formats.md`, the two tests, the dialog comment). | `git grep -c` of the old file name (`RestoreData` + `.lj4r`) = 0; `test_restore_data` green. |
| 16 | ME-147 | The Execute File comment states what is filtered and what still reaches the shell. | "AllowExecute=1 lets a cue list start any program through the shell; enable it only for cue lists you wrote." is in `docs/running.md` and `SECURITY.md` (1 each). |
| 17 | ME-148 | HEAD redaction (table below); the nine path-bearing User Libraries captures and the four others (W2 restore and export, W4 export and import) re-shot under `LJ4EVR_USERDATA`. | The verdict's four-pattern `git grep -c -E` (the LAN prefix, the runner login, the work-PC name prefix, the NIC name) = 0 at HEAD. Pixel scan (scratch `pixscan.py`: the rendered user name in the two faces, cut from two `9c21031` captures, matched in any colour pair over every tracked PNG / BMP / GIF): 862 images, 0 hits (it finds the user name in the HEAD versions of the re-shot captures, measured). |
| 18 | ME-149 (1), (2) | `docs/ci.md` rule 1 rewritten (the `if:` keeps this repository's own PRs off the self-hosted runners; the approval setting is the fork control; the approver reads the workflow diff first), the `ci.yml` header comment the same. `runs-on` unchanged (Part B). | CI below. |
| 19 | ME-150 | REL-HISTORY entry (below); `SOURCE:5`, `NOTICE:36`, `provenance.md` and the CPack part title say "attribution and modification notice" (upstream QLC+ has no NOTICE file); a `spikes/` provenance row (Aileron Regular, CC0 1.0, by Sora Sagano, as bundled in Pillow). | the three records present (this section, `third_party/qlcplus-fixtures/SOURCE`, `docs/provenance.md`). |
| 20 | ME-151 | `file-formats.md`: `Fingers.Ini`, `2532Setup.Ini`, `RGBPalette.Ini`, `Main.Ini [CUSTOMSEL]`. README: CMake 3.25, `dev-mac` build lines, `debug-win` in the preset list, the main desktop capture, the citations sentence. CONTRIBUTING: the golden rule as practised, "no extern globals except the log sink". SECURITY: the fallback contact. `release.yml`: the ME-21 line dropped, "planned for a later version". `releasing.md`: the `v0.x` line, the reporting and fork-approval steps right after the flip. `building.md`: short clone path, python3 for ctest. `generated-files.md`: the art inputs, `Items.Data` decoded, `ITEMS_EXCLUDED`. `w4-gates.md`: the Status column. | Status column: 0 blank rows (92 filled); README says `CMake >= 3.25`; `grep -c ME-21 .github/workflows/release.yml` = 0. |
| 21 | ME-152 | `w4_wire_record.lj4s`: MAC 500 at 1:001 runs movement macro 1, LightCorder records 11 s, sACN on the wire (default `Hardware.Ini`: multicast, universes 1-4). | per-second lines below; ME-24 ticked. |
| 22 | ME-153 | design.md edited for the orchestrator (vault): revision line below. | `scaled in microseconds` 2, `UI-side pin` 2, `arialblack18` 3, `movementmacro.gif` 2, `532 bytes` 2 (each >= 1). |
| 23 | ME-157 | `tools/standard_user_install.ps1`, a `ci.yml` windows-msvc step on the hosted runner. | the four records: CI below. |

### ME-152: the recording wire run (ME-24 HONOUR)

    lightcorder: recording link 1, 512 channels, 40 Hz, 4 MB card, compression on
    lightcorder: 41 frames/s, tick 25000 us, sACN 40 1 1 1 packets/s
    lightcorder: 41 frames/s, tick 25000 us, sACN 40 1 1 1 packets/s
    lightcorder: 40 frames/s, tick 25000 us, sACN 40 1 1 1 packets/s
    lightcorder: 40 frames/s, tick 25000 us, sACN 41 1 1 1 packets/s
    lightcorder: 41 frames/s, tick 25000 us, sACN 40 1 1 1 packets/s
    lightcorder: 40 frames/s, tick 25000 us, sACN 41 1 1 1 packets/s
    lightcorder: 40 frames/s, tick 25000 us, sACN 40 1 1 1 packets/s
    lightcorder: 41 frames/s, tick 25000 us, sACN 40 1 1 1 packets/s
    lightcorder: 41 frames/s, tick 25000 us, sACN 40 2 2 2 packets/s
    lightcorder: 41 frames/s, tick 25000 us, sACN 41 1 1 1 packets/s
    lightcorder: stopped after 441 frames (226233 of 4194304 card bytes)
    lightcorder: 441 frames saved to C:\ProgramData\LJ4EVR\User\W4Entry\LightCorderFiles\LC0001.LLF

441 frames in 11 s = 40.1 frames/s at 40 Hz; `TickHz` unchanged (25,000 us every second); link 1, whose frame changes every
tick, at the 40 packets/s cap (the 41s are the one-second window's edge), links 2-4 at the 1/s keep-alive. Without the
movement macro the same run gives 40-41 frames/s and 1 packet/s per link (the frame never changes). ME-24 ticked.

### ME-148: REL-PRIVATE disposition at HEAD

| Item | Where | Disposition |
|---|---|---|
| the runner's login@host and its `.local` name | `docs/w1/w2/w3-integration.md`, `docs/w4-gates.md` | `<runner-host>` |
| this PC's and atlas-ci2's LAN addresses | the same four files | `<lan-ip>` |
| the work-PC name in the sACN source name | `docs/w1/w2/w3-integration.md` (capture lines) | `<dev-pc>` |
| atlas-ci2's two NIC names | `docs/w1/w2/w3-integration.md`, `docs/fidelity.md` | `<nic>` (the second NIC "a second one") |
| the Dropbox path | `spikes/c-engineering/README.md` | "the design vault (not published)" |
| a host name in a gate's source cell | `docs/w4-gates.md` REL-PRIVATE row | reworded to "the maintainer's earlier repository redaction precedent" |
| the Windows user name in pixels | 13 captures (`docs/screenshots/w4/entry/{user-libraries,user-lib-name,rename-file,backup,restore}-*`, `export-config-TExportConfigForm`, `import-profile-TFixtureImportForm`, `docs/screenshots/w2/{restore-TLibraryRestoreForm,export-config-TExportConfigForm}`) | re-shot under `LJ4EVR_USERDATA=C:\ProgramData\LJ4EVR\` (the same windows from the same entry points; the W2 restore shot is taken after picking the backup, before Restore); the pixel scan is part of the REL-PRIVATE scan from now on |
| `atlas-ci2` as a word, the `lj4evr` runner label | `docs/ci.md`, `ci.yml` comments, older logs | kept: a retired host's name, no address or user; the label follows ME-159 (Part B) |
| test IPs `192.168.10.5`, `192.168.1.x` | `tests/sacn/test_cfg.c`, `tests/ui/test_windows_c.c` | kept: synthetic |
| history | every commit and tag | Part B step 1 (Joey's Q1 ruling: rewrite) |

### ME-150: REL-HISTORY entry

What was measured, by LORD_PROVENANCE and confirmed by the W4 verdict section 0 (scripts in the session scratchpad, nothing
written to the repo):

1. Objects: `git rev-list --objects --all` 3,721 objects, 3,020 blobs, 65 commits, 7 refs, no stash; 0 `reference/` paths.
   772 binary blobs: 422 goldens and 338 captures (provenance rows), 6 `spikes/*.png` (our renders), 4 `packaging/icons/*`
   (`make_icons.py`), the 214-byte synthetic `w3_synthetic.Udf`, and the repo-root `controls_a_intensity_comma.bmp`
   (`d855140`, removed in `364812f`: LJ4EVR's own Intensity control render, viewed). One empty capture,
   `docs/screenshots/w1/acceptance4-sequence-TSaveSeqForm.png` (blob `bb42eb5b`, a 65-byte 0x0 PNG, ME-68), is our own
   output too. No `.GS2`, `.rtf`, `.chm`, `.gif` or `.Mac` file and no profile file in history; no DFM hex block.
2. Hash match: the git blob sha1 of all 18,920 non-empty files under `reference/` (as-is and CRLF->LF, 16,111 distinct)
   against every history blob: 22 equal, all the vendored QLC+ `.qxf` files (Apache-2.0, equal to upstream at the
   recorded commit).
3. Pixel match of our art against Martin's bitmaps: best same-size agreement 0.960, the LED sheets; named counterparts
   at most 0.772; DFM-embedded bitmaps at most 0.758. Gobo near-matches Cross 0.962 and Pin 0.961 (LORD_FIDELITY) are
   trivially simple shapes.
4. Pixel match of committed images against the 210 Source-Images glyphs and the 101 DFM bitmaps: 83 hits, 77 of them
   the 13-px OnLine LED disc of `lj_w_led.c` (129 of 129 opaque pixels of `LedOn.bmp`: a plain circle at a harvested
   size, geometry; the three LED sheets match 128 of 129), 6 `H-Arrow` false positives; no capture ran with Martin's
   images loaded.
5. Text match: 10- and 12-word runs from 370-516 Martin prose sources against every non-`third_party` text blob: citations
   of one or two sentences, function-name lists, DMX command strings; no paragraph of the help or the manuals. The one
   authored text found is Martin's About credits array (`lj_forms_gen.c` since `d855140`, the `TABOUTFORM2` golden since
   `893123d`, every installer since W0): **REL-HISTORY not met**. HEAD is clean after ME-137; history and the eight
   prerelease installer assets follow Joey's ruling Q1 = (a) rewrite (Part B step 1).
6. Secrets: 0 hits for 11 token and key patterns; every author and tagger e-mail is the GitHub noreply address.
   Private details: the REL-PRIVATE table above.

### ME-151: design 13 items 2, 6, 7, 11 and 13

- Item 2 (Joey's answers to inventory section 7 Q1, Q2, Q4): not answered; v1.0.0 builds on tier 1 of note 15 as the
  design says, and the `.LJB` importer stays W5.
- Item 6 (`.bgc` extension, `Setup.<k>` palette layout): ours (PROPOSAL) in v1.0.0; a rig sample can change them.
- Item 7 (Statics 2: 20 slots per page, Add / Replace / Solo scopes, LTP inside): PROPOSAL in v1.0.0; rig `[14 sB.8]`.
- Item 11 (op-list goldens): not needed; BMP goldens with a REVIEWED row per change held through W4 (this pass changed
  159 and reviewed each).
- Item 13 (MediaPlayer): W5, the default; the rows stay greyed with the inert-row rule (fidelity I4).

### ME-153: the design.md revision line (vault, design owner)

> Fix-pass revision W4 r1 (2026-10-23, ME-153, per `delegations/W4_CODE_MASTER_ELDER_VERDICT.md` section 5 item 22, applied by the W4 fix-pass integrator for the orchestrator as design owner): landed the ME-124 / ME-50 sentences (2.0 OS-edge state and the six limits in `core/lj_types.h`; 2.3 rule 3 the Statics 2 link is a UI-side pin; 3.3 cue time scaled in microseconds; 3.5 the two-case unfreeze rule; 4 the six limits; 7.4 the clean-room sentence and `arialblack18`; 7.6 the four runtime typos; 7.7 which GIFs are 1:1, `movementmacro.gif` 447x243 among them; 10 W1 items 4-5), 10 W4 and 12 F8 (the DMX-in window opens in W4 with its input controls greyed; the sACN input path is W5), and the note 16 section 2 width (532 bytes per `.Udf` file-table entry), stated in 5.3 because only design.md is edited.

The same revision replaces the three remaining `arialblack16` mentions (stack table, 2.6, 13 item 4) with `arialblack18`.
Note 16 itself is not edited (only design.md is in this pass's scope); design 5.3 records the correction.

### Not done here

- Part B (history rewrite, PROPOSAL text, caption fallback, `VERSION`, tag, flip, runner switch): after this pass is
  verified, per the verdict.
- The W5 entry gates of verdict section 7 and the LOW rows of section 8: untouched, except LORD_FIDELITY-19, which now
  has its fidelity row.
- Captures of other windows that show the laser warning behind them (`lightcorder-*`, `save-lightcorder-*`,
  `close-prompt-TLaserWarningForm.png`) still show it without the sign; only the warning's own capture was re-shot.

### CI record (items 9, 18, 23; ME-149 (1))

Commits: `575077a` (the pass), `232fa91` and `283dc06` (ME-157: the install dir read from the uninstall key; the
account's own environment), `bff756d` / `5021964` (a temporary fuzz A/B diagnostic), `054e9ac` (the fuzz fix below).

    https://github.com/UnbannableGT/LJ4EVR/actions/runs/39909521611 (054e9ac), attempt 2: all six jobs green:
    generate-check, linux-gcc-asan, linux-clang (with the two 60 s fuzz runs), windows-msvc (with the ME-157 step),
    macos-clang, determinism.

- Runners: the self-hosted `[self-hosted, lj4evr]` jobs ran on the LAN host's runners (`runs-on` unchanged, Part B
  moves them). Attempt 1 of this run, and of 39907695969 before it, failed in linux-gcc-asan and linux-clang after
  their ctest passed (118 of 118 each): `actions/upload-artifact` timed out (`CreateArtifact`, `EAI_AGAIN`) and the
  fuzz step's `apt-get` could not fetch its index (exit 100), the container network on that host. `gh run rerun
  --failed` went green both times. The hosted runners of Part B (ME-159) remove this.
- Item 9 (ME-140): linux-gcc-asan `100% tests passed, 0 tests failed out of 118` with `-fno-sanitize-recover=all` in
  the CMake sanitizer flags, after ME-132. The fuzz step does not take the flag: in its `CMAKE_C_FLAGS` it cut
  `fuzz_gs2` to `INITED cov: 126` where the same build without it starts at 680 (A/B in one job, run 39908906076;
  the first run with the flag ended `DONE cov: 137` / `334` against 708 / 1466 before). It now runs with
  `UBSAN_OPTIONS=halt_on_error=1`, which makes a UBSan report fatal at run time (`docs/ci.md`, W4 fix pass A r1).
  Deviation from the verdict's wording, same effect, recorded here.
- Fuzz (ME-9): `fuzz_gs2` INITED cov 680, DONE cov 708 (483,438 runs); `fuzz_udf` INITED cov 1217, DONE cov 1468,
  143 corpus entries (6,450 runs).
- determinism: the three `dump-frames` files equal, sha256 `5b13f419...63114b50`; `replay-frames.bin` linux = windows
  = macos = `c46fee4ceff60245c9b504a6d7b2028fca0d0ba6008e1f541af7c8279545039d` (ME-128 re-record).
- Item 23 (ME-157), windows-msvc, the four records:

      ME-157 account: runnervmdaln5\ljstd6572 (S-1-5-21-3776380004-1005456608-3524014338-1003)
      1. whoami /groups as that account: Everyone, BUILTIN\Users (S-1-5-32-545), INTERACTIVE, Authenticated Users ...,
         Medium Mandatory Level; OK: no S-1-5-32-544 (Administrators) in the token
      installer /S as that account: exit code 0
      profile C:\Users\ljstd6572, the account's %LOCALAPPDATA% = C:\Users\ljstd6572\AppData\Local
      3. OK: HKU\S-1-5-21-3776380004-1005456608-3524014338-1003\Software\Microsoft\Windows\CurrentVersion\Uninstall\LJ4EVR
         UninstallString="C:\Users\ljstd6572\AppData\Local\Programs\LJ4EVR\Uninstall.exe"; no LJ4EVR key under HKLM
      2. OK: C:\Users\ljstd6572\AppData\Local\Programs\LJ4EVR = the account's %LOCALAPPDATA%\Programs\LJ4EVR
         (help, lj4evr.exe, THIRD_PARTY_NOTICES.txt, Uninstall.exe)
      4. DisplayVersion=0.3.0, VERSION=0.3.0: OK

  The first attempt (`575077a`) started the installer with the runner's environment block, so NSIS saw an empty
  `$LOCALAPPDATA` and installed to `\Programs\LJ4EVR`; `-UseNewEnvironment` gives the account its own block, as a
  real logon does. ME-45 is ticked by this record (the verdict's re-ruling).
- Item 18 (ME-149): (1) the run above; (2) `docs/ci.md` rule 1 rewritten; (3) is Part B, right after the flip.

## Fix pass A r2

Session of 2026-10-23 on the work PC (MSVC 19.44, Ninja + cl). Input: `delegations/W4_PARTA_VERIFY_r1.md`, the independent
verify of round 1 against `origin/main` = `9b9cff7`: all 23 Part A items hold on their own *Verify* lines; the gate failed
only because the latest `ci.yml` run (39910554688, the CI-record commit) was red on two Linux jobs, both after ctest had
passed 118 of 118: linux-gcc-asan in `actions/upload-artifact` (`getaddrinfo EAI_AGAIN`), linux-clang in the fuzz step's
`apt-get` (`deb.debian.org` unresolved), determinism skipped. That is the self-hosted runners' container network, not the
code. No item was failing, so no code, golden or design.md change in this round: no golden re-rendered, no REVIEWED row
added, design.md untouched (the r1 revision line at `design.md:1024` stands). No tag, no `VERSION` bump, no visibility
change; `runs-on` unchanged (Part B, ME-159).

Wording fixes the verifier offered (non-blocking), applied to this log only:

- Item 15 (ME-146): the r1 cell quoted the old file name in full, so the verify grep counted that quote. Reworded; the
  count is now 0 with no exception.
- Item 6 (ME-137): the r1 cell's whole-tree claim (0 files for the three credit names) was wrong: `docs/fidelity.md` quotes
  one in the ME-137 fidelity row, and this log quotes the grep. The verdict's own verify, `-- src tests`, holds (below).
- Items 6 and 20: 144 or 145 caption forms is the TLaserWarningForm race already recorded under acceptance item 1; the
  Status count is 92 in the gate table and 96 over both Status tables of `w4-gates.md`, 0 blank either way.

### Builds and tests (incremental rebuild of the r1 trees at `9b9cff7`)

    build-w4 (Release, SDL)                        0 warnings, 0 errors   ctest 100% passed, 0 failed out of 119
    build-w4-nosdl (Release, LJ_SDL=OFF)           0 warnings, 0 errors   ctest 100% passed, 0 failed out of 118
    build-debug-win (Debug, SDL)                   0 warnings, 0 errors   ctest 100% passed, 0 failed out of 119
    caption_diff (w4_captions.lj4s, U:W4Captions, LJ4EVR_USERDATA = a scratch folder)
                                                   rc 0, 0 ME-16 lines; 145 implemented form(s) of 207, 0 difference(s),
                                                   139 excluded by a fidelity row, 84 absent at runtime

### Part A, item by item (r1 verifier's result; the cheap greps re-run here)

| # | ME | r2 | Verify |
|---|---|---|---|
| 1 | ME-132 | holds | verifier: `test_engine_w2` address guards green on Release and MSVC ASan; red (`heap-buffer-overflow`, `lj_post.c:182`) with the three pre-fix engine files. ctest green here. |
| 2 | ME-133 | holds | verifier: `test_uiscript_w3` ASan-clean; red at `9c21031` (`lj_post_s3_masters`). ctest green here. |
| 3 | ME-134 | holds | verifier: 40668 / 50856 / 58368 / 50884 recomputed independently; `replay-frames.bin` c46fee4c on every tree and OS. |
| 4 | ME-135 | holds | verifier: laser warning 3,283 sign pixels, About logo 6,722; 159 changed goldens = 159 REVIEWED rows. |
| 5 | ME-136 | holds | verifier: Start Up `Ok` / `Cancel` faces viewed; `TSTARTUPFORM` one REVIEWED row. |
| 6 | ME-137 | holds | here: `git grep -c <credit-name> -- src tests` rc 1 (0); `grep -c -a <credit-word>` build-w4 and build-debug-win `lj4evr.exe` 0 / 0; caption_diff 0 differences (above). |
| 7 | ME-138 | holds | verifier: `fidelity_check` 73 of 73, 0 findings; selftest passed. CTest rows green here. |
| 8 | ME-139 | holds | verifier: `test_restore_data` forced-rename line. ctest green here. |
| 9 | ME-140 | holds | verifier: `-fno-sanitize-recover=all` at `CMakeLists.txt:37`; linux-gcc-asan green on 39909521611. CI below. |
| 10 | ME-141 | holds | verifier: two planted noop Help bindings give 2 findings, the tree 0; Fades > Help PASS. |
| 11 | ME-142 | holds | verifier: `assert_prompt "Clear All Inverted Channels ?"` green on three trees. |
| 12 | ME-143 | holds | verifier: bump released after a page change runs `BUMP_SUB_FLASH_1` edge 0. |
| 13 | ME-144 | holds | verifier: `Overruns: 1`. |
| 14 | ME-145 | holds | verifier: Cue Builder and Select Bitmap faces viewed non-blank; four goldens with REVIEWED rows. |
| 15 | ME-146 | holds | here: `git grep -c` of the old file name = 0 (rc 1) after the rewording above. |
| 16 | ME-147 | holds | here: the AllowExecute sentence: `SECURITY.md:1`, `docs/running.md:1` (and this log's r1 quote). |
| 17 | ME-148 | holds | here: the four-pattern `git grep -c -E` rc 1 (0); `git grep -i -c -E "Users[/\]J0"` rc 1 (0). verifier: pixel scan 0 of 862 at HEAD, 9 at `9c21031`. |
| 18 | ME-149 (1),(2) | holds | (2) `docs/ci.md` rule 1 as r1 left it (verifier read lines 44-49). (1) CI below. |
| 19 | ME-150 | holds | verifier: the three records present. |
| 20 | ME-151 | holds | here: `grep -c ME-21 release.yml` 0; `README.md:22,28` `CMake >= 3.25`. verifier: 0 blank Status rows. |
| 21 | ME-152 | holds | verifier: the ten per-second lines and the 441-frame save line quoted above; ME-24 ticked. Not re-run (LAN multicast). |
| 22 | ME-153 | holds | here, vault `design.md`: `scaled in microseconds` 2, `UI-side pin` 2, `arialblack18` 6, `movementmacro.gif` 2, `532 bytes` 2; revision line `design.md:1024` unchanged, byte-equal to the r1 quote. |
| 23 | ME-157 | holds | verifier: the four records in the windows-msvc log of 39909521611 (no Administrators SID, `%LOCALAPPDATA%\Programs\LJ4EVR`, HKU key and no HKLM key, `DisplayVersion=0.3.0`). |

### CI (`ci.yml`, run 39912230811, head `f96982a` = "W4: fix pass A r2")

Attempt 1: five jobs green; windows-msvc red on `test_lock_hold` alone (`FAIL lock hold 31567 us >= 2000 us`, 118 of 119
passed), the W1 wall-clock row on a shared hosted VM (the same row and cause as the W3 and W4 first attempts above);
determinism skipped. `gh run rerun 39912230811 --failed`. Attempt 2: **success, six of six**:

    generate-check   success   self-hosted lj4evr-<runner-host>-3
    linux-gcc-asan   success   self-hosted lj4evr-<runner-host>-1   (-fno-sanitize-recover=all, ME-140)
    linux-clang      success   self-hosted lj4evr-<runner-host>-2
    windows-msvc     success   GitHub-hosted                 (ME-157 standard-user install step included)
    macos-clang      success   GitHub-hosted
    determinism      success   self-hosted lj4evr-<runner-host>-3
        frames-{linux,windows,macos}.bin   1,641,600 bytes each, sha256 5b13f41930362b75a7aa6f9c53f77db2d315a4039ea5a18da664ea1263114b50 (cmp equal)
        replay-frames.bin linux = windows = macos   c46fee4ceff60245c9b504a6d7b2028fca0d0ba6008e1f541af7c8279545039d

The Linux jobs ran on the interim self-hosted runners, as the verdict allows while atlas-ci2 is down; ME-159 (Part B)
moves them to hosted runners. With this run every Part A condition holds: items 1-23 on their *Verify* lines and a
six-green `ci.yml` run on main.

## Part B (release, Joey's rulings of 2026-10-22)

Rulings: Q1 ME-154 = (a) rewrite history with `git filter-repo`; Q2 ME-155 = MIT; Q3 ME-156 = (b) the
`LJ4EVR (User lib: %s)` caption fallback, descriptive mentions kept with the non-affiliation line, the DFM-only About
golden's forum line blanked; Q4 ME-159 = the default (Linux jobs on GitHub-hosted `ubuntu-latest`, same image digest,
self-hosted runners leave the repository); Q5 ME-158 = no hand run of the `.dmg`, v1.0.0 ships with the `Native=1`
recommendation and the known-gap line, the hand run stays a W5 entry gate.

### REL-HISTORY: the rewrite (ME-154, Part B step 1, step 7 record)

1. Backup: `git clone --mirror` of the remote before the rewrite into a local-only mirror (never pushed, not
   published), 4,027 objects. Refs at the backup: `main` = `472e86a`; tags (tag object -> commit) `v0.0.1`
   `05079a18` -> `de7da168`, `v0.1.0` `fd09eb72` -> `74b5eb50`, `v0.2.0` `1053b855` -> `110e5689`, `v0.3.0`
   `512fa433` -> `3f5faef6`. No other refs (no pull-request refs, no stash).
2. Rewrite: `git-filter-repo` 2.47.0 (in the tooling venv) driven as a library over a fresh mirror, with
   `--replace-text` and `--replace-message` (the REL-PRIVATE strings of the ME-148 table, each replaced by its
   placeholder) and a blob callback. Its output: `{'lj_forms_gen.c blobs stripped': 6, 'image blobs replaced': 13}`:
   Martin's About credits array removed from every `src/ui/forms/lj_forms_gen.c` version, and the `TABOUTFORM2`
   golden and the captures that showed the credits or the Windows user name replaced in every commit by their
   fix-pass-A re-renders and re-shoots.
3. Result: 74 commits mapped; 72 get new ids, the 2 pre-W0 commits are unchanged (`cd01346`, `a7b573d`); the 4
   annotated tags rewritten. New `main` = `864a2ce`; tags `v0.0.1` `49a22863` -> `243737e0`, `v0.1.0` `2c27eeb0` ->
   `e2e26ef3`, `v0.2.0` `d012eb99` -> `b015fe22`, `v0.3.0` `aa74fd25` -> `8df5bdd3`. The commit map (every old and new
   id, the branch and the four tag objects) is in the design vault, `delegations/W4_HISTORY_COMMIT_MAP.tsv` (80 rows:
   74 commits, 1 branch, 4 tag objects); commit ids cited in this log before this section are the old ids.
4. Checks before the push: the rewritten tip builds (MSVC, Ninja) and `ctest` passes 119 of 119; its tree differs from
   the old tip's in 3 blobs only (`docs/fidelity.md`, `docs/w4-integration.md` and the
   `hardware-setup-THardwareSetupForm2.png` capture: the strings and pixels the rewrite replaced).
5. Push (2026-10-23, run by the orchestrator): one atomic `git push --force-with-lease` of `main` and the four tags,
   each lease on the backup's value, with the CI and Release workflows disabled for the window and re-enabled after.
   The four prereleases keep their releases on the rewritten tags; all 12 of their assets are deleted, and each one's
   notes end with `Installers withdrawn 2026-10-22 (history cleanup); use v1.0.0 or later.` The working clone was
   reset to `origin/main`.
6. Verified by the release engineer, 2026-10-23:

       git ls-remote origin
         864a2cea56fd096128b38979ceccc27c9d5bebfb  HEAD = refs/heads/main
         49a22863f203cdbaae809687397f219c81e31d2d  refs/tags/v0.0.1  ^{} 243737e01612ab6104be2b7f220a4f2e688a453e
         2c27eeb0b09704535db9fbe9e92ada9622d93de7  refs/tags/v0.1.0  ^{} e2e26ef3116873799ec09e34d7ba6da62ea0a20f
         d012eb99d44c74290e6a0c568537c5bf5499c521  refs/tags/v0.2.0  ^{} b015fe22105fab64922b74954c1665589a6c7103
         aa74fd2525dfba9d08fa19be5a17a5feeb157f14  refs/tags/v0.3.0  ^{} 8df5bdd3ecf77a962b47adaac974576065ab970d
       gh release view v0.0.1 .. v0.3.0     Pre-release, 0 assets each, notes end with the withdrawn line
       git rev-list --objects --all         4,006 objects: 74 commits, 681 trees, 3,247 blobs, 4 tags
       git log --all -G <pattern>, over every blob of every commit, commits matching:
         the three credit names / words                                   0
         the five REL-PRIVATE patterns of the ME-148 verify line          0
         the Windows user name (two patterns)                             0
         "Dropbox"                                                        1: the ME-148 table's words "the Dropbox path", no path
       git log --all --grep, the same patterns in commit messages         0
       author, committer and tagger e-mail                                the GitHub noreply address only

   **REL-HISTORY met**: no Martin authored text, no private detail and no secret in any commit or tag; the eight
   prerelease installers that carried the credits are withdrawn.

### Part B steps 2-3: ME-155, ME-156, ME-159, version 1.0.0

- ME-155 (MIT): the "PROPOSAL until the maintainer's ruling" licence wording is gone from `NOTICE` (so from
  `THIRD_PARTY_NOTICES.txt`), `README.md`, `CONTRIBUTING.md`, `docs/provenance.md`, `docs/releasing.md` and the
  `LJ_ABOUT_LICENCE` comment; `gh repo view --json licenseInfo` = `mit`. The CPack licence page is `LICENSE` (MIT)
  and `MACOSX_BUNDLE_COPYRIGHT` already said MIT; neither carried PROPOSAL.
- ME-156: `LJ_MAIN_CAPTION_FMT` = `"LJ4EVR (User lib: %s)"` (`src/desktop/lj_desktop.h`, still the only caption
  literal); the four script / test assertions and `docs/running.md` follow. `tools/dfm2c.py` `CAPTIONS_BLANKED` emits
  TAboutForm2 `Label3` / `Label4` (the forum line) with an empty caption; `dfm2c.py --check` OK. Goldens re-rendered
  with REVIEWED rows: `TABOUTFORM2` (957 px, the forum line only) and `MAINWINDOW` (855 px, the caption text only).
  The three main-window captures of the entry walkthrough (`main`, `close-prompt`, `tb-cuelist-7`) re-shot; F1's
  fidelity row names the ruling. The non-affiliation line is in About (`LJ_ABOUT_TRADEMARK` and the memo) and README.
  `README.md` had one CP1252 section sign (invalid UTF-8 on GitHub); now UTF-8.
- ME-159: every `runs-on: [self-hosted, lj4evr]` in `ci.yml` (four jobs) and `docker.yml` is `ubuntu-latest`; the
  image digest and the `pull_request` guards are unchanged; `.github/actionlint.yaml` (only the label) removed;
  `docs/ci.md` describes hosted runners. The self-hosted runners are not deregistered here (the flip step).
- `VERSION` 1.0.0; `CHANGELOG.md [1.0.0]` in user terms (what 1.0.0 is, design 1's "what it is not", the known gaps
  with the ME-158 macOS line); README status 1.0.0. `release.yml` now appends the version's CHANGELOG section (up to
  its `Developer detail:` line) to the notes, fails without one, and passes `--latest` for a non-0.x version.

Local checks (work PC, MSVC 19.44, Ninja + cl, incremental):

    build-w4, build-w4-nosdl, build-debug-win    0 warnings, 0 errors
    ctest before the golden update               ui_golden (TABOUTFORM2) and test_desktop (MAINWINDOW) red, the rest green
    ctest after                                  ui_golden, test_desktop, fidelity_check(+selftest), help_check, forms_tables green on all three
    w4_entry_points.lj4s (build-w4, scratch LJ4EVR_USERDATA, prep + a W4Captions row)   rc 0, 155 PASS, 0 SCRIPT FAIL
    caption_diff (w4_captions.lj4s, scratch userdata)   145 implemented form(s) of 207, 0 difference(s), 139 excluded, 84 absent

CI (`ci.yml`, run 39924948793, head `7dce143` = "Release 1.0.0"), attempt 1: **success, six of six, every job on a
GitHub-hosted runner** (ME-159; the GHCR image pulled on `ubuntu-latest` with the job token, no package change needed):

    generate-check   success   ubuntu-latest (container, digest unchanged)
    linux-gcc-asan   success   ubuntu-latest   100% tests passed, 0 tests failed out of 118
    linux-clang      success   ubuntu-latest   118 of 118; fuzz_gs2 INITED cov 680, DONE cov 708; fuzz_udf INITED cov 1217, DONE cov 1464
    windows-msvc     success   windows-latest  119 of 119 (Release and Debug); ME-157 step: no Administrators SID, DisplayVersion=1.0.0, VERSION=1.0.0
    macos-clang      success   macos-14
    determinism      success   ubuntu-latest
        frames-{linux,windows,macos}.bin   1,641,600 bytes each, sha256 5b13f41930362b75a7aa6f9c53f77db2d315a4039ea5a18da664ea1263114b50
        replay-frames.bin linux = windows = macos   c46fee4ceff60245c9b504a6d7b2028fca0d0ba6008e1f541af7c8279545039d

`docker.yml` (run 39924948791, triggered by its own edit) built, smoke-tested and pushed the image on `ubuntu-latest`:
success. `ci.yml` stays pinned to the earlier digest. The `ci.yml` run on the `v1.0.0` tag (39925711243) is also six
of six green.

## Release

### v1.0.0 (Part B step 3, ME-36)

`git tag -a v1.0.0 -m "LJ4EVR 1.0.0"` on `7dce143`, pushed 2026-10-23. `release.yml` run 39925711181: check, windows,
macos, release all success.

    gh release view v1.0.0      "LJ4EVR 1.0.0", isPrerelease false, isDraft false, published 2026-10-23T08:39:41Z;
                                repos/.../releases/latest = v1.0.0
    assets                      LJ4EVR-1.0.0-win64-setup.exe   1,864,168 bytes
                                LJ4EVR-1.0.0-macos.dmg         4,940,435 bytes
                                SHA256SUMS                       184 bytes
    SHA256SUMS                  52d0697ba7d216c623696db881d2d9171f09a369d94e77c3b881ff3df4123f0d  LJ4EVR-1.0.0-win64-setup.exe
                                34db2a0d2ee1473a92d6a88d0ce7c57ac6a93d6c05e4d7456affe783fdb52d68  LJ4EVR-1.0.0-macos.dmg
                                (downloaded, sha256sum -c: both OK)
    notes                       the install / notices / trademark header, then CHANGELOG [1.0.0] up to "Developer detail:"
                                (what 1.0.0 is, what it is not, the known gaps incl. the macOS line of ME-158)

Windows installed tree (the downloaded release asset, work PC, silent per-user install into a scratch folder:
`LJ4EVR-1.0.0-win64-setup.exe /S /D=<scratch>\inst`, exit 0):

    lj4evr.exe                11,665,920
    THIRD_PARTY_NOTICES.txt       15,486   ("released under the MIT licence (LICENSE)", 0 PROPOSAL)
    Uninstall.exe                111,910
    help\  controls cuelists cues desktop files fixtures getting-started hotkeys-midi index macros
           not-available notices output sequences statics-bgcues  (.txt, 15 topics)
    HKCU ...\Uninstall\LJ4EVR   DisplayVersion=1.0.0, UninstallString = <scratch>\inst\Uninstall.exe; no HKLM key
    help_check.py --install-tree         install_check: ...\THIRD_PARTY_NOTICES.txt ok (rc 0)
    lj4evr.exe --version                 LJ4EVR 1.0.0 (rc 0)
    grep -c -a in the installed exe      the credit word 0; the credit surname 0; the forum URL 0;
                                         "LJ4EVR (User lib: %s)" 1; "Martin LightJockey (User lib" 0
    w4_entry_points.lj4s against the installed exe (scratch LJ4EVR_USERDATA prepared by w4_entry_prep.sh plus a
      W4Captions row; shots to a scratch folder)   rc 0, 155 PASS lines, 0 SCRIPT FAIL
    Uninstall.exe /S                     exit 0; the folder, lj4evr.exe and the HKCU key gone

macOS (from the `release.yml` macos job log; no hand run, ME-158):

    lipo        Architectures in the fat file: ... lj4evr are: x86_64 arm64
    codesign    Format=app bundle with Mach-O universal (x86_64 arm64), Signature=adhoc; --verify --deep --strict:
                valid on disk, satisfies its Designated Requirement
    Info.plist  CFBundleName LJ4EVR, CFBundleShortVersionString 1.0.0, CFBundleVersion 1.0.0,
                LSMinimumSystemVersion 12.0, NSHumanReadableCopyright "MIT, 2026 LJ4EVR contributors"; minos 12.0 (both slices)
    .dmg        hdiutil verify: every partition verified; mounted:
                  Applications -> /Applications
                  LJ4EVR.app
                install_check: .../LJ4EVR.app/Contents/Resources/THIRD_PARTY_NOTICES.txt ok
                (the bundle's Resources: THIRD_PARTY_NOTICES.txt, lj4evr.icns, help/ with the same 15 topics)

ME-36 ticked by this record. Not done in this step: the public flip, the runner deregistration (ME-159's second half)
and ME-149 (3) (Part B step 4).

## Public flip (2026-10-23)

- The repository was recreated before the flip so that no pre-rewrite object stays fetchable by SHA (GitHub keeps force-pushed commits reachable until garbage collection). The rewritten main, the tags v0.0.1..v1.0.0 and the five releases were restored; v1.0.0 carries the installers tested in the Release section, sha256 unchanged. Pre-rewrite SHAs return 404.
- The CI image is now `ghcr.io/unbannablegt/lj4evr-build` (built by docker.yml at bc2fbf8); ci.yml run 39969826715 on 9af6d09: six of six jobs green on GitHub-hosted runners. No self-hosted runner is registered (ME-159).
- `gh repo view --json visibility`: PUBLIC at 2026-10-23T14:37:05Z.
- Fork-PR workflow approval: `all_external_contributors` (read back). Private vulnerability reporting: enabled (read back). Topics: lighting, dmx, sacn, lightjockey, show-control, c11, sdl3.
- Anonymous checks: repository page 200; LJ4EVR-1.0.0-win64-setup.exe 200 (1,864,168 B); LJ4EVR-1.0.0-macos.dmg 200 (4,940,435 B).
