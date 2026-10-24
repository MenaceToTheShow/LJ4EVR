# W7: the fixture library (pipeline record)

Wave 7 toward 1.3.0, 2026-10-24. Joey asked for "the fixture library ... for all Martin, Elation, Chauvet (pro and
DJ), Clay Paky, GLP, Acme, Ayrton, and etc fixtures"; the orchestrator read "etc" as ETC plus the other major pro
brands: Martin, Elation, Chauvet DJ, Chauvet Professional, Clay Paky, GLP, Acme, Ayrton, ETC, Robe, High End
Systems, Vari-Lite, ADJ, SGM. This file records the pipeline track: what landed, the numbers, the deviations and
the budgets. The rules live in `docs/fixtures.md` ("W7"); the catalogs (`docs/fixtures/catalog/*.tsv`) and the
authored files are other tracks.

## 0. Joey's bug: "the mac500 icon is outside of the cell"

Root cause confirmed: every `art/icons` bitmap is 50 px tall, the Fixed Size tile is 63x53 with a 15-px caption
(`LJ_FIXICON_*`, `unselectedfixture.gif`), and `lj_w_fixicon.c` blitted the bitmap 1:1 centred in the 36-px body:
y = 15 + (36 - 50) / 2 = 8, so it covered rows 8-14 of the caption and ran 5 px past the tile's bottom, for every
fixture. Fix, in the shared paint path: `lj_blit_rgb_fit` (gfx, integer nearest-neighbour, aspect kept, never
enlarged, nothing outside the box) draws the bitmap into the 60x36 body (x 1..60, y 15..50). Image Size tiles grow
to the bitmap, so they stay 1:1 through the same call.

Measured against `unselectedfixture.gif` (63x53, 1:1): LJ2's MAC 500 ink spans x 21..42, y 23..47 (22x25); our MAC
500 silhouette shrunk to 36x36 spans x 21..41, y 21..50 (21x30). The width matches within 1 px; our own drawing is
5 px taller (D2: the art is ours).

Checks: `draw_goldens` (a 50x50 source into 60x36: 36x36 at the centre, 0 pixels outside, the nearest-neighbour
samples, the transparent key; a 20x10 source copied 1:1; a wide source); `widgets_lj` (a 50x50 bitmap in a fixed
tile: no pixel over the caption, none outside the 63x53 rect, all 1,296 inside the body); the `MAINWINDOW` golden
re-rendered (4,184 px, all inside the six tiles; REVIEWED row). Commit `057e6db`.

## 1-2. Imports

| Source | Commit | Files | Licence files |
|---|---|---|---|
| QLC+ `resources/fixtures/<dir>` | `6f7f290da8b04a360339667234cae5fb9207bfdc` (2026-10-23) | 614, flat in `third_party/qlcplus-fixtures/` | `LICENSE` (Apache-2.0), `NOTICE`, `SOURCE` |
| Open Fixture Library `fixtures/<dir>` | `78816e4f2803ec0ccf907fc634269b91df34e7f7` (2026-10-23) | 171 + `manufacturers.json` in `third_party/ofl-fixtures/<dir>/` | `LICENSE` (MIT), `NOTICE`, `SOURCE` |

The 73 Martin files of W1 (QLC+ `b7a65b8`) are byte-identical at `6f7f290` (`resources/fixtures` did not change
between the two commits), so every existing `qxf_map.tsv` line keeps its file, mode and id. `NOTICE` and
`THIRD_PARTY_NOTICES.txt` (CMake appends the OFL MIT text) credit both; `docs/provenance.md` has the rows.

Per group, after `tools/qxf_map_sync.py` (modes <= 32 channels get an id; precedence authored > OFL > QLC+, dedup
per model and channel count):

| Group | QLC+ files | QLC+ modes | QLC+ new ids | OFL files | OFL modes | OFL new ids | > 32 ch (not shipped) | dedup skipped |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Martin | 73 | 185 | 112 | 24 | 71 | 59 | 14 | 26 |
| Elation | 23 | 91 | 76 | 20 | 81 | 73 | 13 | 10 |
| Chauvet DJ | 162 | 354 | 308 | 29 | 74 | 72 | 12 | 36 |
| Chauvet Professional | 25 | 168 | 150 | 10 | 48 | 43 | 15 | 8 |
| Clay Paky | 46 | 128 | 82 | 6 | 17 | 7 | 52 | 4 |
| GLP | 24 | 60 | 40 | 8 | 36 | 20 | 29 | 7 |
| Acme | 7 | 36 | 18 | 0 | 0 | 0 | 18 | 0 |
| Ayrton | 9 | 27 | 17 | 3 | 9 | 2 | 17 | 0 |
| ETC | 7 | 41 | 28 | 16 | 81 | 81 | 0 | 13 |
| Robe | 47 | 172 | 118 | 11 | 44 | 31 | 60 | 7 |
| High End Systems | 6 | 7 | 5 | 0 | 0 | 0 | 2 | 0 |
| Vari-Lite | 7 | 14 | 12 | 0 | 0 | 0 | 2 | 0 |
| American DJ | 162 | 556 | 457 | 43 | 173 | 169 | 19 | 84 |
| SGM | 16 | 42 | 28 | 1 | 8 | 8 | 6 | 8 |
| **Total** | **614** | **1,881** | **1,451** | **171** | **642** | **565** | **259** | **203** |

`tools/qxf_map.tsv`: 2,061 lines (the 42 exe-bound and 3 extra W1 lines untouched, then 2,016 new extras, ids
1003-3018). 2,019 extra rows show in Fixture Configuration. QLC+'s "Chauvet" directory splits into 162 Chauvet DJ
and 25 Chauvet Professional files (24 by product line, one file already says "Chauvet Professional"). OFL has no
Acme, High End Systems or Vari-Lite directory at the pinned commit. Every OFL file converts; no mode was skipped
for an unresolved channel. 696 colour / gobo / effect wheels carry more than 20 palette candidates
(`tools/qxf2c_overflow.txt`, first 20 kept, ME-26).

`tools/ofl2qxf.py` (stdlib, deterministic, `--selftest`) converts OFL JSON to the `.qxf` subset `qxf2c.py` reads;
`fixtures/authored/README.md` states the authoring rules (manual citation in `<Creator>`, MIT, no manual prose,
blank beats guessed).

## 3. Dedup and precedence

`tools/qxf_map_sync.py` appends a line for every unmapped source mode of the 14 groups with at most 32 channels:
hand > authored > OFL > QLC+ per (group, model, channel count), new ids from the next free one in sorted order of
(manufacturer, model, mode, file), existing lines never touched; `--check` runs in `generate-check` and as the
`qxf_map_sync_check` CTest row. Manufacturer rule (`docs/fixtures.md`): the name the products carry, as OFL's
`manufacturers.json` spells it; ADJ stays "American DJ" (every source file says so); QLC+'s Chauvet split by line.

## 4. Scale

`qxf2c.py` emits one byte string per mode (`lj_profiles_gen_0.c` .. `_5.c`, 400 modes each) and one 56-byte
descriptor (`lj_profiles_gen.c`); `lj_fixlib_by_id()` builds the 13,432-byte `lj_profile` on the heap at first use
(UI thread) and `lj_fixlib_release()` frees them at exit. `--check` covers every part (and a stale one). The 45
W1-W6 tier-2 profiles decode byte-identical to the old static tables (struct bytes without pointers, and the
notes text, compared before and after on every one).

| Budget | Limit | Measured |
|---|---|---|
| exe growth | <= 8 MB for 5,000 modes | Release MSVC Ninja, SDL on: 12,012,544 -> 12,247,552 bytes (+235,008) for 2,016 more modes; the 45 old static profiles (604,440 bytes) are gone, the new data is 511,158 encoded bytes (248 per mode) + 56-byte descriptors + strings, about 450 bytes per mode in all: 5,000 modes about 2.3 MB |
| cold start | unchanged within 50 ms | `lj4evr --ms 1`, dummy video and audio, fresh scratch userdata, 7 runs each: before median 59 ms (min 57), after median 64 ms (min 58) |
| Fixture Configuration tree | opens < 200 ms with everything visible | `test_extras_tree` (Release): open, expand all 2,760 items (2,220 leaves), paint: 1.5 ms |
| generated source | one file small enough for MSVC | 2.85 MB of C in 7 files, the largest 616 KB |

## 5. Visibility (recorded deviation)

`[FIXTURES] ShowExtraProfiles` defaults to 1 (the orchestrator's ruling; `docs/fidelity.md` row "Fixture
Configuration, Available Profiles tree (W7)"); 0 gives the LJ2-exact 201-row tree. Sorted by manufacturer, name,
then mode for extras (exe modes keep the exe order). The tree widget's node bitset grew from 2,048 to 16,384 nodes
and `row_of_item` became one walk (it was a walk per row, quadratic); `LJ_FC_TREE_MAX` is 16,384;
`lj_fixlib_row_by_id` (bisect) replaces two linear row scans (Fixture Configuration, Find profile, which now stops
at its 256-row list limit). `test_extras_tree`: every group folder present, leaf order, End / Home scroll, Add
Fixture of a Chauvet DJ extra and Fixture Info naming it, an icon stem for every extra, the timing above; golden
`W7_FIXTURE_CONFIG_EXTRAS` (REVIEWED). `W4C_FIXTURE_CONFIG` and the W4 walkthrough keep the LJ2 tree
(`ShowExtraProfiles=0` in the test and in `w4_entry_prep.sh`, whose key path counts rows of that tree).

## 6. Coverage report

`tools/fixture_coverage.py` reads `docs/fixtures/catalog/<group>.tsv` (any spelling of the 14 groups: `adj`,
`hes`, `chauvet-pro` ...), the map and the sources, and writes `docs/fixtures/coverage.md`: per group the catalog
models, shipped models and modes (hand rows included), catalog models shipped (by name, or by a `qlc:` / `ofl:` /
`authored:` file in the note), modes wider than 32 channels with their channel count, and the "no manual" list.
Missing catalogs are fine. `--check` joins `generate-check` once the catalogs are committed (they were being
written by the catalog track during this pass, so the report is regenerated after them).

## 7. Authoring lint

`python -I tools/qxf2c.py --lint F.qxf ...` (no file named: every `fixtures/authored/*/*.qxf`, the
`qxf2c_lint_authored` CTest row): one line per problem and exit 1; the checks are listed in `docs/fixtures.md`.
`qxf2c_selftest` plants six problem kinds in a synthetic file and expects each.

## Checks run (2026-10-24, this PC, every app run on SDL's dummy video driver)

| Check | Result |
|---|---|
| Release MSVC, SDL on (`build-w6i-rel`) | 0 warnings; `ctest` 163 / 163 |
| Release MSVC, SDL off (`build-w6i-nosdl`) | 0 warnings; `ctest` 161 / 161 |
| Debug MSVC, SDL on (`build-w6i-dbg`) | 0 warnings; `ctest` 163 / 163 |
| `test_layering`, `fidelity_check`, `help_check`, `replay_all`, `ui_golden`, `caption_diff_selftest` | pass (inside the ctest runs) |
| generate-check tools | `qxf2c`, `fontpack`, `artpack`, `gen_vectors`, `exe_profiles_check`, `qxf_map_sync` `--check`: current |
| determinism | `lj4evr_tests dump-frames` 400 ticks sha256 `5b13f419...4b50` and `replay-frames.bin` `3474ee73...5f34`, identical before and after (engine untouched) |
| `w4_entry_points.lj4s` headless | 156 PASS lines, 0 SCRIPT FAIL, 0 "no binding" (156 before) |

## Deviations and ceilings

- `ShowExtraProfiles` defaults to 1 (section 5).
- `tools/qxf_map.tsv` is read as UTF-8 (mode names from the sources carry non-ASCII); `exe_profiles_check.py` too.
- Dedup keys on the channel count (`docs/fixtures.md`, the ceiling sentence).
- The W1-W6 tier-2 notes text is unchanged; OFL and authored profiles say "Converted from Open Fixture Library
  <dir>/<file> (<mode>)" / "Converted from LJ4EVR fixtures/authored/...".

## 8. W8 pre-tag fixes (design council requests R1 and R2)

The W8 design council (Master Elder verdict, section 4) asked W7 to land two packages before `v1.3.0` is tagged, so
v1.3.0 ids ship with their final binding. Both are taken. No `qxf_map.tsv` line changed: every id keeps its file,
mode and number.

### R1, the converter must-fix list (`tools/qxf2c.py`)

| Rule | Verdict item | What changed |
|---|---|---|
| Position | W8D-6 | Pan / tilt bind only from `PositionPan` / `PositionTilt` (and their `Fine`), or Group `Pan` / `Tilt` without a preset and with no rotation capability (a `Rotation*` preset or "rotat" in the text). The first coarse pan and tilt win; each takes the first fine after it. v1.3.0-rc bound the last one it saw. |
| Multi-head pan/tilt | W8D-2, Q6 default, fidelity row W8-8 | Head 1 (the first positional pair) drives Movement. Every other positional pan / tilt is a visible Levels fader captioned `Pan2`, `Tilt3` ... when its axis repeats, its fine channel paired to it (`pair_lsb`). A lone pan or tilt binds nothing and stays visible. |
| Intensity and Grand Master | W8D-6 | Intensity binds to an `IntensityMasterDimmer` that is in no `<Head>`, else to the first dimmer. Every other `IntensityDimmer` / `IntensityMasterDimmer` gets `ext_gm`, so the Grand Master and blackout reach every head [RN L.459]. An untyped Intensity-group channel (a fan speed, a QLC+ 3 colour) never gets `ext_gm`. |
| RGB or CMY | W8D-5 (e) | `cmy[]` takes the first kind in channel order; channels of the other kind stay visible Levels faders. |
| First triple | W8D-5 (f) | The first R, G and B (or C, M, Y) bind fixture-wide; every later one stays a visible, captioned Levels fader. Cells are W8. |
| Id 170 | W8D-3 | MAC 401 Dual `RGB-1` keeps its v1.3.0-rc colour binding (the hand tier owns it). |

The byte encoding gained one bit: the per-channel flags byte is `1 invisible | 2 disabled | 4 ext_gm`
(`lj_qxf_build` in `src/fixlib/lj_profile.c`). `qxf2c.py` now reads QLC+ `<Head>` (for the master-dimmer rule only).
`tools/ofl2qxf.py` is unchanged: it already applied the positional rule, and the OFL `<Head>` output (W8D-4) is W8
Track E.

**Counts.** Decoding all 2,061 mapped modes before and after (every per-offset field, every dyn record, the flags and
the static functions), 368 ids changed: 367 are W7 extras (ids >= 1003, never released) and one is an exe row.

| Change | Modes |
|---|---:|
| `cmy[]` re-bound to the first triple (or the later triples made visible) | 222 |
| `ext_gm` set on a head dimmer | 97 |
| Movement on different channels | 54 |
| of those, Movement dropped because the partner axis is a rotation motor (ids 1611, 1842, 1843, 2662-2665, 2785, 2786) | 9 |
| intensity re-bound (to a master dimmer, or off a non-dimmer such as "Sensitivity Control") | 15 |
| multi-head modes under the council's definition (two heads with a positional pan or tilt, or two positional coarse pans) | 26 in 14 files |
| modes with a repeated positional axis (the 26, plus 12 with repeated tilts or OFL sources) | 38 in 17 files |

The 26 are ids 1177-1180, 1182, 1183, 1185, 1186, 1188, 1289, 1290, 1419, 1420, 1422, 1423, 1468, 1645, 1646, 1886, 1888,
1889, 1904, 1948, 2322, 2325 and 2326.

**The one released id that changes:** 171, MAC 401 Dual RGB `RGBX All` (exe row, in v1.2.0). Its two
`Dynamic Effect n Intensity` channels (ch 6, ch 10; `IntensityDimmer` in QLC+) now carry `ext_gm`, so the Grand Master
reaches the effect LEDs. Nothing else in it moved. The CHANGELOG names it. No replay, golden or determinism frame uses
it.

### Decoded profile, before and after, for the ids the verdict names

`ch` is 1-based; "invisible" is `level_invisible`. Every line not listed is identical.

```
id 1060: American-DJ-Asteroid-1200.qxf | Standard
  ch5 Pan Continuous R: class PAN -> LEVEL; invisible 1 -> 0
  ch6 Tilt Continuous: class TILT -> LEVEL; invisible 1 -> 0
  - flags pan_ofs=ch5 patch=(4, 1, 5, 3) pt16=1 rgb_on=1 strobe_off=32 tilt_ofs=ch6
  + flags pan_ofs=ch1 pt16=1 rgb_on=1 strobe_off=32 tilt_ofs=ch3
id 1468: American-DJ-Quad-Scan-LED.qxf | 28 Channels mode
  ch8 Mirror 2 Pan: caption Mirror 2 Pan -> Pan2; class PAN -> LEVEL; invisible 1 -> 0
  ch9 Mirror 2 Tilt: caption Mirror 2 Tilt -> Tilt2; class TILT -> LEVEL; invisible 1 -> 0
  ch10 Red (Mirror 2): invisible 1 -> 0
  ch11 Green (Mirror 2): invisible 1 -> 0
  ch12 Blue (Mirror 2): invisible 1 -> 0
  ch13 Master Dimmer (M: ext_gm 0 -> 1
  ch15 Mirror 3 Pan: caption Mirror 3 Pan -> Pan3; class PAN -> LEVEL; invisible 1 -> 0
  ch16 Mirror 3 Tilt: caption Mirror 3 Tilt -> Tilt3; class TILT -> LEVEL; invisible 1 -> 0
  ch17 Red (Mirror 3): invisible 1 -> 0
  ch18 Green (Mirror 3): invisible 1 -> 0
  ch19 Blue (Mirror 3): invisible 1 -> 0
  ch20 Master Dimmer (M: ext_gm 0 -> 1
  ch22 Mirror 4 Pan: caption Mirror 4 Pan -> Pan4; class PAN -> LEVEL; invisible 1 -> 0
  ch23 Mirror 4 Tilt: caption Mirror 4 Tilt -> Tilt4; class TILT -> LEVEL; invisible 1 -> 0
  ch24 Red (Mirror 4): invisible 1 -> 0
  ch25 Green (Mirror 4): invisible 1 -> 0
  ch26 Blue (Mirror 4): invisible 1 -> 0
  ch27 Master Dimmer (M: ext_gm 0 -> 1
  - dyn cmy[0]     ch24 Red (Mirror 4) 0..255
  + dyn cmy[0]     ch3 Red (Mirror 1) 0..255
  - dyn cmy[1]     ch25 Green (Mirror 4) 0..255
  + dyn cmy[1]     ch4 Green (Mirror 1) 0..255
  - dyn cmy[2]     ch26 Blue (Mirror 4) 0..255
  + dyn cmy[2]     ch5 Blue (Mirror 1) 0..255
  - flags pan_ofs=ch22 pt8=1 rgb_on=1 strobe_off=0 tilt_ofs=ch23
  + flags pan_ofs=ch1 pt8=1 rgb_on=1 strobe_off=0 tilt_ofs=ch2
id 1656: Ayrton-MagicBlade-R.qxf | St (20ch)
  ch6 Pan motor contin: class PAN -> LEVEL; invisible 1 -> 0
  ch7 Tilt motor conti: class TILT -> LEVEL; invisible 1 -> 0
  - flags pan_ofs=ch6 patch=(5, 1, 6, 3) pt16=1 rgb_on=1 strobe_off=96 tilt_ofs=ch7
  + flags pan_ofs=ch1 pt16=1 rgb_on=1 strobe_off=96 tilt_ofs=ch3
id 1695: Chauvet-ColorBand-Pix-M.qxf | 20 Channel
  ch4 Red 2: invisible 1 -> 0
  ch5 Green 2: invisible 1 -> 0
  ch6 Blue 2: invisible 1 -> 0
  ch7 Red 3: invisible 1 -> 0
  ch8 Green 3: invisible 1 -> 0
  ch9 Blue 3: invisible 1 -> 0
  ch10 Red 4: invisible 1 -> 0
  ch11 Green 4: invisible 1 -> 0
  ch12 Blue 4: invisible 1 -> 0
  ch13 Red 5: invisible 1 -> 0
  ch14 Green 5: invisible 1 -> 0
  ch15 Blue 5: invisible 1 -> 0
  ch16 Red 6: invisible 1 -> 0
  ch17 Green 6: invisible 1 -> 0
  ch18 Blue 6: invisible 1 -> 0
  ch19 Tilt: class TILT -> LEVEL; invisible 1 -> 0
  - dyn cmy[0]     ch16 Red 6 0..255
  + dyn cmy[0]     ch1 Red 0..255
  - dyn cmy[1]     ch17 Green 6 0..255
  + dyn cmy[1]     ch2 Green 0..255
  - dyn cmy[2]     ch18 Blue 6 0..255
  + dyn cmy[2]     ch3 Blue 0..255
id 2950: Robe-Robin-Tetra-1.qxf | 1-Wash
  ch1 Tilt (8bit): class TILT -> LEVEL; invisible 1 -> 0; pair_lsb 0 -> 2
  ch2 Tilt fine (16bit: class TILT -> LEVEL
  ch14 Background - Dim: class INTENSITY -> LEVEL; invisible 1 -> 0; ext_gm 0 -> 1
  ch17 Flower Effect -: invisible 1 -> 0
  ch18 Flower Effect -: invisible 1 -> 0
  ch19 Flower Effect -: invisible 1 -> 0
  ch22 Flower Effect -: ext_gm 0 -> 1
  ch25 Master Dimmer in: class LEVEL -> INTENSITY; invisible 0 -> 1
  - dyn intensity  ch14 Background - Dimmer intensity (8bit) 0..255
  + dyn intensity  ch25 Master Dimmer intensity (8bit) 0..255
  - dyn cmy[0]     ch17 Flower Effect - Red (8bit) 0..255
  + dyn cmy[0]     ch8 Background - Red (8bit) 0..255
  - dyn cmy[1]     ch18 Flower Effect - Green (8bit) 0..255
  + dyn cmy[1]     ch9 Background - Green (8bit) 0..255
  - dyn cmy[2]     ch19 Flower Effect - Blue (8bit) 0..255
  + dyn cmy[2]     ch10 Background - Blue (8bit) 0..255
id 2957: Robe-Spiider.qxf | (2) Basic [RGBW]
  ch18 Flower effect -: invisible 1 -> 0
  ch19 Flower effect -: invisible 1 -> 0
  ch20 Flower effect -: invisible 1 -> 0
  ch24 Flower effect di: class INTENSITY -> LEVEL; invisible 1 -> 0; ext_gm 0 -> 1
  ch27 Master dimmer: class LEVEL -> INTENSITY; invisible 0 -> 1
  - dyn intensity  ch24 Flower effect dimmer 0..255
  + dyn intensity  ch27 Master dimmer 0..255
  - dyn cmy[0]     ch18 Flower effect - Red 0..255
  + dyn cmy[0]     ch8 Red (all pixels) 0..255
  - dyn cmy[1]     ch19 Flower effect - Green 0..255
  + dyn cmy[1]     ch9 Green (all pixels) 0..255
  - dyn cmy[2]     ch20 Flower effect - Blue 0..255
  + dyn cmy[2]     ch10 Blue (all pixels) 0..255
```

### R2, the loader-tolerance package (G1, W8D-10, W8D-11, and the G2 write side)

| Part | Where | Regression test (fails on `d1081c4`, passes now) |
|---|---|---|
| G1 (1): a bad `chcap` line (offset out of range, text longer than 6, before any `fx`) is logged and skipped; a `chgm` list keeps its in-range entries; a bad list is skipped. Before, either emptied the whole patch | `lj_config_load` (`src/model/lj_show.c`) | `test_model_records` `lj_test_config_g1`; `test_profile_editor` (its four reject cases now assert the skip) |
| G1 (2): a built-in id this build does not have is kept as an unpatched row, logged as an F41-style warning, and saved back byte-identical | `config_check` (`src/model/lj_show.c`) | `test_model_records` `lj_test_config_g1` (id 9999 between a MAC 500 and a dimmer: loads, both bound, saves byte-identical) |
| W8D-10: `lj_engine_rebuild` assigns `profile_of[f]` for every built-in, NULL included, before the address check; output no longer depends on what the row held before | `src/engine/lj_engine.c` | `test_engine_w2` `lj_t_stale_bind` (a MAC 500 with Invert Pan, rebound to id 9999: no `fixture_of[]` entry names the row, `out[] == merged[]`); 19 failures before the fix |
| W8D-11: TConfigWarnForm lists the row as `Fixture n profile id: Not available in LJ4EVR`; Save Anyway writes it | `lj_config_dlg_warnings` (`src/windows/lj_config_dialogs.c`) | `test_config_dialogs` |
| G2 write side (W8D-14): `lj_rec_open_write` refuses a target whose header names the same kind at a higher format ("made by a newer LJ4EVR; not overwritten"), for every record kind | `src/core/lj_textrec.c` | `test_model_records`: after `LJ4 config 2` is on disk, `lj_config_save` and `lj_show_save_config` return -1 and the file is byte-identical |

The engine tests that hand `lj_engine_bind_profile` a test profile now mark that fixture `LJ_FX_GS2`, as the replay
harness already did: a built-in fixture's profile always comes from fixlib. Not done here: the G2 read-side gate
(`lj_rec_open_read` refusing a newer format), which stays with W8 Track B.

### Checks run (2026-10-24, this PC, every app and test run on SDL's dummy video driver, scratch userdata)

| Check | Result |
|---|---|
| Release MSVC, SDL on (`build-w6i-rel`) | 0 warnings; `ctest` 163 / 163 |
| Release MSVC, SDL off (`build-w6i-nosdl`) | 0 warnings; `ctest` 161 / 161 |
| Debug MSVC, SDL on (`build-w6i-dbg`) | 0 warnings; `ctest` 163 / 163 |
| `qxf2c --selftest` / `--check`, `qxf_map_sync --check` | ok; 0 findings, 2,061 map lines; 0 unmapped modes |
| `test_layering`, `fidelity_check`, `replay_all` | pass |
| determinism | `dump-frames` (400 ticks) sha256 `5b13f41930362b75a7aa6f9c53f77db2d315a4039ea5a18da664ea1263114b50`, `replay-frames.bin` sha256 `3474ee7386eab212115c122c07595ec161a0e2e65f5732e89c127cf618f95f34`: identical to the W7 "Checks run" table above (the engine change only reaches a built-in row whose id does not resolve) |
