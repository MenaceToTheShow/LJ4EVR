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
- `Items.Data` (`TListItems` stream, text not art) is decoded to `items[]`
  when the bytes are complete; the current `dfm_text` extraction kept only the
  first 20 bytes of every blob, so the eight streams are recorded as
  `dropped:truncated stream: 20 of N bytes, M items` rather than guessed.

## Pre-commit hook

    cp tools/hooks/pre-commit .git/hooks/pre-commit
    chmod +x .git/hooks/pre-commit        # Git Bash / macOS

It runs `dfm2c.py --check` only when the reference tree exists and blocks the
commit on drift, with the command to run printed.
