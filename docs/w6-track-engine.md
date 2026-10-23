# W6 track engine

Written 2026-10-23 by the W6 `engine` track, branch `w6/engine` from `origin/main` @ `2668de8` ("W6: gates"). Rows are
the six `engine` rows of `docs/w6-gates.md`. Each fixed row names its runnable check and quotes the output of its
Verify line from this worktree (MSVC 19.44, Release, `LJ_SDL=OFF` unless noted). A negative run was made for three
rows: the fix reverted with the new row kept, the row failing as quoted, then the fix restored.

## Rows

| Id | Mode | Status | Change | Check |
|---|---|---|---|---|
| W5/LORD_ENGINE-7 | LAND | done | `lj_audio_state` gains `t0_us`, `samples`, `clocked` (appended). `lj_audio_analyze` anchors `t0` on the first call after a reset (`now_us` less that call's samples) and times each block at `t0 + samples * 1000000 / LJ_AUDIO_RATE`; a poll's wall clock no longer moves a trig. | `test_audio` `test_sample_clock` |
| W5/LORD_ENGINE-8 | LAND | done | `lj_engine_ext_copy` walks `LJ_LAYER_ORDER`; the statics come from `lj_merge_statics_order` (the insertion sort `lj_merge_statics` already used, now shared), so the later activation wins as in the DMX merge. | `test_extapp` `engine_statics` |
| W5/LORD_ENGINE-9 | LAND | done | `lj_app_cuelist_pass` (`lj_app_cuelist_host.c`) runs `lj_app_io_idle` (MediaPlayer clock, DMX-in, audio, remote), then `lj_app_cuelist_step`, then `lj_app_cuelist_host_idle`; `lj_app.c:255` calls it in place of the three calls (the cue list used to step before the I/O). The External Application half of `lj_app_io_idle` reads the engine's published frame, which the cue-list step cannot change within the pass (it only posts commands), so it was not split out. | `test_media` `app_pass` |
| W4/LORD_ENGINE-9 | LAND | done | `lj_engine_status.tick_us_sum` (appended) adds every tick's time beside `tick_no`. TDebugForm samples the pair at each refresh and divides the difference between the newest sample and the oldest within 1 s / 5 s by the ticks between them (`lj_debug_form_load_sample`, `lj_debug_form_load_avg_us`); no tick in between = the last tick's time, so the fixed-view golden `W4DBG_DEBUG` is unchanged. `docs/fidelity.md` row appended (W6 section) superseding the LORD_ENGINE-9 (LOW) row. | `test_debug_form` (two refresh rates) |
| W3/INT-s7-TCSRC | DONE | done at `2668de8`, re-run | none | `ctest -R "^test_media$"` |
| W3/INT-s7-SYNCFRAMES | LAND | done | `lj_mtc` gains `sync_frames`, `qf_run` (appended). `lj_mtc_feed` counts quarter frames since the time code started (a gap past the 0.5 s time-out restarts the count); `lj_mtc_time` returns -1 until `qf_run >= 4 * sync_frames`. `lj_app_cuelist_step` copies Main.Ini `[CUELIST2] SyncFrames` (0 / 1 / 5 / 10, anything else = 0) into the host's decoder with the other `[CUELIST2]` keys. The frame meaning is a PROPOSAL (`PROPOSAL rig W6-SYNC`): LJ2's help does not define it. `docs/midi.md` says so. | `test_midi_map` `lj_t_feed_mtc` |

No row is CLOSED IN WRITING.

## Verify output

W5/LORD_ENGINE-7, `lj4evr_tests test_audio`:

    sample clock: 8 trigs under a +-20 ms jittered wall clock, times equal to the exact clock's (first 1011609 us, last 4506213 us)
    test_audio: PASS (0 failures)

The fix reverted (`now_us - n * 1000000 / LJ_AUDIO_RATE`): `FAIL tests\io\test_audio.c:224 ok`, `test_audio: FAIL (1 failure)`.

W5/LORD_ENGINE-8, `lj4evr_tests test_extapp`:

    engine: two statics on fixture 5, the later-activated one's string wins (A, B, A again on runner 0)
    test_extapp: 0 failure(s)

The old runner-order walk restored: `FAIL tests\io\test_extapp.c:182 f->ext_fixture[0] == 5 && strcmp(f->ext[0], i == 1 ? "STATIC B" : "STATIC A") == 0 ...`.
Replay dump sha unchanged (below).

W5/LORD_ENGINE-9, `lj4evr_tests test_media`; the order in `src/app/lj_app_cuelist_host.c:206`:

    void lj_app_cuelist_pass(uint64_t now_us)
    {
        lj_app_io_idle(now_us);                                          /* W6 LORD_ENGINE-9: the media clock and the inputs first */
        lj_app_cuelist_step(now_us);
        lj_app_cuelist_host_idle(now_us);

    app pass: MP Wait 00:00:02:00 released in the pass the clip reached frame 88200 (next line 2)
    test_media: ok (0 failures)

With `lj_app_io_idle` moved after `lj_app_cuelist_step`: `FAIL tests\io\test_media.c:115 c->cl->next == 1 && c->cl->tc_ms == 1990`
and `FAIL tests\io\test_media.c:116 c->cl->next == 2 && c->cl->tc_ms == 2000`. `lj4evr --selftest` (Release, SDL,
scratch userdata): `selftest cue-list runner: 3 step(s) over 3 idle passes: OK`, exit 0.

W4/LORD_ENGINE-9, `lj4evr_tests test_debug_form`:

    loads from the sum pair: 1 s 899 us / 5 s 979 us at 10 Hz, 899 / 979 at 25 Hz (true means 899 / 979)
    test_debug_form: 0 failure(s)

W3/INT-s7-TCSRC: `src/app/lj_cuelist_run.c:286` sets `r->st.tc_src` (`case LJ_CL_OP_TC_SRC`), `tests/io/test_media.c:148`
asserts it (before this track's rows were added; the row is unchanged). `ctest -R "^test_media$"` at `2668de8`:

    1/1 Test #140: test_media .......................   Passed    0.01 sec
    100% tests passed, 0 tests failed out of 1

W3/INT-s7-SYNCFRAMES, `lj4evr_tests test_midi_map`:

    MTC SyncFrames=5: follows at quarter frame 20 (frame 5)
    test_midi_map: 0 failure(s)

The same row checks that quarter frame 19 still reads -1, that a restart after a 0.6 s gap counts again, and that Off
follows at the first full set (quarter frame 8).

## Build and checks

| Tree | Config | Warnings | ctest |
|---|---|---|---|
| `build-w6e-rel` | Release, `LJ_SDL=ON` | 0 | `100% tests passed, 0 tests failed out of 153` |
| `build-w6e-dbg` | Debug, `LJ_SDL=ON` | 0 | `100% tests passed, 0 tests failed out of 153` |
| `build-base-nosdl` | Release, `LJ_SDL=OFF` | 0 | `100% tests passed, 0 tests failed out of 151` |

`test_layering`: `463 file(s) checked, 221 header(s) compiled alone, ... 0 finding(s)`. `fidelity_check`: `73 F/I
id(s) with a row of 73, sacrifice paragraphs equal, 0 finding(s)`.

Determinism, equal in all three trees and equal to the W5 / `2668de8` baseline (measured before the first edit):

    dump-frames        5b13f41930362b75a7aa6f9c53f77db2d315a4039ea5a18da664ea1263114b50
    replay-frames.bin  9da72c3f56a0b728c1e0adfd006d51f68b7fc787e1d00c03045db27f18560adb (129,914,872 bytes)

No hash changed, so nothing was re-recorded. Standing rules: `git diff v1.1.0 -- src/engine/lj_engine_rules.h` is
empty; `git grep -o "PROPOSAL rig" | wc -l` = 493 (>= 487); no listener, dependency, workflow or OS-specific file
touched.

## Shared files

Edited in place (not append-only tables): `src/app/lj_app.c` (the three idle calls become one, line 255),
`src/app/lj_app_cuelist_host.c` / `.h` (the pass, the SyncFrames key), `src/windows/lj_debug_form.c` / `.h`,
`docs/fidelity.md` (a new W6 section before "What v1.1.0 is not"), `docs/audio.md`, `docs/midi.md`. The fidelity track
also edits `lj_app.c` (Restart) and `fidelity.md`; the two changes sit in different places.

The first full ctest of the two SDL trees showed `test_sacn_loopback` / `dmxin_loopback` failing while another
track's `lj4evr_tests` ran its sACN loopback on the same host at the same time (crossed UDP 5568 traffic); with that
run finished, both trees pass 153 of 153 (above), and the two rows pass on their own.

Not done: gcc and clang were not run locally; CI covers them.
