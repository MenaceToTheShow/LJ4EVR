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

Every W1 form that renders, reviewed at 2x with Read against the DFM numbers and the CHM GIFs (the 1:1 ones:
intensitycontrol 480x98, lampcontrol 133x100, movement2 332x284, beam 206x77, extended 277x81; the rest are reduced captures,
compared for order and proportion):

| Golden | Size | Source |
|---|---|---|
| TSEQUENCEFORM, TCURRENTCUE | 472x207, 612x300 | regenerated with glyphs (ME-48, W0-glyphs) |
| TDMXOUTFORM, TMASTERINTENSITYFORM, TSAVECUEFORM, TSTARTUPFORM | 356x264 client, 670x341, 337x172, 367x410 | new DFM goldens (`ui_golden`); the monitor grid and the master faders are code-built at open and absent from the bindings-free render |
| CTL_MAC500_{INTENSITY,MOVEMENT,COLORS,GOBOS,BEAM,EFFECTS,LAMP,EXTENDED} | 480x98, 332x284, 307x166, 339x187, 206x86, 207x78, 133x100, 283x128 | `ui_golden_ctl`: MAC 500 Mode 2/4 (built-in 12) at 1:001 as master, every window its table offers through `lj_ctl_open` |
| CTL_DMX1_LEVELS | 272x250 | the same show, Generic DMX Dimmer 1 (built-in 19) at 1:017 as master |

TFIXTURECONFIGFORM is TLINKCONFIGFORM3 (no such DFM class), already a golden since W0. Deviations found and recorded in
REVIEWED.tsv: our sans13 text is ~20% wider than the text in the CHM control GIFs (the GIFs were captured with a narrower system
font; font_corpus pins ours to the DFM AutoSize widths), so a few long captions clip where LJ2's do not (Intensity specials
labels, Beam's Help item, Extended's radio captions); the Movement Resolution bar paints its arrows over the caption; Beam's
client is 9 px taller than beam.gif; code-built menus carry no mnemonic underlines.

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
