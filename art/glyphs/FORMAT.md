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
