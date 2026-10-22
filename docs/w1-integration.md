# W1 integration log

Integrator 1 session, 2026-10-21, work PC (MSVC 19.44, CMake 4.1, VS 17 2022 x64, Ninja). Commands and outputs recorded as
run; every blocker a track returned is resolved below (design.md wins on disagreement; the smallest change each time).

## 1. Pre-integration commit

    git status            -> 364 paths (fourteen tracks); reference/ and build-*/ ignored; three build-*.log files at the root
    .gitignore            -> + build-*.log (the desktop track's configure/build logs stay local)
    git commit "W1: tracks landed (pre-integration)"   -> 6fe3c3a, pushed (54cff94..6fe3c3a)

## 2. Full-tree build

    cmake -S . -B build-nosdl -G Ninja -DLJ_SDL=OFF -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=cl   (vcvars64)
    cmake --build build-nosdl          -> 210 steps, every library + lj4evr_tests.exe + lj4evr_render.exe, 0 warnings, 0 errors
    cmake --preset dev-ninja-msvc && cmake --build --preset dev-ninja-msvc   (LJ_SDL=ON)   -> lj4evr.exe, rc 0
    cmake --preset dev-msvc && cmake --build --preset dev-msvc               (VS 17 2022)  -> rc 0
    cmake --preset debug-win && cmake --build --preset debug-win --target lj4evr           -> rc 0 (asserts on)

The in-flight compile errors the tracks reported against each other (lj_desktop_tabs.h, calloc in lj_ctl_lamp.c, C4828 in
lj_profiles_gen.c, test_windows_b.c C2001, lj_startup.c C2015, lj_uiscript.c C2220, test_cfg.c/test_packets.c C2220) were all
gone by the time the tree was committed: every track finished its own file.

First ctest (build-nosdl, 52 rows): 5 failures: render, replay_all, replay_dump, test_layering, test_sacn_loopback.

## 3. Resolutions

| Blocker (track) | Resolution |
|---|---|
| `test_layering`: `model/lj_library.c` includes `<sys/stat.h>` (model) | `tools/test_layering.py` OS_ALLOW row `model/lj_library.c: {sys/stat.h}` (POSIX mkdir; Windows uses `<direct.h>`/`<io.h>`) -> 0 findings |
| replay `blackout_after_masters` rows 16/18/24: GM leaves the generic dimmer at 255 (replay vs engine/fixlib) | design 10 W1 acceptance 7 ("Grand Master 50 % halves MAC 500 ch 2 and the dimmer") and note 15/10 s6 win. Fix in fixlib, not the engine: the QLC+-derived profiles use `LJ_CH_LEVEL` for moving-light channels the GM must not touch, so S3 cannot scale the class. Every generic level channel (DMX Dimmer n, RGB Pack, Smoke, Hazer, Color Scroller 1, Generic Scanner 1, Generic Fixture 2) now carries `.ext_gm = { 1.. }` in `src/fixlib/hand/lj_generics.c`, the engine's existing Extended-GM path (`lj_post_s3_masters`). Rule ids: 11 G3/G4 (standard scaling), ME-25 untouched (SWITCH packs stay `use_gm = 0`). `docs/fidelity.md` row. Scenario unchanged: 15/15 pass |
| replay note: Blackout of the generic dimmer (note 15/10 s6 "blacked out by blackout", no CHM quote) vs design 10 acceptance 7 ("leaves the dimmer") | design wins; recorded in `docs/fidelity.md` |
| replay note: design 3.5 / ME-13 unfreeze formula written as max(t0 + frozen_us, unfreeze_us), behaviour described (and implemented, `freeze_pauses_clock` green) is the min | doc nit for the design owner (vault, not editable here): the formula text, not the behaviour |
| `render`: main window client 791x560 vs the W0 stub's 799x606 (desktop) | the desktop's number is the DFM's (TLJMAINFORM 799x606 outer, 791x560 client, 07 s4.1); `tests/ui/test_render.c` expects 791x560 and prints the mismatch |
| `test_sacn_loopback` flaky under `ctest -j` (sacn) | `test_sacn_rate` also sends to 127.0.0.1:5568; both rows get `RESOURCE_LOCK sacn_port` in CMakeLists.txt. Serial ctest (CI) never saw it |
| ME-62 `lj_miniz` `/w` -> D9025 (fixlib) | `/W0`; the target stays because `model/lj_library.c` (backup/restore, `.lj4b`) and `app/lj_uiscript.c` include miniz; the dependency moved from `lj_layer(fixlib)` to `lj_layer(model)` |
| `qxf2c.py --selftest` and `dfm2c.py --selftest` as CTest rows (fixlib, ME-49) | `qxf2c_selftest`, `dfm2c_selftest` inside the Python3 block |
| windows-b rows missing from `lj_win_table` (`lj_windows.c`): Cue control, Save Cue, List of Cues, Master Intensity opened as "no behaviour file yet"; `lj_cue_form_bind_fns` / `lj_master_intensity_bind_fns` never called | rows filled from the windows-b headers; `lj_win_set_host` binds all three windows' lj_fn rows. `tests/ui/test_uiscript.c` row 4 used `open cue_form` as the "no window" case: now `open warnings` |
| controls-b/c windows not reachable through `lj_ctl_open` (controls-a blocker) | `lj_ctl_open` dispatches MOVEMENT to `lj_ctl_movement_create` (+ `select_from_desktop`) and COLORS/GOBOS/BEAM/EFFECTS to `lj_ctl_c_create`; the registry row carries `foreign = 1` so `lj_ctl_close` / `window_get` / `toggle` / `open_only` / `refresh` (the 100 ms hook calls the kind's refresh) see them and the app host opens them |
| nothing bound the toolbar's control / monitor / freeze / offline rows nor the desktop actions (app track; the W0 noop table was superseded) | `src/app/lj_app.c`: `LJ_FN_CTL_INTENSITY..LAMP` -> `lj_ctl_toggle` (button 7 = Levels, or Extended for a profile without a Levels grid, 15/01 s3), `LJ_FN_VIEW_DMX_OUTPUT`, `LJ_FN_TOGGLE_LIST_OF_CUES`, `LJ_FN_FREEZE_OUTPUT_TOGGLE` (`LJ_CMD_FREEZE`), `LJ_FN_DMX_ONLINE_OFFLINE_TOGGLE` (`LJ_CMD_OFFLINE`); `lj_desktop_bind_action` for CLOSE (the prompt), FIXTURE_CONFIG, HARDWARE_SETUP, STARTUP_OPTIONS, SAVE_SEQUENCE, SAVE_CUE, CTL_OPEN_ONLY; `lj_ctl_set_host(&lj_app_host)`. The app's two quick-save binds were dropped: the windows' handlers (dialog on a never-saved record, `lj_ss_sync_table`) are the faithful ones |
| LORD_FIDELITY-11 defaults (desktop): the app filled `allow_freeze/allow_offline/allow_solo/show_nextprev` with 0 | `lj_app.c` sets all four to 1 after the context is filled (LJ2 ships them ON, 01 s6.1; System Preferences W2 reads Main.Ini) |
| ME-48 dfm2c half (desktop): 35 TSEQUENCEFORM / TCURRENTCUE buttons listed in `glyphmap.tsv` had no `.glyph` because the truncated `dfm_text` never carried their `Glyph.Data` | `tools/dfm2c.py` emits `.glyph` for a listed TBitBtn/TSpeedButton with no blob (`dfm2c_blobs.txt` row "row listed, blob absent"); `glyphmap.tsv` row `sb11 add-to-cue` -> `addtocue` (the art stem); `lj_forms_gen.c` regenerated (35 rows gain `.glyph`); the 95 remaining `"missing"` are TBitBtns of forms with no glyphmap row (W2+). Goldens TSEQUENCEFORM / TCURRENTCUE regenerated with glyphs |
| `lj_library_create` had no `Hotkeys` folder (actions) | `"Hotkeys"` appended to `lj_lib_folders`. The app still resolves `lj_hotkeys_setup_path(pref, 1)` under the pref dir, not the library: one "rec: cannot open .../Hotkeys/Setup1.Hot" log line per start until W2 decides where setups live |
| controls-a: the fixlib hand file's Lamp rows did not follow `lj_ctl_common.h` (captions "Power On/Off" too wide for the 41-px buttons, NOTE without the newline) | `lj_mac500.c`: buttons "Lamp On" / "Lamp Off" (exe palette strings), NOTE with the `\n` after "NOTE !"; `lj_ctl_lamp.c` draws our bulbs (`art/glyphs/lamp.txt`, new `lampoff.txt`, artpack 115 entries) with the caption as hint, like lampcontrol.gif |
| controls-c: gobo tiles painted black (the hand file names a tile "Cone", the art stem is `gobo_cone`) | `lj_ctl_colors.c` `entry_look`: a bitmap name that is no glyph is retried as `gobo_<lowercase>` |
| the memory gauge (desktop), hotkey dispatch from the platform (`lj_desktop_key` has no caller), `lj_ctl_set_confirm` (confirms log and proceed), the acceptance `--script` widget names | left for W2 (recorded in section 7); none is in the design 10 W1 acceptance list except the memory panel text, which paints blank |

## 4. Tests

    ctest --test-dir build-nosdl -j 6        -> 100% tests passed, 0 tests failed out of 55
    ctest --test-dir build-ninja-msvc -j 6   -> 100% tests passed, 0 tests failed out of 55
    ctest --test-dir build-msvc -C Release   -> 100% tests passed, 0 tests failed out of 55
    replay (15 scenarios): fade_linear_40hz 121 ticks 10 expects, snap_050ms 81/7, fade_060ms 11/7, interrupted_fade_restart 161/8,
        cue_priority_slot12 4/7, off_is_transparent 3/7, pair16_pan_true16 46/47, gm_examples_1_to_6 12/48, blackout_after_masters 5/8,
        htp_group_max 5/6, freeze_pauses_clock 531/20, drift_600s_two_slots 24001/33, disabled_offset_silent 1/3, stop_holds 311/6,
        seq_intensities 4/4: 0 fails each (blackout_after_masters was 3 fails before the ext_gm fix)
    test_engine_perf: max tick 230 us over 100 ticks (budget 2000 us), Release
    font_corpus: 826 rows, 34 skipped (face not packed), 0 mismatches   (ME-51 ceiling unchanged)
    test_layering (cl): 285 file(s) checked, 135 header(s) compiled alone, 0 finding(s)
    test_hotkeys: 450 functions, 17 groups; test_fixlib_profiles: rows 204, hand 36, qxf 40, extras 3
    test_sacn_loopback: announce 6, change 3, keep-alive 2, remap 3 term + 3 new, close 6 term; 0 send errors
    dump-frames f1.bin ; dump-frames f2.bin ; cmp -> identical, sha256 5e61478e89c1ccfd4936ce7110a366e4f497c5ac0684493bc29ef189342c6ee7 (400 ticks; the W0 stub's 64236b68... retires with the real engine, ME-60)
    replay_dump -> replay-frames.bin 103819848 bytes (drift_600s_two_slots is 24000 of its 25300 ticks; kept as the replay track wrote it)
    build-ninja-msvc/lj4evr.exe --selftest   -> startup OK; TSEQUENCEFORM screen 360x150 vs surface 360x150, 0 mismatching pixel(s): OK, rc 0
    build-debug-win/lj4evr.exe --selftest    -> same two OK lines, rc 0, no "no binding for handler" line (ME-16 / ME-38)

Rows added by the integrator: `ui_golden_ctl` (`tests/ui/test_golden_ctl.c`), `dfm2c_selftest`, `qxf2c_selftest`; `ui_golden` grew from 6 to 10 forms.

## 5. Goldens (tests/ui/golden, REVIEWED.tsv)

Every W1 form that renders, reviewed at 2x with Read against the DFM numbers and the CHM GIFs. Of the control-window
GIFs only levelcontrol.gif and movement2.gif are 1:1; intensitycontrol, lampcontrol, beam, extended, colorcontrol,
gobocontrol, effectscontrol and the colourrotate / goboindex / goboshake / goboscroll crops are 0.8 reductions (ME-67,
measured by the W1 council: their O/S/F reads 39 px where off.gif, a 1:1 crop, is 48x24; menu words 44/45/22/16 px
against 55/57/28/20 in levelcontrol.gif), read x1.25 since the fix pass (section 10):

| Golden | Size | Source |
|---|---|---|
| TSEQUENCEFORM, TCURRENTCUE | 472x207, 612x300 | regenerated with glyphs (ME-48, W0-glyphs) |
| TDMXOUTFORM, TMASTERINTENSITYFORM, TSAVECUEFORM, TSTARTUPFORM | 356x264 client, 670x341, 337x172, 367x410 | new DFM goldens (`ui_golden`); the monitor grid and the master faders are code-built at open and absent from the bindings-free render |
| CTL_MAC500_{INTENSITY,MOVEMENT,COLORS,GOBOS,BEAM,EFFECTS,LAMP,EXTENDED} | 600x122, 332x284, 382x190, 422x216, 257x96, 257x86, 166x125, 352x159 (fix pass r1; the first integration built them to the reduced captures: 480x98, 307x166, 339x187, 206x86, 207x78, 133x100, 283x128) | `ui_golden_ctl`: MAC 500 Mode 2/4 (built-in 12) at 1:001 as master, every window its table offers through `lj_ctl_open` |
| CTL_DMX1_LEVELS | 272x250 | the same show, Generic DMX Dimmer 1 (built-in 19) at 1:017 as master |

TFIXTURECONFIGFORM is TLINKCONFIGFORM3 (no such DFM class), already a golden since W0. Deviations found and recorded in
REVIEWED.tsv: the Movement Resolution bar paints its arrows over the caption; code-built menus carry no mnemonic underlines.
(The first integration recorded "our sans13 text is ~20% wider than the GIFs' text" with clipped captions in Intensity, Beam
and Extended and a Beam client 9 px taller than beam.gif: all one error, the windows were built to 0.8-scale captures while
the font is right, ME-67; retired by the fix pass, section 10.)

## 6. Gates touched (docs/w1-gates.md)

ME-48 done (dfm2c half + goldens), ME-49 half (`dfm2c_selftest` row; the pre-commit hook install is in section 7), ME-62 done,
ME-16/ME-38 evidence (Debug selftest clean), LORD_FIDELITY-11 defaults set, LORD_FIXTURES-11 / ME-25 unchanged (SWITCH packs
stay unscaled; generic levels scale through ext_gm).

## 7. Left open (recorded, not blocking)

- Hotkeys: `lj_desktop_key` has no caller (the platform's key events reach the focused widget only); the default set is a
  PROPOSAL (Setup 1, four rows from hotkeys1.gif). The app looks for `<pref>Hotkeys/Setup1.Hot`, the library now has a
  `Hotkeys/` folder: W2 picks one and wires the dispatch.
- Memory gauge blank (desktop/ may not read the OS; the app has no free-memory query yet).
- `lj_ctl_set_confirm` unwired: Lamp Off / Reset confirms log and proceed (windows/ offers no Yes/No box with a callback the
  controls can take without including app/).
- The acceptance `--script` (`tests/ui/scripts/w1_acceptance.lj4s`) names widgets the integrator was to align; not run this
  session.
- `.git/hooks/pre-commit` (ME-49): installed on this PC (`cp tools/hooks/pre-commit .git/hooks/pre-commit`, 821 bytes, -rwxr-xr-x); every integration commit above ran `dfm2c --check: OK` through it.
- gcc/clang warnings: no gcc/clang on this PC; CI is the first pass over the W1 tree.

## 8. CI

    git commit "W1: integrated build, tests green locally" -> d21a0a7, push
    gh run list --workflow ci.yml   -> 39693095982 (superseded by the next push; the pre-integration run 39690714057 had failed on
                                       gcc/clang -Wcomment plus the three local failures of section 2)

Round by round (each: `gh run view <id> --log-failed`, fix, commit, push, `gh run watch --exit-status`); Windows was green
from round 1, Linux/macOS are the first gcc/clang pass over the fourteen tracks' files:

| Run | Result | Cause | Fix (commit) |
|---|---|---|---|
| 39693221088 | windows green; gcc/clang red | `/*` inside block comments (`fixlib/lj_profile.h`, generated `lj_profiles_gen.c` header via `qxf2c.py`, `tests/lj_tests.c`, `tests/engine/test_replay.c`); gcc `-Wformat-truncation` in `model/lj_library.c` | 364812f |
| 39693896575 | gcc/clang: `io/lj_net.c` IFF_* and gethostname undeclared under `-std=c11` (glibc feature set); clang `-Wsign-compare` int vs `enum lj_ctl_kind` in `lj_ctl_open_only` | ab8295a: `#define _DEFAULT_SOURCE 1`, `(int)kind` |
| 39694542526 | gcc/clang on windows/, controls/, desktop/: `-Wmissing-field-initializers` (`lj_engine_status x = { { 0 } }` -> `{ 0 }`, three positional `lj_ctl_desc` rows gain `.tag = 0`), clang `-Wempty-body` (five `for (...) ;` -> `{ }`), gcc `-Wmisleading-indentation` (`lj_fixture_config.c` two `if; if` lines split), gcc `-Wformat-truncation` (`lj_hardware_setup.c` result checked, `lj_dmx_monitor.c` t[16], `lj_toolbars.c` counter[24]) | c735264 |
| 39695091515 | linux-clang green; gcc/clang on tests/: `test_controls_a.c` positional `lj_ctl_row`, `test_desktop.c` unused `root`, `test_loopback.c` nanosleep needs `_POSIX_C_SOURCE`; macOS `app/lj_app.c` host struct missing the windows-c `move`/`pos` fields | 1ea106d |
| 39695091515 (same) | ASan: `test_windows_c` heap-use-after-free in `lj_window.c` `bound_dispatch` (the warning box's Cancel handler destroys its own window, the dispatcher then read the freed binding record); LSan: `test_fixlib_profiles` 6840 bytes from `lj_layout_derive` on a stack profile | 8e26039: `bound_dispatch` copies the handler and returns after calling it (one handler per event name); the test's synthetic profile is `static` (the derived tables are process-lifetime by design); `lj_app_host` gains `move`/`pos` wrappers over `lj_platform_move_window` / `lj_platform_window_pos` |
| **39695729252** | **all six jobs green** | | |

Green run: https://github.com/UnbannableGT/LJ4EVR/actions/runs/39695729252 (main @ 8e26039)

    generate-check  test_layering (gcc) 285 file(s), 135 header(s) alone, 0 findings; qxf2c/fontpack/artpack/gen_vectors --check current
    linux-gcc-asan  100% tests passed, 0 tests failed out of 55   (Debug, -fsanitize=address,undefined)
    linux-clang     100% tests passed, 0 tests failed out of 55
    windows-msvc    100% tests passed out of 55; selftest TSEQUENCEFORM 360x150, 0 mismatching pixel(s): OK (Release and Debug, no "no binding for handler"); NSIS installer artifact
    macos-clang     100% tests passed out of 55; selftest TSEQUENCEFORM 360x150, 0 mismatching pixel(s): OK; DragNDrop artifact; test_sacn_loopback passed on all three OSes (unicast 127.0.0.1, no entitlement or allowlist edit needed)
    determinism     frames-linux.bin = frames-macos.bin = frames-windows.bin, 1641600 bytes,
                    sha256 5e61478e89c1ccfd4936ce7110a366e4f497c5ac0684493bc29ef189342c6ee7 on all three (= this PC; ME-60's new reference sha)

The container image digest did not change (no Dockerfile edit, ME-20).

## 9. Acceptance (integrator 2, this PC, 2026-10-21)

Design 10 W1, item by item. Items 1-5, 7, 8 and 10 are `tests/ui/scripts/w1_acceptance.lj4s` run on a fresh
`Default` library (`rmdir User\Default`, the app recreates it) with `build-ninja-msvc\lj4evr.exe --script`; item 10's
restart is `tests/ui/scripts/w1_positions.lj4s`; item 6 is the LAN capture below. Both scripts exit 0 on the Release
and the Debug (asserts, debug heap) build; every assert is in the scripts. Screenshots: `docs/screenshots/w1/acceptance*-<Class>.png`
(one file per open window, from the window surfaces, which `--selftest` proves equal the screen pixel for pixel).

| # | Item | Result | Evidence |
|---|---|---|---|
| 1 | Launch, `Martin LightJockey (User lib: Default)`, three toolbars, status bar, classic chrome | tick (Windows 11); macOS = CI build + `--selftest` only, the ME-21 line stays open for Joey | `assert_caption main`, `assert_visible main.tb_row_seqcue/cuelist/fixture`, `main.sb_status`; `acceptance1-main-TLJMainForm.png` 799x606 |
| 2 | Fixture Configuration adds MAC 500 Mode 2/4 at 1:001 and Generic DMX Dimmer 1 Channel at 1:017; both in the Unassigned list; dragged onto the desktop as `(1) MAC500` / `(2) DMX1`; `Config.dat` text, reloads on restart | tick | tree walked by keyboard (`sbMartin`, `Right`, `Down 60`, `Right`, `Right`; `sbGeneric` ...), `LFixtures` = `2 Fixtures configured`, `lAddresses` = `17 DMX Addresses Used`; `assert_file Config.dat exists`; `LV_Unassigned.0.1` = `MAC 500`; two `drag` lines; `main.icon1` = `(1) MAC500`, `main.icon2` = `(2) DMX1`; restart: `w1_positions.lj4s` asserts both tiles again. `Config.dat` of the run: `fx 1 kind=builtin id=12 gs2= link=1 addr=1 ... user=MAC500 placed=1 x=60 y=65 tab=0 sub=0` / `fx 2 kind=builtin id=19 ... addr=17 ... user=DMX1 placed=1 x=160 y=65`. Shots `acceptance2-config-TLinkConfigForm3.png` (790x590), `acceptance2-unassigned-TUnassignedForm.png`, `acceptance2-desktop-TLJMainForm.png` |
| 3 | MAC 500 master: Intensity, Movement, Colors, Gobos, Beam, Effects, Levels (7 = Extended), Lamp, Reset, Blackout enabled; dimmer master: only Levels | tick | `assert_enabled main.tb_fixture_1..9 1`, `tb_fixture_22 1`; dimmer: `tb_fixture_1 0`, `tb_fixture_2 0`, `tb_fixture_7 1`, `tb_fixture_8 0`; `acceptance3-toolbar-TLJMainForm.png` |
| 4 | Scene 1 shutter open, intensity 100 %, dimmer 50 %, pan/tilt home, all Snap; Add scene; scene 2 intensity 0 %, pan/tilt -16256 : 17120, dimmer 100 %, intensity + pan/tilt Fade, fade 3.00 s, scene 4.00 s; counter `2:2`; Save -> 0001, toolbar field yellow | tick (pan/tilt -16212 : 17245: the Goto Position click lands on the 191-px grid's nearest pixel; -16256 : 17120 is not a pixel) | `sb_seqcounter` `1:1` -> `2:2`; `ESeqNo` `0001`, caption `SEQUENCE [0001 first]`, `tb_seqname` `first` (yellow in `acceptance4-sequence-TLJMainForm.png`); the record `Sequences/00000001.seq`: `scene st=1000 ft=50` / `ch 1:1=20:S 1:2=255:S 1:11=128:S 1:12=0:S 1:13=128:S 1:14=0:S 1:17=128:S` / `scene st=4000 ft=3000` / `ch 1:1=20:S 1:2=0:F 1:11=64:F 1:12=172:F 1:13=195:F 1:14=94:F 1:17=255:F`; shots `acceptance4-sequence-TSequenceForm.png` (360x150 = 354x106 client, the shrink PROPOSAL), `-TIntensity.png` (600x122 since the fix pass), `-TMovement.png`, `acceptance4-levels-TLevels.png` (the Save Sequence dialog has AutoClosed before the shot and is not captured, ME-68) |
| 5 | Cue window: Add Sequence to Current Cue -> slot 1; row `0001 | first | | 1:2 | 1:00 | 0:05 | | Fwd | Auto`; Save Cue -> 0001, caption `CUE [0001 first cue]`; Go/Stop work; `Cue Modified` clears on save | tick (ST/FT show the scene the slot is at: `1:00` / `0:05` before Go, the design's `4:00 | 3:00` are scene 2's; colon and capitals per cuecontrol.gif at 8x, ME-75) | `lj_cue_grid.1.0` `0001`, `.1.1` `first`, `.1.3` `1:2`, `.1.4` `1:00`, `.1.5` `0:05`, `.1.7` `Fwd`, `.1.8` `Auto`; `sb_cuemod` `Cue Modified` -> `""`; after `sbGo`: 0.5 s `1:1=20 1:2=255 1:11=128 1:12=0 1:13=128 1:14=0 1:17=128`, 4.5 s `1:2=0 1:17=255 1:11=64 1:12=172 1:13=195 1:14=94`, 5.5 s (scene boundary) `1:2=255 1:17=128 1:11=128`; `sbStop` holds; `acceptance5-cue-running-TCurrentCue.png` (612x300 outer) |
| 6 | The wire: source `LJ4EVR <host>` on universe 1, priority 100, 0 sequence errors, ~40/s while fading never 80, ~1/s idle; ch 2 255 -> 0 and dimmer 128 -> 255 over 3.0 s +/- 25 ms, hold 1 s, snap back; 16-bit pan LSB non-zero and monotonic; Offline ~1/s with held values, source visible; Online resumes | tick (receiver on atlas-ci2, see below) | capture analysis below |
| 7 | GM 50 % halves ch 2 and the dimmer only; Blackout closes the shutter, leaves the dimmer; Freeze holds, counter advances; Offline -> ~1/s within 1 s, values held, 0 errors; Online resumes | tick | `assert_view gm=128`, `1:2=128`, `1:17=64` (128 * 128 / 255), `1:1=20`; Blackout `1:1=0`, `1:17=64`; Restore; `tb_fixture_20` `freeze=1` / `0`; `tb_fixture_19` `offline=1`, `assert_pps 1 0 2`, `offline=0`; `bAll100` `gm=255`, `1:17=128`; shots `acceptance7-blackout-TMasterIntensityForm.png` (670x341 = 664x297 client), `acceptance7-freeze-TFreezeWarnForm.png`, `acceptance7-offline-TOfflineWarningForm.png`, the wire lines below |
| 8 | DMX Output Monitor: same numbers, grey/green cells, red changed text; overwrite 1:017 = 200 green and on the wire | tick | `M_EnableOverwrite`, cell click at `sg1 177 41`, `sevalue` `200`; `assert_frame 1:17=200`; on the wire `seq 170 ... 200`; `sbClear` -> `1:17=128`; `acceptance8-monitor-TDMXOutForm.png` (the master's 16 channels green, 1:017 selected, status `1:017 (2) DMX1 - Level`) |
| 9 | ctest on all three OSes, replay scenarios, determinism | integrator 1's item; locally after this session's fixes: `ctest --test-dir build-nosdl -j 6` -> `100% tests passed, 0 tests failed out of 55`; `build-ninja-msvc` -> `100% tests passed, 0 tests failed out of 55`; `test_layering: 285 file(s) checked, 135 header(s) compiled alone, 0 finding(s)`; CI: see section 10 | |
| 10 | Every window reopens at its last position (`forms.ini`, LJ2's section names); Sequence 354x106 (shrink PROPOSAL), Cue 612x300, Fixture Configuration 790x590, Master Intensity 664x297 | tick | after the walkthrough `forms.ini` = `[CUE] Left=973 Top=546 Width=612 Height=300 [SEQ] Left=1099 Top=621 [INTENSITY] ... [MOVEMENT] ... [LEVELS] ... [SAVESEQFORM] ... [SAVECUEFORM] ClearSave=0 Left=1111 Top=610 [STATUSWARNINGFORM] ... [FREEZEWARNFORM] ... [DMXOUTFORM] Left=1101 Top=564 Width=356 Height=264`, `Main.Ini [GLOBALINTENSITY] Left=944 Top=525`; the files edited to `[SEQ] 300 300`, `[CUE] 400 350`, `[INTENSITY] 100 100`, `[DMXOUTFORM] 700 200`, `[GLOBALINTENSITY] 500 400`; restart `w1_positions.lj4s`: `assert_pos TSequenceForm 300 300`, `assert_client TSequenceForm 354 106`, `assert_pos TCurrentCue 400 350`, `assert_client TCurrentCue 604 254` (612x300 outer), `assert_pos TMasterIntensityForm 500 400`, `assert_client 664 297`, `assert_pos TDMXOutForm 700 200`, `assert_client 348 218`, `assert_pos intensity 100 100`, `assert_client TLinkConfigForm3 782 544` (790x590 outer, poMainFormCenter: not persisted, as LJ2); `acceptance10-restart-*.png` |
| 11 | `release.yml` on `v0.1.0` | waits for the tag | |

### Item 6: the LAN capture

Receiver: `ssh <runner-host>`, `tools/ref/08-sacn_ref.py` copied to `/tmp/sacn_ref.py` with its DATA line widened
to `slots[1..20]` and prefixed with `time.monotonic()` (the only edits), `python3 -u /tmp/sacn_ref.py monitor <lan-ip> 1`
(<nic>, same LAN as this PC's <lan-ip>). `Hardware.Ini [SACN] Nic=<lan-ip>` for the run (with `Nic=0.0.0.0`
Windows put the multicast on the Hyper-V vEthernet and atlas-ci2 saw nothing; recorded in docs/sacn.md's terms as the NIC
pick a user makes in Hardware Setup). The walkthrough's packets, 18.9 s from the first programming edit to the overwrite clear:

    packets 176, source "LJ4EVR <dev-pc>" (the 16 chars the monitor prints of "LJ4EVR <dev-pc>"), priority 100,
    start code 0x00, sequence steps != +1: 0 (one duplicated seq 0 at stream start, see docs/fidelity.md)
    packets per second: [4, 1, 15, 3, 1, 22, 40, 40, 21, 4, 1, 1, 8, 2, 1, 1, 1, 1, 9]
    GO on the wire at 3.774 s; fade first step 5.474 s, last step 8.474 s: 3.000 s, 121 change packets (25 ms tick)
    pan16 monotonic down 32768 -> 16556, tilt16 monotonic up 32768 -> 50014, pan LSB non-zero on 120 of 120 intermediate ticks
    intensity monotonic down 255 -> 0, dimmer monotonic up 128 -> 255
    hold after the fade 1.000 s, then (20, 255, 128, 0, 128, 0, 128) = scene 1 snapped back
    seconds at 1 packet/s (idle and the Offline span): 1, 4, 10, 11, 14, 15, 16, 17

The monitor's lines (first packet; GO; fade start, +1 tick, middle, end; the snap back; GM 50 %; Blackout; Restore; GM 100 %;
overwrite 200; clear; the three terminations on close):

    209560.115 DATA  <lan-ip> u1     pri 100 seq   0 sc 0x00 LJ4EVR <dev-pc> slots[1..20]=[0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
    209563.889 DATA  <lan-ip> u1     pri 100 seq  20 sc 0x00 LJ4EVR <dev-pc> slots[1..20]=[20, 255, 0, 0, 0, 0, 0, 0, 0, 0, 128, 0, 128, 0, 0, 0, 128, 0, 0, 0]
    209565.589 DATA  <lan-ip> u1     pri 100 seq  24 sc 0x00 LJ4EVR <dev-pc> slots[1..20]=[20, 254, 0, 0, 0, 0, 0, 0, 0, 0, 127, 174, 128, 87, 0, 0, 129, 0, 0, 0]
    209565.614 DATA  <lan-ip> u1     pri 100 seq  25 sc 0x00 LJ4EVR <dev-pc> slots[1..20]=[20, 252, 0, 0, 0, 0, 0, 0, 0, 0, 127, 39, 128, 231, 0, 0, 130, 0, 0, 0]
    209567.089 DATA  <lan-ip> u1     pri 100 seq  84 sc 0x00 LJ4EVR <dev-pc> slots[1..20]=[20, 126, 0, 0, 0, 0, 0, 0, 0, 0, 96, 4, 162, 6, 0, 0, 192, 0, 0, 0]
    209568.564 DATA  <lan-ip> u1     pri 100 seq 143 sc 0x00 LJ4EVR <dev-pc> slots[1..20]=[20, 1, 0, 0, 0, 0, 0, 0, 0, 0, 64, 225, 195, 37, 0, 0, 255, 0, 0, 0]
    209568.589 DATA  <lan-ip> u1     pri 100 seq 144 sc 0x00 LJ4EVR <dev-pc> slots[1..20]=[20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 64, 172, 195, 94, 0, 0, 255, 0, 0, 0]
    209569.589 DATA  <lan-ip> u1     pri 100 seq 148 sc 0x00 LJ4EVR <dev-pc> slots[1..20]=[20, 255, 0, 0, 0, 0, 0, 0, 0, 0, 128, 0, 128, 0, 0, 0, 128, 0, 0, 0]
    209572.315 DATA  <lan-ip> u1     pri 100 seq 153 sc 0x00 LJ4EVR <dev-pc> slots[1..20]=[20, 128, 0, 0, 0, 0, 0, 0, 0, 0, 128, 0, 128, 0, 0, 0, 64, 0, 0, 0]
    209572.664 DATA  <lan-ip> u1     pri 100 seq 156 sc 0x00 LJ4EVR <dev-pc> slots[1..20]=[0, 128, 0, 0, 0, 0, 0, 0, 0, 0, 128, 0, 128, 0, 0, 0, 64, 0, 0, 0]
    209573.065 DATA  <lan-ip> u1     pri 100 seq 159 sc 0x00 LJ4EVR <dev-pc> slots[1..20]=[20, 128, 0, 0, 0, 0, 0, 0, 0, 0, 128, 0, 128, 0, 0, 0, 64, 0, 0, 0]
    209578.140 DATA  <lan-ip> u1     pri 100 seq 167 sc 0x00 LJ4EVR <dev-pc> slots[1..20]=[20, 255, 0, 0, 0, 0, 0, 0, 0, 0, 128, 0, 128, 0, 0, 0, 128, 0, 0, 0]
    209578.615 DATA  <lan-ip> u1     pri 100 seq 170 sc 0x00 LJ4EVR <dev-pc> slots[1..20]=[20, 255, 0, 0, 0, 0, 0, 0, 0, 0, 128, 0, 128, 0, 0, 0, 200, 0, 0, 0]
    209578.940 DATA  <lan-ip> u1     pri 100 seq 173 sc 0x00 LJ4EVR <dev-pc> slots[1..20]=[20, 255, 0, 0, 0, 0, 0, 0, 0, 0, 128, 0, 128, 0, 0, 0, 128, 0, 0, 0]
    TERM  ('edabfa60', 1)
    TERM  ('edabfa60', 1)
    TERM  ('edabfa60', 1)

Between seq 159 (Restore, 209573.065) and seq 167 (GM 100 %, 209578.140) the walkthrough froze, unfroze, went Offline for
2 s and Online for 2 s: the capture shows one packet per second with the values held and no termination or gap, the app's
own counter read `assert_pps 1 0 2` while offline. Rates: 40/s during the fade (seconds 6-7), 1/s whenever nothing moved,
80 never.

### Bugs fixed on the way (app, not scripted around)

| Where | What the walkthrough hit | Fix |
|---|---|---|
| `src/app/lj_app.c` | the Unassigned Fixtures list was built headless (`lj_desktop_set_host` never called): "desktop: Unassigned Fixtures list built headless" | `lj_app_desktop_host` installed after `lj_win_set_host` |
| `src/app/lj_app.c` | the windows-b forms (Cue control, Save Cue, List of Cues, Master Intensity) never reached the screen and "Add Sequence to Current Cue" never reached the engine: `lj_cue_form_set_host` was never called, so `host.show` / `repin` / `load_cue` were NULL | `lj_app_cue_host` { `lj_app_load_cue`, lock + `lj_app_repin_cue(0)`, show/close through `lj_app_host_open` with `fsStayOnTop` from the DFM } |
| `src/windows/lj_save_dialogs.c` | a first Save Sequence left the record out of `show->sequences`, so the cue slot read "load error" | `lj_ss_sync_table` puts a copy of a new number into the table and calls the new `lj_cue_form_repin()` |
| `src/windows/lj_save_dialogs.c`, `lj_cue_form.c` | `FormActivate` (the OS focus event, which arrives late when another app holds the foreground) reloaded the name field after the script had typed into it: `SEQUENCE [0001 No Name]` | the name edit is not reloaded while it has the focus |
| `src/platform/sdl/lj_sdl.c` | `SDL_SetWindowMinimumSize` got `min_width + frame`: TCurrentCue opened 620 wide (client 612) against the DFM's 612x300 / 604x254 | the constraints are outer sizes already; the frame extra dropped |
| `src/windows/lj_dmx_monitor.c` | seven of the ten columns fitted the 297-px grid (DefaultColWidth 32) | `def_cw = 24`, measured from `dmxout.gif` (reduced), rig item |
| `src/windows/lj_fixture_config.c` | the notes page opened on `O/V Profile Notes` (DFM ActivePage) with the LJ notes on one unwrapped line | `LJ Profile Notes` in front at open; `notes_show` wraps at blanks to the memo width |
| `src/app/lj_uiscript.c` | `shot` captured the window `type`/`key` last targeted even after it had destroyed itself (Save Sequence AutoClose): a use-after-free that crashed the Release build on two runs | `lj_us_cur()` re-validates the pointer against the window lists |
| `src/app/lj_uiscript.c`, `lj_ctl_common.c`, `lj_master_intensity.c`, `lj_unassigned.c` | the script's widget names for code-built widgets did not exist | `row<ch>_<role>` names resolved through `lj_ctl_elem_find`, `fader_master`/`fader_<n>`, `TUnassignedForm` resolution, `main.sb_*` panels, fixture tiles, cue-grid cells, list-view cells, menu items by name, `key <name> <count>`, `drag` with a source point, `assert_client`, `assert_pos`, `assert_file` under the library (docs/ui-script.md) |

Things the script had wrong and the app had right: `Bsave` is disabled until a record has a number (`BSaveNew` / `Save New Cue`
are the first saves); a click on a selected icon de-selects it in Inclusive mode (the script uses `None` first); the control
windows follow the master (a dimmer master closes Intensity / Movement); the current sequence sits above the cue, so `New/Clear`
before `Go`; MAC 500 Mode 2/4 pan/tilt are channels 11-14, not 3-6; the sequence-form time fields commit on the Save / Add
clicks, and `cbSync` is not a way to commit them (it copies the scene time into the fade time).

### Left open after the walkthrough

- Mac (item 1): CI's `macos-clang` build + `--selftest` only; the ME-21 line (native chrome default) stays open for Joey.
- Item 11 waits for the `v0.1.0` tag.
- `lj_window: TDMXOutForm: unknown event OnMouseWheelDown/Up` was logged at every monitor open: closed by the fix pass
  (ME-74, section 10).
- docs/fidelity.md rows added this session: time-field separator, Loop/Trig capitalisation, monitor column width, notes page,
  the blank Martin button, the Cue window size, the save dialogs' name field, the MAC 500 Fade default, design item 4 vs 6,
  the duplicated seq 0, the Unassigned FixtureType column, the time-field caret.

## 10. Fix pass r1 (W1 code council verdict, 2026-10-21, this PC)

The nine must-fix items of `W1_CODE_MASTER_ELDER_VERDICT.md` section 4, applied on `b26ef82`, one commit
`W1: council fix pass r1`. Commands and outputs as run (MSVC 19.44, Ninja, vcvars64).

| Item | Change | Check |
|---|---|---|
| ME-67 | `lj_ctl_common.h` constants x1.25 to the 1:1 anchors (pitch 26, O/S/F 48x24, column 51, bar 169, specials 226 / 78 / 306); chrome Intensity 594x78 + normal caption (600x122), Lamp 160x81 (166x125), Extended 346x134 (both `LJ_BS_SINGLE`); `lj_ctl_lamp.c` two 49x25 buttons; `lj_ctl_extended.{h,c}` boxes 171x101 at the 174 pitch, radios 16-px pitch, fader x 129 h 91; `lj_ctl_colors.h` column 49, O/S/F 48, bar line 26, tiles 31x30 at 32 / 31, rotation 161, special gap 5; `lj_ctl_colors.c` rotation panel 41 tall (buttons 25x31, bar w-61 x 25), specials 105x21 / 70x24 at 73 / 112x24, narrow Fade 30, shake column 152; `lj_ctl_gobos.c` mode pair 81x24, index bar 166x25, shake panel 151x41; `lj_ctl_beam.c` icons 28x24, bar 202 - 56, line 26; `lj_ctl_effects.c` 34x34 button at (54,4), line 41; `ui/widgets/lj_w_scrollctl.{h,c}` 165x41 (buttons 29x30 at y 6, well (60,2) 45x37, x 2 / 31 / 105 / 134: goboscroll.gif is the same 0.8 crop, and the 31x30 tile would not fit a 36x30 well). Comments `measured from <gif>, reduced x1.25, rig item` throughout | `ui_golden_ctl --write-golden`: CTL_MAC500_INTENSITY 600x122, COLORS 382x190, GOBOS 422x216, BEAM 257x96, EFFECTS 257x86, LAMP 166x125, EXTENDED 352x159 (MOVEMENT 332x284 and DMX1_LEVELS 272x250 unchanged, byte-identical); PIL on the new Intensity golden: menu words 54 / 56 / 28 / 21 px (`Preferences` 56 = the 1:1 set), O/S/F red face x 4-48 in the 48-px button; no caption clips; `REVIEWED.tsv` rows rewritten; `test_controls_a` / `test_controls_c` / `test_widgets_lj` geometry rows updated to the new numbers |
| ME-69 | `lj_sdl.c`: `memcpy(&sacn, newcfg, ...)` before `lj_unlock()`, `lj_sacn_open` after; `lj_engine.h` sentence on `lj_engine_take_sacn_cfg` | build + `test_sacn_cfg` / `test_engine_ownership` green |
| ME-70 | `test_controls_a.c`: `uint8_t v, st, st2;`, casts dropped; `ci.yml` builds the whole `debug-win` tree, runs `ctest --preset debug-win` (new test preset) and greps the Debug selftest for `unknown event` too | `ctest --test-dir build-debug-win -j 6` -> `100% tests passed, 0 tests failed out of 55` (fresh MSVC Debug, the config the verdict found at 54/55) |
| ME-71 | `lj_engine.c:173` deleted (the GM Fade t0 shift at unfreeze); `lj_engine_rules.h` `LJ_GM_FADE_RUNS_FROZEN 1` naming note 11 S9 and the rig row | replay `tests/engine/replay/gm_fade_under_freeze.lj4r`: `expect_view 279 gm=77`, `280 gm=76`, `400 gm=0` through a freeze over ticks 100-199 (plus 99 gm=192, 150 gm=159 while frozen); `replay_all` 16 scenarios 0 fails; `fidelity.md` row |
| ME-72 | `lj_toolbars.c` x tables from `toolandstatusbars.gif` bevel columns minus the 9-px frame (PIL, 12-px white runs per row band): row 3 4..154, 196, 221, 248, 300, 330, 365, 400, 450, 474, 498, 528, 664, 704, 740; rows 1-2 were 8-17 px right of the capture throughout and take the measured values (field 39-216, Seq 222, 276 / 301 / 326, counter 355-413, Cue label 427, field 464-641, Cue 647, 705 / 730 / 755; C-L field 43-220, CueList 226, 291 / 316 / 341, Background Cue 366, Statics 476, 570, 595, 620 / 660 / 700 / 740) | `test_desktop --write-golden` (MAINWINDOW 799x606); `test_desktop` asserts `tb_fixture_8` x = 196 and `tb_fixture_10` x = 248 |
| ME-74 | `lj_widget.h` / `lj_widget.c`: `LJ_WEV_MOUSE_WHEEL_DOWN` / `_UP` = `OnMouseWheelDown` / `OnMouseWheelUp`; `lj_window.c` fires them by notch sign before the widget's own wheel handling (the grid scrolls through `lj_w_stringgrid.c`, the monitor's two bindings stay no-ops so a notch is not applied twice); `lj_app.c --selftest` opens and closes the DMX Output Monitor under a counting log sink | `lj4evr --selftest` (Release and Debug): `selftest TDMXOUTFORM: opened and closed, 0 unresolved DFM event name(s): OK`; `ci.yml` greps the Debug selftest |
| ME-75 | `lj_cue_form.c`: `fmt_time` deleted, ST / FT through `lj_fmt_time` (BPM suffix stays local), display tables `Fwd Rev Bounce Rnd` / `Auto Manual Audio` (record words unchanged); `test_windows_b` and `w1_acceptance.lj4s` assert `4:00` / `3:00`, `1:00` / `0:05`, `Fwd` / `Auto` / `Manual` | `test_windows_b` green; acceptance item 5 asserts green |
| ME-68 | `lj_uiscript.c lj_us_shot_one`: a hosted window with no native handle is skipped (closed by AutoClose, or the Cue / Sequence windows while Fixture Configuration is up: `lj_app_cue_show_hook` platform-closes them), a 0x0 surface fails the verb; `docs/ui-script.md` sentence | re-run below: PIL opens every file under `docs/screenshots/w1/`, every `-TCurrentCue.png` 612x300, `-TIntensity.png` 600x122; the 65-byte `acceptance4-sequence-TSaveSeqForm.png` and the pre-fix 620x300 `acceptance2-unassigned-TCurrentCue.png` are gone (the dialog has AutoClosed and the Cue window is platform-closed at those shots), as are the other six shots of platform-closed windows |
| ME-73 | this section; `CHANGELOG.md` 0.1.0; `README.md` docs list = `ls docs`; `CONTRIBUTING.md` main is not branch-protected (API 404); `docs/ci.md` W1 section and the `windows-msvc` row; `docs/generated-files.md` seven rows; `docs/w1-gates.md` Status column from verdict s7 and the acceptance statuses; `docs/fidelity.md` ME-55, ME-58, ME-67, ME-72, ME-71, ME-87 rows, LORD_FIDELITY-7 and Loop/Trig rows closed, seq-0 row reworded; `lj_statusbar.c` PROPOSAL rig; W0-last-event gate text = right click reverses the cycle | |

    cmake --build build-nosdl ; ctest --test-dir build-nosdl -j 6            -> 100% tests passed, 0 tests failed out of 55
    cmake --build --preset dev-ninja-msvc ; cmake --build --preset debug-win  -> rc 0, 0 warnings
    ctest --test-dir build-debug-win -j 6                                     -> 100% tests passed, 0 tests failed out of 55   (ME-70)
    build-ninja-msvc\lj4evr.exe --selftest ; build-debug-win\lj4evr.exe --selftest
        -> startup OK; TDMXOUTFORM opened and closed, 0 unresolved DFM event name(s): OK; TSEQUENCEFORM 360x150, 0 mismatching pixel(s): OK; rc 0 both
    rm docs/screenshots/w1/acceptance*.png ; rmdir %APPDATA%\LJ4EVR\LJ4EVR\User\Default
    build-ninja-msvc\lj4evr.exe --script tests\ui\scripts\w1_acceptance.lj4s  -> rc 0, every assert green, 15 shot lines (3/4/2/2/2/4/5/6/7/8/8/8/8 windows)
    forms.ini [SEQ] 300 300 [CUE] 400 350 [INTENSITY] 100 100 [DMXOUTFORM] 700 200 ; Main.Ini [GLOBALINTENSITY] 500 400
    build-ninja-msvc\lj4evr.exe --script tests\ui\scripts\w1_positions.lj4s   -> rc 0, 7 shots (acceptance10-restart-*)
    PIL over docs/screenshots/w1/*.png                                        -> 72 files open, 0 errors; 13 x -TCurrentCue.png all 612x300; 9 x -TIntensity.png all 600x122

Observed, not a verdict item: from `acceptance7-blackout` to `acceptance10-all-windows` the main window shot is 799x604
(606 before `open master_intensity` and again in the restart run); the OS resized the SDL window by 2 px once during
the walkthrough; nothing in the pass touches the main window size. Recorded for the W2 app owner.

### CI of the fix pass

| Run | Commit | Result | Cause | Fix |
|---|---|---|---|---|
| 39708644481 | 3fcb05a | windows-msvc, linux-gcc-asan red | the job's own `findstr /c:"unknown event"` matched the new selftest line `0 unknown event line(s): OK`; gcc `-Wformat-truncation` on `lj_ctl_elem_add`'s `row%d_%s` into `name[16]` (the acceptance-walkthrough commit `b26ef82` had never reached CI: it went up with this push) | 90cf0c7: selftest line reads `0 unresolved DFM event name(s): OK`; `lj_ctl_elem.name[32]` with `%u` and unsigned operands (3 + 10 + 1 + 7 + 10 + NUL = 32) |
| **39709471199** | **90cf0c7** | **all six jobs green** | | |

    linux-gcc-asan / linux-clang    100% tests passed, 0 tests failed out of 55
    windows-msvc                    100% tests passed out of 55 (Release) and 100% tests passed out of 55 (Debug, ME-70); both selftests OK, no `no binding for handler`, no `unknown event`
    macos-clang                     100% tests passed out of 55; selftest OK
    determinism                     frames-linux.bin = frames-macos.bin = frames-windows.bin, 1641600 bytes, sha256 5e61478e89c1ccfd4936ce7110a366e4f497c5ac0684493bc29ef189342c6ee7 (unchanged: ME-71 touches no scenario of dump-frames)

Green run: https://github.com/UnbannableGT/LJ4EVR/actions/runs/39709471199 (main @ 90cf0c7). Not done here: `VERSION` 0.1.0 and the
`v0.1.0` tag (the tagger's step after one verifier confirms this pass against the verdict), ME-21 (Joey's Mac run), ME-45 (the
non-admin install from the tagged installer).
