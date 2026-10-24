# W6 track dmxpatch

Branch `w6/dmxpatch`, from `origin/main` @ `2668de8` ("W6: gates"). Rows of `docs/w6-gates.md` owned by this track:
W5/GP-DMXIN and W5-INT/STATICS-LATCH. Both LAND. Toolchain: MSVC cl 19.44 + Ninja (vcvars64), SDL from the
`release-3.4.18` source (`FETCHCONTENT_SOURCE_DIR_SDL3`). No `reference/` junction was needed.

## W5/GP-DMXIN: LANDED

Global Patch per-channel "Use / Ignore DMX In Enable" (popup `DMX in Enable Switch` > `Use` / `Ignore`, DFM items
`P_EnableUse` / `P_EnableIgnore`). The CHM (`globalpatch.htm`) says: channels marked as ignoring the switch "will always
be overwritten by the DMX in channel values regardless of overwrite Enable state".

- Model: `lj_patch_ch.ign_en` (`src/model/lj_model.h`), appended after `htp_mask`. `lj_patch_is_default` compares field
  by field, because the new byte leaves tail padding that `memcmp` would read. `lj_patch_validate` refuses a value
  above 1.
- `.GPA` / `GlobalPatch.GPA` ("LJ4 patch 1"): the key ` ign=1` goes at the end of a `ch` line and is written only when
  set. An absent key reads as 0 (Use), so a v1.1.0 file loads unchanged and saves byte-identical. `ign` outside 0..1
  refuses the file (`docs/file-formats.md`).
- Engine S1 (`lj_post_s1_patch_src`): the DMX-in source applies when Overwrite Enable is on or the channel has
  `ign_en`.
- Window (`lj_global_patch.c`): both items are no longer greyed. They set or clear the switch on the selected DMX-in
  row; on a row without a DMX-in source they do nothing. The `In mode/Sw` column reads `Abs/Ign` (PROPOSAL rig).
  Clearing the DMX-in source, or Set Values without a DMX In source, resets the switch.
- Replay: `gpatch ... ign=0|1` (`docs/replay-grammar.md`); scenario `tests/engine/replay/w5/dmxin_s1_ignore.lj4r`.
- Docs: a superseding F48 row in `docs/fidelity.md` (section "W6 dmxpatch"), and `docs/dmx-in.md`.
- Verify-line deviation: the gate wording "an Ignore channel keeps its patch output while DMX-in Overwrite Enable is
  on" is read through the CHM. An Ignore channel takes DMX-in while Overwrite Enable is **off** and a Use channel
  keeps the programmed value; with the gate on, both take DMX-in. The rows below assert exactly that.
- Not done here (release engineer): the README / `fidelity.md` "What v1.1.0 is not" paragraphs describe the tagged
  v1.1.0 and stay byte-equal (`fidelity_check` compares them). The 1.2.0 sacrifice paragraph and the CHANGELOG must drop
  the "Use / Ignore DMX In Enable" item.

Verify (build-rel, Release, SDL ON):

```
> lj4evr_tests test_engine_w5
test_engine_w5: PASS (0 failures)
> lj4evr_tests replay tests/engine/replay/w5/dmxin_s1_ignore.lj4r
replay dmxin_s1_ignore: 6 ticks, 4 expects, 0 fails
replay: 1 scenario(s), 0 failed
> ctest -R "^test_model_records$"
100% tests passed, 0 tests failed out of 1
> ctest -R "^(test_dmx_in|fidelity_check|test_engine_w5|replay_w5_dmxin_s1_ignore|replay_dump)$"
1/6 Test  #48: replay_dump ......................   Passed    2.03 sec
3/6 Test #105: test_dmx_in ......................   Passed    0.15 sec
4/6 Test #118: fidelity_check ...................   Passed    0.08 sec
5/6 Test #120: test_engine_w5 ...................   Passed    0.01 sec
6/6 Test #150: replay_w5_dmxin_s1_ignore ........   Passed    0.01 sec
> python tools/fidelity_check.py --repo .
fidelity_check: 73 F/I id(s) with a row of 73, sacrifice paragraphs equal, 0 finding(s)
```

What the rows assert:

- `test_engine_w5`: with Overwrite Enable off, Use channel 1:10 keeps 77 and Ignore channel 1:12 gives 200. With the
  gate on, both give 200.
- `dmxin_s1_ignore.lj4r`: gate off, 1:1 (Use) = 100 and 1:2 (Ignore) = 200. Gate on, both = 40. Gate off again,
  1:1 = 100 and 1:2 = 40. After `gpatch 1:2 ign=0`, both = 100.
- `test_model_records`: `ign=1` round-trips. A literal v1.1.0 line loads with `ign_en` 0 and saves back
  byte-identical. `ign=2` is refused by both the loader and the validator.
- `test_dmx_in`: both items are live. Ignore on row 2 gives `Abs/Ign` in the row and reaches `show->patch` on Apply.
  Use clears it. A row without a DMX-in source stays 0.

## W5-INT/STATICS-LATCH: LANDED

DMX-in "Static n, Latch" no longer calls `lj_statics2_press` on the current page (`lj_dmx_in.c:670` at `bae0441`).
The new `lj_statics2_latch(slot, on)` (`src/windows/lj_statics2_form.c`) does three things:

- At 50 % it latches slot n of the Statics page shown, with `c = LJ_REP_ADD`, whatever its Flash or replace mode.
  This follows the CHM: "flash and merge/replace modes disregarded".
- It records the slot's global index.
- Below 50 % it posts `LJ_CMD_STATIC_DEACTIVATE` for that recorded index, so a page change in between does not
  move the release.

A repeated rise does not post again, and a fall with nothing latched does nothing. Which page LJ2 addresses is not
written; we use the current page (PROPOSAL rig 14 sB.8 item 9, as the Static 01..20 hotkeys). Fidelity row: section
"W6 dmxpatch" of `docs/fidelity.md`.

Verify: `test_dmx_in` (block "W6 STATICS-LATCH"). A Flash-mode, Replace Page slot (page 1, slot 3) is activated
through `LJ_FN_DMXIN_STATIC`:

- The posted command is `STATIC_ACTIVATE a = lj_static_global_index(1,3), c = LJ_REP_ADD, st = &pages[0].slot[2]`.
- A second rise posts nothing.
- The Statics page is set to 2 (not the current page any more). The fall then posts
  `STATIC_DEACTIVATE a = lj_static_global_index(1,3)`.

```
> lj4evr_tests test_dmx_in
  W5DMX_EXPORT_TABLE: identical
test_dmx_in: 0 failure(s)
```

## The other W5 dmxin blockers named in the brief: already landed in W5 (no change)

- `lj_library_list` skips ValueTables: `src/model/lj_library.c:236` `!lj_ieq(name, "ValueTables")`.
- Fingers DMX-in pages: `src/windows/lj_fingers.c:187` calls `lj_dmxin_emulate`.
- W4DMX_* goldens: 0 files in `tests/ui/golden/` (`ls tests/ui/golden | grep -c W4DMX` = 0). The rows in
  `REVIEWED.tsv` stay, because that file is append-only. One stale comment remains in the shared `tests/lj_tests.c`
  table (line 239, "goldens W4DMX_*"). It is left alone under the append-only rule for that file.

## Tree checks (worktree, all three trees)

| Tree | Build | Warnings / errors | ctest |
|---|---|---|---|
| build-rel (Release, SDL ON) | ninja rc 0 | 0 / 0 | `100% tests passed, 0 tests failed out of 154` |
| build-nosdl (Release, SDL OFF) | ninja rc 0 | 0 / 0 | `100% tests passed, 0 tests failed out of 152` |
| build-dbg (Debug, SDL ON) | ninja rc 0 | 0 / 0 | `100% tests passed, 0 tests failed out of 154` (rerun with `-j 4`; the first `-j 8` run had `test_sacn_loopback` fail once while other tracks ran loopback tests on the same machine; it passed alone and in the full rerun) |

- `test_layering`: `463 file(s) checked, 221 header(s) compiled alone, ... 0 finding(s)`.
- `fidelity_check`: `0 finding(s)`.
- `PROPOSAL rig` count: 494 (at least 487).
- `git diff v1.1.0 -- src/engine/lj_engine_rules.h`: empty.
- The private-detail grep over the diff: 0 hits.
- The replay dump grows by one scenario. CI's determinism job compares the three OSes with each other, not against
  a pinned sha.
