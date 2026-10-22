# W3 integration log

Integrator 1 session, 2026-10-22, work PC (MSVC 19.44, CMake 4.1, VS 17 2022 x64, Ninja). Nine tracks wrote
disjoint parts against `docs/w3-contracts.md`; this pass builds the landed tree, resolves every blocker the
tracks returned (or records why it stays open), and gets ctest and CI green. design.md wins on disagreement;
each fix is the smallest change that works. Commands and outputs are recorded as run.

## 1. Pre-integration commit

    git status            -> 146 paths (nine tracks); reference/ and build-*/ ignored
    staged-file check     -> no path under reference/, no .GS2 / .Mac / .Udo / .gif staged;
                             src/engine/lj_shapes_gen.c carries the provenance banner (gen_vectors.py --shapes,
                             OUR samples, no Martin bytes); tests/fixlib/test_gs2.c builds its 0x266C-byte record by
                             hand; tests/ui/scripts/w3_synthetic.Udf (214 bytes) is our exporter's output
    git commit "W3: tracks landed (pre-integration)"  -> 0d131fa, pushed (63d1f3a..0d131fa)

## 2. Full-tree build

    cmake -S . -B build-int3-nosdl -G Ninja -DLJ_SDL=OFF -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=cl
          (-DLJ4EVR_REFERENCE_DIR=D:/lj4evr/reference for the developer-PC rows)      (vcvars64)
    cmake --preset dev-ninja-msvc && cmake --build --preset dev-ninja-msvc            (LJ_SDL=ON, Release)
    cmake --preset debug-win      && cmake --build --preset debug-win                 (LJ_SDL=ON, asserts on)
    cmake --preset dev-msvc       && cmake --build build-msvc --config Release && --config Debug

The landed tree built with 0 warnings and 0 errors under /W4 /WX in all five trees. The in-flight errors the
tracks reported against each other (`lj_engine.c` C4013 `lj_genmacro_restart`, `lj_replay.c(877)` C2220,
`lj_mac550.c` C4002, `lj_fsinfo.c` C4996 localtime, `test_profile_editor_form.c` C4129, `test_uiscript_w3.c`
C4101, the four unresolved `lj_app_w3.h` externals) were all gone by the time the tree was committed; no track's
local `/D_CRT_SECURE_NO_WARNINGS`, `/wd4129` or `LJ_WARNINGS_AS_ERRORS=OFF` is needed. First ctest on the landed
tree: 97 of 98, the one failure `test_uiscript_w3:263` (row 1 below).

## 3. Resolutions

| Blocker (track) | Resolution |
|---|---|
| `lj_show_gs2_load_all` is a stub; `test_uiscript_w3:263` "written but not listed after Reload profile data" (gs2, desktop-app) | `model/lj_show.c` body: lists `UserFixtureDef\*.GS2` through a new `lj_library_list_files` (`model/lj_library.h`, appended; the directory walk stays private to `lj_library.c`), `lj_gs2_load` per file, a bad file logged and skipped, no folder = 0 profiles (bound `LJ_GS2_LOAD_MAX` 1024, PROPOSAL) |
| `lj_app_bez_reload` stub, Bezier shapes reach the engine only at the next start (movement, desktop-app) | `lj_app_w3.c`: `lj_bez_set_load(<library>\MacroShapes)` outside the lock, `show->bez` swapped under `lj_lock`, the old set freed after; run at start beside the shape reload; freed in `lj_app_w3_stop` |
| pending_genmacro posts `LOAD_GENMACRO` with b = 0, so Restart macro on Cue load never fires on that path (genmacro -> desktop-app) | `lj_app_seam.c`: `lj_app_post_genmacro(number, restart)`; the pending service passes `gm->restart_on_load`, `lj_app_load_genmacro` (Add Macro to Cue, CHM 5841) keeps b = 0 |
| `lj_win_table` has no `LJ_WIN_HOTKEY_FUNCTION` row (cuelist-window) | row added in `lj_windows.c` (bodies in `lj_cuelist_form.c`); the Debug open-every-window script opens and closes it with no ME-16 assert |
| Runner never reads Main.Ini `[CUELIST2]` IgnoreFirstWait / AutoCue / MTCTimeout (cuelist-window -> desktop-app) | `lj_app_cuelist_step` copies them into `lj_cl_run` before each step (MTCTimeout clamped 0..2, default 0 Stop). SyncFrames stays a stored preference: the runner has no sync-frame input (section 7) |
| ME-31 remainder: Export All / Reload profile data / Import, and `windows/lj_gs2_dialogs.c` not started (gs2) | `lj_gs2_dialogs.c` written: TFixtureImportForm lists `<library>\Import\` (.Udf .Udo .GS2, Type / Status columns), Import all files / Import files (check modified = existing kept); TExportFixtureForm writes the selected user profile as `<library>\Export\<name>.Udf`; TExportAllForm writes every user profile as `Export\<gs2_file>` through `lj_gs2_save` (raw-record rule). No native file dialog in W3, so the folders are PROPOSAL (fidelity row). Fixture Configuration binds Import Profile, Export All, Create Export File and Reload profile data (no longer greyed); Reload rebuilds the tree. Gate script row: `test_uiscript_w3` 8b imports a second profile from `Import\`, checks Export All writes both byte-equal to their `UserFixtureDef` sources, then edits one on disk and checks Reload shows the new name |
| `lj_gs2.h` says the .Udf file table is 266 bytes; measured 532 per entry (gs2) | header comment corrected (docs/file-formats.md already had the measured layout) |
| LORD_C-17: the 60-second fuzz step is the integrator's (gs2) | `ci.yml` linux-clang: `build-fuzz` with `-DLJ_FUZZ=ON -fsanitize=fuzzer-no-link,address,undefined` on the whole tree, `fuzz_gs2` and `fuzz_udf` 60 s each; docs/ci.md W3 section |
| Three gdc entry points declared locally in the test and the .c (gdc-fades-followspot) | `lj_gdc_form_command`, `lj_seq_fades_apply`, `lj_followspot_form_apply_scene` moved into `lj_gdc_form.h` |
| `lj_dmx_monitor.c` fader adds the root x,y twice (`lj_widget_abs` already sums them) (gdc-fades-followspot, found) | the two extra lines removed (paint and hit). The same report against `lj_gdc_form.c` (genmacro) was already fixed by its owner |
| `rp_gfs_at` indexes `scenes[cur_scene]` with the 1-based view field (movement, found) | already fixed by its owner (`cur_scene - 1`, `lj_replay.c:1121/1148`) |
| NIT-26: PI_Customize keeps "Customize Fixture" (profile-editor -> desktop-app) | `lj_main_window.c`: the item's caption is a buffer that `lj_main_menu_sync` fills from `lj_customize_popup_caption(popup fixture)` before the popup opens (the hotkey-setup items use the same pattern) |
| `LJ_ACT_CUSTOMIZE_FIXTURE`, `LJ_ACT_FIXTURE_APPLY`, `LJ_ACT_NEW_CUELIST`, `LJ_ACT_SAVE_CUELIST` unbound (profile-editor, cuelist-window) | already bound by desktop-app in `lj_app_w3.c` (Fixture Apply: Config.dat, `Setup.<n>.pal`, `lj_engine_rebind`) |
| TTabSheet captions show `&&` (profile-editor) | `lj_w_pagecontrol.c` draws tab captions with the `&` prefix rule, as buttons and LEDs do (widths already folded `&&`). `W3PE_PROFILE_EDITOR` re-rendered |
| TTabControl alClient child covers the strip (profile-editor) | `lj_w_tabcontrol.c` gets the tabs layout hook; for a TTabControl it places an alClient child in the display rect (x 4, y 24 for one row, from tc1 404x269 / ODPanel21 4,24 396x241 in the DFM). `W3PE_CUSTOMIZE` re-rendered: the Color 1 / Color 2 / Gobo 1 / Gobo 2 strip is visible |
| lv1's last header section paints past the view (cuelist-window) | `lj_w_header_paint` stops the last section at the view's edge; a caption that no longer fits is dropped (the existing no-clip rule). `WIN_CUELIST_FORM` re-rendered |
| ME-100 alBottom re-layout order (cuelist-window, gdc) | tried: sorting each align group by the current edge (VCL DoAlign) moved the code-built Statics 2 rows (`WIN_STATICS2_FORM` 11,496 px), so it was reverted. Both forms keep their workaround (no re-layout at open with default prefs). Open, section 7 |
| Action-linked menu items do not run the TAction's OnExecute (cuelist-window) | kept the form's workaround (binds `ActionExecute` for `rt->item->action` with no fn). A generic `lj_window` binding would have to resolve 74 Action-linked items across every form under the ME-16 assert; deferred, section 7 |
| `enum lj_cl_op` has no SET_TC_SRC (cuelist-window -> cuelist-core) | kept the window's own source (forms.ini `[CUELIST2] TCSource`, PROPOSAL); the runner's shown time code matches it or reads `--:--:--:--`. Open, section 7 |
| LORD_FIDELITY-16 / F8 points at the W2 capture (cuelist-window) | `docs/fidelity.md` F8 names `docs/screenshots/w3/cuelist-toolbar-leds.png`; the W2 capture (both LEDs lit) is deleted |
| `lj_w3_bad.GS2` stray in the repo root (gs2) | `test_uiscript_w3` writes its scratch files to the working directory; `.gitignore` gains `/lj_w3_*` |
| ME-103: re-record the replay-frames sha once; add "scaled in microseconds" to design 3.3 (cuelist-core) | sha below (section 4); CI compares the three OSes, nothing stores it. The design sentence is the design owner's (the vault is read-only for this pass): section 7 |
| `lj_midi_coremidi.c` cannot compile on Windows (cuelist-core) | the macOS CI job compiles it (section 8) |
| Delete Macros has no record kind for `.bez` (movement) | kept: `remove()` + `LJ_ACT_BEZ_RELOAD`, which now reloads (row 2). An `LJ_REC_BEZ` kind is not needed for one folder of named files |
| ME-104 TSelectBGCueForm has no behaviour file (desktop-app) | open, section 7 (windows owner) |
| ME-45 standard-user install, ME-21 Joey's Mac run (desktop-app) | manual, section 7 |

## 4. Tests

    ctest (build-int3-nosdl with LJ4EVR_REFERENCE_DIR, all rows incl. test_layering):
        100% tests passed, 0 tests failed out of 98
    ctest -E test_layering (build-ninja-msvc, build-debug-win, build-msvc Release, build-msvc Debug):
        100% tests passed, 0 tests failed out of 97      (each)
    gs2_check (reference set, 48 shipped .GS2 + udf200 + 12 samples vs tools/ref/16-gs2_ref.py):
        260 files, 0 with differences
    test_gs2: ok (0 failures)          test_udf_crc: ok (0 failures)
    profiles_ids_w3: tiers 1-9 full (6: 3 of 3, 7: 5 of 5, 8: 8 of 8, 9: 4 of 4)
    exe_profiles_check: 201 rows, 86 hand ids, 45 map ids, 0 finding(s)
    gen_vectors --check: lj_trig_gen.c up to date, tests/vectors.h up to date
    gen_vectors --shapes --check: src/engine/lj_shapes_gen.c up to date (66 shapes x 8192 samples)
    font_corpus: 826 rows, 34 skipped (face not packed), 0 mismatches
    test_uiscript_w3: 0 failure(s) (incl. the ME-31 8b row)
    lj4evr --selftest (build-debug-win, build-msvc\Debug): rc 0; 5 OK lines; no "no binding for handler",
        "unknown event", "behaviour file", "matches no DFM"
    Debug app --script opening and closing 21 W3 window ids (customize_fixture / channel_config need a master
        fixture; test_profile_editor_form opens them under the Debug ctest): rc 0, no ME-16 assert

Replay (`lj4evr_tests replay`, 34 globbed + the wire row), every scenario 0 fails. W3 added seven:

| scenario | ticks | expects | | scenario | ticks | expects |
|---|---|---|---|---|---|---|
| cuelist_run (W3) | 521 | 33 | | bgcue_over_statics | 10 | 19 |
| channel_gm_disabled (W3) | 3 | 3 | | blackout_after_masters | 5 | 8 |
| fanout_auto_delay (W3) | 101 | 8 | | blind_keep_contents | 9 | 9 |
| gdc_followspot (W3) | 9 | 11 | | chase_030ms | 57 | 35 |
| genmacro_sine_amp186 (W3) | 41 | 23 | | cue_priority_slot12 | 4 | 7 |
| genmacro_vs_bgcue (W3) | 90 | 15 | | cue_time_scaling (W2, W3 ME-103 expect) | 4397 | 34 |
| movemacro_six_shapes (W3) | 211 | 27 | | disabled_offset_silent | 1 | 3 |
| wire/drift_600s_two_slots_wire (own row) | 24004 | 15 | | drift_600s_two_slots | 24001 | 33 |
| | | | | fade_060ms | 11 | 7 |
| | | | | fade_linear_40hz | 121 | 10 |
| | | | | freeze_pauses_clock | 531 | 20 |
| | | | | gm_examples_1_to_6 | 12 | 48 |
| | | | | gm_fade_under_freeze | 401 | 8 |
| | | | | htp_group_max | 5 | 6 |
| | | | | interrupted_fade_restart | 161 | 8 |
| | | | | layer_flash_current_statics | 12 | 16 |
| | | | | link_to_cue | 22 | 20 |
| | | | | loop_bounce_random_limits | 169 | 48 |
| | | | | off_is_transparent | 3 | 7 |
| | | | | pair16_pan_true16 | 46 | 47 |
| | | | | pair16_static_bg | 82 | 10 |
| | | | | seq_intensities | 5 | 10 |
| | | | | snap_050ms | 81 | 7 |
| | | | | statics_link | 7 | 8 |
| | | | | statics_ltp_reorder | 13 | 18 |
| | | | | stop_holds | 311 | 6 |
| | | | | transparent_cue_load | 13 | 21 |

Determinism: `dump-frames` (400 ticks) twice in Release and once in Debug: `cmp` equal, 1,641,600 bytes, sha256
`5b13f41930362b75a7aa6f9c53f77db2d315a4039ea5a18da664ea1263114b50` (W2 was `5e61478e...`: the W3 engine changes,
genmacro rounding and ME-103 microsecond scaling, move frames). `replay-frames.bin` (ctest `replay_dump`) in all
four Windows trees: 129,138,640 bytes, sha256 `977db5f89f81cbb4141213b116b01f866dfb32d101fabbb34951fb46d670e2dd`
(ME-103 re-record; CI compares Linux / Windows / macOS, section 8).

Ownership greps (ME-2, ME-23): `lj_table_` / `lj_cl_` / `lj_cuelist` under `src/engine/*.c` code: none (comments
only); `free(` in `src/engine`: only `lj_engine_destroy`; file I/O in `src/engine`: none; `lj_engine_` /
`lj_sacn_` / `lj_table_` calls in `src/io/lj_midi*.c`: none (one comment naming the engine constant). No float /
double / volatile in the SDL-free directories.

## 5. Goldens (tests/ui/golden, REVIEWED.tsv)

The W3 goldens landed reviewed by their tracks (W3GM_EDITOR, W3MM_* five, W3GDC_* four, W3PE_* three,
WIN_CUELIST_FORM, W2D_DELETESEQ and CTL_MAC500_MOVEMENT re-renders); each has a REVIEWED.tsv row with the DFM
numbers its track checked. This pass re-rendered three, each row appended with what was measured:
WIN_CUELIST_FORM (header stops at the view edge: section shadow x 422, lv1 sunken edge x 425), W3PE_CUSTOMIZE
(tc1 strip visible, ODPanel21 edge row 113 = sheet top 88 + 24 + 1), W3PE_PROFILE_EDITOR (`&` captions, tab widths
unchanged). The reverted ME-100 attempt is why WIN_STATICS2_FORM is unchanged.

## 6. Gates touched (docs/w3-gates.md)

ME-31 remainder (Export All / Reload / Import with the 8b script row), LORD_C-17 (CI fuzz step), LORD_FIDELITY-16
(F8 points at the W3 capture, W2 capture deleted), NIT-26 (popup caption), ME-103 (sha re-recorded; design
sentence owed), ME-16 (Debug selftest and the open-every-window script), ME-2 / ME-23 greps (section 4).

## 7. Left open (recorded, not blocking)

- ME-100: a re-layout of a form with several alBottom panels stacks them in creation order. The VCL order (by
  current edge) breaks the code-built Statics 2 rows, which are created without meaningful positions; a fix needs
  those rows placed before the first layout. TCuelist2Form and TGenericMegaForm skip the re-layout at default
  prefs.
- Action-linked menu items: `lj_window` binds OnClick names only. TCuelist2Form binds its Action items itself.
- `LJ_CL_OP_TC_SRC`: the Cuelist window's time-code source button is window state (forms.ini `[CUELIST2]
  TCSource`, PROPOSAL), not runner state.
- `[CUELIST2] SyncFrames`: stored by the window, not used by the MTC decoder.
- design 3.3 "scaled in microseconds" (ME-103): for the design owner; the engine and the cue_time_scaling expect
  at tick 4396 (109,890 ms) are in.
- ME-104: TSelectBGCueForm has no behaviour file (`select_bgcue` logs "has no behaviour file yet");
  `save_bgcue`, `statics2_prefs`, `static_props`, `save_preset` likewise (W2 list).
- The import / export dialogs have no File Exists, Export Warning or Oldstyle-name sub-dialog, and no native file
  dialog (PROPOSAL folders `Import\` / `Export\`).
- Header captions wider than their (clipped) section are dropped, not clipped.
- ME-45 (standard-user install) and ME-21 (Joey's Mac run of the .dmg) are manual.
- LOW rows outside this pass: LORD_C-4 / C-5 / C-7 / C-8, LORD_FIDELITY-13 / -14 / -15, integration s7 / s9.
- `lj_cuelist_form_save` does not refresh a `show->cuelists` table copy with the same number.

## 8. CI

    0d131fa (pre-integration)  pushed with the tracks' tree
