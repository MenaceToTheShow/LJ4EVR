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
| CI 0d131fa / ba6ea46, gcc and clang: `lj_movemacro_editor.c` -Wmissing-field-initializers (`{ "N1", "-" }`, three positional `lj_ctl_desc`), `lj_profile_editor_form.c:871` -Wformat-truncation (`dflt[8]`) | `.on_click = NULL` / `.tag = 0` designators; `dflt[16]`; and, found by reading every small buffer the W3 files print into, `t[12]` in `lj_gdc_form.c:420` and `lj_profile_editor_form.c:467`, `ad.t[3][12]` in `lj_movemacro_editor.c` |
| CI ba6ea46 linux-gcc-asan: heap-use-after-free in `test_cuelist_form` (THotkeyFunctionDialog Ok destroys its window inside its own click; `lj_window.c:440` then reads it) | root cause in `ui/lj_window.c`: `lj_window_destroy` called while that window is inside `lj_window_event` / `lj_window_popup_event` marks it dead and the outermost event frees it on return. Every Ok, Cancel or close handler that destroys its own window (more than twenty across the W3 windows, found by grep) is covered by the one guard |
| CI ba6ea46 linux-gcc-asan: LSan 15,360 bytes in `test_profile_editor` (`lj_sfunc_buttons` via `lj_layout_derive`) | `fixlib/lj_layout.c` `lj_window_add`: a window dropped for having no rows now frees its special groups' buttons |
| CI ba6ea46 linux-clang fuzz step: `libclang_rt.asan*.a` missing in the CI image | the step installs `libclang-rt-19-dev` per run (ponytail comment: bake it into docker.yml) |
| CI 284c536 fuzz_gs2 reached cov 12 only (inputs capped at 4096 bytes never pass the 20,851-byte size check) | the target zero-pads each input to a whole record and CI passes `-max_len=20851`: cov 708 / ft 1222 in 60 s on Linux; a local MSVC `/fsanitize=fuzzer` run of 60 s: 1,373,080 runs, no finding |
| CI 284c536 windows-msvc Debug ctest: `test_lock:32/39` (a lock pair preempted on the runner held > 2 ms and logged) | the loop allows up to two such lines and the deliberate 3 ms hold is counted relative to them (`tests/core/test_lock.c`) |

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

ME-31 remainder (Export All / Reload / Import with the 8b script row), LORD_C-17 (CI fuzz step, both targets 60 s
green), LORD_FIDELITY-16
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

    0d131fa (pre-integration)  run 39843823840: windows-msvc red (test_uiscript_w3:263, the gs2 stub), macos-clang,
                               linux-clang, linux-gcc-asan red (lj_movemacro_editor.c missing-field-initializers,
                               lj_profile_editor_form.c format-truncation); generate-check green
    ba6ea46 (integration)      run 39847323314: windows-msvc, macos-clang (CoreMIDI backend compiled:
                               lj_midi_coremidi.c.o), generate-check green; linux-gcc-asan red (ASan
                               test_cuelist_form, LSan test_profile_editor); linux-clang red (fuzz: no compiler-rt)
    284c536 (CI fixes)         run 39848757895: five green; windows-msvc red on test_lock (Debug ctest, runner
                               preemption)
    e655c79 (lock, fuzz_gs2)   run 39850233302: green, all six jobs.
                               fuzz_gs2 1,428,002 runs / cov 708, fuzz_udf 16,273,089 runs / cov 175, 61 s each
                               determinism: frames-{linux,macos,windows}.bin 1,641,600 bytes, sha256
                               5b13f41930362b75a7aa6f9c53f77db2d315a4039ea5a18da664ea1263114b50 (equal)
                               replay-frames.bin linux / windows / macos
                               977db5f89f81cbb4141213b116b01f866dfb32d101fabbb34951fb46d670e2dd (equal; ME-78, ME-103)
                               https://github.com/UnbannableGT/LJ4EVR/actions/runs/39850233302

## 9. Acceptance (integrator 2, this PC, 2026-10-22)

Design 10 W3, item by item, on Windows 11 (work PC, MSVC 19.44, the four-monitor layout of the W2 record). The walkthrough
is `tests/ui/scripts/w3_acceptance.lj4s` (448 lines, 43 asserts) on a fresh `Default` library (`<userdir>User\Default`,
`<userdir>forms.ini`, `<userdir>Default.lj4b` deleted first; the user folder was copied aside before the first run),
run with `build-ninja-msvc\lj4evr.exe --script` (Release) and `build-debug-win\lj4evr.exe --script` (asserts, debug
heap): both rc 0. Screenshots: `docs/screenshots/w3/<step>-<Class>.png`, one per open window (the confirm box is now
captured too, see the app fixes). Every window shot was compared with its CHM GIF; the fixes are listed below the table.

| # | Item (design 10 W3) | Result | Evidence |
|---|---|---|---|
| 1 | A 20-line cue list runs against PC elapsed time, manual lines ignoring waits; the same list as replay `cuelist_run` | tick | walkthrough sections "20-line cue list" and "run against PC elapsed" (below); replay `cuelist_run: 521 ticks, 33 expects, 0 fails` |
| 2 | A `.Cl2` with an `MP Play` line loads with that line red and runs the rest | tick | `cuelist-mp-play-TCuelist2Form.png` (line 2 red, `<>` on line 1); log lines below; `cl_assert_line 2 error`, `cl_assert_line 3 done`, `assert_view cue_number 2` |
| 3 | `genmacro_sine_amp186` and `genmacro_vs_bgcue` pass | tick | ctest lines below; live half: `genmacro-amp186-TCueMacroControlForm.png`, `assert_text lMinMax "Min/Max : -186/186"` |
| 4 | Movement traces match `17-position_ref.py` through `vectors.h`; the circle shows on the wire | tick | `test_movemacro: 576 trace(s) vs 17-position_ref.py, 0 mismatch(es)`, `gen_vectors_check`; sACN capture on atlas-ci2 and on this PC (below): radius 25801..25802, 5.000 s/rev, 40.1 packets/s |
| 5 | The 48 shipped `.GS2` import with every field equal to the oracle and round-trip byte-exact through `gs2_raw` | tick | `gs2_check.py` on the 48 files: `48 files, 0 with differences`; the whole reference set `260 files, 0 with differences` |
| 6 | A `.Udf` with a corrupted footer CRC is rejected with the oracle's message | tick | both strings below; ctest `test_udf_crc: ok (0 failures)` |
| 7 | The prompt texts of `[12 Q9]` appear verbatim | tick for the W3 prompts (four `assert_prompt` lines and shots); the Q9 rows of other waves listed below | below |
| 8 | Every 15/06-15/09 id has a hand file (CI check by id) | tick | `profiles_ids_w3` lines below |
| + | Restart persistence of every W3 window | tick (14 windows; the `poOwnerFormCenter` / `poMainFormCenter` ones are centred each time, design 7.10) | below |

### Item 1: the 20-line list (walkthrough) and `cuelist_run`

Cues 0001 `odd` and 0002 `even`; Cuelist window New, right-click Add twice (10 lines each), Set Current Cue on every
line (odd lines 0001, even 0002), Wait Elapsed 1 s on lines 2-20 through the edit panel, lines 10 and 15 Set Advance >
Manual, Preferences > Use Color Codes, Save as `twenty` (`CueLists/00000001.Cl2` asserted). `sbPCElapsed`, Go:

    assert_view cue_number 1   line 1 at 0 s           assert_view cue_number 2   line 2 at 1 s
    assert_view cue_number 1   line 9 at 8 s           cl_assert_line 10 next / cl_assert_line 9 done
    assert_view cue_number 1   10.5 s: the manual line ignored its 1 s wait
    cl_execute -> assert_view cue_number 2   line 10 on Go !
    assert_view cue_number 2   line 14 at Go + 4 s     cl_assert_line 15 next
    cl_execute -> assert_view cue_number 1   line 15 on Go !
    assert_view cue_number 2   line 20 at Go + 5 s     cl_assert_line 20 done

Shots `cuelist-20-lines-*` and `cuelist-run-*` (line 20 grey = done, line 15 selected; waits green, cue column blue,
`<>` on line 1, as `cuelist1.gif`). `cuelist_run` is the replay counterpart (a 20-line list against synthetic elapsed
time through the same runner `lj_cl_run_step`): manual line 3 ignores its 5 s wait, `mp_play` line 5 skipped, Wait MTC
held until Go, Goto beyond the end, Flash Seq, Load BGCue:

    47: replay cuelist_run: 521 ticks, 33 expects, 0 fails

### Item 2: MP Play

`tests/ui/scripts/w3_mp_play.Cl2` (ours, 3 lines: cue 1; `mp_play`; Wait Elapsed 0.50 s with cue 2):

    tests/ui/scripts/w3_mp_play.Cl2 line 2: mp_play not available, kept and skipped (load error)
    cuelist: line 2 skipped (load error: MP Play)

`cuelist-mp-play-TCuelist2Form.png`: line 2 `MP Play` in red, the rest black; Go, then `cue_number 2` and line 3 done.

### Item 3: generic macros (ctest `replay_all -V`, `test_genmacro -V`, build-ninja-msvc)

    47: replay genmacro_sine_amp186: 41 ticks, 23 expects, 0 fails
    47: replay genmacro_vs_bgcue: 90 ticks, 15 expects, 0 fails
    47: replay: 34 scenario(s), 0 failed
    81: test_genmacro: 0 failure(s)

Live: MAC 500 master, Generic Macro Editor, Select all, `Sine` (row 7 of the Macros.Mac order), Keep Amplitude
Symmetrical, Amp+ bar to 185 and one Inc step: `lMacro "Sine"`, `lMinMax "Min/Max : -186/186"`, `gm_engine_on=1
gm_slot_on=1`; then Disable Macros and `gm_engine_on=0` (rank 6 would otherwise move the pan / tilt under item 4).

### Item 4: movement traces and the wire

    82: test_movemacro: 576 trace(s) vs 17-position_ref.py, 0 mismatch(es)
    50: 17-position_ref.py: all checks passed
    50: gen_vectors --check: tests\vectors.h up to date
    47: replay movemacro_six_shapes: 211 ticks, 27 expects, 0 fails
    47: replay fanout_auto_delay: 101 ticks, 8 expects, 0 fails

Walkthrough: Home Selected Fixtures, `mm_set 1 1 100 0` (System Ellipse, amplitude 100, frozen): `assert_pos 1 25802 0`
twice, 500 ms apart (17-position_ref.py `macro_offset`); Fan Out over five MAC 500s with Delta Pan 127: `assert_pos` 1 / 3 /
4 / 5 / 6 = -32512 / -16256 / 0 / 16256 / 32512 (17 s5.2).

Wire: `tests/ui/scripts/w3_wire_circle.lj4s` (one MAC 500 at 1:001, the frozen check, then `mm_set 1 1 100 50` = Cycle
5.00 sec for 8 s). Two monitors decoded pan (slots 11/12) and tilt (13/14) from the universe 1 multicast: a
`08-sacn_ref.py`-style receiver (scratch, stdlib only) on `ssh <runner-host>` (<nic>, <lan-ip>) and the
same on this PC (<lan-ip>). Release and Debug runs rc 0. atlas-ci2, the first 2 s of the circle, every third packet:

    283886.6905 DATA <lan-ip> u1 seq   8 LJ4EVR <dev-pc> pan  25689 tilt   2411 r 25802 slots[11..14]=[228, 89, 137, 107]
    283886.7655 DATA <lan-ip> u1 seq  11 LJ4EVR <dev-pc> pan  25351 tilt   4800 r 25801 slots[11..14]=[227, 7, 146, 192]
    283886.8405 DATA <lan-ip> u1 seq  14 LJ4EVR <dev-pc> pan  24780 tilt   7186 r 25801 slots[11..14]=[224, 204, 156, 18]
    283886.9152 DATA <lan-ip> u1 seq  17 LJ4EVR <dev-pc> pan  24001 tilt   9470 r 25802 slots[11..14]=[221, 193, 164, 254]
    283886.9902 DATA <lan-ip> u1 seq  20 LJ4EVR <dev-pc> pan  22993 tilt  11707 r 25802 slots[11..14]=[217, 209, 173, 187]
    283887.0652 DATA <lan-ip> u1 seq  23 LJ4EVR <dev-pc> pan  21798 tilt  13804 r 25801 slots[11..14]=[213, 38, 181, 236]
    283887.1403 DATA <lan-ip> u1 seq  26 LJ4EVR <dev-pc> pan  20389 tilt  15812 r 25802 slots[11..14]=[207, 165, 189, 196]
    283887.2158 DATA <lan-ip> u1 seq  29 LJ4EVR <dev-pc> pan  18823 tilt  17647 r 25802 slots[11..14]=[201, 135, 196, 239]
    283887.2906 DATA <lan-ip> u1 seq  32 LJ4EVR <dev-pc> pan  17091 tilt  19329 r 25801 slots[11..14]=[194, 195, 203, 129]
    283887.3655 DATA <lan-ip> u1 seq  35 LJ4EVR <dev-pc> pan  15178 tilt  20864 r 25801 slots[11..14]=[187, 74, 209, 128]
    283887.4405 DATA <lan-ip> u1 seq  38 LJ4EVR <dev-pc> pan  13163 tilt  22191 r 25801 slots[11..14]=[179, 107, 214, 175]

    wire_remote.log circle packets 320 span 7.975 s r 25801..25802 swept 574.2 deg -> 5.000 s/rev max gap 0.026 s pps 40.1
    wire_local.log  circle packets 320 span 7.985 s r 25801..25802 swept 574.2 deg -> 5.006 s/rev max gap 0.032 s pps 40.1

The radius is the oracle's 25802 within the integer tables' 1 unit; the figure starts at index 0 (25802 : 0, the frozen
assert) and turns in the window's 5.00 s; 3 TERM on quit.

### Item 5: the 48 shipped `.GS2`

    python -I tools/gs2_check.py --tests D:\lj4evr\build-ninja-msvc\lj4evr_tests.exe reference/installer/extracted_2.209/app/User/UserFixtures/*.GS2
    OK   reference/installer/extracted_2.209/app/User/UserFixtures/_Setup.GS2
    OK   reference/installer/extracted_2.209/app/User/UserFixtures/Generic PanTilt (16bit A).GS2
    ... (48 OK lines)
    48 files, 0 with differences
    ctest gs2_check (build-int3-nosdl, LJ4EVR_REFERENCE_DIR): 260 files, 0 with differences
    84: test_gs2: ok (0 failures)

Each line compares every field of `lj4evr_tests gs2_dump` with `tools/ref/16-gs2_ref.py` and requires `roundtrip ok`
(our `gs2_raw` write equals the file) and the oracle's own `serialize(parse(x)) == x`.

### Item 6: a corrupted `.Udf` footer CRC

`tests/ui/scripts/w3_synthetic.Udf` (ours, 214 bytes) with byte 178 (the first footer byte, the CRC) XOR 0xFF:

    oracle  python -I tools/ref/16-gs2_ref.py dump w3_badcrc.Udf  -> AssertionError: footer CRC mismatch
    ours    lj4evr_tests gs2_dump w3_badcrc.Udf                   -> error footer CRC mismatch
    85: test_udf_crc: ok (0 failures)

`gs2_import` (the UI verb) and Import Fixture Profile fail with the importer's message, `LJ_GS2_ERR_CRC`.

### Item 7: the `[12 Q9]` prompt texts

Live, each with `assert_prompt` and a shot of the box (`*-TConfirmForm.png`):

| Prompt | Where | Shot |
|---|---|---|
| `Current CueList was modified` / `This will clear the current cuelist` (exe 0x188D9C / 0x17FD7C) | Cuelist New on a modified list | `cuelist-new-prompt-TConfirmForm.png` |
| `Modified cuelist includes link to other cuelist - run without saving first ?` (0x17DF50) | Go on a modified list holding a Link Cuelist line (`w3_link.Cl2`) | `cuelist-link-prompt-TConfirmForm.png` |
| `Any unapplied profile changes will be lost ?` (0x319674) | Generic Fixture Profile, Cancel after typing in Author | `profile-editor-prompt-TConfirmForm.png` |

`Followspot settings changed but not saved - save now?` and `Clear all followspot settings, are you sure ?` are asserted
by `test_gdc_fades_fs`, `Error : Maximum number of Macros exceeded` by `test_genmacro`. A grep of every Q9 literal
against `src/` finds 66 of 86. The 20 absent ones belong to other waves' windows: Fixture Configuration (`Clear Selected
Fixtures ?`, `Do you wish to change the fixtures DMX address ?`, `Clear data for new fixtures ?`, `Apply Default DMX
values to new/modified fixtures ?`, `Copy fixture, are you sure?`, `Clear selected fixtures, are you sure?`, `Place all
Fixtures on this tab ?`), Delete Files (`Delete All Custom Selections ?`, `Deleting Files. Please Wait..`), cue slots
(`No free sequence slots in cue - sequence not added to cue`, `No priority slots available - add sequence as
non-priority sequence ?`), Statics 2 (`Get Page Error !`, `Page Error (Idx = %d)`, `Error!`), lasers, format updates,
restart, patch, snapshot, password and startup status (`%s emitting!`, `Enable laser output ?`, the three update strings,
`Do you wish to restart LightJockey now ?`, `Clear All Inverted Channels ?`, `Cancel will loose any recorded data - do you
wish to Cancel ?`, `Does your momma kow you are doing this ?`, `This window will selfdestruct ...`); and the note's
`Close LightJockey ?` (the code has the DFM's `Close Lightjockey ?`). Design 10 W4's caption diff owns them.

### Item 8: profiles_ids (ctest `profiles_ids_w3 -V`)

    88: check_profiles: tier 6 15/06 Atomic 3000: 3 of 3 ids have a matching hand file
    88: check_profiles: tier 7 15/07 MiniMAC: 5 of 5 ids have a matching hand file
    88: check_profiles: tier 8 15/08 RoboColor: 8 of 8 ids have a matching hand file
    88: check_profiles: tier 9 15/09 MAC 550 / 575 / 700: 4 of 4 ids have a matching hand file
    (tiers 1-5 as in W2: 4 / 2 / 8 / 11 / 9 ids, every one matched)

### Persistence (forms.ini, Main.Ini)

`tests/ui/scripts/w3_positions_1.lj4s` (open, `move`, close the 14 persisting W3 windows, on the walkthrough's end state)
then `w3_positions_2.lj4s` (reopen, `assert_pos`): both rc 0. After the pair:

    forms.ini [CUELIST2] Left=100 Top=1600 Width=545 Height=506   [SAVECUELIST2FORM] Left=140 Top=1620
    [SELECTCUELIST2FORM] Left=180 Top=1640 Width=377 Height=318   [CLSETCUETRIGFORM] Left=220 Top=1660
    [SAVEMACRODIALOG] Left=300 Top=1700   [LOADMACROLIST] Left=340 Top=1720 Width=200 Height=200
    [MOVEMENTMACROFORM] Left=380 Top=1740   [FANOUTFORM] Left=420 Top=1760 Width=539 Height=255
    [AUTODELAYFORM] Left=460 Top=1780   [CUEMACROFORM] Left=500 Top=1800   [GENERICMEGAFORM] Left=540 Top=1820 Width=750 Height=500
    [FOLLOWLISTFORM] Left=580 Top=1840 Width=200 Height=230   [DEFAULT2FORM] Left=620 Top=1860
    Main.Ini [MACRO] Left=260 Top=1680   (the Generic Macro Editor)

Centred each time and never persisted (DFM `Position`): Log TimeCode, Delete Macros, Edit FollowSpot Settings, Generic
Fixture Profile, Customize Fixture, Import, Export (`poOwnerFormCenter`), Channel Configuration (`poMainFormCenter`).

### What the walkthrough and the GIF comparison found (fixed in the app)

| Where | Found | Fix |
|---|---|---|
| `LJ_FN_HOME_SELECTED_FIXTURES` | `fn: no binding for LJ_FN_HOME_SELECTED_FIXTURES`: the hotkey and Fixture toolbar button 9 did nothing, so Home left pan / tilt at DMX 0 (-32768) | `lj_ctl_movement_home_selected()` (`controls/lj_ctl_movement.c`): every selected fixture with pan / tilt to 0 : 0, state Snap, no window needed (04 s4.1); bound in `lj_app_w3.c` |
| Cuelist New (toolbar, window, File menu) | no `Current CueList was modified` prompt; the window's own New freed its list without the runner letting go of it (a stopped run keeps the list pinned) and leaked an app-loaded one | one path: the form runs `LJ_ACT_NEW_CUELIST`; the app asks (`[PREFS] WarnCueList`) and replaces through `cl_replace` (unpin, then free) |
| Cuelist run with a Link line | `Modified cuelist includes link to other cuelist - run without saving first ?` missing | `cl_op` asks before Top / Run / Run from mark / Go-on-idle on a modified list holding `link_cuelist`; OK runs the op |
| Generic Fixture Profile Cancel | a typed field never prompted: the TEdits have no OnChange, so `dirty` stayed 0 | Cancel reads the controls into the record and compares it with the record before |
| TConfirmForm | the longer Q9 lines ran past the 153-px bevel and the 267-px client (`Modified cuelist ...` cut at the edge) | the box widens to its longest line and the bevel grows to the text; short prompts keep the DFM geometry (PROPOSAL) |
| Cue list grid | `<>` missing on lines without a command (cuelist1.gif lines 1 / 4; exe 0x179F14 beside Auto / Man); Use Color Codes did nothing | `lj_cl_cmd_grid[none] = "<>"`; colour codes: waits green, other commands blue, the Cue/Trig column blue (07 s7; hues PROPOSAL) |
| TListView (every report view) and the cue grid | a last column wider than the view drew its grid line past the view edge: a 1-px clBtnFace line down the Cuelist command panel (x 495) | the last column stops at the view edge (`lj_w_listview.c`, `lj_cuelist_form.c` lv_paint) |
| Header sections | a caption wider than its clipped section was dropped (`Comment/Exe Parameter` blank; Intensity Groups `UserName` blank) | clipped per glyph at the section edge (`cuelistactive.gif` shows `Comme`) |
| Generic Macro Editor | `Min/Max -186/186` (the format called unlocated), lTime `... sec`, a black preview, black bars with a blue centre fill and bare numbers | exe formats `Min/Max : %d/%d` (0x3399F8) and `Cycle time: %.1f seconds` (0x33A8EC); preview #0000FF with the cyan 0 line and centre line, raw shape grey 3 px (macrotrackingmacroout.gif, 1:1); bars #808080 filled white from the left with `Pos Amplitude n` / `Neg Amplitude n` / `Cycle time: n sec` / `Cycle time: never` (0x33AB7C-0x33ABC8) |
| Movement Macro trace, Fan Out preview | centre cross #4040FF | red (movementmacro.gif 1:1 col 69 / row 107; fanoutdialog.gif) |
| Pan/Tilt Macro Auto Delay | the three values were bare labels | read-only white edits, greyed unless their style is chosen (autodelay.gif) |
| Edit FollowSpot Settings | four equal 107-px columns (the grid had one width) | `lj_w_grid.col_w` (ColWidths) and 20 / 30 / 150 / rest (editfollowspot.gif) |
| Channel Configuration | GM Control `Enabled`, lFixType without the fixture number, lDmxAdr `1:001` | `Yes` / `No`, `(n) <profile>`, the `1/38` form (channelconfiguration.gif) |
| `--script` `shot` | the open confirm box was never captured | `lj_win_message_window()` captured once |

Script-side corrections (the app was right): the stay-on-top Unassigned list opens centred over the desktop and took
drops 3-6 (`move TUnassignedForm 1800 1600` first); `click` takes no `.row.col`, so `Sine` is row 7 by coordinates; no
bar x gives exactly 186 (x 126 = 185, then one Inc step); the Cuelist window shows a loaded list on its 100 ms timer
(`wait 300` after `cl_load`); the `RECONCILE` lines resolved (`eElapsedSec`, `sbSelectAll`, the Fan Out `DeltaPan` bar
right-click = max 127 instead of a drag on pb1, the profile editor on a New Profile record).

Goldens re-rendered and reviewed (REVIEWED.tsv rows): `WIN_CUELIST_FORM`, `WIN_INTENSITY_GROUPS`, `W3GM_EDITOR`,
`W3MM_MOVEMACRO`, `W3MM_BEZIER`, `W3MM_FANOUT`, `W3MM_AUTODELAY`, `W3GDC_EDIT_FOLLOWSPOT`, `W3PE_CHANNELCONFIG`.
Compared and left as they are: `W3GDC_GENERIC_DMX` (genericcontrol.gif: map colours and fader match),
`W3GDC_SET_FADE_STATUS` (sequencefadecontrol.gif), `W3GDC_SELECT_FOLLOWSPOT` (selectfollowspotsetting.gif),
`W3MM_CUEMACRO` (cuemacrocontrol.gif), `W3PE_PROFILE_EDITOR` (userdeffixtureinfotab.gif), `W3PE_CUSTOMIZE` (no LJ2
capture of the window; customizepop.gif is the popup). Button glyphs (arrows, check marks, CW / CCW, Top / Run) stay W4 art.

### Tests after the fixes

    ctest build-int3-nosdl (all rows, LJ4EVR_REFERENCE_DIR): 100% tests passed, 0 tests failed out of 98
        test_layering: 398 file(s) checked, 190 header(s) compiled alone, 0 finding(s)
        gs2_check: 260 files, 0 with differences
    ctest -E test_layering, build-ninja-msvc and build-debug-win: 100% tests passed, 0 tests failed out of 97 (each)
    0 warnings under /W4 /WX in the three trees
    build-debug-win lj4evr --selftest: rc 0, 5 OK lines; no "no binding for handler", "unknown event", "behaviour file",
        "matches no DFM"

Not fixed, recorded in docs/fidelity.md: the Cuelist `Elapsed` label (DFM caption form `Elapsed 0000:00:00`; the GIF
reads `Elapsed:` and a time, exe 0x17891C, format not located), the Movement Macro trace density (128 points; the GIF's
figure is denser), the Tweak row's O/S/F (the W1 model), header and cell text clipped per glyph or dropped where VCL clips
per pixel.

### CI

    288c4ab (W3: acceptance walkthrough)  run 39857752487: green, all six jobs (windows-msvc, macos-clang, linux-clang
                                         with the 60 s fuzz step, linux-gcc-asan, generate-check, determinism)
                                         https://github.com/UnbannableGT/LJ4EVR/actions/runs/39857752487

## Fix pass r1 (W3 code council, `W3_CODE_MASTER_ELDER_VERDICT.md` s4, ME-107..ME-123)

Ruling REV PATCH; one commit "W3: council fix pass r1", this PC (MSVC 19.44, Windows 11), 2026-10-22. Item by item with
the verifier line of verdict s4:

| ME | Change | Verifier evidence |
|---|---|---|
| ME-107 | `lj_kv_get` copies only the matching key's value; any other value is skipped at any length (`model/lj_kv.c`) | `test_cuelist` 127-char Execute File `pstr` round trip; `test_followspot` 100-fixture `sel=` round trip; `test_model_records` 63-char `gs2=` keeps `ipan` / `swap` / `placed` |
| ME-108 | `lj_cl_exec` commits `st.last`, `line_t0_us` and the next line (or the end) before any host callback; `in_exec` makes a nested Go return -1 and a nested pass a no-op; the pass stops when a callback ended the list or changed / unpinned it | `test_cuelist_run` row A (Hotkey Command = Cuelist Stop: IDLE, cue 2 never loaded) and row B (Cuelist Go from the line: depth 1, rc -1) |
| ME-109 | `lj_mm_incr` from the programmed byte, then `incr * speed_pct / 100` (floor, 0 % frozen); the 127 cap reading tagged `PROPOSAL rig 17 s8 item 7` on `LJ_MOVEMACRO_PCT_MAX`; replay `macro_ctl` 0..999 | replay `movemacro_speed_pct`: `expect_mm 9 1 pan=160` (control at tick 2: 32 + 8 x 16; the probe text's tick 5 gives 4 x 32 + 5 x 16 = 208, recorded in the file) and 490 % `pan=1887` at tick 29, 0 fails |
| ME-110 | `lj_sel_master_profile` returns `lj_show_profile` (GS2 included); `lj_app_gs2_reload` closes every control built from the old array (`lj_ctl_drop_profiles`) and re-resolves the Generic Macro Editor (`lj_genmacro_editor_reprofile`) before `lj_show_gs2_free`, then reopens the controls | `test_uiscript_w3` s8b: a GS2 fixture's master profile is in `show->gs2`, its control and the editor open, Reload profile data, both still open on the new array, refreshed (linux-gcc-asan in CI). The dimmer-derived synthetic GS2 has a Levels window, not Intensity: the row opens the profile's first window |
| ME-111 | cell text clipped per glyph at the cell edge in `lj_w_listview.c` and the cue grid's `lv_paint` (as the header) | `test_cuelist_form` ink in the Command cell of the Hotkey Command line; `WIN_CUELIST_FORM` re-rendered (196 px, that cell only), REVIEWED.tsv row; fidelity row "Header and report-view text" updated |
| ME-112 | Select Background Cue (TSelectBGCueForm2, `lj_win_table` row `select_bgcue`): the BG cue files with `(T)` before a transparent one, double-click = `LJ_ACT_LOAD_BGCUE`, Pop_Delete, Follow Active BG-Cue; sbLoad handled on its OnClick and Down while the list is open; BG Cue - Sequence Mode from the slot popup (Audio Trigged / No Loop -> the slot mode); Edit Sequence and Use Color Code greyed with fidelity rows | `test_uiscript_w3` s9a: `click TBGCueForm.sbLoad` opens the list latched, `(T)07` listed, double-click loads BG cue 7, sbLoad up after the second click, Toggle BG Cue List twice, Sequence Mode No Loop written, no `has no behaviour file yet`; Debug `--selftest` rc 0 with no such line; a Debug `--script` (`open bgcue_form`, `click TBGCueForm.sbLoad`, `assert_visible TSelectBGCueForm2.lv1 1`, `open select_bgcue`) rc 0 |
| ME-113 | `lj_ini_find` / `lj_ini_sets` NULL-safe; 48 `x ? lj_ini_get* : dflt` ternaries and 15 one-line section wrappers (`sv_geti`, `form_geti`, `main_geti`, `acs_geti`, `statics_geti`, `geti`, `pref`, `main_pref`, `lg_pref`, `sl_pref`) inlined; `lj_handler_noop` (`ui/lj_window.h`) replaces 52 copies; 14 `find()` wrappers without the dead ternary; `lj_w_listview_index()` for the 37 index reads; `LJ_STATICS2_SLOTS_PER_PAGE` defined once (`model/lj_model.h`); `lj_cue_pages_load/save` moved into `model/lj_cuepage.c`; `lj_file_exists` -> `core/lj_fs_exists`; the second `lj_fs_mkdir` (`model/lj_library.c`) deleted; `OS_HEADERS` + `direct.h io.h dirent.h` with allow rows; ME-84: the sACN Source Name backs up to a UTF-8 character boundary before the 63-byte cut | `test_layering: 398 file(s) checked, 190 header(s) compiled alone, noop handlers 0, ini ternaries 0, 0 finding(s)`; `test_sacn_packets` 22-character row (61 bytes, ends on 0xAC); `ui_golden`, `ui_golden_ctl` and every form golden byte-unchanged |
| ME-114 | `fuzz_udf` overwrites the footer CRC with `mz_crc32(body)` before `lj_udf_parse`; CI seeds `udf-corpus/` with `lj_w3_synthetic.Udf` (test_uiscript_w3's `lj_udf_export` output) and passes `-max_len=32768` | CI fuzz step log (recorded in `docs/ci.md` after the run) |
| ME-115 | `lj_engine_freeze` shifts `mmc.t0_us` on unfreeze like a runner's fade | replay `movemacro_fade_freeze`: tick 71 `mm_amp=47 mm_fade=1` |
| ME-116 | `lj_cl_pass` acts on the present -> absent time-code edge at a Wait MTC: Stop = end, Restart = run from line 1, Ignore = wait (`lj_mtc_time` returns -1 after `LJ_MTC_TIMEOUT_MS`) | three `test_cuelist_run` rows; the default (Stop) gives IDLE on the edge |
| ME-117 | Delete Generic Macros posts `LOAD_GENMACRO gm NULL` only when the view's `gm_cue_number` is the deleted one; another number is freed directly | `test_uiscript_w3` s9b: macro 3 runs, macro 7 deleted, `gm_cue_number` stays 3 |
| ME-118 | `ch_caption[LJ_PROFILE_CH][7]`, `eChannelName.max_length = 6`, `chcap` refuses 7 | `test_profile_editor` chcap rows (`Yellow` accepted, `TooLong` refused) |
| ME-119 | Flash Sequence cell `%u%c%u s` with `lj_locale_decimal()` | `test_cuelist_form`: `0001 1.5 s` under C, `0001 1,5 s` under de-DE |
| ME-120 | `OffsetMIDITimecodes1`, `OffsetPCTime1`, `OffsetMIDITimecodes2` (Sort MIDI wait times), `M_ImportOldCuelist` added to the greyed set; the W4 comment on the live item deleted | `test_cuelist_form` (all four disabled); four `docs/fidelity.md` rows |
| ME-121 | `test_engine_ownership` genmacro load / delete (NULL, tick, free) / re-save / tick; `lj_cl_` / `lj_cuelist` in the engine-layer regex of `test_layering.py` | `test_engine_ownership: 0 failure(s)` (ASan in CI); a planted `lj_cl_bogus()` call in a copy of `engine/lj_engine.c` gives `cue-list symbol inside engine/` and exit 1 (local check) |
| ME-122 | `lj_str_rtf_strip(dst, cap, rtf, len)` parses a terminated copy when the field holds no NUL; the editor passes `LJ_GS2_NOTES` | `test_gs2`: an all-0x01 4097-byte heap field strips to 4097 bytes (ASan: no over-read); a 9-byte bound reads `ab` |
| ME-123 | `CHANGELOG.md [0.3.0]`, README status and the three docs rows, `w3-gates.md` Status column (verdict s7), `replay-grammar.md` W3 paragraph, fidelity F19 / F24 / F47 / I4 / I5 rows and the one F20 row, the stale `lj_shapes_gen.h` clause | the files |

design 13 item 13: default kept (W5). Joey did not pull the MediaPlayer into W3 or drop it; the MP rows stay greyed
with the inert-row rule (design 8.5, fidelity I4).

ME-124 (the design sentences, design 7.7 `movementmacro.gif`, note 16 s2 width) belongs to the orchestrator as design
owner, in the vault. It is not in this commit; the design.md revision line is quoted here when that revision lands.

### Tests after the fix pass

    ctest build-int3-nosdl (LJ_SDL=OFF, LJ4EVR_REFERENCE_DIR): 100% tests passed, 0 tests failed out of 98
    ctest build-ninja-msvc (Release, SDL) and build-debug-win (Debug, SDL): 100% tests passed, 0 tests failed out of 98 (each)
    0 warnings under /W4 /WX in the three trees; tools/fuzz/fuzz_udf.c compiled standalone with cl /W4 /WX: clean
    replay: 36 scenario(s), 0 failed (movemacro_speed_pct 6 expects, movemacro_fade_freeze 4 expects added)
    replay-frames.bin (ctest replay_dump), all three trees: 129,664,080 bytes,
        sha256 b14d4b292708b88227b5fedd319537b05944c6a9947755475ac935b86468d05c (re-recorded once: ME-109, ME-115 add rows)
    build-debug-win lj4evr --selftest: rc 0, 5 OK lines; no "has no behaviour file yet", "no binding", "unknown event"
    w3_acceptance.lj4s on a fresh Default library (the user folder copied aside and restored after): Release rc 0,
        Debug rc 0; the eleven re-shot captures in docs/screenshots/w3/ differ from the previous run only in the main
        window's memory gauge (x 13..48, y 590..598) and the live movement trace

### CI (fix pass r1)

    70e6670 (W3: council fix pass r1)  run 39867308177: green, all six jobs, after one re-run of linux-clang
                                        (first attempt: test_lock_hold "lock hold 3210 us >= 2000 us" on the shared
                                        runner, a preemption; the re-run passed, no code touched)
                                        fuzz_gs2 DONE cov 708; fuzz_udf DONE cov 1463 (ME-114, was 175)
                                        https://github.com/UnbannableGT/LJ4EVR/actions/runs/39867308177

## Release v0.3.0 (2026-10-22, this PC)

Ruling: AUTHORIZE WITH GATES after the REV PATCH fix pass r1 (`W3_CODE_MASTER_ELDER_VERDICT.md` section 4, landed in
70e6670 + d5d31e9).

    3f5faef  Release 0.3.0   (VERSION 0.3.0; CHANGELOG 0.3.0 "What v1.0.0 is not" + known gaps = verdict sections 5-6;
                              README status already 0.3.0 from ME-123)
    ci.yml   39868854204 green, six jobs (3f5faef)
    git tag -a v0.3.0 -m "LJ4EVR 0.3.0 - W3 show control"; git push origin v0.3.0   -> tag on 3f5faef
    release.yml 39869504951 green first run

Release: https://github.com/UnbannableGT/LJ4EVR/releases/tag/v0.3.0 (prerelease, 0.x)

| Asset | Bytes | sha256 |
|---|---|---|
| `LJ4EVR-0.3.0-win64-setup.exe` | 1723124 | cc8e5fb943a7a66f6389f7f413f516dae895cf3dddaeb2bf7806496570329a2c |
| `LJ4EVR-0.3.0-macos.dmg` | 4335448 | 3326af8a8859c7e6ce5464cbac5c47f0aeb476b52a5dcf5b6149254f594feaa9 |
| `SHA256SUMS` | 184 | |

`gh release download v0.3.0` into the scratch area; Get-FileHash of both files equals `SHA256SUMS`. Installer
VERSIONINFO: ProductName=LJ4EVR ProductVersion=0.3.0 FileVersion=0.3.0.

Windows installer, this PC (Windows 11, unelevated PowerShell, no UAC prompt; `whoami /groups` shows S-1-5-32-544
"Group used for deny only", i.e. a filtered admin token, not a standard user, so ME-45 stays open for Joey):

    (User\Default and Default.lj4b moved aside first: the script's fresh-library precondition)
    LJ4EVR-0.3.0-win64-setup.exe /S                      -> rc 0
    %LOCALAPPDATA%\Programs\LJ4EVR\                      lj4evr.exe 9854464, Uninstall.exe 111743, THIRD_PARTY_NOTICES.txt 2055
    HKCU\...\Uninstall\LJ4EVR                            DisplayName "LJ4EVR 0.3.0", DisplayVersion 0.3.0
    Start Menu\Programs\LJ4EVR 0.3.0\                    (per-user)
    lj4evr.exe --version                                 LJ4EVR 0.3.0                                                   rc 0
    lj4evr.exe --script tests\ui\scripts\w3_acceptance.lj4s
                                                         from a scratch cwd holding a copy of tests\ui\scripts\ and an
                                                         empty docs\screenshots\w3\: rc 0, every assert green, 10 shot
                                                         lines, 47 PNGs (not committed); PIL verify over the 47: 0 errors,
                                                         all 10 -TLJMainForm.png 799x606; stderr only the fresh-library
                                                         "ini: cannot open" / "rec: cannot open" lines
    Uninstall.exe /S _?=<instdir> (copy run from scratch) -> rc 0; HKCU uninstall key gone, Start Menu folder gone,
                                                         install directory gone
    User\Default and Default.lj4b restored from the copies moved aside; the run's library kept in the scratch area.

Gotcha: `shot` does not create its directory. The first run, without `docs\screenshots\w3\` under the cwd, failed at
script line 305 ("cannot write ...", "no window open to capture", rc 1); its library was set aside and the run repeated
on a fresh one. In the repo the directory exists, so CI and repo-root runs are unaffected.

macOS: no Mac on this PC. `LJ4EVR-0.3.0-macos.dmg` checked by size and signature only: 4335448 bytes, first block zlib
(`78 da`, UDZO) and a `koly` UDIF trailer (version 4) in the last 512 bytes. **ME-21 Mac acceptance is open for Joey**:
run the .dmg on his Mac (right-click LJ4EVR.app > Open, or `xattr -d com.apple.quarantine`), screenshot the Cuelist
window and one menu, and latch a cue from a real MIDI device. **ME-45**: one install of this installer on a true
standard-user account (verdict section 5). Both block `v1.0.0`; ME-45 also blocks the public flip.
