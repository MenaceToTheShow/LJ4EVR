# W5 integration log

Started 2026-10-23. Base: `52b999a` ("W5: contracts"); the seven tracks' returns committed as `f57fd58` ("W5: tracks
landed (pre-integration)"): 114 paths, 0 under `reference/`, 0 build trees, sha1 of every new or changed file against
13,637 `reference/` files: 0 equal; the private-detail grep (user name, host name, both LAN prefixes of this PC, the
Dropbox root, `Users\`) over the 114 paths: 0 hits beyond the retired-host word `atlas-ci2` in a `release.yml` comment
(kept, ME-148).

## Integrator 1: build and tests

Toolchain: MSVC cl 19.44 + Ninja (vcvars64), SDL from the `release-3.4.18` source (`FETCHCONTENT_SOURCE_DIR_SDL3`).
Trees: `build-w5i-rel` (Release, `LJ_SDL` ON), `build-w5i-dbg` (Debug, ON; the `debug-win` settings),
`build-w5i-nosdl` (Release, OFF), `build-w5i-fuzz` (Debug, OFF, MSVC `/fsanitize=address`, `LJ_FUZZ_N=200000`).

At `f57fd58` the integrated tree already built: the breakages the tracks reported (other tracks' half-edited files)
were gone once every track had returned. Measured there: 0 warnings / 0 errors in all three trees; ctest 152 / 150 /
152 of 152 / 150 / 152; `test_layering` 0 findings.

### Fixes in the integration commit

| Blocker (track) | Fix | Check |
|---|---|---|
| `lj_library_list` shows `<userdata>/User/ValueTables` as a library (dmxin) | the folder is skipped (`src/model/lj_library.c`) | `test_model_library`: a `ValueTables` folder beside the libraries is not listed |
| DMX-in Audio Gain mapped 127 = 0 dB, the Fingers jogs 128 = 0 dB (audio ask 3) | `windows/lj_dmx_in.c` calls `lj_audio_dmx_gain` / `lj_audio_dmx_noise`: one mapping | `test_dmx_in`: `LJ_FN_DMXIN_AUDIO` 128 -> gain 100 %, noise 200 |
| Fingers "DMX in (1-12)" .. "(37-48)" pages not wired (dmxin) | `lj_fingers_fader` on those pages calls `lj_dmxin_emulate((page - 10) * 12 + fader, level)` | `test_da2532_fingers`: page "DMX in (13-24)", fader 2 at 100 % -> DMX-in raw channel 14 = 255, `live`; 0 % -> 0 |
| `LJ_WIN_REMOTE_SETUP` reachable by id only (remote) | Setup > `&RS-232 / TCP Remote ...` after Global Patch (code-built, PROPOSAL rig W5-REM-4), `LJ_ACT_REMOTE_SETUP` appended, bound in `app/lj_app_io.c` | `test_desktop`: item present, enabled, bound (`menu_leaf_bound`) |
| Run Minimized persisted with no effect (audio ask 1) | `lj_app_io_start` opens the Analyzer when `[AUDIO] Enabled` and not Run Minimized (ponytail: no minimized state) | W5-X-OFF unchanged: `test_uiscript_w5` (fresh Hardware.Ini, audio off) |
| The W4DMX_* goldens referenced by no test (dmxin) | deleted (their REVIEWED rows stay: the file is append-only) | `ui_golden`, `test_dmx_in` green |
| `w3_acceptance.lj4s` asserts an `mp_play` line red (media) | `w3_mp_play.Cl2` line 2 is `da_play` (still inert); the stale `cuelist_run.lj4r` NOTE reworded (comment only) | `replay_all`, `replay_dump` unchanged |
| caption_diff 2 differences: TDMXINFORM `lChIn` / `lFreq` now carry the live count and rate | two F55 runtime-fill rows in `tools/caption_diff_excl.tsv` | caption_diff 0 differences |
| ci.yml fuzz step had only `fuzz_gs2` / `fuzz_udf` (all W5 parser tracks) | the five W5 libFuzzer targets, 60 s each, in the linux-clang step | the CI run below |
| fuzz rows fixed at 20000 iterations | `tests/lj_fuzz.h`: `LJ_FUZZ_N` overrides the count for a local long run | the 200k run below |

Not done here (integrator 2 or later, as the tracks returned them): ME-165 (`lj_widget_set_enabled`, one `LJ_NA_HINT`;
the tracks un-greyed with direct `enabled` writes and added no literal: `git grep -c "Not available in LJ4EVR" -- src`
= 24, as at the contracts commit), the Global Patch per-channel Use / Ignore DMX In Enable (needs a model field and an
engine S1 rule, W5-X-ENGINE), masters showing the DMX-in remote state, Statics Latch through a pinned path, Pass
Keypresses (an `lj_app_key` check), the `.Udf` half of W5-LJB-1, and every docs / fidelity / file-formats / provenance
row the tracks returned as text.

### Measured on the integration commit

    build-w5i-rel   (Release, SDL)        0 warnings, 0 errors   ctest 100% passed, 0 failed out of 153
    build-w5i-nosdl (Release, LJ_SDL=OFF) 0 warnings, 0 errors   ctest 100% passed, 0 failed out of 151
    build-w5i-dbg   (Debug, SDL)          0 warnings, 0 errors   ctest 100% passed, 0 failed out of 153
    test_layering                         465 file(s) checked, 222 header(s) compiled alone, noop handlers 0, ini ternaries 0,
                                          scene getters 0, set-text/lib/ctx copies 0, noop Help handlers 0, 0 finding(s)
                                          (with the W5 OS_ALLOW / PAIR_EXEMPT rows: io/lj_socket.c, io/lj_serial_win.c,
                                          io/lj_serial_posix.c + dirent.h, io/lj_vtr_gen.c, io/lj_audio_gen.c)
    fuzz rows (ctest, 20000 each)         fuzz_sacn_rx, fuzz_vtr, fuzz_artnet, fuzz_remote, fuzz_ljb: Passed (none DISABLED)
    fuzz rows, MSVC ASan, 200000 each     fuzz_sacn_rx / fuzz_vtr / fuzz_artnet / fuzz_remote / fuzz_ljb:
                                          "200000 iterations, PASS", rc 0, no ASan report
    replay_all, replay_w5_*               Passed (5 dmxin S1 scenarios, audio_trig, the contract scenario)
    font_corpus, ui_golden, ui_golden_ctl Passed
    caption_diff (w4_captions.lj4s + the four W5 window ids appended, scratch LJ4EVR_USERDATA)
                                          rc 0; 146 implemented form(s) of 207, 0 difference(s), 140 excluded by a fidelity row,
                                          84 absent at runtime (TExternalAppSetupForm is the new implemented form)
    goldens W5 changed                    12: CTL_EXTERNAL_EXTENDED, MAINWINDOW, W5AU_ANALYZER, W5AU_ANALYZER_REALTIME,
                                          W5DMX_{DMX_IN, EXPORT_TABLE, TRANSLATION, TRANSLATION_EDIT, VALUE_TABLE_ERROR},
                                          W5MED_MEDIA_PLAYER, W5X_EXTAPP_SETUP, WIN_CUELIST_FORM: each has a 2026-10-23 REVIEWED row
    determinism (dump-frames)             Release twice, Debug, no-SDL: equal, 1,641,600 bytes,
                                          sha256 5b13f41930362b75a7aa6f9c53f77db2d315a4039ea5a18da664ea1263114b50 (= W4)
    replay-frames.bin                     129,664,080 bytes, sha256 c46fee4ceff60245c9b504a6d7b2028fca0d0ba6008e1f541af7c8279545039d
                                          in all three trees (= W4: the W5 scenarios sit outside the replay_dump glob)
    build-w5i-dbg lj4evr --selftest       rc 0, six OK lines; 0 "no binding" / "unknown event" lines
    W5-X-ENGINE                           grep "recv|send(|fopen" src/engine/*.c = 0
    W5-X-DEP                              FetchContent_Declare 1; git diff v1.0.0 -- third_party empty
    W5-X-RIG                              git diff v1.0.0 -- src/engine/lj_engine_rules.h empty; "PROPOSAL rig" 358 -> 464
    ME-159                                grep self-hosted|pull_request_target|secrets. in workflows, beyond GITHUB_TOKEN = 0

Not run on this PC: gcc (no Linux toolchain here); the linux-gcc-asan job is the first gcc compile of the W5 files, and
`test_serial_pty` runs only on Linux / macOS.

### CI round 1 (`f57fd58`, `4c22a41`) and the fixes

windows-msvc and generate-check green; linux-gcc-asan, linux-clang and macos-clang red:

- clang `-Wcomment`: `windows/lj_dmx_in.c:9` and `:89` wrote `ValueTables/*.vtr` inside a block comment; now
  `ValueTables/<name>.vtr`. A scan of every `.c` / `.h` under `src`, `tests` and `tools/fuzz` for `/*` inside a comment:
  0 left.
- gcc `-Wformat-truncation`: `model/lj_ljb.c:221`. The return value of that call and of 31 more W5 `snprintf` calls
  that write a fixed-size array into a smaller buffer is now checked, the repo idiom
  `if (snprintf(d, ...) < 0) d[0] = 0;` (ljb, serial_posix, audio_form, backup_restore, dmx_in, test_dmxin,
  test_audio_form, test_dmx_in).
- `tools/fuzz/fuzz_vtr.c:14` held `'\'` (a backslash lost to a shell heredoc): `'\\'`. It only builds with
  `LJ_FUZZ=ON` (clang), so no Windows tree saw it.

Local clang check, a stand-in for the two clang jobs: zig cc 0.16.0 (clang) in the session scratchpad, the CI flags
(`-std=c11 -Wall -Wextra -Wpedantic -Wshadow -Wvla -Werror -O2`), every SDL-free `.c` for `x86_64-linux-gnu`: 339
files, 0 failures beyond the three that need their platform (`lj_midi_coremidi.c`, `lj_midi_winmm.c`) or a CMake
define (`test_corpus.c`); `src/io`, `src/model`, `tests/io`, `src/app` and `src/platform/sdl` (SDL3 headers) for
`aarch64-macos`: 0 failures. MSVC trees and ctest unchanged after the fixes (153 / 151 / 153 passed).

### CI round 2 (`2feb44b`) and the fixes

windows-msvc, generate-check and linux-clang green: `test_serial_pty` passed on Linux, and the fuzz step printed an
`INITED` / `DONE` pair for each of the seven targets (`fuzz_gs2`, `fuzz_udf` and the five W5 parsers, 60 s each).
Red:

- linux-gcc-asan `-Wmisleading-indentation`: the round-1 idiom left `if (...) d[0] = 0; next;` on one line in
  `windows/lj_dmx_in.c:243` and nine rows of `tests/io/test_dmxin.c`; each is now braced. A scan of every `.c` for an
  unbraced `if` / `for` / `else` body followed by more code on the same line: only the `else do { } while` of
  `tests/ui/test_windows_c.c:378`, which gcc has always accepted.
- macos-clang `test_remote`: the listener's backlog was `LJ_REMOTE_CONNS` (4), so macOS dropped one of five
  simultaneous SYNs. The fifth client was then never accepted and refused, and the dropped one was `cl[0]`, whose close
  the test waits for. The backlog is now `4 * LJ_REMOTE_CONNS`; the cap of 4 open connections is unchanged.
- macos-clang `test_extapp`: the test sent `GO` microseconds after closing the peer, before the loopback FIN arrived,
  so the bytes went into the dead socket (normal TCP). The test now waits 100 ms after the close, as a real peer loss
  does. The extapp code is unchanged.

MSVC trees after these fixes: ctest 153 / 151 / 153 passed (including `test_layering`).
