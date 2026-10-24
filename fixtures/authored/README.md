# LJ4EVR-authored fixture definitions

Fixture definitions written for LJ4EVR from the manufacturer's own published user manual or DMX
chart, for fixtures and modes that neither the Open Fixture Library nor QLC+ carries (or carries
wrongly). They outrank both: hand files > authored > Open Fixture Library > QLC+
(`docs/fixtures.md`, "W7").

## Where

One directory per group, then one `.qxf` per fixture:

    fixtures/authored/<group>/<Manufacturer>-<Model>.qxf

`<group>` is one of `martin`, `elation`, `chauvet-dj`, `chauvet-pro`, `clay-paky`, `glp`, `acme`,
`ayrton`, `etc`, `robe`, `hes`, `vari-lite`, `adj`, `sgm` (the names of `docs/fixtures/catalog/`).
The directory decides the manufacturer folder the fixture shows under in Fixture Configuration.

## Rules

1. **Cite the manual.** `<Creator>` names it: `<Name>` = the manual's title, revision or date
   and URL; `<Author>` = who wrote the file. Repeat the citation in an XML comment at the top. The
   lint refuses a file without a manual URL.
2. **Facts only.** A DMX channel chart is a fact: channel order, ranges, defaults and the short
   function names. Never copy the manual's prose, images, logos or gobo pictures. Manuals
   themselves are never committed (downloaded PDFs live only under the gitignored
   `reference/manuals/<group>/`).
3. **Blank beats guessed.** A channel, range or mode you cannot read from the manual is left out,
   never invented. A fixture with no obtainable manual is not authored; it is listed as "no
   manual" in `docs/fixtures/coverage.md`.
4. **Never from LightJockey's library.** Martin's `.GS2` / `.Udf` files and anything under
   `reference/` are Martin-copyrighted and are not a source.
5. **32 channels.** A mode wider than 32 channels may be written down but is not shipped
   (LightJockey 2's limit and the `.GS2` format); the coverage report lists it.
6. **Licence.** MIT, as the rest of LJ4EVR.

## Format

The QLC+ `.qxf` subset `tools/qxf2c.py` reads (namespace `http://www.qlcplus.org/FixtureDefinition`):

- `<Manufacturer>`, `<Model>`, `<Type>`, `<Creator>` (`<Name>`, `<Version>`, `<Author>`);
- `<Channel Name="..." [Default="0-255"] [Preset="..."]>` with an optional `<Group Byte="0|1">`
  (`Intensity`, `Colour`, `Gobo`, `Pan`, `Tilt`, `Shutter`, `Beam`, `Prism`, `Effect`, `Speed`,
  `Maintenance`, `Nothing`) and `<Capability Min="0-255" Max="0-255" [Preset="..."] [Res1="#rrggbb"]>text</Capability>`
  rows in ascending, non-overlapping order;
- `<Mode Name="...">` with `<Channel Number="0..n-1">channel name</Channel>`;
- a 16-bit fine channel is `Byte="1"` of the same group (or a `...Fine` preset) after its coarse
  channel in every mode that uses it.

How groups and presets become LightJockey channel classes, palettes and control windows is the
table in `docs/fixtures.md` ("Group + Preset -> class / dyn / palette").

## Checks

    python -I tools/qxf2c.py --lint fixtures/authored/<group>/<file>.qxf   # one file: a line per problem, exit 1
    python -I tools/qxf2c.py --lint                                         # every authored file (CTest qxf2c_lint_authored)
    python -I tools/qxf_map_sync.py                                         # give the new modes their ids
    python -I tools/qxf2c.py                                                # regenerate src/fixlib/lj_profiles_gen*.c
