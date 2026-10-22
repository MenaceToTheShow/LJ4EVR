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
(no Setup1.Hot line), ME-51 (`font_corpus` fails above 34 skipped rows; `docs/fonts.md` names the 34 rows'
faces and forms), ME-64 (`test_menu` VK 0x83 -> `F20`; the row found F20..F24 printed as "F1:".."F1>", fixed in
`lj_menu_shortcut_text`).

Integrator gate rows not in this pass (brief: build + tests): ME-43 full align, ME-44 pairing / identifier
checks, ME-46 dfm2c `OnFoo` selftest row, ME-47 Items.Data decode (113 truncated rows in `tools/dfm2c_blobs.txt`),
ME-54 shared scrollbar fitting (five `fit_sb` copies remain), ME-57 the five sans13 glyphs, ME-84 sACN name cut.

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
    f76d936 (Linux-only cycle) run 39802625197: green, all six jobs; no ld64 duplicate-library warning;
                               replay-frames.bin sha 776cf859...4a5474 on the three OSes
    955a229 (ME-51, ME-64)     run 39803758128: green, all six jobs; determinism prints replay-frames.bin
                               776cf859abfbb8f418003a944ca7261f1fca89f8eb337a817a5af7ed2c4a5474 for linux, windows, macos
                               https://github.com/UnbannableGT/LJ4EVR/actions/runs/39803758128

## 9. Acceptance (integrator 2, this PC, 2026-10-22)

Design 10 W2, item by item, on Windows 11 (work PC, MSVC 19.44, 4 monitors: 2560x1440 at 0,0 and 0,1440, two
1080x1920 portrait). The walkthrough is `tests/ui/scripts/w2_acceptance.lj4s` (291 lines, 35 asserts) run on a fresh
`Default` library (`<userdir>User\Default`, `<userdir>forms.ini` and `<userdir>Default.lj4b` deleted first) with
`build-ninja-msvc\lj4evr.exe --script` (Release) and `build-debug-win\lj4evr.exe --script` (asserts, debug heap):
both exit 0. Screenshots: `docs/screenshots/w2/<step>-<Class>.png`, one per open window, from the window surfaces.

| # | Item (design 10 W2) | Result | Evidence |
|---|---|---|---|
| 1 | The nine replay scenarios pass | tick | ctest lines below (`replay_all`, `replay_wire_drift`, every build tree) |
| 2 | A MIDI note latches a cue on both OSes | tick on Windows through a real WinMM device; macOS: CI half only, the real-device Mac run stays open for Joey (ME-21) | below |
| 3 | A 10-minute two-slot run shows zero cumulative drift on the wire | tick: 0 ticks at t = 600 s | capture below |
| 4 | The Statics LED turns green on activation; the Statics 2 slot popup shows Martin's duplicated `Replace Page` / `Replace Slot` items | tick | walkthrough: `assert_text TStatics2Form.ReplacePage1` / `ReplacePage2` = `Replace Page`, `ReplaceSlot1` / `ReplaceSlot2` = `Replace Slot` (the popup's DFM items), then `M_RecordCurrentScene`, slot click, `assert_led statics 1`, `assert_view statics_any=1`, `assert_frame 1:17=255`; `statics-TLJMainForm.png` (Statics and Background Cue LEDs green), `statics-TStatics2Form.png` (slot 1 green) |
| 5 | Every 15/02-15/05 id has a hand file (CI check by id) | tick | `profiles_ids` below |
| + | Restart persistence of every W2 window | tick (12 windows; the centred dialogs are never persisted, design 7.10) | below |

### Item 1: ctest (build-ninja-msvc, `ctest -R ^replay_all$ -V`, `-R ^replay_wire_drift$ -V`)

    47: replay bgcue_over_statics: 10 ticks, 19 expects, 0 fails
    47: replay chase_030ms: 57 ticks, 35 expects, 0 fails
    47: replay cue_time_scaling: 2401 ticks, 30 expects, 0 fails
    47: replay layer_flash_current_statics: 12 ticks, 16 expects, 0 fails
    47: replay link_to_cue: 22 ticks, 20 expects, 0 fails
    47: replay loop_bounce_random_limits: 169 ticks, 48 expects, 0 fails
    47: replay seq_intensities: 5 ticks, 10 expects, 0 fails
    47: replay statics_ltp_reorder: 13 ticks, 18 expects, 0 fails
    47: replay transparent_cue_load: 13 ticks, 21 expects, 0 fails
    1/1 Test #47: replay_all .......................   Passed    1.04 sec
    68: replay drift_600s_two_slots_wire: 24004 ticks, 15 expects, 0 fails
    1/1 Test #68: replay_wire_drift ................   Passed    0.87 sec

Full runs after this section's fixes: `build-int-nosdl`, `build-ninja-msvc`, `build-debug-win`, `build-msvc` Release and
Debug, each `100% tests passed, 0 tests failed out of 78`; 0 warnings on every build. `test_layering: 358 file(s) checked,
170 header(s) compiled alone, 0 finding(s)`. `--selftest` (Release and Debug) rc 0, now with a MIDI line:
`selftest MIDI-IN: 1 input device(s) listed, injected 90 3C 7F polled back: OK`; no `no binding for handler`, no
`unknown event`.

### Item 2: MIDI note latches a cue

Devices on this PC: `midiInGetNumDevs` = 1, `Studio 68c MIDI In` (a hardware port with nothing plugged in); no loopMIDI,
but its driver `teVirtualMIDI64.dll` is installed. A 50-line helper (`midiloop.c`, scratch, not in the repo: it creates
the teVirtualMIDI port `LJ4EVR Loop`, which every program sees as a WinMM input, and plays 90 3C 7F / 80 3C 00 into it
after 8 s) gave a real device path:

    midi in 0: Studio 68c MIDI In (rc 0)
    midi in 1: LJ4EVR Loop (rc 0)
    send 90 3C 7F: 1
    send 80 3C 00: 1

- Learn in the app (walkthrough, inject path): MIDI-IN window, Cues tab, `Enable Cue Latch`, number 2, Learn, `midi 90 3C 7F`:
  `assert_text TMidiInForm.lvCue.60.1 "2"`, `MIDIIn\MIDIMap.dat` = `map kind=cue ch=0 note=60 num=2`; New Cue, `assert_view
  cue_number=0`, `midi 90 3C 7F`, `assert_view cue_number=2` (`midi-learn-TMidiInForm.png`: row 60 `2 bounce` selected).
- Real device (recorded which: WinMM, not the inject seam): the same library with `MIDIIn.Ini [SETUP] DeviceCode=1`,
  `midiloop 8 10` started, then the app with `assert_view cue_number=0`, `wait 14000`, `assert_view cue_number=2`: rc 0
  (`midi-device-*.png`). Control run without the port: `midi: no input device 1`, `SCRIPT FAIL line 4: cue_number is 0,
  expected 2`, rc 1.
- macOS: `lj_midi_coremidi.c` builds on `macos-clang` (CI) and the new `--selftest` MIDI line lists the CoreMIDI inputs and
  runs the inject path there; ctest `test_midi_map` runs the dispatch on all three OSes. The real-device Mac run is the ME-21
  line for Joey.

### Item 3: the 10-minute two-slot run on the wire

Library `U:Drift` holding exactly the show of `tests/engine/replay/wire/drift_600s_two_slots_wire.lj4r` (two DMX dimmers at
1:001 / 1:002; seq 1 `flip` 2 scenes x 4000 ms on ch 1, seq 2 `third` 3 scenes x 333 ms on ch 2; cue 1 = slots 1 and 2,
loaded by `Main.Ini [STARTUP] Cue=1`), `lj4evr U:Drift --script` with `wait 615000` (rc 0). Two monitors ran the whole
time: `tools/ref/08-sacn_ref.py monitor` on `ssh <runner-host>` (multicast on <nic>, the W1 copy with
`time.monotonic()` stamps) and the same tool on this PC with `time.perf_counter()` stamps (the clock the app's
`lj_clock_us` reads, so no rate difference between sender and receiver). The cue-load instant is the first ch-1 flip
minus 4.000 s (tick 160). Analysis (scratch `drift_analyse.py`):

    drift_local.log (this PC, QueryPerformanceCounter)
    packets 5908, OOO lines 0, span 614.775 s, max packets in one second 12
    slot 1 flips 153, slot 2 steps 1846
    slot 1 boundary 150: 600.0001 s after the cue load = tick 24000.00 (expected 24000, 600.000 s) -> drift +0.1 ms
    slot 1: worst |observed tick - 160 k| over 153 flips = 0.02 ticks
    slot 2 boundary 1800: 599.4001 s = tick 23976.00 (expected 23976)
    slot 2 boundary 1801: 599.7501 s = tick 23990.00 (expected 23990)
    slot 2 boundary 1802: 600.0752 s = tick 24003.01 (expected 24003)
    slot 2: worst |observed tick - ceil(333 n / 25)| over 1846 steps = 0.03 ticks
    fit of the flips: receiver seconds per sender second = 1.0000000 (-0.0 ppm)
    drift_remote.log (atlas-ci2, its own oscillator)
    packets 5908, OOO lines 1, span 614.797 s, max packets in one second 12
    slot 1 boundary 150: 600.0220 s after the cue load = tick 24000.88 (expected 24000, 600.000 s) -> drift +22.0 ms
    slot 2 boundary 1801: 599.7720 s = tick 23990.88 (expected 23990)
    fit of the flips: receiver seconds per sender second = 1.0000366 (+36.6 ppm)

The monitor lines at the cue load, the first flip and boundary 150 (this PC):

    401519.7931 DATA  <lan-ip> u1     pri 100 seq   0 sc 0x00 LJ4EVR <dev-pc> slots[1..8]=[0, 10, 0, 0, 0, 0, 0, 0]
    401523.7927 DATA  <lan-ip> u1     pri 100 seq  36 sc 0x00 LJ4EVR <dev-pc> slots[1..8]=[255, 10, 0, 0, 0, 0, 0, 0]
    402119.7928 DATA  <lan-ip> u1     pri 100 seq 129 sc 0x00 LJ4EVR <dev-pc> slots[1..8]=[0, 20, 0, 0, 0, 0, 0, 0]

Drift at t = 600 s: 0 ticks (+0.1 ms on the sender's clock). On atlas-ci2 the 22 ms is that receiver's clock running
36.6 ppm fast (600 s x 36.6e-6 = 22.0 ms; the fit over all 153 flips gives the same rate), not the stream: every boundary
of both slots sits on its absolute tick (`ceil(333 n / 25)`, 11 E10); the relative re-arm that would put boundary 1801 on
tick 25214 is absent. Rates: at most 12 packets in any second (ME-24's ~40 ceiling never reached), 3 TERM on quit.
atlas-ci2 printed one `OOO seq 0 after 0` at stream start while this PC's capture of the same stream has one seq 0:
the receiver has two NICs on the LAN (docs/fidelity.md row).

### Item 4: the walkthrough (`w2_acceptance.lj4s`) and what it found

Every step asserts (`assert_*` lines in the script) and leaves screenshots: `loop-options-*` (Bounce 1..3 x2, Link to Cue
0001), `cue-time-*` (slot 1 250 %), `Cues/00000002.cue` line `slot 1 seq=1 on=1 trig=auto dir=bounce spt=1 atpt=1 tol=0
first=1 last=3 loops=2 end=link goto=0 link=1 time=250 chase=0`, `link-to-cue-*` (`cue_number=2`, 30 s later `1`:
pending_link_cue serviced by the app), `chase-*`, `bgcue-*` (`assert_led bgcue 1`, `bg_any=1 bg_number=1`),
`statics-popup-*` / `statics-*` (item 4), `smoke-*` (`smoke_active=1` for the 2 s toggle, then 0), `htp-groups-*`
(`lvGroups.0.2` = `2`), `global-patch-*` (`GlobalPatch.GPA`: `ch 1:2 ... htp=1`, `ch 1:17 ... inv=1 ... htp=1`),
`midi-learn-*` / `midi-latch-*` (item 2), `restore-*` (the restored GPA line asserted again), `system-prefs-*` (Solo
hidden and back, `Main.Ini [SOLO] Enabled=0`), `end-*`. Each window shot was compared with its CHM GIF (`loopcontrol`,
`cuetime`, `bgcuecontrol`, `staticcontrol` + `staticsreplacematrix`, `smokecontrol`, `midiinseq`, `globalpatch`,
`userlibrestore`, `systempreferencesdialog`, `toolandstatusbars`); docs/fidelity.md has the pairs (new section) and the
rows for what still differs.

App bugs fixed (in the app, not scripted around):

| Where | What the walkthrough hit | Fix |
|---|---|---|
| `engine/lj_engine.c`, `windows/lj_cue_form.c` | `cue_number` stayed 0 after Save New Cue: the view took the number only at LOAD_CUE, and the save numbers the pinned record in place | the view reads `e->cue->number` every tick (as `bg_number`); the save writes the number under `lj_lock` |
| `windows/lj_cue_form.c` | Release exit 127 / Debug segfault after the walkthrough: New Cue memset the current cue, which after a Link-to-Cue load is also the `cues` table's record; the MIDI load of cue 0002 then freed it as an unsaved working record and `lj_show_free` freed it again | New Cue on a loaded record leaves it to the table and makes a fresh working record; `test_uiscript_w2` section 7 (load, New Cue, load another, teardown under the debug heap) |
| `windows/lj_bgcue_form.c`, `app/lj_app_seam.c`, `desktop/lj_desktop.h` | a BG cue could not be built in the app: LV1DragDrop and Clear Slot logged "waits for an app re-pin entry", `Save Background Cue` had no behaviour file | the window edits its own record (`lj_bgcue_form_work`), `LJ_ACT_BGCUE_APPLY` (appended) has the app pin a copy and post LOAD_BG in one critical section (an unsaved record is held by the app, not the table); TSaveBGForm (Save / Save New / transparent / Auto Close) in the same file; `LJ_WIN_SAVE_BGCUE` row |
| `ui/widgets/lj_w_listview.c` | the List of Sequences could not drag its already-selected row (OnClick fired only on a new row) | a press on the selected row emits CLICK, as the VCL does; `test_widgets_lists` row |
| `ui/widgets/lj_w_listview.c` | HTP Groups refills every 100 ms and `set_items` reset the scroll: groups 13-24 unreachable; the MIDI-IN grid jumped to note 0 after Learn | the same rows keep the scroll; `lj_w_listview_select` scrolls a row into view (Learn uses it); `test_widgets_lists` rows |
| `platform/sdl/lj_sdl.c` | a drop onto a stay-on-top window under a later-opened one went to the later one | `window_at` tries `SDL_WINDOW_ALWAYS_ON_TOP` windows first |
| `windows/lj_fixture_config.c` | 1 run in 3: `Warnings: address not 1..512` on Save: the 100 ms refresh rewrote `eAdr` between the typed digit and Enter (a typist sees the caret jump home: "17" can become "71") | the refresh leaves a focused edit alone (`eAdr`, `EUserName`) |
| `controls/lj_ctl_movement.c` | Position Presets was built and never shown (`no screen position`) | shown through `lj_win_show` (fsStayOnTop) when created |
| `windows/lj_backup_restore.c` | Target Library Path showed the archive's path | the open library's folder (`userlibrestore.gif`) |
| `windows/lj_bgcue_form.c` | empty BG slots red, `bgcuecontrol.gif` green | empty = green |
| `windows/lj_smoke_control.c`, `ui/widgets/lj_w_fader.*` | Smoke faders read `0%`, the GIF `FF` / `10s` / `35s` | `lj_w_fader.text` hook; hex / seconds |
| `windows/lj_cue_time.c` | no scale ticks on the tracks | nine white ticks (PROPOSAL) |
| `windows/lj_midi_in_form.c` | `Enable ...` box left of the text, the GIF has it right | `taLeftJustify` |

Goldens regenerated and reviewed (REVIEWED.tsv rows): `WIN_BGCUE_FORM`, `WIN_SMOKE_CONTROL`, `W2A_TCUETIMEFORM`,
`W2D_MIDIIN_SEQ`.

Script-side corrections (the app was right): the loop-option edits commit on KeyUp, so each `type` is followed by `key End`;
`REndLoop` has Columns=3 over 4 items, so Link to Cue is column 2 row 2; the bounce run needs 30 s at 250 % (the tick wait
assumed the app had just started); the Smoke button fires while held, so the walkthrough uses the 2 s toggle (shipped
`[SMOKE] Level=0 Duration=0` makes the Timer silent); HTP needs a row selected before Add; Global Patch fills its panel
from the 200 ms timer, so the script waits after moving the row; Bsave is greyed until a BG cue has a number; the dimmer
icon is de-selected by a second click (None first); an existing `Default.lj4b` asks "File already exists", so the archive is
deleted before a run. New verb `move <Form> x y` (docs/ui-script.md): every window opens centred, and the List of
Sequences covers the Background Cue window.

Not fixed, recorded: every window opens centred until moved, where VCL `poDesigned` uses the DFM Left/Top (fidelity row);
the Smoke cycle-progress bar and button art, the Restore folder tree (W4); `GroupInfo.dat` logs `rec: cannot open` /
`not an LJ4 groups file` once on a fresh library (`lj_show_save_config` reads the absent file for its htp lines; harmless,
defaults used); `MIDI-IN: device -1 did not open` prints -1 for the requested index when the open fails.

### Persistence (forms.ini)

`tests/ui/scripts/w2_positions_1.lj4s` (open, `move`, close every persisting W2 window) then `w2_positions_2.lj4s`
(reopen, `assert_pos`): both rc 0. `forms.ini` after the pair:

    [LOOPFORM] Left=100 Top=1600   [CUETIMEFORM] Left=140 Top=1620 Width=670 Height=309   [BGCUEFORM] Left=180 Top=1640
    [SAVEBGFORM] Left=220 Top=1660   [STATICS2FORM] Left=260 Top=1680 Width=510 Height=450   [SMOKEFORM] Left=300 Top=1700
    [PRESETFORM] Left=380 Top=1740 Width=210 Height=402   [GROUPFORM] Left=420 Top=1760 Width=220 Height=250
    [HTPGROUPSFORM] Left=460 Top=1780   [SNAPSHOTSCENEFORM] Left=540 Top=1820   [MIDIINFORM] Left=580 Top=1840

Position Presets and User Palette are one class (TPresetForm), so they share `[PRESETFORM]` as in LJ2: Position Presets
reopens where the palette window was moved last. The W2 forms declaring `poOwnerFormCenter` / `poMainFormCenter` /
`poScreenCenter` / `poDesktopCenter` (Global Patch, Intensity Groups, System Preferences, the Delete dialogs, User Libraries,
Backup, Restore, Notes, About, Striking Lamps, Edit Hotkeys, cue page dialogs) are centred each time and never persisted
(design 7.10). Default DMX Output has no window (`lj_default_output.h`). The positions are on the second display: on this
PC a `SetWindowPos` onto the primary display (y < 1440) lands 1440 px lower, raw Win32 included (measured with
`GetWindowRect`), so the scripts stay on display 2.

### Item 5: profiles_ids (ctest -R ^profiles_ids$ -V)

    64: check_profiles: tier 1 15/01 MAC 500 / Roboscan 918: 4 of 4 ids have a matching hand file
    64: check_profiles: tier 2 15/02 MAC 600: 2 of 2 ids have a matching hand file
    64: check_profiles: tier 3 15/03 MAC 250 family: 8 of 8 ids have a matching hand file
    64: check_profiles: tier 4 15/04 MAC 2000 family: 11 of 11 ids have a matching hand file
    64: check_profiles: tier 5 15/05 Roboscan 518 / 812 / 218: 9 of 9 ids have a matching hand file
    1/1 Test #64: profiles_ids .....................   Passed    0.05 sec

### docs/fidelity.md

Rows appended: Smoke readouts (closed), Smoke progress bar / art (W4), Cue Time ticks (PROPOSAL), BG empty slots (closed),
Restore window, MIDI-IN, first open of a `poDesigned` form, the seq-0 duplicate (receiver side). New section "Fidelity
contract rows touched by W2": F8, F20, F25, F30, F48, F51, F52, I2, each with its screenshot pair or `deferred: needs rig`.

### CI

    acaee38 (W2: acceptance walkthrough)  run 39815257660: macos-clang red, `lj_sdl.c:296` clang
                                         -Wlogical-not-parentheses in the new window_at pass test (MSVC and gcc are silent on it)
    2804ca4 (the fix)                     run 39816435207: green, all six jobs
                                         https://github.com/UnbannableGT/LJ4EVR/actions/runs/39816435207
                                         macos-clang: selftest MIDI-IN: 0 input device(s) listed, injected 90 3C 7F polled back: OK
                                         windows-msvc (Release and Debug): the same line, 0 devices on the hosted runner
                                         replay-frames.bin linux / macos / windows 776cf859abfbb8f418003a944ca7261f1fca89f8eb337a817a5af7ed2c4a5474
                                         (unchanged by this pass: no engine output changed)
