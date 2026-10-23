# Changelog

Format: Keep a Changelog; versions are semver tags `vX.Y.Z` and `VERSION` is the
single source of the number.

## [Unreleased]

### 1.1.0 (W5, on `main`, not yet tagged)

The seven features 1.0.0 left out are in, each replacing its greyed `Not available in LJ4EVR` chrome:

- **Art-Net output.** Setup > Hardware Setup: any link can send Art-Net 4 instead of sACN, with its own Port-Address.
  LJ4EVR polls for nodes every 2.5 s and sends each node only the universes it asks for; with no node, nothing is
  sent. The status line shows packets/s and the node count. Art-Net(TM) Designed by and Copyright Artistic Licence.
- **Audio Analyzer and audio trig.** Hardware Setup > Audio turns capture on (off on a new installation); &Audio >
  Start Audio Analyzer opens LightJockey's analyzer window with its real-time, time-line and equalizer views. Cue
  slots and background cues in Audio trig mode follow the beat; Emulate Audio Trig does the same from a hotkey.
- **`.LJB` import.** User Libraries > Restore lists LightJockey 2 `.LJB` backups. One is imported into a new library
  (fixtures, groups, sequences, default output, statics, generic macros, cue names), with a report of everything
  it could not convert. A damaged or hostile file is refused and nothing is written.
- **DMX-in over sACN and Art-Net.** Hardware Setup > Enable DMX in, one input universe. The Dmx In window shows the
  incoming channels live, assigns functions (binary cue latch, masters, Go, statics, smoke, audio settings ...) that
  take effect on Save, and value-translation tables (`.vtr`). The Global Patch takes DMX-in channels as sources.
- **RS-232 / TCP remote.** Setup > RS-232 / TCP Remote: LightJockey's 3-byte remote protocol over a serial port or
  TCP (port 7232, 127.0.0.1 unless you pick an interface; off by default; no authentication).
- **External Application string output.** The External Application/Device fixture's command strings go to a serial
  port or a TCP receiver on each scene change (Define Application Link on the fixture icon).
- **LJ MediaPlayer.** The nine MP cue-list commands, the media time code and the player window are live. It plays
  WAV files through the system's audio output; video and other formats are refused with a message.

Also: the Hardware Setup status line shows the real sACN packets/s per universe (1.0.0 always showed 0); a master
driven by DMX-in shows its mode text red and ignores the mouse.

Also from the W5 code council's fix pass (`W5_CODE_MASTER_ELDER_VERDICT.md`, ME-167..ME-186):

- DMX-in functions run only on a frame a sender (or Fingers) delivered: a saved Master Intensity assignment no longer
  drives the Grand Master to 0 at start with no input. The Overwrite Enable gate follows its channel after DMX-in is
  turned off and on, and Disable DMX-in functions opens it at once.
- The remote's slot, background-slot and smoke-timer commands set their state; a repeated "off" no longer turns it on.
- MP Load File refuses a network path, a device name or a non-WAV file before touching the file system (no SMB
  connection from opening a cue list), and reads a relative name from the library folder. The WAV size cap is 64 MB.
- Art-Net subscribes only nodes on the chosen interface's subnet (at most 4 rows per address); DMX-in over sACN and
  Art-Net ignores packets from outside the configured interface's subnet. `[ARTNET] In` is no longer saved.
- Select Bitmap shows large icons; the User Libraries list has its Date column; &Audio shows only while the Audio
  Analyzer is enabled; the cue list's MediaPlayer button has its own glyph and stays latched when clicked; Pass
  Keypresses works in the Analyzer and the player; LightCorder samples on the engine tick.
- Hostile-input fixes: a duplicate statics slot, a 2^32 / 2^64 group mask, a cue list over 9999 lines, an `.LLF`
  header claiming more frames than it holds and an out-of-range `DmxIn.dat` function number are refused.
- Wait Elapsed lines count from the previous wait's due time, so a long list does not drift; Automatic Cue update on
  Run starts from the last non-transparent cue.
- Help gains three topics: DMX-in and remote control, Audio Analyzer and MediaPlayer, Importing a LightJockey 2 backup.

Still left out (README "What v1.1.0 is not"): the reference-rig comparison, audio CD and Winamp, video, and the
Global Patch per-channel "Use / Ignore DMX In Enable" items.

Known gaps:

- macOS: the build is tested by CI on every change, but neither 1.0.0 nor 1.1.0 has been run by hand on a Mac, and the
  macOS microphone permission prompt the Audio Analyzer triggers has not been seen on a real Mac. Use the system's
  window frames there (`--native`, or `[CHROME] Native=1` in `Main.Ini`), the recommended setting on macOS.
- The installers are not code-signed: Windows SmartScreen warns (More info > Run anyway); on macOS right-click
  LJ4EVR.app > Open the first time.
- A few timings and layering rules are best estimates until they are compared with LightJockey on a real rig; each is
  marked "deferred: needs rig" in `docs/fidelity.md`.
- The remote has no authentication: keep it on 127.0.0.1 unless the controller is on a lighting network you trust.
- The Art-Net OemCode is OemUnknown (0x00FF) until a code is registered.
- The Statics 2 page layout and the background-cue file (`.bgc`) are LJ4EVR's own choices until a LightJockey sample
  shows the original.

## [1.0.0] - 2026-10-23 - W4 Fidelity sweep and release

The first full release. LJ4EVR 1.0.0 plays LightJockey 2 shows over sACN (E1.31) on Windows and macOS: fixture
configuration and patch, sequences, cues, cue lists, background cues, statics, generic and movement macros, follow
spots, fixture controls, hotkeys and MIDI, user libraries with backup and restore, and every LightJockey window in its
classic look, opened from the same menu, button or key as in LightJockey.

New since 0.3.0:

- Every remaining window opens from its documented place: the user library dialogs (new, rename, backup, restore),
  the warnings and file-update prompts, the laser warning, the DMX-in windows, 2532 Direct Access and Virtual Fingers
  on the keyboard and on MIDI, and the Debug window with the engine's counters.
- LightCorder (Cuelist > Tools) records what LJ4EVR sends to `.LLF` files in the library's `LightCorderFiles` folder.
- Help opens a built-in viewer with 15 topics written for LJ4EVR; every Help item in the menus reaches it.
- All art is LJ4EVR's own: toolbar glyphs, icons, colour swatches, gobo tiles and fixture pictures. A `Source-Images`
  folder in the user data can replace any of them with your own pictures.
- The main window caption reads `LJ4EVR (User lib: <name>)`.
- Windows: a per-user installer (no administrator rights needed) with an uninstaller in Settings > Apps. macOS: a
  `.dmg` with `LJ4EVR.app` for Intel and Apple Silicon (macOS 12 or later). Both carry `THIRD_PARTY_NOTICES.txt`.
- LJ4EVR is released under the MIT licence. LightJockey is a trademark of Martin Professional; LJ4EVR is not
  affiliated with or endorsed by Martin Professional or HARMAN.

What 1.0.0 is not (design section 1): it ships without the Audio Analyzer and audio trig, DMX-in over sACN input and
the `.vtr` tables, the RS-232/TCP remote protocol, External Application string output, the `.LJB` importer, the LJ
MediaPlayer and Art-Net. All seven are planned for a later version. Every menu item, cue-list command and toolbar
button that belongs to one of them is present, greyed, with the hint `Not available in LJ4EVR`. The DMX-in windows
open, with their input controls greyed.

Known gaps:

- macOS: the build is tested by CI on every change, but 1.0.0 has not yet been run by hand on a Mac. Use the
  system's window frames there (`--native`, or `[CHROME] Native=1` in `Main.Ini`), the recommended setting on macOS
  for this version.
- The installers are not code-signed: Windows SmartScreen warns (More info > Run anyway); on macOS right-click
  LJ4EVR.app > Open the first time.
- A few timings and layering rules are best estimates until they are compared with LightJockey on a real rig; each is
  marked "deferred: needs rig" in `docs/fidelity.md`.
- Select Bitmap shows a list instead of large icons, and the User Libraries list has no Date column.
- The Statics 2 page layout and the background-cue file (`.bgc`) are LJ4EVR's own choices until a LightJockey sample
  shows the original.

Developer detail:

- W4 track 0 (ME-125): full VCL align with AdjustClientRect insets and the DFM tab page rect (`test_align` 0
  misplaced), group-header pairing / identifier / OS_ALLOW checks, dfm2c event names and whole blobs from the binary
  DFMs, shared scrollbar fitting, five sans13 glyphs; `widthcorpus.py` retired.
- W4 contracts (`docs/w4-contracts.md`): 102 W4 window ids in ten families with stub behaviour files, the W4 actions,
  one current-scene getter and one set-text helper (ME-129), the caption dump verb and `tools/caption_diff.py`.
- W4 docs-release: Help opens a bundled viewer (`LJ_WIN_HELP`, code-built) with 15 topics of our own text
  (`help/*.txt`, compiled in by `tools/help_check.py`; the `help_check` CTest refuses French / German text and any
  8-word run shared with Martin's help); every main-menu, desktop-popup and icon-popup Help item reaches it
  (`test_help_viewer`). Both installers carry `THIRD_PARTY_NOTICES.txt` with the full Apache-2.0, QLC+ NOTICE, miniz
  MIT and SDL3 zlib texts plus the topics (ME-36): beside `lj4evr.exe`, and inside `LJ4EVR.app/Contents/Resources` on
  macOS, where the `.dmg` now holds `LJ4EVR.app`, the Applications link and our icon. `release.yml` checks the
  silently installed NSIS tree and the mounted `.dmg`, logs `lipo -info`, `codesign -dv` and `plutil -p`, refuses a
  `vcruntime` dependency, and its notes carry the sacrifice list. About: the licence line is one constant (MIT, ME-155)
  and the provenance trademark line is shown (ME-37). New docs: `docs/user-guide.md`, `docs/provenance.md`,
  `SECURITY.md`; `.bez` and the Import / Export / Override folders in `docs/file-formats.md`.
- Release (W4 Part B, Joey's rulings of 2026-10-22): history rewritten without Martin's About credits and private
  details (ME-154, `docs/w4-integration.md`); licence MIT (ME-155); main caption `LJ_MAIN_CAPTION_FMT` = the
  `LJ4EVR (User lib: %s)` fallback and the DFM-only About golden's forum line blanked (`dfm2c.py` CAPTIONS_BLANKED,
  ME-156); every CI job on GitHub-hosted runners, the Linux jobs on `ubuntu-latest` in the same pinned container
  (ME-159); `release.yml` notes take this section.

## [0.3.0] - 2026-10-22 - W3 Show control

Cue lists, generic and movement macros, Generic DMX Control, follow spots, sequence fades, user fixture profiles and the
15/06-15/09 hand profiles (design section 10 W3; acceptance walked through in `docs/w3-integration.md`, gates in
`docs/w3-gates.md`).

- Cue list core (`src/model/lj_cuelist.c`, `src/app/lj_cuelist_run.c`): the `.Cl2` record with every 03 s6.3 command
  word (CD / DA / MP lines load red, are skipped and written back unchanged); the runner on the UI thread inside the
  10 ms loop (design 2.3 rule 4): auto and manual lines, Wait Elapsed / PC clock / date change / MTC, Goto Line, Link
  Cuelist, Flash Sequence, Hotkey Command, marks, Run to line, tracking; MTC quarter frames from `lj_midi_feed`.
- Cuelist window (`src/windows/lj_cuelist_form.c`): TCuelist2Form with its grid, the edit panel per command, the recent
  commands, Lock / Browse, marks, Save / Select / Set Cue Trig / Log TimeCode, the time-code source row, the Hotkey
  Command picker, colour codes.
- Generic macros (`src/model/lj_genmacro.c`, `src/engine/lj_genmacro.c`, `src/windows/lj_genmacro_editor.c`): rank 6
  with the cue and editor sources, the 66 regenerated shapes (`lj_shapes_gen.c` from `gen_vectors.py --shapes`), a
  user's `Macros.Mac` override, `.CMA` files, the Generic Macro Editor and its save / select dialogs.
- Movement (`src/model/lj_movemacro.c`, `src/engine/lj_movemacro.c`, `src/windows/lj_movemacro_editor.c`): the six
  system shapes and Bezier shapes as integer tables, the Movement Macro and Bezier editors, Fan Out, Auto Delay, Delete
  Macros and the Cue Macro Control.
- Generic DMX Control, sequence fades and follow spots (`src/model/lj_gdc.c`, `lj_seqfades.c`, `lj_followspot.c`,
  `src/windows/lj_gdc_form.c`): the F35 command grammar, Set Fade Status, Select / Edit FollowSpot Settings.
- `.GS2` / `.Udf` (`src/fixlib/lj_gs2.c`): import and export with the raw-record rule (design 5.3), byte-exact on the
  48 shipped files, Import / Export / Export All / Reload profile data, the RTF notes stripper, two libFuzzer targets.
- Profile editor (`src/windows/lj_profile_editor_form.c`): the Generic Fixture Profile editor (11 tabs), Customize
  Fixture, Channel Configuration with per-channel captions and Grand Master control.
- Profiles (`src/fixlib/hand/`): 15/06 Atomic 3000, 15/07 MiniMAC, 15/08 RoboColor, 15/09 MAC 550 / 575 / 700.
- Desktop and app (`src/app/`, `src/desktop/`): the W3 actions and hotkeys, the cue-list and follow-spot toolbar
  rows, pending generic-macro loads, the W3 script verbs and the walkthrough `tests/ui/scripts/w3_acceptance.lj4s` with
  its captures in `docs/screenshots/w3/`.
- Integrator: the W3 widgets (Bezier canvas, string grids), the nine W3 replays, the fuzz step in CI.

### Fix pass r1 (W3 code council, ME-107..ME-123)

- ME-107: a long field (a 64-character Execute File path, a 100-fixture follow-spot selection, a long `gs2=`) no
  longer breaks the keys after it on load.
- ME-108: a Hotkey Command line that stops, restarts or reloads its own cue list commits the line first; Cuelist Go
  from inside a line is refused instead of recursing.
- ME-109: the Cue Macro Control's Speed scales the macro rate (50 % = half as fast); `macro_ctl` takes 0..999.
- ME-110: a fixture bound to a user `.GS2` opens its own control windows; Reload profile data re-resolves open
  controls and the Generic Macro Editor before the old profiles are freed.
- ME-111: report-view and cue-grid cells clip their text at the cell edge instead of blanking it.
- ME-112: Select Background Cue (the `(T)` list, double-click loads, sbLoad latched) and BG Cue - Sequence Mode.
- ME-113: one do-nothing handler and NULL-safe INI reads (the copies deleted, counted to 0 in `test_layering.py`);
  the cue-page file layer in one file; one `lj_fs_mkdir`; the CRT directory headers allow-listed; the sACN Source Name
  cut on a UTF-8 character boundary.
- ME-114: `fuzz_udf` recomputes the footer CRC and starts from an exported `.Udf`.
- ME-115: a Freeze pauses the Cue Macro Control's fade instead of ending it.
- ME-116: the MTC time-out preference (Stop / Restart / Ignore) acts when the time code drops out at a Wait MTC.
- ME-117: deleting a generic macro stops the running one only when it is that macro.
- ME-118: Channel Configuration captions keep 6 characters (DFM MaxLength 6).
- ME-119: the Flash Sequence cell uses the host decimal separator.
- ME-120: Offset / Sort MIDI and PC time codes and Import Old Format Cuelist are greyed, with fidelity rows.
- ME-121: the ownership evidence for generic macros (delete, re-save, tick) and no cue-list symbol under `engine/`.
- ME-122: the RTF notes stripper reads at most the field's 4097 bytes.
- ME-123: this changelog, the README status, the `w3-gates.md` Status column, the replay-grammar W3 paragraph and the
  W3 fidelity rows.

### What v1.0.0 is not

Unchanged from 0.2.0 and the README: no Audio Analyzer / audio trig, no DMX-in over sACN input or `.vtr` tables, no
RS-232/TCP remote protocol, no External Application string output, no `.LJB` importer, no LJ MediaPlayer, no Art-Net.
All seven are W5; their menu items, cue-list commands (CD / DA / MP lines load red and are skipped) and toolbar buttons
stay present, greyed, hint `Not available in LJ4EVR`.

### Known gaps (W3 code council, `W3_CODE_MASTER_ELDER_VERDICT.md` sections 5-6)

- W4 track 0 (before the W4 contracts commit, ME-125): ME-43, the rest of ME-44, ME-46, ME-47, ME-53, ME-54, ME-57.
  This ends the carry.
- W4 entry: ME-126 legacy profile rename, ME-127 Movement Macro preview, ME-128 16-bit rank 6 from the 8.8 sample,
  ME-129 one scene getter and the set-text helper, ME-130 the `U:` library create, ME-131 the unwired-dialog list
  (the W4 work list, incl. the gs2 import / export sub-dialogs); ME-124 the design sentences (design owner, vault).
- ME-45 (Joey): one install of this `v0.3.0` installer on a true standard-user account. Blocks the public flip and
  `v1.0.0`.
- ME-21 (Joey): the Mac run of this `.dmg`, plus the MIDI latch on macOS. Blocks `v1.0.0`.
- Before the public flip: LORD_C-8, Execute File reaches `system()` with `&`, `|` and `;` unfiltered (off by default,
  `AllowExecute=0`).
- LOW: ListPop CD / DA / MP greyed without a hint (LORD_FIDELITY-6); `DipSwitch1` and the Notes scrollbar painted
  while `Show Adr DIPSwitch` is off (LORD_FIDELITY-7); Delete Macros pads names with a date (LORD_FIDELITY-8); the
  Function column shows class names (LORD_FIDELITY-9); the licence constant, two untagged literals, the statics2 popup
  capture and the Fan Out auto-close row (LORD_FIDELITY-10, -11); Wait Elapsed measured from the wake (LORD_ENGINE-8);
  Automatic Cue update on Run latches every earlier cue (LORD_ENGINE-9); `lj_pe_begin` on a built-in inflates the
  footprint (LORD_FIXTURES-3); three hand-profile header clauses (LORD_FIXTURES-8); an empty `cmd=` loses its pstr
  (LORD_C-7); `groups=` without `ERANGE` (LORD_C-9); no `.Cl2` line cap (LORD_C-10); W2 LORD_C-4, -5, -8;
  `file-formats.md` lacks `.bez` and the Override / Import / Export folders (WIZARD_STYLES-8); `lj_app_w3.c` rename
  (WIZARD_STYLES-12); integration s7 items; ME-66 and the earlier fidelity rows carried.

## [0.2.0] - 2026-10-22 - W2 Playback

Everything an operator uses to run a show beyond one cue (design section 10 W2; acceptance walked through in
`docs/w2-integration.md`, gates in `docs/w2-gates.md`).

- Engine (`src/engine/`): the twelve W2 command kinds; ranks 2-7 (flash, current sequence, Statics 2 with
  Add / Replace / Solo scopes and levels, the Background Cue, generic macros, Smoke); loop options and every 03 s5.5
  rule (bounce, random without repeat, limits, end actions incl. Link to Cue through `pending_link_cue`); chase
  mode on one 1 ms tap timer; cue time scaling; transparent cues; HTP groups, Global Patch and default output swaps;
  the `test_lock_hold` and `test_chase` rows.
- Replay (`tests/engine/replay/`): the nine W2 acceptance scenarios plus the 10-minute wire run
  (`replay_wire_drift`); the `determinism` job compares `replay-frames.bin` on the three OSes.
- Model (`src/model/`): cue pages, BG cues, Statics 2 pages / slots / groups (`Statics.dat`), HTP group names,
  `.GPA` patch, default output, presets, palettes, notes, generic macros; every numeric key range-checked (ME-77);
  user libraries, Delete Files listing, `.lj4b` restore into a new library.
- MIDI and hotkeys (`src/io/`): MIDI-IN on WinMM and CoreMIDI with Learn and the five tabs, posted to the UI
  thread; Edit Hotkeys with 8 setups under the library.
- Profiles (`src/fixlib/hand/`): the tier-1 hand profiles 15/02 MAC 600, 15/03 MAC 250 family, 15/04 MAC 2000
  family, 15/05 Roboscan 518 / 812 / 218 (`profiles_ids`, `exe_profiles_check` in CI).
- Windows A (`src/windows/`): Loop Options, Cue Time Control, cue pages, transparent and flash cues.
- Windows B: Background Cue with Save, Statics 2 with Statics Preferences and Static entry properties, Smoke
  Control, Position Presets.
- Windows C: Global Patch, Fixture / Intensity / HTP groups, user palettes, Snapshot (static and real-time),
  default DMX output, Fixture Configuration Export to text file.
- Windows D: System Preferences (five tabs), About, Delete Files (five kinds), User Libraries, Backup / Restore,
  Notes, Striking Lamps, Edit Hotkeys, MIDI-IN.
- Desktop and app (`src/desktop/`, `src/app/`): toolbar LEDs and gates from System Preferences, the hotkey setup
  menu, the memory gauge, pending loads serviced every idle pass, the W2 script verbs, the W2 walkthrough
  `tests/ui/scripts/w2_acceptance.lj4s` with its captures in `docs/screenshots/w2/`.

### Fix pass r1 (W2 code council, ME-88..ME-99)

- ME-88: a Statics 2 link slot pins its sequence when activated and is re-pinned when the sequence is re-saved or
  deleted (`statics_link.lj4r`).
- ME-89: the About text paints in its DFM colour and stops at the scrollbar.
- ME-90: Delete Files (and List of Sequences delete) reach the app's deleter: runners and link statics let go first.
- ME-91: Statics Preferences, the page name box and Static entry properties; the pages array never moves; the nine
  `[STATICS2]` keys.
- ME-92: Fixture Configuration Export to text file in the exe's format; the four inert boxes on their shipped keys.
- ME-93: Use Global Chase Rate is one `Main.Ini [CUE]` key, default on; the three engine menu rows post on toggle;
  the Sequence window's Trig is no chase tap.
- ME-94: the saved HTP-masters preference is posted at start.
- ME-95: a MIDI device that fails to open keeps its `DeviceCode`.
- ME-96: `test_lock_hold` runs serially, counts from the first lock, 2 ms bound in Release.
- ME-97: `Min Limit:` / `Max Limit:`, the pool's Mode-column words (`Solo Alll`), `Delete Selected Presets?`.
- ME-98: `pair16_static_bg.lj4r`, `blind_keep_contents.lj4r`; the replay-frames sha re-recorded.
- ME-99: this changelog, the README status, `docs/file-formats.md` merged, the `w2-gates.md` Status column.

### What v1.0.0 is not

Unchanged from 0.1.0 and the README: no Audio Analyzer / audio trig, no DMX-in over sACN input or `.vtr` tables, no
RS-232/TCP remote protocol, no External Application string output, no `.LJB` importer, no LJ MediaPlayer, no Art-Net.
All seven are W5; their menu items, cue-list commands and toolbar buttons stay present, greyed, hint
`Not available in LJ4EVR`.

### Known gaps (W2 code council, `W2_CODE_MASTER_ELDER_VERDICT.md` sections 5-6)

- W3 pre-fork (before the W3 contracts commit): ME-100 = ME-43 full align, ME-44 header pairing (+ `OS_HEADERS`),
  ME-46 dfm2c event-name check, ME-47 `Items.Data` from the binary DFMs, ME-53 widthcorpus, ME-54 shared scrollbar
  helpers, ME-57 five sans13 glyphs, ME-84 UTF-8 name cut; ME-101 the ME-86 orphan-API cleanup and the two
  `test_layering` counts.
- W3 entry (before `v0.3.0`): ME-102 release-then-free posts, ME-103 cue time in microseconds, ME-104 Select BG Cue
  and Sequence Mode, ME-105 Copy From / Clear Fixture Data, ME-106 one tier id list; ME-45 a non-admin install of
  this `v0.2.0` installer; ME-50 (+4) design sentences (design owner).
- ME-21 (Joey): the Mac run of this `.dmg`, plus the real-device MIDI latch on macOS (acceptance item 2's Mac half;
  Windows is ticked on a real WinMM device).
- LOW: `Statics.dat` duplicate `slot` line (LORD_C-4), `.lj4b` in-place restore path check order (LORD_C-5),
  CoreMIDI parser has no runnable check (LORD_C-6), `groups=` without `ERANGE` (LORD_C-7), more than 512
  `MIDIMap.dat` lines message (LORD_C-8), `GO` / `STOP` / `TRIG` on static runner indices (LORD_ENGINE-7), progress
  wrap after unfreeze (LORD_ENGINE-9), blank Delete Sequences `Modified` column (LORD_FIDELITY-12), About licence
  label is a PROPOSAL (LORD_FIDELITY-13), two untagged literals (LORD_FIDELITY-14), popup capture evidence
  (LORD_FIDELITY-15), placeholder LED art (LORD_FIDELITY-16), THotkeyFunctionDialog has no behaviour file,
  TSavePresetForm unused, `GroupInfo.dat` logs twice on a fresh library, windows open centred instead of at DFM
  Left/Top; ME-66 and the W1 fidelity rows carried.

## [0.1.0] - 2026-10-21 - W1 First light

The program runs a show: fixtures configured, a sequence programmed, a cue
running, DMX on the wire over sACN (design section 10 W1, acceptance 1-10
walked through in `docs/w1-integration.md`).

- Engine (`src/engine/`): the design 3.x pipeline in C11 integers, command
  array + `lj_lock`, runners for the current sequence, 12 cue slots, statics
  and the background cue; snap / fade per channel with 16-bit pairs as one
  value; post stages S1-S9 (masters, blackout, patch, pan/tilt, overwrite,
  freeze / offline); GM Fade; movement and generic macros; 16 replay
  scenarios under `tests/engine/replay/` (`replay_all`), `test_engine_perf`,
  the `determinism` frame dump (sha 5e61478e...).
- Fixture library (`src/fixlib/`): the 201 exe profile rows, tier-1 hand
  profiles (MAC 500 / 918, the generics), tier-2 QLC+ profiles through
  `tools/qxf2c.py` and `qxf_map.tsv`; `lj_layout_derive` builds every
  control-window table from the profile.
- Model + I/O (`src/model/`, `src/io/`): the user library on disk as text
  records (`Config.dat`, sequences, cues, cue lists, `DefaultOut.dat`,
  `forms.ini` / `Main.Ini` positions, hotkey setups), `.lj4b` backup /
  restore; the sACN sender (E1.31-2018, 40 packets/s cap, keep-alive,
  discovery, Stream_Terminated), `Hardware.Ini [SACN]`.
- Windows (`src/windows/`, `src/controls/`, `src/desktop/`): main window with
  the three toolbars, status bar, fixture desktop and Unassigned list;
  Sequence control, Cue control, Save Sequence / Save Cue, List of Cues,
  Master Intensity, Fixture Configuration, Hardware Setup, Startup Options,
  DMX Output Monitor, the warning boxes; the fixture controls Intensity,
  Movement, Colors, Gobos, Beam, Effects, Levels, Lamp and Extended, code-built
  from the profile tables with O/S/F buttons, drag bars, scroll controls and
  palette tiles.
- App (`src/app/`): the UI script driver (`--script`, `docs/ui-script.md`),
  15 acceptance screenshots under `docs/screenshots/w1/`, the startup status
  window, `--selftest` extended with the DMX monitor binding check.
- Tests: 55 CTest rows on three OSes (`ui_golden` 10 forms, `ui_golden_ctl`
  9 control windows, `test_desktop` MAINWINDOW), CI runs `ctest` on the MSVC
  Debug tree too.

### What v1.0.0 is not

Unchanged from 0.0.1 (below) and the README: no Audio Analyzer / audio trig,
no DMX-in over sACN input or `.vtr` tables, no RS-232/TCP remote protocol, no
External Application string output, no `.LJB` importer, no LJ MediaPlayer, no
Art-Net. All seven are W5; their menu items, cue-list commands and toolbar
buttons stay present, greyed, hint `Not available in LJ4EVR`.

### Fix pass r1 (W1 code council, ME-67..ME-75)

- ME-67: the seven code-built control windows were built to CHM captures that
  are 0.8 reductions; every constant is now x1.25 to the 1:1 anchors (O/S/F
  48x24, Intensity 600x122, Beam 257x96, Lamp 166x125, Extended boxes 346x101,
  scroll control 165x41); goldens regenerated.
- ME-69: the engine thread copies the sACN config under the lock.
- ME-70: `test_controls_a` writes `uint8_t` state (a fresh MSVC Debug `ctest`
  is 55/55); `windows-msvc` runs the Debug `ctest`.
- ME-71: the GM Fade runner keeps running while frozen and is not shifted at
  unfreeze (note 11 S9; replay `gm_fade_under_freeze`).
- ME-72: the fixture toolbar's x table measured from `toolandstatusbars.gif`
  (1:1): Lamp at 196 after the 17-px gap, Controls 248, Groups 300, All 330.
- ME-74: `OnMouseWheelDown` / `OnMouseWheelUp` are dispatched; no `unknown
  event` line at the DMX monitor open (`--selftest` asserts it).
- ME-75: the cue grid prints `1:00 | 0:05` (one `lj_fmt_time`) and `Fwd` /
  `Auto`.
- ME-68: `shot` skips a window AutoClose has already closed; every W1 screenshot
  re-taken.

### Known gaps (W1 code council; W2 entry gates unless noted)

- ME-43 full align, ME-44 header pairing, ME-46 dfm2c event-name check,
  ME-47 `Items.Data` from the binary DFMs, ME-51 skip ceiling, ME-53
  widthcorpus, ME-54 shared scrollbar helpers, ME-57 five sans13 glyphs.
- ME-45 non-admin install screenshot (needs the `v0.1.0` installer).
- ME-76 `exe_profiles_check --check` runs nowhere; ME-77 loaders cast before
  they validate; ME-78 `replay-frames.bin` is compared nowhere (the
  `determinism` job compares `dump-frames` only).
- ME-50 design.md sentences (design owner); ME-21 the Mac run (Joey).
- Carried from the design verdict for W2: the W2 rows of
  `DESIGN_COUNCIL_MASTER_ELDER_VERDICT.md` section 5 (link-to-cue, statics,
  chase, cue pages).
- LOW: ME-64 F20 test row, ME-66 `test_golden.c` loop, ME-79 O/S/F focus
  ring, ME-80 pinned-static unfreeze, ME-81
  `test_lock_hold`, ME-82 clock / log under the lock, ME-83 negative macro
  index, ME-84 UTF-8 name cut, ME-85 dead `LJ_NOOP` table, ME-86 orphan API,
  ME-87 status-bar and monitor PROPOSALs; hotkey dispatch, `lj_ctl_set_confirm`
  and the memory gauge from `docs/w1-integration.md` section 7.

## [0.0.1] - 2026-10-21 - W0 Foundation

The build skeleton: every one of the 208 LJ2 forms renders from its DFM facts
through the in-repo fonts, headless and on screen, byte-identical on three OSes.
Nothing drives a light yet.

- Font pipeline: five clean-room bitmap faces (`art/fonts/`, `fontpack`),
  advance census from the 208 DFMs (`tools/widthcorpus.py`), `font_corpus`
  0 mismatches on 792 of 826 AutoSize-label rows (the 34 rows on nine
  unpacked faces are skipped, ceiling 34).
- `tools/dfm2c.py`: DFM text -> `src/ui/forms/lj_forms_gen.c` (3632 control
  rows, 1081 menu items, 59 widget classes, 0 unknown) with `--check` for CI.
- Widgets (`src/ui/widgets/`), classic frame, menu bar, scrollbars; the
  headless renderer `lj4evr_render` writes all 208 forms plus the main window;
  six reviewed goldens under `tests/ui/golden/` with `REVIEWED.tsv`, gate
  `ui_golden` byte-exact on Windows, macOS and Linux.
- The app `lj4evr`: `Main.Ini` (`[CHROME] Zoom`, `Native`), main window with
  the generated `TLJMainForm` menu, TSEQUENCEFORM + TCURRENTCUE, `forms.ini`
  positions, engine stub thread; `--selftest`, `--save`, `--version`.
- Engine stub frame dump, `determinism` job: 1641600 bytes identical on the
  three OSes.
- Static CRT on Windows (no `VCRUNTIME140.dll` dependency).

- CMake project: C11, one static library per `src/` layer, presets for
  Windows (MSVC, Ninja+cl), macOS, Linux CI and the two release builds;
  SDL3 `release-3.4.18` by FetchContent with the MSVC 19.44 workarounds
  (`docs/msvc-sdl-build.md`).
- W0 contract headers for core, gfx, ui, forms, engine and platform
  (`docs/w0-contracts.md`); `ui/lj_widget.c` with its test.
- `tools/test_layering.py`: include graph, naming, OS-header, float/atomics
  checks and each header compiling alone, under CTest.
- `VERSION` -> `version.h`, CPack version, bundle version.
- Packaging: per-user NSIS template (no admin), macOS `Info.plist`,
  Windows UTF-8 manifest, our own icon set (`packaging/icons/`).
- CI: `docker/Dockerfile` (debian:trixie) and `docker.yml`; `ci.yml` with
  generate-check, linux-gcc-asan, linux-clang, windows-msvc, macos-clang and
  determinism; `release.yml` tag-driven Windows + macOS installers with
  `SHA256SUMS`.
- Docs: building, ci, releasing, conventions, contributing, `w0-integration`.

### What v1.0.0 is not

v1.0.0 tags at the end of W4 and ships without the Audio Analyzer and audio
trig, DMX-in over sACN input and the `.vtr` tables, the RS-232/TCP remote
protocol, External Application string output, the `.LJB` importer, the LJ
MediaPlayer and Art-Net. All seven are W5. Every menu item, cue-list command
and toolbar button that belongs to one of them stays present, greyed, with
the hint `Not available in LJ4EVR`. (design.md section 1, mirrored in the
README.)

### Known gaps (W0 code council, ME-43..ME-66; W1 entry gates unless noted)

- Align pass: DFM rects stand at creation; no per-class `AdjustClientRect`
  insets or alTop/alLeft ordering when the client resizes (ME-43).
- One `.h` per `.c` not enforced; 18 widget files share group headers (ME-44).
- No About form; version is `lj4evr --version` only; NSIS has no
  `VIProductVersion`; the non-admin install is checked once on the tag,
  the screenshot is W1 (ME-45).
- Unknown DFM event names only warn; `Items.Data` (8 list/tree views) is
  not decoded from the binary DFMs; `glyphmap.tsv` matches nothing yet
  (ME-46..ME-48).
- `dfm2c --check` relies on an uninstalled pre-commit hook; `--selftest`
  is not in CI (ME-49).
- 34 corpus rows on nine unpacked faces; five sans13 CP1252 glyphs are
  boxes; the active caption gradient end is unverified (ME-51, ME-57, ME-58).
- `setlocale(LC_ALL, "C")` makes `lj_locale_decimal()` always `'.'` (ME-52).
- Two DFM parsers (`dfm2c.py`, `widthcorpus.py`); scrollbar fitting is
  duplicated five times (ME-53, ME-54).
- Check-box text and group-box caption 1-2 px off one Windows 2000 capture,
  no Win98-era reference (ME-55).
- LOW: signed right shift in the engine stub, `lj_render_main` path bound,
  `lj_miniz` `/w` and no consumer, F20..F24 shortcut text, pref path CP1252
  conversion, three find-form loops (ME-60..ME-66).
- macOS frame, Spaces and Mission Control behaviour unchecked on a real Mac
  (ME-21, open for Joey).
