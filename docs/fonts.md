# Fonts: the clean-room pipeline (design 7.4, verdict ME-18)

## Clean-room statement

Every glyph in `art/fonts/*.txt` is original pixel art drawn freehand to harvested
metrics. No screen capture, font bitmap, rendered text or outline of any Microsoft or
Martin font was used as a tracing layer. What was taken from the originals is numbers:
the cell height, ascent, descent and overhang of each face and the advance width of each
CP1252 byte, as GDI reports them on a Windows PC at 96 dpi, and the widths of the
AutoSize labels in the LightJockey form files, which are the same numbers measured by
Delphi in 1999-2005. Numbers are facts and are not copyrightable; the pixels are ours
(CC0). `NOTICE` carries the one-sentence form of this.

## The pipeline

| Step | Tool | Input | Output | Runs |
|---|---|---|---|---|
| 1 | `tools/widthcorpus.py` | `reference/.../dfm_text/*.dfm.txt` (208 forms) | `tests/fonts/widths.tsv` (826 AutoSize `TLabel` rows), `tests/fonts/font_census.tsv` (19 fonts) | developer, when the reference tree changes (never) |
| 2 | `tools/fontharvest.c` | the census | `tests/fonts/advances_<name>.tsv` (17 files) | developer, Windows + GDI only, never CI |
| 3 | `tools/fitfont.py` | `widths.tsv` alone | fitted advances vs the harvest | developer cross-check |
| 4 | `art/fonts/<name>.txt` | a human and the advances | pixel art (`FORMAT.md`) | drawing |
| 5 | `tools/fontpack.py` | the sheets + advances | `src/gfx/lj_fonts_gen.c` | developer; CI runs `--check` |
| 6 | `lj4evr_tests font_corpus`, `font_cells` | `widths.tsv`, the packed fonts | 0 mismatches | CI |

Everything in `tests/fonts/` is committed; it is numbers and captions. The reference
tree is gitignored and only step 1 reads it.

## What the census found (2026-10-21)

`font_census.tsv`, every visual control of the 208 forms resolved to its font (own
`Font.*`, else the nearest ancestor with one, else the form, else `MS Sans Serif -11`):

| Face | DFM height | Style | Controls | Harvested cell (height/ascent/descent) | File |
|---|---|---|---|---|---|
| MS Sans Serif | -11 | regular | 2886 | 13 / 11 / 2 | `sans13` |
| MS Sans Serif | -11 | bold | 453 (+1 bold+underline) | 13 / 11 / 2, overhang 1 | `sans13b` |
| Arial | -11 | regular | 36 | 14 / 11 / 3 | `arial14` |
| Arial | -11 | bold | 94 | 14 / 11 / 3 | `arial14b` |
| Arial | -12 | regular / bold | 2 / 3 | 15 / 12 / 3 | `arial15`, `arial15b` |
| Arial | -13 | bold | 6 | 16 / 13 / 3 | `arial16b` |
| Arial | -15 | bold italic | 2 | 17 / 14 / 3 | `arial17bi` |
| Arial | -16 | bold | 2 | 19 / 15 / 4 | `arial19b` |
| Arial Black | -13 | regular / bold | 1 / 1 (the Fingers form itself) | 18 / 14 / 4 | `arialblack18`, `arialblack18b` |
| Courier | -11 | regular / bold | 2 / 5 | 13 / 11 / 2 (bold overhang 1) | `courier13`, `courier13b` |
| Courier | -13 | regular | 2 | same strike as -11 | `courier13` |
| MS Sans Serif | -13 | regular / bold | 1 / 14 | 16 / 13 / 3 (bold overhang 1) | `sans16`, `sans16b` |
| MS Sans Serif | -16 | bold | 2 | 20 / 16 / 4, overhang 1 | `sans20b` |
| MS Sans Serif | -19 | bold | 4 | 24 / 19 / 5, overhang 1 | `sans24b` |

The five faces with sheets (`sans13`, `sans13b`, `arial14`, `arial14b`, `arialblack18`)
cover 792 of the 826 label rows; the other 34 rows (and 56 non-label controls) need the
remaining twelve files, each a `--skeleton` away. `arialblack16` in the design was a
guess: the harvested cell of `Arial Black -13` is 18 px, so the face is `arialblack18`.

## The bold rule

A bold `MS Sans Serif` or `Courier` label is GDI-synthesized bold (there is no bold
strike): every advance is the regular advance + 1 and `DrawText`, which is what
Delphi's `TLabel.AutoSize` measures, adds `tmOverhang` (1) once more per string.
`GetTextExtentPoint32` does not. So a label width is `sum(advances) + overhang`; the
harvest carries `overhang` in its header, the sheets carry it in theirs, and
`lj_text_width()` must add it (`lj_font.overhang`). TrueType faces (Arial, Arial Black)
have a real bold and overhang 0. `tools/fontharvest.c` asserts the rule with
`DrawText DT_CALCRECT` for every face; `fitfont.py` reproduces all 72 determinable
`sans13` advances and all 60 `sans13b` advances from the corpus alone once the overhang
is subtracted.

## Redrawing a glyph

1. Open `art/fonts/<name>.txt`, find `glyph <code> <advance> <label>`.
2. Edit the `<height>` rows under it with `#` and `.`; keep every row exactly
   `<advance>` characters. Ink narrower than the cell is fine; the right-hand column
   is usually the inter-character gap.
3. `python -I tools/fontpack.py` then `ctest -R font_` (or `lj4evr_tests font_corpus`).
   `lj4evr_render --font-proof <name> out.bmp` draws the whole face for a look.
4. Never paste a glyph from a screenshot or a font file; draw it.

## Regenerating the harvest

Only on Windows, only when a new face appears in the census:

    python -I tools/widthcorpus.py
    cl /nologo /W4 /WX /permissive- /utf-8 /std:c11 /D_CRT_SECURE_NO_WARNINGS tools\fontharvest.c gdi32.lib user32.lib
    fontharvest.exe tests\fonts\font_census.tsv tests\fonts
    python -I tools/fitfont.py                       # sans13 cross-check, expect 72 / 72

The harvest is deterministic for a given Windows font set; a different result on another
PC means a different font version, and the committed numbers win (they match the DFMs).

## Proof sheet and corpus result (fonts-pack step 3, 2026-10-21)

All five sheets validate (`tools/fontpack.py`: 222 of 224 glyphs inked per face; the two
blanks are space and no-break space). Proof of every face, every code, on one sheet:
`build-fonts/proof/proof_all.png` (1:1) and `proof_all_x3.png` (3x nearest), rendered by
`build-fonts/proof/proof.c` (the build directory is gitignored; the renderer is 50 lines of
`lj_text()` over the packed fonts, rebuilt with the `build.cmd` recipe of `build-fonts-sans13`).
Inspected at 3x: baselines, stroke weight and cell spacing are consistent within each face,
nothing collides across advance cells in the pangram, menu and fixture sample strings, and
the accelerator underline sits at row `ascent + 1` in all five. No art fix-up was needed.
Cosmetic, left as is: `sans13b` draws `Š Ž š ž` (0x8A 0x8E 0x9A 0x9E, advance 4) as the
undefined-byte box where `sans13` draws 3-column letters.

`lj4evr_tests font_corpus` over the whole of `tests/fonts/widths.tsv`:

    font_corpus: 826 rows, 34 skipped (face not packed), 200 mismatches
      sans13b: 200 mismatches

Every one of the 200 is a bold `MS Sans Serif` label 1 px narrow (`'&Min' got 20 want 21`,
`'CD' got 17 want 18`, ...): the bold rule above, `tmOverhang` not yet added by
`lj_text_width()`. The regular and TrueType faces are exact: `sans13` 0, `arial14` 0,
`arial14b` 0, `arialblack18` 0 mismatches, so the advance tables are right. With the
three-line patch below applied on a copy (verified on this PC, same `test_corpus.c`):

    font_corpus: 826 rows, 34 skipped (face not packed), 0 mismatches
    font_cells: 5 faces, 0 failures

The patch, for the gfx track (`lj_font.h`, `lj_font.c`) and then one line here:

1. `gfx/lj_font.h`: `int height, ascent, descent, overhang;` (overhang = GDI `tmOverhang`,
   1 for the synthesized-bold raster faces, 0 otherwise).
2. `gfx/lj_font.c`, end of the loop in `lj_text_width()`: `if (w) w += f->overhang;`
   (`DrawText` adds it once per non-empty string; `lj_text()` ignores it).
3. `tools/fontpack.py`: emit `.overhang = %d,` in place of the `/* overhang %d */` comment
   (the `ponytail:` line marks the spot), then re-run it.

Two names still disagree with the harvest: `gfx/lj_font.h`, `ui/lj_window.c`,
`ui/lj_widgets_lists.c` and `app/lj_render_main.c` say `lj_font_arialblack16`; the packed
face is `lj_font_arialblack18` (cell 18, ME-18). Until they are renamed `lj4evr_tests` and
`lj4evr_render` do not link; the corpus numbers above were taken with a throw-away
`const lj_font lj_font_arialblack16 = { 0 };` shim that is not in the tree.

## Art pack (glyphs and icons)

`tools/artpack.py` packs `art/glyphs/*.txt` and `art/icons/*.txt` (format:
`art/glyphs/FORMAT.md`: a palette line, then rows of palette characters, bottom-left
pixel = the transparency key) into `src/gfx/lj_art_gen.c` as `lj_art_table[]` /
`lj_art_count` of `gfx/lj_glyphs.h`, exactly the shape `lj_glyphs.c` already looks up
(no change to `lj_glyphs.c` was needed). `--check` is the CI staleness gate;
`lj4evr_tests art_table` checks the packed table. Contact sheet of every entry:
`build-fonts/proof/art_sheet_x3.png`.

Drawn (our own, CC0, from the function names of design 7.7 and notes 07 section 3.4, no
Martin bitmap opened): the 24 of the brief, `add-scene insert delete previous next first
last play stop pause record up down left right check cross lamp reset blackout freeze
online offline sync`, plus `rewind fastforward new save load` because the Sequence and
Cue toolbars need them, each at 16x16 (`<name>`) and 24x24 (`<name>-24`), and one 50x50
fixture-desktop silhouette `icons/movinghead.txt` (design 7.8). 59 entries, 26,628 pixels.

`tools/glyphmap.tsv` now names every `TSpeedButton` of `TSEQUENCEFORM` and `TCURRENTCUE`
(35 rows; `missing` where no glyph exists yet: add-to-cue, blind, random trig, stopwatch,
finger, speaker, clock, macro scale, macro editor). Note for `dfm2c.py`: those buttons carry
no `Glyph.Data` in the DFM (LJ2 loaded them at runtime from `Source-Images\Glyphs`), and
`Gen.art()` consults the map only for blobs, so the rows are inert until the generator
looks up `(form, object)` for every `TSpeedButton`/`TBitBtn`/`TImage` and writes the stem
into `.glyph`. The `lj_art_entry.img.px` cast: the pixel arrays are `static const`; the
table casts to `uint32_t *` because `lj_surface.px` is not const, and nothing writes them.
