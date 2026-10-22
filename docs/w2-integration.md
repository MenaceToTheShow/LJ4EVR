# W2 integration log

Integrator 1 session, 2026-10-22, work PC (MSVC 19.44, CMake 4.1, VS 17 2022 x64, Ninja), plus gcc 13.3 on
atlas-ci2 (native, not the CI container) for the Linux warning set. Commands and outputs recorded as run.
Every blocker a track returned is resolved or recorded below. design.md wins on disagreement, and each fix
is the smallest change that works.

## 1. Pre-integration commit

    git status            -> 175 paths (eleven tracks); reference/ and build-*/ ignored
    git commit "W2: tracks landed (pre-integration)"   -> f66d76c, pushed (6550a77..f66d76c)
    CI run 39795267241    -> linux-gcc-asan and linux-clang red (section 3, first two rows); macOS green

## 2. Full-tree build

    cmake -S . -B build-int-nosdl -G Ninja -DLJ_SDL=OFF -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=cl   (vcvars64)
    cmake --preset dev-ninja-msvc && cmake --build --preset dev-ninja-msvc     (LJ_SDL=ON)
    cmake --preset debug-win      && cmake --build --preset debug-win          (asserts on)
    cmake --preset dev-msvc       && cmake --build build-msvc --config Release && --config Debug

All five built with 0 warnings and 0 errors on the landed tree. All five ctest runs passed 78 of 78. The tracks
had already fixed the in-flight errors they reported against each other: the lj_toolbars.c C2220,
lj_main_window.c C4996, lj_global_patch.c C4701, test_windows_c.c syntax and the unresolved
lj_test_profiles_w2 were all gone by the time the tree was committed.

## 3. Resolutions

| Blocker (track) | Resolution |
|---|---|
| CI linux-clang: `lj4evr_render` link fails, `lj_desktop.c` undefined `lj_win_open` / `lj_win_toggle` | desktop, controls and windows are siblings that call each other. GNU ld reads archives once, in order. `CMakeLists.txt` now links the three as a static cycle (`target_link_libraries` both ways), which CMake repeats on the link line |
| CI linux-gcc-asan: `-Werror=format-truncation` in `lj_ctl_rgbpack.c:142`, `lj_global_patch.c:108`, plus `lj_statics2_form.c:154/351` and `lj_user_palettes.c:175` (found with gcc on atlas-ci2) | buffers sized for the full `%d` range: `cap[16]`, `num[12]`, `t[12]`, `numtxt[][12]`, `tabtxt[][16]`, `no[12]` |
| ME-30 storage (model): `lj_static_page.slot[20]`, stride 20, so slots 21..64 had nowhere to live | `LJ_STATICS2_SLOTS_MAX` 64 moved to `model/lj_model.h` (it was an enum in `lj_statics.h`). `slot[LJ_STATICS2_SLOTS_MAX]`. The global index stride is now 64 (`lj_static_global_index`, `lj_static_scope_hits`, the engine scope / Release Page arithmetic, `LJ_ACT_STATIC_CLEAR`, the replay harness). `Statics.dat` slot indices are 1..64. `LJ_STATICS2_SLOTS_PER_PAGE` (20) stays the default `n` and the Static 01..20 hotkey count (design R8). The Statics 2 form shows up to 64 rows. Test rows moved to the new stride (page 2 slot 1 = 64); `slot 65` is the new reject row. The `WIN_STATICS2_FORM` golden at the default 20 is unchanged |
| ME-30 Main.Ini half (model -> app) | already in the app track: `lj_app_seam.c` reads `[STATICS2] SlotsVisible`, clamped 1..64, into `ctx->static_visible` |
| ME-77 `.Hot` (model -> midi): `sscanf %u`, no range on `num` | `io/lj_hotkeys.c` reads every number through `lj_kv_range` / `lj_kv_range_lead`: vk 0..65535, shift/ctrl/scrl 0..1, num 0..99999999, enabled/dialogs 0..1. `test_hotkey_setups` gains 5 reject rows (vk 65537, shift 2, num -1, num 2^32+1, enabled 256) and one accept row |
| Grand Master handler for `LJ_FN_MASTER_LEVEL` (midi -> windows) | `lj_master_intensity.c` `fn_master`: `lj_master_intensity_set(0, (min(prog,100) * 255 + 50) / 100)`, bound with the other master rows. `test_windows_b` row: prog 50 posts SET_MASTER 128 |
| Edit Hotkeys / MIDI-IN skip `LJ_FNF_EXT` (midi -> windows-d) | already done by windows-d (`lj_edit_hotkeys.c:54`, `lj_midi_in_form.c:284`) |
| Hotkey setups under the library, `Setup1.Hot` start-up line (midi -> app) | already done by app (`lj_app_seam.c` `lj_hotkeys_setup_load(<library root>, n)`). Debug `--selftest` prints no `cannot open .../Setup1.Hot` |
| `lj_library_create` has no `Hotkeys` folder (midi -> model) | already in `lj_lib_folders` |
| `LJ_FN_PROG_SEQ_FLASH` must carry the latch (windows-a -> app) | already done by app: `cmd.c = c->flash_latch` |
| "Release Flash on Load" (windows-a -> app) | `lj_app_load_cue` posts `LJ_CMD_FLASH` b 0 c 0 in the same critical section when forms.ini `[CUE] Flash Auto Release` is set. `test_uiscript_w2` section 6 runs a latched flash across a cue load with the box clear (survives) and set (released) |
| `test_uiscript_w2` heap corruption once a cue is loaded through `lj_app_load_cue` (found by the row above) | `lj_show_free` freed `current_cue` and then the same pointer again from the `cues` table. The app shuts down this way after any cue load, in W1 too. `lj_show_free` now drops a current record that is also its table's entry (seq, cue, cuelist) before freeing the tables |
| `LJ_ACT_LOAD_BGCUE` arg 0 = clear (windows-b -> app) | already done by app (`lj_app_bgcue_clear`) |
| `STATIC_DEACTIVATE` Release Page `c` = page (windows-b -> engine) | already done by engine (`lj_engine.c`, `!c->c ||` page compare) |
| BGCUE_FORM owner (windows-b vs contracts windows-a) | one owner: windows-b's `lj_bgcue_form.c` and its single `lj_win_table` row. windows-a never wrote one. The four window-toggle fns bind in `lj_desktop.c` only when unbound |
| `LJ_ACT_GLOBAL_PATCH / FIXTURE_GROUPS / DEFAULT_OUT / SNAPSHOT` bindings, real-time snapshot at 20/s (windows-c -> app) | already done by app (`lj_app_seam.c` action table; `lj_snapshot_tick(now_us)` in the idle pass) |
| `lj_actions.h`: ten `LJ_FN_STATIC_11..20` rows on one physical line (windows-c) | split one row per line. The token stream is unchanged (the multi-line comment inside the macro is legal C: comments become one space before directives are processed) |
| `tools/glyphmap.tsv` rows for TBGCUEFORM, TGLOBALPATCHFORM (windows-b, windows-c) | appended: sbNew new, sbSave save, sbLoad load, sbDelete delete; bOk check, bLocatePrev up, bLocateNext down (globalpatch.gif: green up/down arrows, tick on Apply Patch); TPRESETFORM bSave save, bClear delete. `dfm2c.py` regenerated, `--check: OK`. iVideo (TSNAPSHOTSCENEFORM) and bAssign (TPRESETFORM) stay without art |
| RGB pack menu reads UserPalette, the GIF reads Palette (windows-a -> fixlib) | `lj_ctl_menu_rgb[] = Palette Preferences Fades Help` in `fixlib/hand/lj_hand.c`, used by the generic RGB pack windows (`lj_generics.c`) |
| `CTL_RGB_*` golden belongs in `ui_golden_ctl` (windows-a -> integrator, LORD_FIDELITY-18) | left in `test_windows_a_w2` as `W2A_CTL_RGB10_RGBPACK`. A second copy of the same bytes in another row tests nothing more |
| `lj_win_classes[LJ_WIN_USER_PALETTES]` "TUserPaletteForm" vs the TPresetForm instance (windows-c) | kept. `lj_win_find("TPresetForm")` must keep resolving Position Presets, and `user_palettes` resolves the palette window |
| User palette list: the Name column never drew (found reviewing the new golden) | `lj_w_listview.c`: a DFM column `Width` of -1/-2 (LVSCW_AUTOSIZE / _USEHEADER) now sizes like `AutoSize` (the last column fills); it was a negative width. 13 DFM columns carry -2; only the palette golden changed |
| W1 Default library cue fails to load in W2: `item 8: number out of range` (found opening every W2 window in the Debug app) | `lj_cue_form_add_sequence` memset the slot, so v0.1.0 saved `spt=0 atpt=0`, which ME-77 now refuses. The form calls the model's `lj_cue_add_sequence` (defaults 1/1). The loader reads 0 as 1 for v0.1.0 files (`test_model_w2_records` row); 257 still fails |
| replay `loop_bounce_random_limits:76/:78` (replay -> engine) | fixed by the engine track before integration (`ended` bit; 03 s5.5 Goto "jump ... and stop", F14 hold). The scenario keeps 40 |
| brief "bg below statics" vs design 3.4 (replay) | design wins: rank 5 (BG cue) above rank 4 (statics), 03 s8, 11 C5. `bgcue_over_statics` asserts that |
| ME-76 `exe_profiles_check` in `generate-check` (profiles -> integrator) | `ci.yml` loop reads `qxf2c fontpack artpack gen_vectors exe_profiles_check`; `docs/ci.md` sentence retired |
| ME-78 replay-frames.bin across the OSes (replay -> integrator) | each of linux-gcc-asan, windows-msvc, macos-clang publishes `sha256(replay-frames.bin)` as job output `replay_sha`; `determinism` prints the three and fails unless equal. `docs/ci.md` W2 section |
| profiles ids 33 36 41 118 PROPOSAL footprints; gcc/clang unseen (profiles) | recorded open (section 7). gcc 13 -O0 ASan on atlas-ci2 compiles and runs the tree clean (section 4) |
| Model preset names (windows-c) | recorded open (section 7) |

## 4. Tests

    ctest (build-int-nosdl, build-ninja-msvc, build-debug-win, build-msvc Release, build-msvc Debug):
      100% tests passed, 0 tests failed out of 78        (each; profiles_ids enabled, tiers 1-5)
    lj4evr --selftest (build-debug-win, build-msvc\Debug): rc 0; no "no binding for handler", no "unknown event",
      no "cannot open ...Setup1.Hot"
    Debug app, --script opening and closing every W2 window id (27 ids; position_presets needs a master with a
      Movement control): rc 0, no ME-16 assert
    atlas-ci2, gcc 13.3 -std=c11 -Wall -Wextra -Wpedantic -Wshadow -Wvla -Werror -O0 -fsanitize=address,undefined,
      every SDL-free .c + tests + the app's SDL-free files, then every lj4evr_tests CTest row:
      COMPILE_FAIL=0  LINK_RC=0  LINUX_FAILS: (none)
    generators: qxf2c 0 findings, fontpack current, artpack current, gen_vectors up to date,
      exe_profiles_check 201 rows / 66 hand ids / 45 map ids / 0 findings, dfm2c --check OK
    font_corpus: 826 rows, 34 skipped (face not packed), 0 mismatches; widthcorpus regenerated into scratch = byte-identical
    test_layering: Passed (two MIDI OS_ALLOW rows present: io/lj_midi_winmm.c, io/lj_midi_coremidi.c)

Replay (`lj4evr_tests replay`, 24 globbed + the wire row), every scenario 0 fails:

| scenario | ticks | expects | | scenario | ticks | expects |
|---|---|---|---|---|---|---|
| seq_intensities (W2) | 5 | 10 | | blackout_after_masters | 5 | 8 |
| transparent_cue_load (W2) | 13 | 21 | | cue_priority_slot12 | 4 | 7 |
| statics_ltp_reorder (W2) | 13 | 18 | | disabled_offset_silent | 1 | 3 |
| layer_flash_current_statics (W2) | 12 | 16 | | drift_600s_two_slots | 24001 | 33 |
| bgcue_over_statics (W2) | 10 | 19 | | fade_060ms | 11 | 7 |
| chase_030ms (W2) | 57 | 35 | | fade_linear_40hz | 121 | 10 |
| cue_time_scaling (W2) | 2401 | 30 | | freeze_pauses_clock | 531 | 20 |
| loop_bounce_random_limits (W2) | 169 | 48 | | gm_examples_1_to_6 | 12 | 48 |
| link_to_cue (W2) | 22 | 20 | | gm_fade_under_freeze | 401 | 8 |
| wire/drift_600s_two_slots_wire (W2, own row) | 24004 | 15 | | htp_group_max | 5 | 6 |
| | | | | interrupted_fade_restart | 161 | 8 |
| | | | | off_is_transparent | 3 | 7 |
| | | | | pair16_pan_true16 | 46 | 47 |
| | | | | snap_050ms | 81 | 7 |
| | | | | stop_holds | 311 | 6 |

Rule ids arbitrated: 03 s5.5 Goto end action + F14 (engine side, `loop_bounce_random_limits`), design 3.4 rank
order with 03 s8 / 11 C5 (scenario side, `bgcue_over_statics`), LORD_ENGINE-13 KeepContentsBlind
(`layer_flash_current_statics`), `LJ_RANDOM_NO_REPEAT` PROPOSAL (`loop_bounce_random_limits`, 12 expects
depend on it).

Determinism (ME-78 local): `--dump-frames` of the 24 scenarios twice in Release and once in Debug:
`cmp` equal, 116,538,720 bytes, sha256 `776cf859abfbb8f418003a944ca7261f1fca89f8eb337a817a5af7ed2c4a5474`
(= the ctest `replay_dump` file). `dump-frames` (400 ticks) twice: equal,
sha256 `5e61478e89c1ccfd4936ce7110a366e4f497c5ac0684493bc29ef189342c6ee7`.

Ownership greps (ME-2, ME-23): `lj_table_` under `src/engine/*.c` code: none; `free(` in `src/engine`: only
`lj_engine_destroy`; file I/O in `src/engine`: none; `lj_engine_ / lj_sacn_ / lj_table_` in
`src/io/lj_midi*.c`: none; `lj_fn_run*` callers: desktop key/toolbar/statusbar handlers and `lj_midi_dispatch`,
all on the UI thread (`lj_app_midi_dispatch` drains the queue in the idle pass).

## 5. Goldens (tests/ui/golden, REVIEWED.tsv)

Every W2 golden's size checked against its DFM (outer = client + 6 wide, + 44 tall for a caption and a menu):
TLOOPTOOLSFORM 359x291 (client 353x247), TGETPAGEINFOFORM 260x98 (tool window, client 254x77), TBGCUEFORM
390x168 (384x124), TSMOKEFORM 316x257 (310x213), TSNAPSHOTSCENEFORM 381x325 (375x281), TINTENSITYGROUPFORM
331x392 (325x348); outer DFM Width/Height equal for TCUETIMEFORM 670x309, TREORDERCUEPAGESFORM 302x344,
TDELETESEQUENCESFORM 515x290, TFUNCTIONSFORM 758x469, TNOTEFORM 332x247, TAUTOLAMPFORM 289x239,
TSYSTEMPREFSFORM 380x327, TGLOBALPATCHFORM 630x454, TPRESETFORM width 210. TSTATICS2FORM is 510 wide and grows
to 450 for the default 20 rows (PROPOSAL). Read as PNG and compared with the GIFs: RGB pack (rgbcolorcontrol.gif),
Global Patch (globalpatch.gif), Background Cue (bgcuecontrol.gif), User Palette (usercolorpalette.gif), Statics 2,
Loop Options, Smoke, Snapshot, Edit Hotkeys, MIDI-IN, System Preferences, Delete Sequences, Cue Time, Intensity
Groups. None of these GIFs is 1:1, so no window size was taken from them (ME-67).

Changed or added by this pass (rows appended to REVIEWED.tsv): `W2A_CTL_RGB10_RGBPACK` (menu word),
`WIN_BGCUE_FORM`, `WIN_GLOBAL_PATCH` (glyphs), `WIN_INTENSITY_GROUPS`, `WIN_USER_PALETTE` (new).

Not taken, on purpose: About (the version label changes every release; `test_windows_d` asserts it equals
`VERSION`), User Libraries / Backup / Restore (host paths in the view; the windows-d rows assert the text).

## 6. Gates touched (docs/w2-gates.md)

ME-30 (storage + Main.Ini key; Statics Preferences form still open), ME-76, ME-77 (`.Hot`; v0.1.0 cue compat),
ME-78 (CI half; the three-OS sha is in the first green run below), ME-2 / ME-23 greps (section 4),
ME-16 (Debug selftest and the open-every-window script), LORD_FIDELITY-18 (menu caption), w1-int s7 hotkeys
(no Setup1.Hot line).

## 7. Left open (recorded, not blocking)

- ME-30: Statics Preferences (`TStatics2PrefsForm`), Static Properties and the page / slot name dialogs have no
  behaviour file (windows-b deferral). `lj_win_open` logs "has no behaviour file yet" for statics2_prefs,
  static_props, select_bgcue, save_bgcue, save_preset, hotkey_function. Visible Slots is read from Main.Ini only.
- Profiles 33, 36, 41 (Roboscans via the DMX Interface) and 118 (MAC 250 Laser): PROPOSAL footprints until a
  rig item or the interface manual.
- `lj_preset` has no name field; Position Presets lists "Preset NNN".
- A missing `Palettes\<Kind>.Pre` logs two lines when a user palette window opens (no `lj_file_exists` in core
  yet, ME-86).
- BG cue empty slots draw red where bgcuecontrol.gif draws green.

## 8. CI

    f66d76c (pre-integration)  run 39795267241: linux-clang red (lj4evr_render link), linux-gcc-asan red
                               (format-truncation), macos-clang green, generate-check green
    f8e3909 (integration)      run 39799818715: linux-gcc-asan red, gcc 14 only (lj_ctl_rgbpack.c:142
                               "output may be truncated before the last format character": "Unit %d" needs
                               5 + 11 + 1 bytes, gcc 13 had accepted 16); linux-clang, macos-clang (CoreMIDI
                               backend compiled: lj_midi_coremidi.c.o), windows-msvc (lj_midi_winmm.c.obj;
                               Release + Debug ctest 78/78, selftests) green
    96c3471 (gcc 14 buffers)   run 39801602850: green, all six jobs. determinism:
                               frames-{linux,macos,windows}.bin 1641600 bytes, sha256 5e61478e...c6ee7 (equal)
                               replay-frames.bin linux / windows / macos
                               776cf859abfbb8f418003a944ca7261f1fca89f8eb337a817a5af7ed2c4a5474 (equal; ME-78)
    the macOS log of 39799818715 showed "ld: warning: ignoring duplicate libraries" (the link cycle); the
    cycle is now Linux-only (next commit, its run below)
