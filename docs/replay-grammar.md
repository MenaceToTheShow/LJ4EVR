# Replay scenario grammar (`tests/engine/replay/*.lj4r`)

Design 3.7 made precise. One scenario per file; `lj4evr_tests replay <file>...` builds
the show the way the UI thread does (records loaded and pinned, commands posted at the
named ticks), calls `lj_engine_tick` with synthetic time and asserts frames.
`tests/engine/lj_replay.h` is the entry (`lj_replay_run`). The harness owns the file
grammar; the engine never sees it.

Conventions: CP1252 text, one statement per line, `#` to end of line is a comment,
blank lines ignored. Numbers are decimal unless prefixed `0x`. Strings are quoted when
they contain spaces; a quote inside is doubled (design 5.2). `l:a` is link 1-4 and
address 1-512; `v` is 0-255; `S` is a fade-state letter `O` `S` `F`. Ticks are integers
counted from 0: tick k runs at `now_us = k * tick_us`. The engine is created, bound and
told `tick_us` before tick 0; commands `at k` are posted (under the lock, `at_us` =
`k * tick_us` unless given) before tick k runs; `expect k` is checked after tick k ran.
A line form the harness does not know is a grammar error and fails the scenario, naming
its line number. Event lines (`at`, `expect*`, `run`, `perf`) are in tick order: an `at k`
comes before any `expect k`, and a tick that already ran is a grammar error. A line's
grammar is checked before its tick runs, so a bad `expect` never advances the clock. The
harness reads the lines itself (the FAIL message needs the line number, which `lj_rec`
keeps private) and parses the fields with `sscanf`/`strtol`, bounded.

## Header and build lines (before the first `at`)

| Line | Meaning |
|---|---|
| `LJ4 replay 1` | first line, mandatory; `1` = this grammar version |
| `tick_us=25000` | tick period (default `LJ_TICK_US_DEFAULT`); clamped like `lj_engine_set_tick_us` |
| `patch n profile=<id> link=<l> addr=<a> [ipan=0 itilt=0 swap=0 ibo=0 igm=0 lsgm=0 sub=<0..8> gs2=<file>]` | fixture n (1..100) with exe row id (`19` = Generic / DMX Dimmer / 1 Channel, `12` = Martin / MAC 500 / Mode 2/4) at link:address; flags as `Config.dat`; `gs2=` loads a user profile through `lj_gs2_parse` (W3; the W1 harness rejects it) |
| `profile n key=value ...` | after fixture n's `patch`: a per-fixture copy of its built-in profile with fields overridden (what a `.GS2` would carry), bound with `lj_engine_bind_profile`. Keys: `use_gm bo_at_gm0 invert_int invert_range limit_gm` (0/1; `invert_int=1 invert_range=1` = "Invert Intensity value range"), `imin imax` (the intensity function's Min/Max), `ioffset` (1-based), `class<k>=none\|intensity\|shutter\|strobe\|pan\|tilt\|color\|gobo\|beam\|effect\|level\|speed\|lamp\|special\|switch\|smoke\|haze\|haze_fan`, `disabled<k>` (`level_disabled`), `ddmx<k>` (`default_dmx`), `extgm<k>`; k = 1..footprint |
| `default l:a=v [l:a=v ...]` | `default_out[]` entries (rank 0); unlisted channels are 0. `patch` and `profile` first copy the profile's `default_dmx` into `default_out` for the footprint (what Fixture Configuration does on save, LORD_FIXTURES-11); a `default` line after them overrides |
| `gpatch l:a [src=none\|out\|in sl=<l> sc=<c> mode=abs\|lotp\|htp\|gt0\|lt255 inv=0\|1 min=<v> max=<v> htp=<mask>]` | one global-patch row (02 s7.1) |
| `submaster k name="<5 chars>" mode=prop\|lotp\|htp\|none` | sub-master k (1..8) mode |
| `seq n name="<name>" [sync=0 gst=0 gft=0]` | starts sequence n; the following `scene` / `ch` / `fx` lines belong to it until the next `seq`, `cue`, `bg`, `static`, `macro` or `at` |
| `scene st=<ms> ft=<ms>` | appends a scene (scene time, fade time) to the current sequence |
| `ch l:a=v:S [l:a=v:S ...]` | channels of the current scene; unlisted channels are Off; a channel no patch line covers is a grammar error (the UI cannot program one) |
| `fx n preset=<1..99>` | position preset reference of fixture n in the current scene (presets come from `preset` lines) |
| `fx n macro=<type>,<amp_pan>,<amp_tilt>,<cycle_pan>,<cycle_tilt>,<dir>,<delay>,<tweak>,<step> mst=<S>,<S>,<S>,<S>,<S>,<S>` | movement macro of fixture n in the current scene, fields of `lj_movemacro` in order, `mst` = the six per-parameter fade states |
| `preset n k pan=<int16> tilt=<int16>` | position preset k of fixture n (readout units, 17 s1) |
| `cue n [name="<name>" cuetime=<pct> macro=<amp>,<speed> transparent=0\|1 bgcue=<n> genmacro=<n>]` | starts cue n; `cuetime` default 100 |
| `slot k seq=<n> [on=1 trig=auto\|manual\|audio dir=fwd\|rev\|bounce\|rnd spt=1 atpt=1 tol=0 first=0 last=0 loops=0 end=stop\|out\|goto\|link goto=0 link=0 time=0 chase=0]` | slot k (1..12) of the current cue; defaults as the `.cue` record (design 5.2) |
| `bg n [name="<name>" transparent=0\|1]` then `bgslot k seq=<n> [mode=auto\|audio\|noloop on=1]` | background cue n with its slots 1..5 |
| `static p.s [name="<name>" act=toggle\|flash rep=add\|slot\|page\|group\|all\|solo_slot\|solo_page\|solo_group\|solo_all ft=<ms> level=<pct> groups=<mask>]` then `ch` lines | static slot s (1..20) of page p (1..64), recorded scene from the following `ch` lines; `link=<seq>,<scene>` instead of `ch` lines makes a link slot |
| `macro n [name="<name>" restart=0\|1]` then `mch l:a shape=<0..65> base=<0..255\|off> amp=<pos>,<neg> cycle=<ds> dir=fwd\|rev\|bounce delay=<0..8191>` | generic DMX macro n (14 sA) |
| `htp k l:a [l:a ...]` | HTP group k (1..24) membership (sets `htp_mask` bit k-1 on those channels) |

Validation: every record built is passed through its `lj_<x>_validate()`; a violation is
a scenario failure naming the line. A `slot seq=` that names no `seq` line is NOT an
error: it is the `load error` case (the harness hands the engine `seq[i] = NULL`).

## Command lines: `at <tick> <cmd> [args]`

One `lj_cmd` per line, posted before the tick runs; `[at_us=<us>]` at the end overrides
the timestamp (chase-tap resolution). Every `LJ_CMD_*` kind has a form:

| Form | Command |
|---|---|
| `at k load_cue <n> [edit] [nobg]` | `LJ_CMD_LOAD_CUE` with the cue's sequences, bg cue and macro pinned as the UI would (`edit` = Edit Cue, links do not fire; `nobg` = the UI had no bg record to pin, so a latched `bgcue=` becomes `pending_bg`, W2) |
| `at k load_bg <n>` / `at k load_bg 0` | `LJ_CMD_LOAD_BG` (0 = clear) |
| `at k load_genmacro <n> [restart]` / `at k load_genmacro 0` | `LJ_CMD_LOAD_GENMACRO` |
| `at k current <seq> <scene> [blind]` / `at k current 0` | `LJ_CMD_CURRENT_SEQ`: pin sequence `seq` (in `cmd.seq[0]`, and `show->current_seq`) as the current sequence at 0-based `scene`; `0` = none |
| `at k go <runner>` / `at k stop <runner>` | `LJ_CMD_GO` / `LJ_CMD_STOP`; runner = `slot<1..12>`, `flash`, `current`, `bg<1..5>`, `all` |
| `at k trig <runner> [fwd\|rev\|bounce\|rnd\|default] [scenes=<n>]` | `LJ_CMD_TRIG` (a manual or audio trig; `default` = the slot's programmed direction) |
| `at k enable slot<k> 0\|1 [edit\|out]` | `LJ_CMD_ENABLE` |
| `at k time cue\|slot<k> <pct>` | `LJ_CMD_TIME` (0 = CUE) |
| `at k chase slot<k> 0\|1 [fade=0\|1]` | `LJ_CMD_CHASE` |
| `at k master gm\|sub<1..8> <0..255> [prop\|lotp\|htp\|none]` | `LJ_CMD_SET_MASTER`; a mode word sets `show->submaster_mode[k-1]` (the model field the engine reads, as the Master Intensity window does) and travels in `c`; without one `c` carries the current mode (the `submaster` line's, default `prop`) |
| `at k gm_fade out\|in <ms>` | `LJ_CMD_GM_FADE` |
| `at k blackout 0\|1` / `at k freeze 0\|1` / `at k offline 0\|1` | `LJ_CMD_BLACKOUT` / `LJ_CMD_FREEZE` / `LJ_CMD_OFFLINE` |
| `at k solo <fixture> 0\|1` | `LJ_CMD_SOLO` |
| `at k static p.s 0\|1` | `LJ_CMD_STATIC` with the pinned slot (`st`; NULL on release); `a` = `(p-1) * 20 + (s-1)` |
| `at k flash <seq> 0\|1` | `LJ_CMD_FLASH` |
| `at k overwrite l:a <v>` / `at k overwrite l:a clear` / `at k overwrite clear` | `LJ_CMD_OVERWRITE` |
| `at k htp <group> <0..255>` | `LJ_CMD_HTP_GROUP` |
| `at k seq_int slot<k> <0..255>` / `at k seq_int mode default\|htp\|lotp` | `LJ_CMD_SEQ_INT` |
| `at k macro_ctl <amp_pct> <speed_pct>` | `LJ_CMD_MACRO_CTL` |
| `at k disable_fade 0\|1` | `LJ_CMD_DISABLE_FADE` |
| `at k sacn_cfg` | `LJ_CMD_SACN_CFG` with a default config (`lj_sacn_cfg_default`); after tick k the harness checks `lj_engine_take_sacn_cfg` returns that pointer once and NULL the second time |

## Expectations

| Form | Meaning |
|---|---|
| `expect <tick> l:a=v [l:a=v ...] [layer=<rank>]` | after tick ran: `out[link-1][addr-1] == v` for each pair; with `layer=`, `layer_of[]` of every listed channel equals the rank (`LJ_L_*` numbers 0..7) |
| `expect_merged <tick> l:a=v [...]` | the same against `merged[]` (before post-processing; LORD_ENGINE-21) |
| `expect_pair <tick> l:a=<0..65535> [...]` | 16-bit: `out[msb] * 256 + out[msb+1]` (the LSB is the next address) equals the value; `pair16_pan_true16` asserts this on every tick |
| `expect_range <tick> l:a=<lo>..<hi>` | inclusive range (drift / jitter rows) |
| `expect_status <tick> slot<k> empty\|running\|stopped\|load_error\|disabled\|edit\|out [scene=<x>:<y>]` | `lj_engine_status.slot_status` / `slot_scene:slot_scenes` (x 1-based). The harness takes one `lj_engine_view` copy after every tick, as the app's idle pass does; every `expect_*` reads that copy |
| `expect_pending <tick> link_cue\|bg\|genmacro <n>` | the `pending_*` field equals n right after the tick (then cleared by the read) |
| `expect_view <tick> gm=<v> [blackout=0\|1 freeze=0\|1 offline=0\|1 chase_ms=<ms>]` | view fields |
| `run <tick>` | tick up to and including `<tick>` with no assertion (the drift scenarios) |
| `perf [ticks=100] max_tick_us=2000 [fixtures=100 slots=12 bg=5 statics=20 genmacro=2048]` | the performance guard: the scenario's own build lines are the load (the four load keys are accepted as documentation); runs `ticks` more ticks and fails if any `lj_engine_tick` took longer than `max_tick_us` (`lj_clock_us`; printed but not gated under ASan) |

Tolerances: none. Frames are integer and deterministic (no float anywhere in
`engine/`), so an `expect` is exact. A failed expect prints `FAIL <path>:<line> tick k
l:a expected v got w (out|merged, layer r)`, is counted, and the scenario continues; a
grammar or build error stops it. `lj4evr_tests --dump-frames <out.bin> <file.lj4r>...`
(CTest row `replay_dump`, writes `replay-frames.bin` in the build dir) appends every
scenario's frames after a 64-byte header (`LJ4R` + the file name, NUL-padded), each tick
as `merged[2048] + out[4][512] + tick_no` (8 LE bytes); the `determinism` CI job compares
the dumps of the three OSes byte for byte. `lj4evr_tests test_replay_grammar` is the
harness's own check: a bad line is rejected with its line number, an expect or `ch` on
an unpatched channel fails, the header is mandatory.

## Example (design 3.7, `fade_linear_40hz`)

```
LJ4 replay 1
tick_us=25000
patch 1 profile=19 link=1 addr=1
patch 2 profile=19 link=1 addr=2
seq 1 name="fade" scene st=10000 ft=2550 ch 1:1=255:F 1:2=0:F
cue 1 slot=1 seq=1
at 0 load_cue 1
expect 1 1:1=3 1:2=252
expect 51 1:1=128 1:2=127
expect 102 1:1=255 1:2=0
```

(`seq ... scene ... ch ...` on one line is allowed: a line may carry one `seq`, one
`scene` and one `ch` group in that order, as the design's example does; `cue n slot=k
seq=m` likewise starts a cue and fills slot k.)

## The twelve W1 scenarios (design 10 W1 acceptance 9)

`fade_linear_40hz`, `snap_050ms`, `fade_060ms`, `interrupted_fade_restart`,
`cue_priority_slot12`, `off_is_transparent`, `pair16_pan_true16`, `gm_examples_1_to_6`
(Example 3: programmed 100 % on a 0-255 inverted channel at GM 100/50/0 % -> DMX
0/128/255; Example 6: 100-200 inverted with the limit on -> 100/150/200; strobe values 50
and 230 unchanged), `blackout_after_masters`, `htp_group_max`, `freeze_pauses_clock`,
`drift_600s_two_slots`. Gate rows add `disabled_offset_silent` (LORD_FIXTURES-11),
the ME-25 (S3) SWITCH rows (fixture 17 of `gm_examples_1_to_6`, the "`switch_unscaled`"
evidence) and `expect_merged` in six scenarios (LORD_ENGINE-21). Bonus rows:
`stop_holds` (rig M8) and `seq_intensities` (the 03 s5.9 12-cell table). The numbers in
the files follow the notes (11 F3 `lj_fade_value` with nearest rounding, 11 G1-G5 with
LORD_ENGINE-18's one factor per mode, 11 s5.2 (d)); GM 50 % is level 127 of
`LJ_GM_DOMAIN` 255.

## W2 command lines (contracts seat, 2026-10-21; parsed in `lj_replay.c`, acted on by the W2 engine track)

Every W2 `LJ_CMD_*` kind has a form. Build lines are unchanged; `static` slots, `bg` cues and the
cue's slots are the pinned records the W2 forms point at, as the windows do.

| Form | Command |
|---|---|
| `at k bg_on bg<1..5>\|all 0\|1` | `LJ_CMD_BG_ON` (BGCue Toggle slot n / All Slots Off/On) |
| `at k static_act p.s [level=<0..100>] [scope=add\|slot\|page\|group\|all\|solo_slot\|solo_page\|solo_group\|solo_all]` | `LJ_CMD_STATIC_ACTIVATE` with `st` = the pinned slot; `level` / `scope` default to the slot's own (b 0xFFFF / c 0xFF); a `link=` slot carries its sequence in `seq[0]` (NULL when no `seq` line defines it: the link error), as the Statics 2 window posts it (ME-88; the W1 `static p.s 1` form does the same) |
| `at k static_deact p.s` / `at k static_deact all` / `at k static_deact page <p>` | `LJ_CMD_STATIC_DEACTIVATE` (release one, Release All, Release Page) |
| `at k smoke button 0\|1 [ms]` / `smoke timer 0\|1` / `smoke level\|duration\|cycle\|haze\|fan <v>` / `smoke reset` | `LJ_CMD_SMOKE` with `a` = `enum lj_smoke_ctl`; `[ms]` after `button` = the toggle-for-X-seconds preference |
| `at k flash_latch <seq> 0\|1` | `LJ_CMD_FLASH` with `c` = 1 (Latch Flash Sequences); `flash_latch 0 0` = Release latched Flash |
| `at k transparent use\|force\|ignore` | `LJ_CMD_TRANSPARENT` |
| `at k chase_tap slot<k>\|all [at_us=<us>]` | `LJ_CMD_CHASE_TAP`: one tap of the 1 ms chase timer |
| `at k cue_time cue\|slot<k> <0..999>` | `LJ_CMD_CUE_TIME` (== `time`): the 13 faders |
| `at k loop slot<k> [dir=] [first=] [last=] [loops=] [end=] [goto=] [link=] [spt=] [atpt=] [tol=]` | edits the loaded cue's slot record the way the Loop Options dialog does (validated), then `LJ_CMD_LOOP_OPTS` with `slot` pinned |
| `at k link` | the harness services `pending_link_cue` the way `lj_app_service_pending` does: `LJ_CMD_LOAD_CUE` of that number with its sequences pinned (fails when nothing is pending after tick k-1) |
| `at k htp_groups [masters=0\|1]` | `LJ_CMD_HTP_GROUPS` with `patch` = the show's table |
| `at k global_patch` | `LJ_CMD_GLOBAL_PATCH` |
| `at k default_out` | `LJ_CMD_DEFAULT_OUT` |
| `at k pref keep_seq_blind\|global_chase_rate\|disable_chase_fade\|statics_fadetime\|hotkey_trig_dir 0\|1` | `LJ_CMD_PREFS` |

`expect_view` gains the fields `bg_any statics_any flash_latched flash_seq transparent smoke haze
chase_tap_us cue bg snapshot_tick`.

## W2 replay-track lines (2026-10-22)

| Form | Meaning |
|---|---|
| `seed <n>` | build line: the `LJ_RNG_SEED` the scenario's random-direction expects were computed with (decimal or `0x`); a different engine seed fails the scenario on this line, so a changed seed never reads as a loop-rule regression |
| `at k ack_bg` | the harness services `pending_bg` the way `lj_app_service_pending` does: `LJ_CMD_LOAD_BG` of that number with `bgseq[]` pinned (fails when nothing is pending after tick k-1); the twin of `at k link` |
| `at k gpatch l:a key=value ...` | a runtime Global Patch edit: the `gpatch` build line applied to `show->patch` before tick k (the UI edits under `lj_lock`); posts nothing, so the `htp_groups` / `global_patch` line that follows hands it over; `lj_patch_validate` runs after it |
| `at k default l:a=v ...` | a runtime Default Output edit of `show->default_out` before tick k; posts nothing (`at k default_out` hands it over) |
| `at k scene_edit <seq> <scene> l:a=v:S ...` | fix pass r1 (ME-88): the pinned sequence's scene (0-based) edited in place before tick k, as the UI edits under `lj_lock`; posts nothing. A link static (and any runner on that sequence) shows the edit from tick k |
| `# NOTE <rule ids> ...` | a comment by convention: every W2 scenario names the note / rule / rig ids each block of expects asserts, and the first W1-engine failure it records |

A runtime edit's grammar is checked before the clock runs to k-1, so a bad edit runs no tick (`at_us=` on an edit is an
error). The `loop` form edits the cue the last `load_cue` / `link` loaded (W1 pointed it at the build-time cue only).
`lj4evr_tests test_replay_w2` is the check: each new form malformed is rejected with its line number, a bad runtime
edit leaves `ticks == 0`, and a good edit shows on tick k and not on k-1.

## The nine W2 scenarios (design 10 W2 acceptance 1) and the wire run

`transparent_cue_load`, `statics_ltp_reorder`, `layer_flash_current_statics`,
`bgcue_over_statics`, `chase_030ms`, `cue_time_scaling`, `loop_bounce_random_limits`,
`link_to_cue` and `seq_intensities` (the 03 s5.9 table, W1 bonus row, extended) carry their
event lines since 2026-10-22 (replay track). Each file's `# NOTE` lines cite the rule ids per
block. Run against the W1 engine (HEAD `6550a77`, the replay harness of this pass), the first
failing line of each:

| Scenario | W1 result | First failing line (W1) |
|---|---|---|
| `seq_intensities` | pass (10 expects) | - (not a new rule; locks the switch to `lj_seq_intensity_rule`) |
| `transparent_cue_load` | 6 fails / 21 | `:46 tick 3 1:1 expected 0 got 200` (Ignore) |
| `statics_ltp_reorder` | 18 fails / 18 | `:27 tick 0 1:1 expected 100 got 0` (STATIC_ACTIVATE) |
| `layer_flash_current_statics` | 8 fails / 16 | `:33 tick 3 1:1 expected 200 got 100` (STATIC_ACTIVATE); `:46` latched flash |
| `bgcue_over_statics` | 10 fails / 19 | `:34 tick 0 1:1 expected 200 got 0`; `:39 tick 2 1:1 expected 20 got 250` (BG_ON) |
| `chase_030ms` | 21 fails / 35 | `:34 tick 6 view chase_tap_us expected 30000 got 0`; `:35 tick 7 1:1 expected 255 got 0` |
| `cue_time_scaling` | pass (30 expects) | - (W1 scaling code; acceptance evidence) |
| `loop_bounce_random_limits` | 19 fails / 48 | `:48 tick 20 1:1 expected 40 got 60` (LOOP_OPTS); `:76 tick 108` W1 Goto shows the old scene; `:83 tick 132` random without LJ_RANDOM_NO_REPEAT |
| `link_to_cue` | pass (20 expects) | - (W1 link path; acceptance evidence) |

`wire/drift_600s_two_slots_wire.lj4r` (4.00 s and 0.333 s scene times, zero cumulative drift at
600 s: boundaries on ticks 24000, 23990, 24003) sits outside the `replay_all` / `replay_dump` glob;
CTest runs it headless as `replay_wire_drift`, and the acceptance integrator repeats it beside a
live sACN capture (acceptance 3). It passes on the W1 engine (11 E10 is W1 code).

## Fix pass r1 scenarios (W2 code council, 2026-10-22)

| Scenario | ticks | expects | What it pins |
|---|---|---|---|
| `statics_link` | 7 | 8 | ME-88: a link slot outputs its sequence's scene, follows an in-place scene edit, a link to a missing sequence outputs nothing, the level scales it |
| `pair16_static_bg` | 82 | 10 | ME-22 (LORD_ENGINE-12's probe): a 16-bit pair faded through a static at level 50 % with Use Fadetime and through a BG cue slot stays one value (tick 3: 1229, not a per-byte 1101 / 1357) |
| `blind_keep_contents` | 9 | 9 | LORD_ENGINE-13, 11 F21: `[PREFS] KeepSeqBlind` both ways (`pref keep_seq_blind 1` keeps the output while blind, `0` takes the current sequence off it) |

## W3 lines (contracts seat, 2026-10-22; parsed in `lj_replay.c` by the track that owns the rule)

The engine never sees these forms. The UI-side ones (cue list, Fan Out, Auto Delay, follow spot, Generic DMX Control,
Sequence Fades, `fx_edit`) do in the harness exactly what the window does on the UI thread: edit the pinned current
scene under the lock, then post `LJ_CMD_CURRENT_SEQ`, so their frames are the windows' frames.

Build lines:

| Line | Meaning |
|---|---|
| `patch n ... gs2=<file>` | now accepted: `lj_gs2_load` of `<file>` relative to the scenario's folder (a synthetic record a test wrote, never Martin's), bound with `lj_engine_bind_profile` (gs2 track) |
| `cl n [name="<name>"] [tracking=0\|1]` then `clline adv=auto\|manual cmd=<word> [param=<n>] [pstr="<s>"] [ref=<n>] [cue=<n>] [trig=<mask>] [comment="<s>"]` | a cue list in the `.Cl2` `line` grammar (the line number is the order); `cmd` words are `lj_cl_cmd_words`; a CD / DA / MP word gets `load_error = 1` as `lj_cuelist_load` sets it; validated with `lj_cuelist_validate` (cuelist-core) |
| `bez n name="<name>"` then `node x= y= ix= iy= ox= oy= [line=1]` lines | Bezier shape n, sampled with `lj_bez_sample` into `show->bez` slot n-1 = macro type `LJ_MM_TYPE_BEZ0 + n - 1` (movement) |
| `follow n [name="<name>"] sel=<f,f,...> [static=<p.s> autoclr=0\|1]` then `ch` lines | follow spot setting n into `show->followspot[n-1]` (gdc-fades-followspot) |
| `gdcgroup g l:a [l:a ...]` | Generic DMX Control group g membership (`show->gdc`) |
| `gm_shapes <file>` | `lj_gm_shapes_load` of a synthetic override (ME-34); `at k gm_shapes loaded` posts it |

Command lines (`at <tick> ...`):

| Form | Command / harness action |
|---|---|
| `at k load_genmacro <n> [restart] [editor]` | W2 form; `editor` = `c = LJ_GM_SRC_EDITOR` |
| `at k genmacro_on engine\|slot 0\|1` | `LJ_CMD_GENMACRO_ON`, `a` = `LJ_GM_SRC_ENGINE` / `LJ_GM_SRC_CUE` |
| `at k genmacro_restart [editor]` | `LJ_CMD_GENMACRO_RESTART` |
| `at k load_cue <n> ... nogm` | genmacro track: the UI had no macro record to pin, so a latched `genmacro=` becomes `pending_genmacro` (the cue's old macro stops); twin of `nobg` |
| `at k ack_genmacro` | services `pending_genmacro` as `lj_app_service_pending` does: `LJ_CMD_LOAD_GENMACRO` with the record pinned (fails when nothing is pending after tick k-1); twin of `ack_bg` |
| `at k gm_shapes builtin\|loaded` | `LJ_CMD_GM_SHAPES` with NULL or the `gm_shapes` build table |
| `at k macro_fade out\|in\|off\|on\|freeze\|run [ms]` | `LJ_CMD_MACRO_FADE` (`enum lj_mm_ctl`, b = ms) |
| `at k fx_edit <seq> <scene> <fixture> macro=<...> [mst=<...>]` | runtime edit of a movement macro in the pinned sequence (the `fx` build syntax); posts nothing (the Movement Macro window's edit) |
| `at k fanout <dpan> <dtilt> home=0\|1 order=<f,f,...>` | `lj_fan_out` on the current scene's pan / tilt (readout) of the listed fixtures, then `CURRENT_SEQ` |
| `at k spread pan\|tilt order=<f,f,...>` | `lj_fan_spread`, then `CURRENT_SEQ` |
| `at k autodelay linear\|circular\|segments\|random <max> [count=<n>] [segments=<n>] order=<f,f,...>` | `lj_mm_auto_delay` into the current scene's macro delay bytes (random uses the scenario `seed`), then `CURRENT_SEQ` |
| `at k followspot <n>` / `at k followspot 0` | `lj_followspot_apply` into the current scene (all Snap), then `CURRENT_SEQ`; 0 = Clear |
| `at k gdc "<command line>"` | `lj_gdc_parse` + `lj_gdc_run` on the current scene with the harness's one `lj_gdc_state`, then `CURRENT_SEQ`; a parse error fails the scenario naming the line |
| `at k seqfades <attr,attr,...> <sfx> [all]` | `lj_seqfades_apply` on the selected fixtures (`sel=` of the last `fanout` / `follow`, else all), then `CURRENT_SEQ`; attrs = `intensity movement color gobo beam effects level movemacro`, sfx = `all_off all_snap all_fade off_snap off_fade snap_off snap_fade fade_off fade_snap` |
| `at k cl_run <n> top\|line=<l>` | pins cue list n and runs `lj_cl_run_op(TOP / RUN)` |
| `at k cl_go` / `cl_back` / `cl_stop [clear]` / `cl_jump <l>` / `cl_mark goto\|run <m>` / `cl_run_to <l>` | the other `enum lj_cl_op` |

Cue list stepping: between tick k-1 and tick k the harness calls `lj_cl_run_step` at every `LJ_CL_STEP_MS` (10 ms) of
synthetic time t in `((k-1) * tick_us, k * tick_us]`, now_us = t, before tick k runs: the 10 ms UI wake of design 2.3
rule 4. Its `lj_cl_host`: `load_cue` = the `load_cue` form's pinned load, `post` = `lj_engine_cmd`, `clock` = synthetic
midnight + t (day 0), `timecode` = PC elapsed only (MIDI / CD / DA / MP = -1), `exec` refuses, `fn` records the call.

Expectations:

| Form | Meaning |
|---|---|
| `expect_cl <tick> state=idle\|running\|wait_go [next=<l>] [last=<l>] [fn=<n>]` | the runner's `lj_cl_status` after that tick's steps; `fn` = Hotkey Command calls the harness host recorded so far (cuelist-core). An `at k cl_*` line first runs the 10 ms steps up to k * tick_us, then the click at exactly k * tick_us |
| `expect_view <tick> ...` | gains `gm_engine gm_slot gm_cue gm_editor mm_amp mm_speed mm_fade mm_frozen` |
| `expect_mm <tick> <fixture> pan=<idx> tilt=<idx>` | `mm_idx_pan` / `mm_idx_tilt` of the view (ME-35: two runners holding one fixture's macro advance it one step per tick) |

## The five W3 scenarios (design 10 W3 acceptance 1, 3, 4)

`cuelist_run` (cuelist-core), `genmacro_sine_amp186` and `genmacro_vs_bgcue` (genmacro), `movemacro_six_shapes` and
`fanout_auto_delay` (movement). Committed by the contracts seat with their header comment and W2-grammar build lines
only (0 expects, so `replay_all` stays green); each owning track adds the W3 lines and the expects with the rule ids in
`# NOTE` lines, as W2 did.

