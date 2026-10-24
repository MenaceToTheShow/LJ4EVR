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
