# .LJB import

LightJockey 2 writes a user-library backup as one `.LJB` file (note 18 s1). LJ4EVR reads it once and converts what
note 18 decodes into a new library of LJ4 text records (design 5.3). It never reads LJ2's `.seq` / `.cue` / `.Cl2`
files natively, and it never writes into an existing library. Code: `src/model/lj_ljb.c` (`model/lj_ljb.h`).

## Using it

User Libraries > Restore User Library lists `*.LJB` files next to our `.lj4b` backups. Picking one shows the
library name from `Info.$$$` and the backup's notes from `Notes.$$$`, with the RTF stripped. Restore asks
`Import "<name>" as a new user library ?`. The import creates `<name>`, or `<name> (2)` .. `<name> (99)` when that
name is taken. It never uses `Default`. The notes box then shows the report, and the User Libraries restart prompt
selects the new library. Entry point and prompt texts are PROPOSAL (LJ2 restores a `.LJB` over a library, 06 s4).

## The container checks (W5-LJB-1)

Every check runs in `lj_ljb_open` before any entry is inflated:

1. The CRC-32 of every byte before the 36-byte trailer must equal the trailer's first word. This is checked first.
2. Entry count n is 1..8192 (PROPOSAL). The index must lie inside the file (`ioff + isz <= len - 36`), and it must
   inflate to exactly n x 532 bytes.
3. Each entry needs a non-empty name with no NUL inside either ShortString. Flags must be 0x300 (deflate) or 0x1
   (stored, and then `csz == usz`); any other flag is refused. The stored bytes must sit before the index
   (`off + csz <= ioff`). A deflated entry may claim at most 1032 x its stored size, which is zlib's ceiling.
4. The summed uncompressed size is at most 512 MB (PROPOSAL), checked before anything is allocated. The file itself
   is size-checked before it is read.
5. Path rules (W5-LJB-2), for the folder and the name: every component must be non-empty and must not be `.` or
   `..`. Refused: control bytes, any of `<>:"/\|?*` (so drive letters and absolute paths fail too), a trailing dot
   or space, and the device names `CON PRN AUX NUL COM1-9 LPT1-9` with or without an extension. A non-empty folder
   ends with `\`. Two paths that are equal case-insensitively (CP1252 letters folded) are refused.
6. `lj_ljb_extract` inflates through miniz `tinfl` into a `usz + 1` buffer. The output must be exactly `usz` bytes
   with the entry's CRC-32.

Each joined name must also fit `lj_library_path`'s cap. The import also extracts every entry (size + CRC) before it
converts anything, so one bad entry refuses the whole file. Errors are the `LJ_LJB_ERR_*` texts in
`(char *err, size_t cap)`. On any refusal the new folder is deleted, so nothing is left behind. Nothing from the file
is ever executed or written under a name the file chose: converted records get LJ4 names, and user profiles are
written as `UserFixtureDef\LJBSlot<nnn>.GS2`.

## What is converted (W5-LJB-3)

Every record's size is checked before any field is read. Strings stay CP1252 bytes, and control bytes become `?`.
Times stay integer ms. The two TDateTimes are not imported (no `double` in `model/`). Padding is ignored.

| LJ2 file | Size check | Becomes | Inferred, tagged `PROPOSAL rig 18` |
|---|---|---|---|
| `Config.dat` | 16,546 | `Config.dat` fixtures: profile id = the type code (exe row id, ME-8, never renumbered), link / address from the 0-2047 code, user name, placed, icon x / y, tab | code = (link-1)*512 + address-1 (s3); tab = byte - 1 (s8) |
| `UserFixtureDef\FixtureDef.<k>` | 20,851 + `lj_gs2_read` | `UserFixtureDef\LJBSlot<k+1>.GS2`, fixture k+1 becomes kind GS2 | |
| `GroupInfo.dat` | 12,972 | the 99 group names and members | one flag per slot (s8) |
| `Sequences\NNNNNNNN.seq` | 0x18C8 + count x 6,312 | number, name, per scene times, fade state and value per channel | count byte 0x5B (s4.1); Off / Snap / Fade = 0 / 1 / 2 (s4) |
| `DefaultOut.dat` | 2,748 | default output | |
| `Statics.dat` | 127,342 | Statics 2 page "Page 1": the 20 entries' names, recorded scenes and fade times | page name (s6) |
| `CueMacros\NNNNNNNN.CMA` | 26,659 | number, name, per channel shape, base, amplitudes, delay, cycle | field roles (s9); the cycle index is copied as deciseconds; direction fwd |
| `Cues\NNNNNNNN.cue` | 1,132 | number, name, slot 1 sequence | slot 1 enabled = byte 0x118, trig = byte 0x94 (s7) |
| `Cues\CuePageInfo.Dat` | 6 + count x 35 | page names (no cues: membership is undecoded) | 35 bytes per page (s8) |
| `Info.$$$` | >= 0x204 | the library name (sanitised; the source path at 0x04 is never read) | |
| `Notes.$$$` | | `Notes.txt` through `lj_str_rtf_strip` | |

`DirInfo.Ini [NEXT]` is set past the highest imported number of each kind.

## The report (W5-LJB-4)

Everything not converted gets one line in the Restore window and in `<library>\LJBImport.txt` (PROPOSAL name). That
covers files LJ4EVR cannot read (cue lists, BG cues, `Setup.<k>` palettes, presets, `Special.dat`, `Scanner1.dat`,
`Patch2.Cfg`, the `.ini` files, `Selection.Dat`), records that fail a size or range check, fixtures with no LJ4EVR
profile or an address code past 2047, and what a converted file lost (cue slots 2-12 and options, static modes and
levels, cue-page contents). The `LJMData\` files (LJ Manager) are counted on one line. The window shows the first 64
lines; the file has all of them.

## Tests

All rows use synthetic containers written by `ljb_pack` in `tests/model/test_ljb.c`. No LJ2 file is in git.

- `fuzz_ljb`: 20,000 seeded mutations through open / extract, with the CRC recomputed on every second one.
  `tools/fuzz/fuzz_ljb.c` is the libFuzzer target (`-DLJ_FUZZ=ON`).
- `test_ljb`: one row per container refusal, the path rows, a traversal file imported under a scratch root (refused,
  nothing written, no library left), every converted kind read back through the LJ4 loaders, a second import to
  `MyShow (2)`, the Restore window flow headless, and the LORD_C-9 `notes.txt` cap.
- `ljb_sample` (W5-LJB-5): runs on the developer PC only. It reads the note 18 sample through
  `LJ4EVR_REFERENCE_DIR` and prints counts, never a path or a name. On any other machine it prints `SKIP`.
