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
with gcc 14 `format-truncation` at `lj_config_dialogs.c` `rg_save` (`"Entry%d"` into `key[16]`). The mingw builds of
gcc 14.2 and gcc 16.2 do not reproduce it, because mingw's `snprintf` is an inline wrapper, not the builtin. So every
`snprintf` with integer conversions in a file changed since v0.3.0 was scanned. The scan computes the worst-case
length (11 per `%d`, 10 per `%u`, 8 per `%X`, width as given) against the declared buffer, and 41 buffers that could
not hold it were widened (to the next multiple of 8 above the worst case). No text changes, so no golden moves.
Four trees 0 warnings, ctest unchanged, and the clang sweep is clean.
