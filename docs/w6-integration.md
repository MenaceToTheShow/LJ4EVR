# W6 integration log

Started 2026-10-23. Base: `origin/main` @ `2668de8` ("W6: gates"), the commit every track branched from. The five
track branches were merged into `main` one at a time with `--no-ff`; after each merge the no-SDL Release tree was
built and `ctest` run. The gate table and the per-row Status are in `docs/w6-gates.md` (section 3). This file is the
log: merges, the fix-forward commits, the tracks' own measurements, the integrated matrix, the acceptance re-run, the
pre-push scans and CI.

Every app run of this integration used SDL's dummy video driver (`SDL_VIDEO_DRIVER=dummy`): no window reached the
screen of the developer PC. A first integrator instance was stopped mid-run after it had opened app windows; the
second instance kept its merges and fix-forward commit and re-ran every check below headless.

## 1. Merges

| Commit | Merge | Conflicts | Resolution | no-SDL Release after the merge |
|---|---|---|---|---|
| `a382ed2` | `w6/net` (`0a76075`) | none | none | `100% tests passed, 0 tests failed out of 152` |
| `e863802` | `w6/dmxpatch` (`065a59d`) | none | none | `100% tests passed, 0 tests failed out of 153` |
| `4132808` | `w6/engine` (`bf45bb0`) | `docs/fidelity.md` | both appended sections kept, dmxpatch's first (`tests/io/test_extapp.c` merged clean: both added includes) | `100% tests passed, 0 tests failed out of 153` |
| `4a15aad` | `w6/hardening` (`9ff4533`) | `CMakeLists.txt` | both appended test blocks kept, net's first (`src/io/lj_sacn.c` merged clean: hardening's checked `snprintf` lines beside net's `lj_net_ip_fmt`) | `100% tests passed, 0 tests failed out of 153` |
| `d701f79` | `w6/fidelity` (`4139aa8`) | `CMakeLists.txt`, `docs/fidelity.md`, `tests/ui/test_cuelist_form.c` | all appended blocks kept in merge order (net, hardening, fidelity; dmxpatch, engine, fidelity; CLSAVE before ACTIONS) | `100% tests passed, 0 tests failed out of 154` |

## 2. Tracks (folded from `docs/w6-track-*.md`)

Every track built three trees (Release SDL ON, Debug SDL ON, Release SDL OFF) at 0 warnings with ctest 100%, ran
`test_layering` (0 findings) and `fidelity_check` (0 findings), and closed no row in writing. Per-row evidence is in
`docs/w6-gates.md` section 3; what follows is what the tracks reported beyond it.

**net** (5 rows). Seam change (`docs/w5-contracts.md` seam 3): `io/lj_socket.h` gains
`int lj_socket_mcast_out(lj_socket s, uint32_t nic_ip, uint8_t ttl)` (IP_MULTICAST_IF and IP_MULTICAST_TTL, the
Windows DWORD / POSIX u_char split). The sACN sender now opens `lj_socket_udp(0, 0, 0)` and is non-blocking like
every other seam handle: a full send buffer counts as a send error (logged once per distinct error) where the old
socket would have blocked the engine tick. `lj_extapp_ip_parse` and `lj_remote_ip_parse` are gone from their headers;
`tools/test_layering.py` check 12 counts any dotted-IPv4 copy (21 on the unmodified tree, 0 now). The brief's
External Application serial-output item was NOT NEEDED: `io/lj_extapp.c` already opens and writes through
`lj_serial_open` / `lj_serial_write`, and neither serial backend has a stub.

**dmxpatch** (2 rows). `lj_patch_ch.ign_en` is appended after `htp_mask`; `lj_patch_is_default` compares field by
field (the new byte leaves tail padding that `memcmp` would read). `.GPA` ` ign=1` is written only when set, so a
v1.1.0 Global Patch saves byte-identical. The gate wording for GP-DMXIN was read through the CHM (`globalpatch.htm`):
an Ignore channel always takes DMX-in, a Use channel only with Overwrite Enable on. The three other W5 dmxin blockers
of the brief (`lj_library_list` and ValueTables, the Fingers DMX-in pages, the W4DMX_* goldens) had landed in W5.
One stale comment ("goldens W4DMX_*") stays in the append-only `tests/lj_tests.c` table.

**engine** (6 rows, one DONE). `lj_audio_state` gains `t0_us`, `samples`, `clocked`; `lj_engine_status` gains
`tick_us_sum`; `lj_mtc` gains `sync_frames`, `qf_run` (all appended). Three rows were proven with the fix reverted
and the new row kept: `FAIL tests\io\test_audio.c:224 ok`; `FAIL tests\io\test_extapp.c:182 ...`;
`FAIL tests\io\test_media.c:115 c->cl->next == 1 && c->cl->tc_ms == 1990`.

**hardening** (10 rows, one DONE). `fuzz_ljb` gains its index mode (CTest row and the `fuzz_ljb_index` libFuzzer
target, `LJ_FUZZ_LJB_INDEX`); locally under MSVC `/fsanitize=fuzzer`, 60 s each: `fuzz_ljb #3 INITED ft: 70 corp:
1/265b ... DONE ft: 106`, `fuzz_ljb_index #3 INITED ft: 79 corp: 1/1744b ... DONE ft: 302`. With the four fixes
reverted the new rows fail: `FAIL tests/ui/test_cuelist_form.c:407 lj_table_get(&ctx->show->cuelists, 7) == NULL`,
`FAIL tests/model/test_ljb.c:293 ...`, `FAIL tests/model/test_model_w2_library.c:163 log_lines == 0 (fresh-library
save_config logged 2 line(s))`, `FAIL LORD_C-11: a BMP taller than its cell must give our art`. The five
`generate-check` tools each pass `--check` locally (`qxf2c`, `fontpack`, `artpack`, `gen_vectors`,
`exe_profiles_check`). The CI halves of four rows wait for a run on `main` (section 6): `ci.yml` does not run on a
pushed branch.

**fidelity** (12 rows). `tools/ref/pe_tpf0.py` (PE resource and TPF0 reader, every offset bounded) reproduces all
208 LightJockey.exe texts of `dfm_text/` and wrote the three side-exe texts, which stay gitignored under
`reference/`. `tools/ref/golden_anchors.py` wrote the ME-162 rows. 317 goldens were re-rendered for the 3DLight line
plus six for the tab and caret rows, and one is new (`W2A_TSEQUENCEFORM_CARET`); `REVIEWED.tsv` gained 330 rows.
`TLJMediaForm` (538,340) and `TLJSoundApp` (450,185) are `poDesigned` in their DFMs and now open there too.

Concurrent-run note (net, dmxpatch, engine, fidelity). While several tracks ran `ctest` on the same PC,
`test_sacn_loopback` and once `dmxin_loopback` failed on received-packet counts with `0 send errors`: the tests of
different trees share UDP 5568, and `RESOURCE_LOCK` serialises only within one `ctest`. Each passed alone and in a
quiet full re-run. The integrated tree is measured on a quiet PC (section 4).

## 3. Fix-forward commits

- `49e52dc` "W6: test_uiscript_w2 shares the sACN port lock". The LORD_FIDELITY-19 restart row sends universe 1 to
  127.0.0.1:5568, and `test_sacn_loopback`'s receiver counted those packets under `ctest -j8` (the first integrated
  run: `test_sacn_loopback` failed in `build-w6i-rel`, `build-w6i-dbg` and `build-w6i-fuzz`). `RESOURCE_LOCK
  sacn_port` now covers the three rows; every run since is 100%.
- `tests/ui/scripts/w5_acc_dmxin_3.lj4s` (this commit): the Art-Net input step asserted `[ARTNET] In=10`, a key that
  ME-178 (W5 fix pass r1) stopped saving; it now asserts `[DMXIN] Source=1` (Art-Net), which `lj_dmxin_cfg_save`
  writes. A script fix only: the W5 walkthrough ran before ME-178.

## 4. Measured on the integrated tree

Toolchain: MSVC cl 19.44 + Ninja (vcvars64), SDL from the `release-3.4.18` source. Every tree was built with
`--clean-first`. `ctest -j8` runs with the process kept off logical CPUs 8-11 (see "ASan and one CPU group" below).

    build-w6i-rel   (Release, SDL)        [592/593], 0 warnings, 0 errors   ctest 100% tests passed, 0 tests failed out of 156
    build-w6i-nosdl (Release, LJ_SDL=OFF) [352/353], 0 warnings, 0 errors   ctest 100% tests passed, 0 tests failed out of 154
    build-w6i-dbg   (Debug, SDL)          [592/593], 0 warnings, 0 errors   ctest 100% tests passed, 0 tests failed out of 156
    build-w6i-fuzz  (Debug, OFF, MSVC /fsanitize=address; the new windows-msvc CI step's settings)
                                          0 warnings, 0 errors              ctest 100% tests passed, 0 tests failed out of 154
    test_layering                         463 file(s) checked, 221 header(s) compiled alone, noop handlers 0, ini ternaries 0,
                                          scene getters 0, set-text/lib/ctx copies 0, noop Help handlers 0,
                                          hint/edit_int/seam copies 0, popup_at 2, ip-parse copies 0, 0 finding(s)
    fidelity_check                        73 F/I id(s) with a row of 73, sacrifice paragraphs equal, 0 finding(s)
    help_check                            19 texts against 200060 reference 8-word runs; 18 topics
    dfm2c --check                         OK
    replay_all                            replay: 36 scenario(s), 0 failed
    ui_golden / ui_golden_ctl             208 of 208 form(s) have a REVIEWED.tsv row; 208 form(s), 0 failure(s) / 0 failure(s)
    caption_diff (w4_captions.lj4s, U:W4Captions, a fresh scratch LJ4EVR_USERDATA, the three side-exe texts by --dfm)
                                          script rc 0, 0 SCRIPT FAIL, 0 ME-16 lines;
                                          149 implemented form(s) of 210, 0 difference(s), 142 excluded by a fidelity row,
                                          89 absent at runtime; rc 0
    lj4evr --selftest (Debug, scratch userdata)
                                          rc 0, six OK lines; 0 "no binding" / "unknown event" lines
    fuzz rows, MSVC ASan, LJ_FUZZ_N=200000 (build-w6i-fz200k, --clean-first, 0 warnings), each rc 0, no ASan report:
                                          fuzz_sacn_rx: 200000 iterations, PASS
                                          fuzz_vtr: 200000 iterations, PASS
                                          fuzz_artnet: 200000 iterations, PASS
                                          fuzz_remote: 200000 iterations, PASS
                                          fuzz_ljb: 200000 iterations, PASS
                                          fuzz_ljb index: 200000 iterations, 66348 opened, PASS
    determinism (dump-frames)             Release twice, Debug, no-SDL: equal, 1,641,600 bytes,
                                          sha256 5b13f41930362b75a7aa6f9c53f77db2d315a4039ea5a18da664ea1263114b50 (= W5, = W4)
    replay-frames.bin                     129,939,560 bytes, sha256 a1d1f2def045b0dc1cad89e489bf2625b6dbef25d4e4756c8d1ea500ef3d3d62
                                          in all four trees

`replay-frames.bin` changed, and the reason is one new scenario. W4's `c46fee4c...` became `9da72c3f...` at the W5
ME-179 re-record (fix pass r1). W6 adds `tests/engine/replay/w5/dmxin_s1_ignore.lj4r` (GP-DMXIN), which the
`replay_dump` glob picks up. The same dump of the other 43 scenarios (that file left out) is
`9da72c3f56a0b728c1e0adfd006d51f68b7fc787e1d00c03045db27f18560adb`, 129,914,872 bytes: byte-equal to W5. The 24,688
new bytes are the new scenario's.

Standing rules over `v1.1.0..HEAD`: `git diff v1.1.0 -- src/engine/lj_engine_rules.h` empty; `PROPOSAL rig` 505
(>= 487); one `FetchContent_Declare` (SDL3), `third_party` unchanged; no `self-hosted`, `pull_request_target` or
secret beyond `GITHUB_TOKEN` in the workflows; no listener added (the remote idle timeout and the Art-Net re-open
keep every listener OFF by default).

ASan and one CPU group. Unpinned, the ASan tree's `replay_dump` dies with `Exception: SegFault` (the first integrated
run, and 1 of 1 again here). It is not a W6 change: the W5 ASan tree (`build-w5i-fuzz`, v1.1.0 code, 100% when W5
measured it) now fails the same way. Pinned, the same binary passes (`--repeat until-fail:5`: 5 of 5). The same
`--dump-frames` run of `drift_600s_two_slots.lj4r` on fixed CPU sets: logical CPUs 8-11 crash 3 of 3, CPUs 0-7 crash
0 of 3, CPUs 12-19 crash 0 of 3. A single-threaded, deterministic run that fails on one core group only points at
that core group, not the code. The windows-msvc CI step (section 6) runs the same ASan ctest on another machine.

## 5. Acceptance re-run

`tests/ui/scripts/w4_entry_points.lj4s` on `build-w6i-rel`: a fresh scratch userdata prepared by
`w4_entry_prep.sh`, `U:W4Entry`, the working directory a scratch folder (so the shots land there, not over the
committed captures), `SDL_VIDEO_DRIVER=dummy`, `SDL_AUDIO_DRIVER=dummy`; the visible top-level windows of the process
counted every 100 ms:

    rc=0 max_visible_windows=0
    PASS lines: 156 (the script has 156 `log "PASS ..."` rows; 155 at v1.1.0, plus TSavePresetForm2)
    SCRIPT FAIL lines: 0

The dummy driver logs `SDL_CreatePopupWindow: That operation is not supported` 25 times (menus and popups have no
native window there); the script's asserts read our own window tree, so every row still runs.

The W5 acceptance set (`docs/w5-integration.md` "Acceptance"), every step in order from fresh scratch userdatas
written by `tools/ref/w5_wire.py prep`, with the same wire peers on loopback, `SDL_VIDEO_DRIVER=dummy`:

| Row | Scripts | Result |
|---|---|---|
| 1 Art-Net | `w5_acc_artnet_1`, `_2` (with `w5_wire.py node 40 --answer 20`) | rc 0, rc 0 |
| 2 Audio | `w5_acc_audio_1`, `_2`, `_audio_real` (three clicks played into a loopback capture device) | rc 0, rc 0, rc 0 (see below) |
| 3 `.LJB` | `w5_acc_ljb_1`, `_2` (after `lj4evr_tests test_ljb --write`) | rc 0, rc 0 |
| 4 DMX-in | `w5_acc_dmxin_1`, `_2` (live sACN), `_3` (Art-Net in) | rc 0, rc 0, rc 0 |
| 5 TCP remote | `w5_acc_remote` (with `w5_wire.py tcp-send 7232 ...`) | rc 0 |
| 6 External Application | `w5_acc_extapp` (with `w5_wire.py tcp-recv 7000 32 --close 12`) | rc 0 |
| 7 MediaPlayer | `w5_acc_media` | rc 0 |

13 of 13 app runs rc 0, 0 SCRIPT FAIL. The session's environment also sets `SDL_AUDIO_DRIVER=dummy`, under which SDL
lists one recording device and `w5_acc_audio_real` fails (`audio: device "..." not present; using the default
device`, `SCRIPT FAIL line 10: 1:1 is 40, expected 80`); the W5 build fails the same way there. Row 2 was therefore
run with `SDL_AUDIO_DRIVER=wasapi` (audio only; video stays dummy): the app records from the loopback capture device
named in `Hardware.Ini` at 44100 Hz mono S16, the three trigs fire (1:1 = 80), rc 0 for all three scripts.

## 6. Pre-push scans and CI

Pre-push, over `git diff v1.1.0..HEAD` (437 added or changed paths) and the messages of `v1.1.0..HEAD`:

- `reference/`: 0 paths; sha1 of each changed file (as-is and CRLF->LF) against the 18,923 `reference/` files: 0 equal.
- REL-PRIVATE (the ME-148 table: user name, work-PC and host names, both LAN prefixes of this PC, its NIC names, the
  account mail, the design-vault root and the user-profile path): 0 new hits in the added lines, 0 in the messages,
  0 in the file names. The three changed captures (`copy-from-TCopyFromForm.png`, `copy-from-TLinkConfigForm3.png`,
  `save-preset-TSavePresetForm2.png`) viewed: our windows only, no path, user, host or address.

CI: section 6 continues below with the run on the pushed `main`.
