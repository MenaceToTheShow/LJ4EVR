# art/glyphs, art/icons: the hand-drawn colour art

One text file per image, CC0, our own drawing (D2: no Martin bitmap is traced or shipped;
the function of each glyph and its size are the facts). `tools/artpack.py` packs every
`.txt` in both directories into `src/gfx/lj_art_gen.c` as `lj_art_table[]` of
`gfx/lj_glyphs.h`; a widget asks `lj_glyph_get("<stem>")`.

## File

```
palette .=C0C0C0 #=000000 G=00FF00 d=008000
................
....d...........
....ddd.........
(one row per pixel row, every row the same width)
```

- Line 1: `palette` then `<char>=RRGGBB` pairs. `.` must be present: it is the
  transparency key, and the bottom-left pixel must be `.` (that pixel IS the key
  `lj_blit_rgb()` reads, as Delphi did). Any other printable ASCII character is a colour.
- Then the rows, top first; width = row length, height = row count, 1..256 each.
  No comment lines (`#` is a colour).
- The name is the file stem: `play.txt` -> `"play"`, `play-24.txt` -> `"play-24"`.
  Stems must be unique across `glyphs/` and `icons/`.

## Sizes

- `glyphs/<name>.txt` is 16x16 (the glyph of a 25x25 `TSpeedButton`, 23x22 and smaller
  buttons); `glyphs/<name>-24.txt` is the 24x24 variant for larger faces.
- `icons/<name>.txt` is a ~50x50 fixture-desktop silhouette (design 7.8).

## Workflow

1. Draw or edit the `.txt`.
2. `python -I tools/artpack.py` regenerates `src/gfx/lj_art_gen.c`;
   `python -I tools/artpack.py --check` (CI) fails if the generated file is stale.
3. `lj4evr_tests art_table` checks the packed table (names, sizes, key rule).

`tools/glyphmap.tsv` maps a DFM button `(form, object)` to a stem here; dfm2c.py writes
the stem into `lj_ctl_desc.glyph`, and `"missing"` draws an empty face.

## W4: the full set (art track)

- `art/glyphs/`: one stem per LightJockey 2 `Source-Images\Glyphs` file name (210, listed in
  `art/lj2names.tsv` with the cell size each was drawn to: the LJ2 strip's height and its width
  divided by the strip's state count), the dialog-button glyphs of the DFMs (`tools/glyphmap.tsv`),
  the VCL Kind set (`ok`, `cancel`, `help`, `close`, `yes`, `no`, `retry`, `ignore`, `abort`, `all`)
  and three pictures (`about-logo`, `init-logo`, `laser-warning`). Only the file names and the
  BMP header sizes were taken from the install; every pixel is ours.
- `art/icons/`: 58 fixture-desktop silhouettes, 50 px tall; `desktop/lj_fixture_icons.c` picks one per
  profile by name rules, then by channel classes.
- `art/gobos/gobo_<name>.txt`: 31x30 schematic gobo / effect tiles (F17), `<name>` = the LJ2 palette
  name in lower-case alphanumerics. Black square, white pattern, the four corner pixels are the key.
- `art/gels.txt`: our swatch per LJ2 colour-palette name: one colour (flat), two (diagonal halves),
  three or four (stripes) or `text:<LABEL>` (white tile with the label). `lj_palette_look()` resolves
  a palette entry name: Source-Images, our tile, our gel.
- `art/lj2names.tsv` also drives the Source-Images override (design 7.8): with
  `<userdata>/Source-Images/` present, `lj_glyph_get(stem)` returns the owner's own `Glyphs\<file>`
  (left cell of the strip, bottom-left key) instead of ours. Nothing from that folder is committed.

`python -I tools/artpack.py` prints the census (210 LJ2 names, 58 icons, tiles, gel rows);
`lj4evr_tests art_census` checks every profile resolves to an icon and every named palette entry to a
tile or a gel (0 unresolved).
