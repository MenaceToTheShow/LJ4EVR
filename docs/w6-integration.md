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

### CI round 1 (`b3afd86`, run 40054045006) and the fixes

windows-msvc and macos-clang green; linux-clang, linux-gcc-asan and generate-check failed in "Initialize
containers" (`Error response from daemon: manifest unknown`, `Docker pull failed with exit code 1`); determinism
skipped. The GitHub account that owns the repository and the build image was renamed from `UnbannableGT` to
`MenaceToTheShow` on 2026-10-23 (`gh repo view UnbannableGT/LJ4EVR` answers `MenaceToTheShow/LJ4EVR`, and `git push`
prints the new location). Repository URLs redirect; GHCR image names do not: `ghcr.io/unbannablegt/lj4evr-build` is
gone, and the private package now answers under `ghcr.io/menacetotheshow/lj4evr-build`. The same failure would have
hit any push after the rename. Fixes (`ci.yml`, `docker.yml`, `docker/Dockerfile`, `docs/ci.md`): the four
`image:` lines and `docker.yml`'s `IMAGE` name the new owner with the unchanged digest
`sha256:731edd12...c91722` (the image did not move, so LORD_C-12-part records no digest change).

The W6 gates run (`2668de8`, run 40038960636) had already failed windows-msvc once on `test_llf`
(`FAIL ...test_llf.c:250 strstr(logged, "Lightcorder card memory full - recording stopped") != NULL`), on code equal
to v1.1.0. The test's 8192-byte log sink drops lines that do not fit, and the 8176-frame run logs about 204
per-second lines (about 13 KB): locally the sink held 8186 bytes, and the card-full line fitted by a few bytes.
`8803f37` widens the sink to 32768 bytes; `test_llf` passes in all four local trees.

### CI round 2: green, run 40054522037 (`a38b065`)

https://github.com/MenaceToTheShow/LJ4EVR/actions/runs/40054522037: six of six jobs green on the first run
(generate-check, linux-gcc-asan, linux-clang, windows-msvc, macos-clang, determinism). The MSVC ASan ctest is a step of
windows-msvc, not a seventh job.

    linux-gcc-asan   ctest --preset ci-linux-asan: 100% tests passed, 0 tests failed out of 155
    linux-clang      ctest --preset ci-linux-clang: 100% tests passed, 0 tests failed out of 155
                     fuzz step, 60 s each from the CTest seeds (INITED, then DONE):
                       fuzz_sacn_rx    #2 INITED cov: 279 ft: 280 corp: 1/638b
                       fuzz_artnet     #4 INITED cov: 130 ft: 142 corp: 3/791b
                       fuzz_remote     #2 INITED cov: 146 ft: 147 corp: 1/6b
                       fuzz_vtr        #2 INITED cov: 139 ft: 140 corp: 1/148b
                       fuzz_ljb        #2 INITED cov: 832 ft: 833 corp: 1/265b     #48367 DONE cov: 995 ft: 2980
                       fuzz_ljb_index  #2 INITED cov: 2593 ft: 2594 corp: 1/1744b  #125049 DONE cov: 3059 ft: 10785 (LORD_C-7)
                       fuzz_gs2 / fuzz_udf as before (corp: 1/1b, 1/222b)
    windows-msvc     Release and Debug ctest: 100% tests passed out of 156, 156;
                     "MSVC AddressSanitizer, LJ_SDL=OFF, Debug ctest": 100% tests passed out of 154 (LORD_C-12)
    macos-clang      ctest --preset release-mac: 100% tests passed out of 157
    determinism      frames-{linux,windows,macos}.bin: 5b13f41930362b75a7aa6f9c53f77db2d315a4039ea5a18da664ea1263114b50
                     replay-frames.bin L / W / M: a1d1f2def045b0dc1cad89e489bf2625b6dbef25d4e4756c8d1ea500ef3d3d62 (= the local trees)
    generate-check   success, with the `if [ -f ]` guard gone (WIZARD_STYLES-14)

The windows-msvc runner passed the ASan ctest with `replay_dump` included and no CPU pinning, which supports the
CPU-group reading of section 4.

## 7. Clean-up

The five `w6/*` branches were merged into `main` (`git branch -r --merged main` listed all five) and deleted on
`origin` and locally; the five worktrees (each clean, at its branch's pushed head, no junction left inside) were
removed with `git worktree remove`; `git worktree list` shows `main` only. The empty parent folder of the worktrees
is left for the maintainer to delete.

Left for the release step: the version bump to 1.2.0, the CHANGELOG, the 1.2.0 "what it is not" paragraph (GP-DMXIN
takes its item off; `docs/w6-gates.md` section 3), and the ME-189 known-gap line in the release notes. Older docs
(`CONTRIBUTING.md`, `docs/releasing.md`, the earlier logs) still name the repository as `UnbannableGT/LJ4EVR`; GitHub
redirects those URLs.

## Must-fix r1 (W6 council verdict section 4, items 1-14)

2026-10-23, on `55527ac`. One commit, `W6: must-fix r1`. Toolchain as section 4 (MSVC cl 19.44 + Ninja, SDL from the
`release-3.4.18` source). Every app run used `SDL_VIDEO_DRIVER=dummy` in the foreground under a timeout; no window
reached the screen. `VERSION` stays 1.1.0; no tag; ME-207 (release) and ME-208 (design, vault) are not part of this
pass. Each mutant below was applied to the working tree, built in `build-w6i-nosdl`, run, and the file restored from
its saved copy (for ME-194, applied to a scratch copy of `src/`).

| # | ME | Change | Verify (quoted) |
|---|---|---|---|
| 1 | ME-190 | `LJ_CMD_DMX_IN_LIVE` (appended kind) sets the appended `lj_post_ctx.dmx_in_live`; `lj_post_s1_patch_src` returns the channel's own value for every channel while it is 0; `lj_io_dmxin_apply` posts it with Overwrite Enable (= `dmxin.enabled`). The replay harness posts it at build when the scenario has an `at k dmx_in` line (`docs/replay-grammar.md`). New `tests/engine/replay/w5/dmxin_s1_ignore_off.lj4r`; `test_engine_w5` gains the live-0 row | `replay dmxin_s1_ignore_off: 5 ticks, 2 expects, 0 fails`; `replay dmxin_s1_ignore: 6 ticks, 4 expects, 0 fails`; mutant (`!c->dmx_in_live` removed): `FAIL tests/engine/replay/w5/dmxin_s1_ignore_off.lj4r:18 tick 2 1:2 expected 100 got 0 (out, layer 1)`, `replay dmxin_s1_ignore_off: 5 ticks, 2 expects, 3 fails`. `replay-frames.bin` re-recorded once: 129,960,144 bytes, sha256 `3474ee7386eab212115c122c07595ec161a0e2e65f5732e89c127cf618f95f34` in all three trees; the same dump of the other 44 scenarios (the new file left out): 129,939,560 bytes, `a1d1f2def045b0dc1cad89e489bf2625b6dbef25d4e4756c8d1ea500ef3d3d62` (= W6 integration); `dump-frames` 1,641,600 bytes, `5b13f41930362b75a7aa6f9c53f77db2d315a4039ea5a18da664ea1263114b50` (Release, Debug, no-SDL) |
| 2 | ME-191 | `lj_app_io_idle` adds the growth of `lj_audio_dropped()` since the last pass to `audio_state.samples` (unsigned difference) | `test_audio`: `ring drops: 2 onsets 320 ms apart, 10143 samples dropped between them: 2 trig(s)`, `test_audio: PASS (0 failures)`; mutant (the add removed): `ring drops: ... 1 trig(s)`, `FAIL D:\lj4evr\tests\io\test_audio.c:258 c->audio_state->trigs == 2` |
| 3 | ME-192 | `panel_fill`: `cbIgnoreEnable` enabled for a DMX-in source, checked = `ign_en` (refilled after the popup Use / Ignore too); `<- Set Values` takes the box for a DMX-in source. New UI-script verb `assert_checked` (`docs/ui-script.md`) | `test_uiscript_w3: 0 failure(s)` with the rows: popup `P_EnableIgnore` -> `assert_enabled TGlobalPatchForm.cbIgnoreEnable 1`, `assert_checked ... 1`; `P_EnableUse` -> `assert_checked ... 0`; box clicked, `bSet`, `SaveGlobalPatch1` -> `assert_file_line GlobalPatch.GPA "ch 1:1 src=in sl=1 sc=1 mode=abs inv=0 min=0 max=255 htp=0 ign=1"`; a DMX Out source typed and set -> `assert_enabled ... 0`. No golden moved |
| 4 | ME-194 | `lj_dmxin.c` formats with `lj_net_ip_fmt`; `IP_SCAN_RE` takes the quad anywhere in the literal, `IP_FN_RE` any return type | `test_layering: 463 file(s) checked, 221 header(s) compiled alone, ..., popup_at 2, ip-parse copies 0, 0 finding(s)`; the fix reverted in a scratch copy: `io/lj_dmxin.c: a dotted-IPv4 sscanf / snprintf; use lj_net_ip_parse / lj_net_ip_fmt (WIZARD_STYLES-9)`, `... ip-parse copies 1, 1 finding(s)` |
| 5 | ME-196 | the two comments back on their own declarations | `v1.1.0` and here: the same `lj_app_post_locked` line with `/* W3 (ME-102): caller holds lj_lock; ... */`, and the same `lj_platform_sleep_us` line with `/* SDL_DelayPrecise; the engine thread's sleep_until (design 3.1) */`; `test_layering` 0 findings (headers compiled alone) |
| 6 | ME-197 | one `lj_log` line naming the key when a non-empty `[SACN] Nic`, a `UnicastN` entry or `[DMXIN] Nic` is refused | `test_net`: `refused: Nic= 10.0.0.5 -> nic_ipv4 0, logged:` `sacn: Hardware.Ini [SACN] Nic is not a dotted IPv4 address: the OS default interface sends`, `sacn: Hardware.Ini [SACN] Unicast1: an entry is not a dotted IPv4 address (dropped)`, `dmxin: Hardware.Ini [DMXIN] Nic is not a dotted IPv4 address: every interface listens`; `test_net: PASS` |
| 7 | ME-198 | `comp_ok` takes `'0'` as a device digit | `test_ljb`: `ljb container: 0 failures after the refusal rows` (`COM0.txt`, `LPT0`, `lpt0\` refused; `COM\xB4.txt`, `COM10.txt` accepted), `test_ljb: PASS (0)` |
| 8 | ME-199 | `left/top` dropped from the TGroupForm and THTPGroupsForm descs; a fidelity row names the 12 Positions we centre on the main window | `w6_podesigned.lj4s` (empty scratch userdata, dummy video): rc 0, 0 SCRIPT FAIL, with `assert_pos TGroupForm 389 278` (220x250 outer: 100 + (799 - 220) / 2, 100 + (606 - 250) / 2) and `assert_pos THTPGroupsForm 364 230` (270x346: 100 + (799 - 270) / 2, 100 + (606 - 346) / 2); a wrong-value probe: `SCRIPT FAIL line 3: TGroupForm position is 389x278, expected 100x100`; `fidelity_check` green |
| 9 | ME-200 | `Help1Click` binding deleted (TSavePresetForm2); `w4_entry_prep.sh` creates `User/Default` | `w4_entry_points.lj4s` from a userdata prepared by the script alone (`Default W4Aux W4Entry W4Update`), a scratch working directory, dummy video and audio: `rc=0`, `PASS lines: 156`, `SCRIPT FAIL: 0`, `no binding: 0` |
| 10 | ME-201 | the code comment, a fidelity row superseding row 375's wording, two `file-formats.md` rows; the UI script's `type` / `key` reach TSavePresetForm2 | `git grep -n "SAVESELECTION" -- src/windows/lj_user_palettes.c`: no output; `grep -c "SAVEPRESET2\|SyncFrames" docs/file-formats.md`: 2; `test_uiscript_w3`: `ME-201 Save on [Enter]: 0 entries before, 1 after Return`; mutant (the Enter test reads another key): `ME-201 Save on [Enter]: 0 entries before, 0 after Return`, `FAIL D:\lj4evr\tests\app\test_uiscript_w3.c:301 lj_user_palettes_count() == n0 + 1 && lj_user_palettes_save_window() == NULL` |
| 11 | ME-204 | `golden_anchors.py --selftest` (synthetic golden and DFM text: the anchor probe hit and miss, the 3DLight count, three bad BMPs) and CTest `golden_anchors_selftest`; `--date` and `--reviewer` required | `ctest -R golden_anchors_selftest`: Passed (`golden_anchors --selftest: OK`); without `--date`: `usage: golden_anchors.py [-h] --ref REF [--since SINCE] --date DATE --reviewer REVIEWER [--note NOTE] [--write]`, `golden_anchors.py: error: the following arguments are required: --date`, rc 2 |
| 12 | ME-206 | `lj_w_edit_set_text` puts the caret at 0; the Sequence form's ES / EF call `lj_w_edit_select(w, len, 0)` after the write | `widgets_basic: 0 failure(s)` (caret 0 after `set_text("4:00")`); `test_windows_a`: `ME-206 Sequence ES: caret 4 after the form writes "4:00"`, `W2A_TSEQUENCEFORM_CARET: identical`; three goldens moved, each with a REVIEWED row naming the caret: `W4DD_TSAVESELECTIONFORM` (17 px), `W4SD_NAME_SLOTS` (19 px), `W4SD_STATIC_SLOT_NAME` (22 px); against the v1.1.0 goldens only the ME-161-3DLIGHT caption-button pixels differ (23, 23, 69), so the caret is where v1.1.0 drew it |
| 13 | ME-193 | the bullet deleted from `help/not-available.txt`, `help_check.py --write`, `docs/user-guide.md`; both files appended to the release-step sentence (`w6-gates.md` s3) | `git grep -n -i "Use / Ignore" -- help src/windows/lj_help_topics_gen.c docs/user-guide.md`: no output (rc 1); `help_check: 19 texts against 200060 reference 8-word runs`, `help_check: 18 topics`; `help_check selftest: OK` |
| 14 | ME-195, -202, -203, -205 | fidelity rows appended to the W6 fidelity track table (Debug tabs, multi-select, the Export Warning body, beside the ME-199 and ME-201 rows); 12 REVIEWED rows; `(F47).;` -> `(F47);`; headings: one H2 `Fidelity rows touched by W6` over three H3s (levels only); `w6-gates.md` s3 gains the GP-DMXIN and ME-190 rulings and the W5 blocker sentence | `fidelity_check: 73 F/I id(s) with a row of 73, sacrifice paragraphs equal, 0 finding(s)`; `grep -n "OwnerDraw" docs/fidelity.md`: line 379; `grep -n "MultiSelect" docs/fidelity.md`: line 382; ME-203: the newest REVIEWED row of each of the 12 forms names its GIF (12 of 12, split on tabs); `grep -c "(F47)\.;" src/fixlib/hand/*.c`: 0 in each; `grep -c "^## Fidelity rows touched by W6" docs/fidelity.md`: 1; `ui_golden` / `ui_golden_ctl` Passed |

ME-203 note: for four of the twelve forms the window is not sizeable, and the rows say so instead of "sizeable":
TCUEMACROFORM (bsSingle, `cuemacrocontrol2.gif` reduced and cropped), TFINGERSSTATUS (bsToolWindow,
`fingersstatuslarge.gif` shows part of the panel), TFIXTUREINFOFORM (bsToolWindow, `fixtureinfo.gif` reduced; the
seat's title match `fixturefoldermode.gif` is the profile editor's tab) and TSAVEPRESETFORM2 (the help has no capture
of this box; the title match `savecuelistdialog.gif` is the Save Cue List dialog).

ME-190 note: the verdict's "the other 44 equal `a1d1f2de...`" needs the harness to enable the input from tick 0 in a
scenario with any `dmx_in` line. With the bit posted at the first `dmx_in` line instead, `dmxin_s1_ignore.lj4r`
changed at tick 0 (its Ignore channel no longer took the empty input before the first frame) and the 44-scenario dump
read `bd70d29d...`; the build-time post restores `a1d1f2de...` exactly.

D4: `git grep -o "PROPOSAL rig" -- src tests tools | wc -l` is 396 at `55527ac` and 396 here (the Save Palette comment
was reworded with its tag kept); the whole tree 503 -> 504 (the new fidelity row's tag). `git diff v1.1.0 --
src/engine/lj_engine_rules.h` empty. No listener, dependency, secret or workflow change.

Builds and tests (every tree `--clean-first` on the must-fix tree, then rebuilt incrementally after the last change,
`tests/engine/lj_replay.c`; 0 warnings, 0 errors each time):

    build-w6i-rel   (Release, SDL)        [592/593]   ctest 100% tests passed, 0 tests failed out of 158
    build-w6i-nosdl (Release, LJ_SDL=OFF) [352/353]   ctest 100% tests passed, 0 tests failed out of 156
    build-w6i-dbg   (Debug, SDL)          [592/593]   ctest 100% tests passed, 0 tests failed out of 158
    test_layering                         463 file(s) checked, 221 header(s) compiled alone, ..., popup_at 2, ip-parse copies 0, 0 finding(s)
    fidelity_check                        73 F/I id(s) with a row of 73, sacrifice paragraphs equal, 0 finding(s)
    help_check                            19 texts against 200060 reference 8-word runs; 18 topics
    replay_all                            replay: 36 scenario(s), 0 failed
    ui_golden / ui_golden_ctl             Passed / Passed (three goldens re-rendered for ME-206, REVIEWED rows above)

The new rows: `golden_anchors_selftest`, `replay_w5_dmxin_s1_ignore_off` (158 = 156 + 2; no-SDL 154 + 2).

Pre-commit scans over the working-tree diff against `55527ac` (46 paths) and the commit message: `reference/` 0 paths,
sha1 of each changed file (as-is and CRLF->LF) against the 18,926 `reference/` files 0 equal; REL-PRIVATE (the
ME-148 table: user name, work-PC and host names, both LAN prefixes of this PC, its NIC names, the account mails, the
design-vault root, the user-profile path, plus the old account name) 0 hits in the added lines, the new file and the
message. No capture was re-shot.

### Must-fix r1: CI round 1 (`d1025e4`, run 40061418618) and the fix

generate-check, linux-clang and macos-clang green; linux-gcc-asan and the windows-msvc ASan step failed on
`test_uiscript_w3`, determinism skipped. gcc ASan: `ERROR: AddressSanitizer: heap-use-after-free` in
`lj_window_event` (`src/ui/lj_window.c:657`) from `lj_us_key` (`src/app/lj_uiscript.c:603`), the memory freed by
`destroy_now` during the same `key` statement. The ME-201 row is the first script that closes a window with a key:
Return in the Save Palette name field saves, Auto Close destroys TSavePresetForm2 on the key-down, and the script
driver then sent the key-up to the freed window. Fix (this commit): `lj_us_key` re-validates its window after each
key-down (and before each repeat), and `lj_us_type` before each character. The local Release / Debug trees do not
run ASan, so they passed; the local MSVC ASan tree (`build-w6i-fuzz`, LJ_SDL=OFF, Debug, `/fsanitize=address`, the
CI step's settings) now runs `test_uiscript_w3` Passed, and its whole ctest, kept off logical CPUs 8-11 (section 4):
`100% tests passed, 0 tests failed out of 156`. After the fix, rebuilt incrementally at 0 warnings:

    build-w6i-rel   ctest 100% tests passed, 0 tests failed out of 158
    build-w6i-nosdl ctest 100% tests passed, 0 tests failed out of 156
    build-w6i-dbg   ctest 100% tests passed, 0 tests failed out of 158

### Must-fix r1: CI round 2, green (`fdb94a2`, run 40064261235)

https://github.com/MenaceToTheShow/LJ4EVR/actions/runs/40064261235: six of six jobs green (generate-check,
linux-gcc-asan, linux-clang, windows-msvc, macos-clang, determinism).

    linux-gcc-asan   100% tests passed, 0 tests failed out of 157
    linux-clang      100% tests passed, 0 tests failed out of 157
    windows-msvc     Release and Debug: 100% tests passed out of 158, 158; MSVC ASan (LJ_SDL=OFF, Debug): out of 156
    macos-clang      100% tests passed out of 159
    determinism      frames-{linux,windows,macos}.bin: 5b13f41930362b75a7aa6f9c53f77db2d315a4039ea5a18da664ea1263114b50
                     replay-frames.bin L / W / M: 3474ee7386eab212115c122c07595ec161a0e2e65f5732e89c127cf618f95f34 (= the local trees)

Left for the next steps (not this pass): ME-207 (release sequence: VERSION, CHANGELOG, the 1.2.0 paragraph, the D4
count record, the owner name in `CONTRIBUTING.md` / `docs/releasing.md`, RC CI, the scans, tag `v1.2.0`, assets),
ME-208 (design, vault), and one verifier's check of each Verify line above.

## Release

### v1.2.0 (ME-207)

2026-10-24 (UTC), after the must-fix r1 lines (items 1-14) were verified on `f670fbf` (CI run 40064926688, six of six).
Every app run used `SDL_VIDEO_DRIVER=dummy` (and `SDL_AUDIO_DRIVER=dummy`) in the foreground under a timeout; no window
reached the screen.

`Release 1.2.0` (`e2ece32`): `VERSION` 1.2.0 (configure re-run in `build-w6i-rel`, `-nosdl`, `-dbg`: `lj4evr.exe
--version` = `LJ4EVR 1.2.0`; ctest 158 / 156 / 158 of 158 / 156 / 158 passed, 0 warnings), `CHANGELOG [1.2.0]` (the
WIZARD_STYLES-6 list plus ME-190, -191, -192, -197, -199 and -206 in user terms, what 1.2.0 is not, the known gaps with
the ME-189 microphone line, developer detail), the README status line, a "What v1.2.0 is not" paragraph in README and
`docs/fidelity.md` (the v1.0.0 and v1.1.0 paragraphs untouched; `fidelity_check` now compares all three:
`fidelity_check: 73 F/I id(s) with a row of 73, sacrifice paragraphs equal, 0 finding(s)`, selftest passed; with the
new fidelity.md paragraph's last full stop removed: `the "What v1.2.0 is not" section is not byte-equal to README.md's`,
rc 1), and `CONTRIBUTING.md` / `docs/releasing.md` naming `MenaceToTheShow/LJ4EVR` (`git grep -n UnbannableGT --
CONTRIBUTING.md docs/releasing.md`: no output, rc 1). The earlier wave logs stay as written.

    ci.yml run 40066853105 (e2ece32)   success: generate-check, linux-gcc-asan, linux-clang, windows-msvc, macos-clang,
                                       determinism (six of six)
    tag                                git tag -a v1.2.0 -m "LJ4EVR 1.2.0" on e2ece32 (= v$(cat VERSION)), pushed
    release.yml run 40067527564        check, windows, macos, release: success
    gh release view v1.2.0             "LJ4EVR 1.2.0", isPrerelease false, isDraft false, published 2026-10-24T05:49:10Z;
                                       repos/MenaceToTheShow/LJ4EVR/releases/latest = v1.2.0
    assets                             LJ4EVR-1.2.0-win64-setup.exe   1,940,372 bytes
                                       LJ4EVR-1.2.0-macos.dmg         5,144,815 bytes
                                       SHA256SUMS                       184 bytes
    SHA256SUMS                         efd1bc70fd993ff8b9fea75f3e284be8da3a4372bf65007f0144fc226d440814  LJ4EVR-1.2.0-win64-setup.exe
                                       8187e553bba804db2e881c8c4fd96d53e3680e53d749887ba88c3533a02c3c46  LJ4EVR-1.2.0-macos.dmg
                                       (downloaded, sha256sum -c: both OK)
    notes                              the install / notices / trademark header, then CHANGELOG [1.2.0] up to "Developer
                                       detail:"; gh release view v1.2.0 --json body -q .body | grep -c -i microphone = 1

Windows installed tree (the downloaded release asset, silent per-user install into a scratch folder:
`LJ4EVR-1.2.0-win64-setup.exe /S /D=<scratch>\inst120`, exit 0; no LJ4EVR install or uninstall key existed before):

    lj4evr.exe                12,060,160
    THIRD_PARTY_NOTICES.txt       15,645
    Uninstall.exe                111,921
    help\  audio-media controls cuelists cues desktop files fixtures getting-started hotkeys-midi import index input
           macros not-available notices output sequences statics-bgcues  (.txt, 18 topics)
    HKCU ...\Uninstall\LJ4EVR   DisplayVersion=1.2.0, UninstallString = "<scratch>\inst120\Uninstall.exe"; no HKLM key
    help_check.py --install-tree         install_check: <scratch>\inst120\THIRD_PARTY_NOTICES.txt ok (rc 0)
    lj4evr.exe --version                 LJ4EVR 1.2.0 (rc 0)
    w4_entry_points.lj4s against the installed exe (scratch LJ4EVR_USERDATA prepared by w4_entry_prep.sh alone:
      Default W4Aux W4Entry W4Update; a scratch working directory for the shots)
                                         rc 0, 156 PASS lines, 0 SCRIPT FAIL, 0 "no binding" lines (18 s)
    w5_acc_media.lj4s against the installed exe (scratch LJ4EVR_USERDATA from `w5_wire.py prep`, U:W5Acc)
                                         rc 0, 0 SCRIPT FAIL; `media: <scratch>\ud5\User\W5Acc\tone.wav loaded, 44100 Hz,
                                         1 ch, 176400 frames`; clip.mp3 / clip.avi: `not a local .wav file, kept and skipped`
    Uninstall.exe /S                     exit 0; the folder, lj4evr.exe and the HKCU key gone

macOS (the `release.yml` macos job log; no hand run, ME-189):

    lipo        Architectures in the fat file: build-release-mac/lj4evr.app/Contents/MacOS/lj4evr are: x86_64 arm64
    codesign    Format=app bundle with Mach-O universal (x86_64 arm64), Signature=adhoc; --verify --strict:
                valid on disk, satisfies its Designated Requirement
    Info.plist  "CFBundleShortVersionString" => "1.2.0", "LSMinimumSystemVersion" => "12.0"; minos 12.0
    .dmg        5,144,815 bytes (downloaded), UDIF trailer at size - 512 = `koly` (6b 6f 6c 79); hdiutil verify: every
                partition verified; mounted: Applications -> /Applications, LJ4EVR.app; install_check:
                .../LJ4EVR.app/Contents/Resources/THIRD_PARTY_NOTICES.txt ok

Windows release job: the `dumpbin /dependents ... | findstr /i "vcruntime msvcp ucrtbase" && exit /b 1` step passed;
installer VERSIONINFO `ProductName=LJ4EVR ProductVersion=1.2.0 FileVersion=1.2.0`; install_check on the silent CI
install ok.

D4, corrected record (WIZARD_STYLES-5): `git grep -o "PROPOSAL rig" | wc -l` was **503** at `55527ac`, not the 505
quoted in section 4 and in `docs/w6-gates.md` s3 (505 was the count at `d701f79`, before `b3afd86` deleted the track
logs that carried 2). `-- src tests tools`: 393 at `v1.1.0` -> 396 at `55527ac` -> 396 at `v1.2.0`; `git diff v1.1.0
v1.2.0 -- src tests tools`: 0 removed lines and 3 added lines carry the tag. Whole tree at `v1.2.0`: 508 (the must-fix
fidelity row, the two "What v1.2.0 is not" paragraphs and the CHANGELOG line). `git diff v1.1.0 --
src/engine/lj_engine_rules.h`: empty. Every `PROPOSAL rig` tag stays (D4 carried, Joey).

Scans over the release (`v1.1.0..v1.2.0`, 22 commits, 451 paths), the release notes and every commit message:
`reference/` 0 paths, and the git-blob sha1 of each of the 451 changed files (as-is and CRLF->LF) against the 18,923
non-empty `reference/` files: 0 equal. REL-PRIVATE (the ME-148 table: user name, work-PC and host names, this PC's LAN
prefixes, its NIC names and descriptions, the account mails, the design-vault root, the user-profile path): 0 hits in
3,399 added and message lines. The old account name appears in 7 of those lines, each a record of the 2026-10-23
rename (`docs/ci.md:76`, `docs/w6-gates.md:4`, `docs/w6-integration.md` section 6 and the end of section 7, and the
`a38b065` message); it is the public former login of this repository, kept as written. No capture was re-shot in the
release step.

ME-207 ticked by this record. Open after the tag: ME-208 (design.md v1.2.0 paragraph, orchestrator) and the verdict's
section 5 carries (the reference rig, the Mac hand run, the OemCode, ME-209, ME-210, ME-211).

### ME-208 (design.md, orchestrator)

Applied 2026-10-24 by the orchestrator as design owner in the design vault (not in this repository): `design.md`
section 1 gains the "What v1.2.0 is not" paragraph matching the README, section 10 gains "W6 Carried rows" with its
shipped line, and section 13 gains the W6 revision line. `grep -c v1.2.0 design.md` = 3. ME-208 ticked; W6 is closed.
