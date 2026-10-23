# Generated files

Committed generated files, the tool that writes each, and the check that keeps
them honest (design 9.2). Every generated `.c`/`.h` opens with a banner naming
the tool, its version and the SHA-256 of every input; nobody edits them by hand.

| File | Tool | Inputs | Check |
|---|---|---|---|
| `src/ui/forms/lj_forms_gen.c`, `.h` | `tools/dfm2c.py` | `reference/installer/exe_resources/dfm_text/*.dfm.txt` (208, never committed), `tools/dfm2c_drop.txt`, `tools/glyphmap.tsv` | developer pre-commit hook runs `dfm2c.py --check`; CI compiles and runs `lj4evr_tests forms_tables` |
| `tools/dfm2c_unknown.txt` | `tools/dfm2c.py` | same | diffed by `--check`; must stay empty (a new line = a property without a slot in `lj_form.h`) |
| `tools/dfm2c_blobs.txt` | `tools/dfm2c.py` | same | diffed by `--check`; every blob has a disposition |
| `tests/ui/dfm_census.tsv` | `tools/dfm2c.py` | same | diffed by `--check`; the class counts of design 7.2 |
| `src/fixlib/lj_profiles_gen.c`, `.h` | `tools/qxf2c.py` | `tools/qxf_map.tsv`, `third_party/qlcplus-fixtures/*.qxf` (QLC+ commit in the banner) | `qxf2c.py --check` in `generate-check`; `qxf2c_selftest` CTest row |
| `tools/qxf2c_overflow.txt` | `tools/qxf2c.py` | same | diffed by `--check`; the wheels with more than 20 capabilities (ME-26), printed by `generate-check` |
| `src/fixlib/lj_exe_profiles.c` | `tools/exe_profiles_check.py` | `reference/installer/exe_profile_table.txt` (never committed), `src/fixlib/hand/*.c`, `tools/qxf_map.tsv` | `--check` recomputes the `binding` column; runs in the `generate-check` loop of `ci.yml` and as the `exe_profiles_check` CTest row (ME-76; the regeneration diff runs only where `reference/` is present) |
| `src/gfx/lj_fonts_gen.c` | `tools/fontpack.py` | `art/fonts/*.txt`, `tests/fonts/advances_*.tsv` | `fontpack.py --check` in `generate-check`; `font_cells`, `font_corpus` |
| `src/gfx/lj_art_gen.c` | `tools/artpack.py` | `art/glyphs/*.txt`, `art/icons/*.txt`, `art/gobos/*.txt`, `art/gels.txt`, `art/lj2names.tsv` | `artpack.py --check` in `generate-check`; `art_table` |
| `tests/vectors.h` | `tools/gen_vectors.py` | `tools/ref/08-sacn_ref.py`, `14-macros_mac_decode.py`, `16-gs2_ref.py`, `17-position_ref.py` | `gen_vectors.py --check` in `generate-check` and the `gen_vectors_check` CTest row |
| `src/engine/lj_trig_gen.c` | `tools/gen_vectors.py` | `tools/ref/17-position_ref.py` | same row |
| `src/engine/lj_shapes_gen.c` | `tools/gen_vectors.py --shapes` (W3 genmacro) | the authored definitions in `gm_shapes()` of `tools/gen_vectors.py`; no reference data | `gen_vectors.py --shapes --check` (CTest `gen_vectors_shapes_check`) and `--shapes --check --mutate` (CTest `gen_vectors_shapes_mutated`, WILL_FAIL: one flipped sample of the committed file must be caught) |

**Frozen (ME-53).** `tests/fonts/widths.tsv` and `tests/fonts/font_census.tsv` were written once in W0 by
`tools/widthcorpus.py` from the 208 DFMs, which never change; the tool (a second DFM parser) was deleted in W4
track 0. The two files are frozen facts: nobody regenerates them, `font_corpus` reads `widths.tsv`, and a new
face found in the forms is added to both by hand with a reviewer.

## dfm2c.py

    .venv/Scripts/python.exe -I tools/dfm2c.py           # regenerate all five files
    .venv/Scripts/python.exe -I tools/dfm2c.py --check   # regenerate in memory, diff, exit 1 on drift

Stdlib only, Python 3.12. It needs the reference tree; without it the tool
exits 2 and prints why, so CI (which has no reference tree) never runs it.

The contract (design 7.6, verdict ME-12) is a drop-list, not a keep-list:
every DFM property of every object lands in a slot of `ui/forms/lj_form.h`
except the eight on `tools/dfm2c_drop.txt` (`OldCreateOrder`, `PixelsPerInch`,
`Scaled`, `Cursor`, `DefaultMonitor`, `Ctl3D`, `ParentCtl3D`, `Font.Charset`).
A property with no slot is counted in `dfm2c_unknown.txt`; the fix is a new
slot in `lj_form.h` and a line in the generator's slot maps, never a longer
drop-list.

What the tables hold, in one line each:

- Geometry, captions (CP1252 bytes as octal escapes, Martin's typos verbatim
  [D13]), hints, tab orders, event-handler names, every class-specific Delphi
  enum as its identifier string (`Kind = bkOK`), sets as their text
  (`Options = "[goFixedVertLine]"`), colours as `LJ_CLR()`, tri-state bools.
- Children nested in DFM order, which is Delphi z-order; coordinates stay
  relative to the parent.
- `Action = acX` resolved at generation into caption/hint/shortcut/enabled/
  checked/image_index/group_index where the control did not say, `.action`
  kept for binding.
- Fonts: a row with `Font.*` carries the raw facts and the resolved face; a row
  without one inherits the parent's resolved face (the form's, ultimately
  `MS Sans Serif -11`) and says so with `.font_inherited = 1`.
- Forms that stored `Width`/`Height` (the 98 `AutoScroll` forms) get
  `client_width`/`client_height` derived from the classic frame metrics of
  design 7.3 (border 4/3/0 px, caption 19/15, menu bar 19) and
  `.client_derived = 1`; the raw `width`/`height` are kept beside them.
- Menus: every `TMainMenu`/`TPopupMenu` as an `lj_menu_desc` in
  `lj_form_desc.menus[]`, items as nested `lj_menu_item_desc` trees (1081).
- Art blobs (`Glyph.Data`, `Picture.Data`, `Icon.Data`, `TImageList.Bitmap`)
  never reach the repo [D2]: the slot gets the name from `tools/glyphmap.tsv`
  (`form<TAB>object<TAB>name`, hand-maintained, append rows, never clobber) or
  `"missing"`; `dfm2c_blobs.txt` records `glyph:<name>` or
  `dropped:no-glyphmap-row`.
- `Items.Data` (`TListItems` stream, text not art) is decoded to `items[]`;
  all eight streams are complete since ME-47 and recorded as `decoded`
  (a truncated or malformed stream would be `dropped:<reason>`, never guessed).
- List text that is not a caption is emitted empty and recorded as
  `dropped:<reason>` (`ITEMS_EXCLUDED` in `dfm2c.py`): today one row, the About
  box's `RichEdit1.Lines`, Martin's credits (ME-137, "D2: authored credits, not
  a caption").

## Pre-commit hook

    cp tools/hooks/pre-commit .git/hooks/pre-commit
    chmod +x .git/hooks/pre-commit        # Git Bash / macOS

It runs `dfm2c.py --check` only when the reference tree exists and blocks the
commit on drift, with the command to run printed.

## gen_vectors.py --shapes (W3 genmacro track)

    .venv/Scripts/python.exe -I tools/gen_vectors.py --shapes            # write src/engine/lj_shapes_gen.c
    .venv/Scripts/python.exe -I tools/gen_vectors.py --shapes --check    # exit 1 when the committed file differs
    .venv/Scripts/python.exe -I tools/gen_vectors.py --shapes --compare  # developer PC: each definition against Macros.Mac

`lj_shapes_gen.c` holds the 66 generic-macro shapes of note 14 sA.6 in `Macros.Mac` file order (the duplicate
`2*Sine^2` at #1 and #12 kept), 8192 int16 samples each at +-32640 = 127.5 x 256, as plain C data (66 x 8192 samples,
about 2.8 MB of text: the contracts allowed generator code instead; one table is the shortest thing that is exact on
every compiler and needs no float in `engine/`). Every sample comes from an authored definition in `gm_shapes()`:
the sine family is exactly `round(32640 * sin)` (design 3.6), the others are our own piecewise formulas. Martin's file
is never read for emission and nothing from `reference/` is committed (ME-34); `--compare` reads it (path from
`LJ4EVR_MACROS_MAC`, else `reference/installer/extracted_2.209/app/Macros.Mac`) through
`tools/ref/14-macros_mac_decode.py` and prints how far each definition sits from it. On 2026-10-22, 53 of the 66
definitions reproduce the decode sample for sample; the 13 that do not:

| # | Shape | max abs diff | samples differing | Why |
|---|---|---|---|---|
| 17 | Bounce | 2 | 1957 | `abs(sin(pi (k / 4621.85)^2))`, the chirp constant fitted |
| 18 | Bounce 2 | 9 | 4266 | `abs(sin(pi (k / 3819.05)^3))`, fitted |
| 26 | Sqrt^3 | 1 | 3 | `32660.05 (k / 8192)^1.5`, fitted |
| 29 | Log | 1 | 2 | `3621.28 ln k`, fitted |
| 30 | Inverted Log | 1 | 2 | `32640 - Log(k - 99)` |
| 31 | Swapped Log | 1 | 2 | `Log(2k)`, then the negative mirror |
| 56 | Swapped Peak | 1 | 2588 | `32766.5 (k / 4096)^1.5` and its mirror |
| 57 | Swapped Peak^2 | 126 | 7726 | `(k / 4096)^3`: Martin's table saturates next to the swap (range -32616..32640 here, -32640..32640 there) |
| 58 | Swapped Peak^4 | 249 | 5712 | `(k / 4096)^6`, same reason (+-32592 here) |
| 59 | Ripple | 1 | 3 | Sqrt^3 with the sign flipped on 1024j..1024j+512 |
| 63 | Spiky | 128 | 7895 | the Swapped Peak swept by a 512-sample ramp |
| 64 | Static | 32512 | 8176 | our noise: 512 held steps of 16 samples from xorshift32 (TV static has no formula) |
| 65 | Static 2 | 63872 | 8176 | the same, bipolar |

`test_genmacro` asserts the count, the names, every range above (the note's ranges, with 57 / 58 ours) and the sine
family against `tests/vectors.h`. A user's own `Macros.Mac` replaces the table at run time (`<library>\Override\`,
`docs/fidelity.md`).

## W4 docs-release: the help topics

| File | Tool | Inputs | Check |
|---|---|---|---|
| `src/windows/lj_help_topics_gen.c`, `.h` | `tools/help_check.py --write` | `help/*.txt` (our own topic text; line 1 the title, line 2 blank, ASCII, at most 72 columns) | CTest `help_check` (`--check`: the files current, every topic valid, no French / German words, and where `reference/` exists no run of 8 words shared with `lj-Help_chm_2.209.txt`, `LJ_2.100_Helpfile.txt` or `lj-Help2_6b6.txt`; `docs/user-guide.md` is checked the same way); `help_check_selftest` plants an 8-word run, a German topic and a stale file and must catch each; `test_help_viewer` |

The topics are compiled in, index first and then in the order `help/index.txt`
lists their titles; `lj_help_viewer_topic("<file stem>")` opens one. Both
installers also ship the `.txt` files (`docs/building.md`).
