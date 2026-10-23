# File formats: the LJ4 text records and the ini keys

Written 2026-10-21 by the W1 model track. Source: design 5.1/5.2 (the grammar), 18 s2
(LJ2's folder and file names), 03 s1-s7 (the limits the validators enforce). The
readers and writers live in `src/model/lj_*.c` over `core/lj_textrec` and `core/lj_ini`;
`tests/model/test_records.c` round-trips every grammar below, `test_library.c` the folder
and numbering rules.

## Common rules (design 5.2)

- Every record file is CP1252 text, LF lines, one record per file. Line 1 is
  `LJ4 <kind> <format>`; a file whose header does not match is rejected by
  `lj_rec_open_read`.
- Then `key=value` lines and list items `word n field=value ...`. The reader splits a line
  at the first `=` or space; fields inside a list item are `key=value` separated by single
  spaces (`model/lj_kv.h`).
- Strings are quoted when they contain a space or a quote; a quote inside is doubled:
  `name="Caf" "Bl"" X"` holds `Caf "Bl" X`. Bytes 128-255 are stored as they are in
  memory (no UTF-8 on this path).
- Times are integer milliseconds; percentages and counts are plain integers. Lists are
  comma-separated with no spaces (`members=1,2,5`).
- Channels are written as `link:addr=value:state` with state `O`, `S` or `F`, and only
  non-Off channels are written (an Off channel carries no value; 256 channels per `ch`
  line, more lines follow). `DefaultOut.dat` writes `link:addr=value` for non-zero values.
- Unknown keys are skipped, so an old build reads a new file. A line that cannot be
  parsed, an enum word that is unknown in a required place, a count that does not match,
  or a validator violation rejects the whole file: the loader returns -1, the log and
  the `err` buffer carry `<path> item N: <reason>` where N counts key/value items after
  the header (blank and `#` lines are not counted by core).
- Indices in list items are 1-based (`slot 1`, `fx 7`, `page 1`); the structs are 0-based.
- A record's number is assigned on first save and never changes; file names are the
  number as 8 digits (`00000012.seq`); the windows display 4 digits
  (`lj_number_text`: `0012`, `12345` when larger) [F28].

## Per-file grammar

### `Sequences\NNNNNNNN.seq` (`lj_sequence.h`, kind `seq`, format 1)

```
LJ4 seq 1
num=12
name="MX-4 Color Chase 1"
type=
modified=2026-10-21T19:06:02
sync=0 gst=0 gft=0
scenes=3
scene st=1000 ft=50
ch 1:1=255:F 1:2=128:S 3:11=0:S
fx 5 preset=3
fx 7 macro=1,127,127,40,40,0,0,0,512 mst=2,1,1,1,1,1
scene st=460 ft=50
ch 1:1=0:F
scene st=1000 ft=2000
```

`sync gst gft` are the three 03 s3.4 options. `scenes=` must come before the first
`scene` line and equal the number of `scene` lines (1..999). `macro=` is type, amp_pan,
amp_tilt, cycle_pan, cycle_tilt, dir, delay, tweak, step; `mst=` the six fade states.
`modified=` is written as the save time and skipped on read: `lj_sequence` has no
timestamp fields (design 4), so `created=` is not written either. Validator: scene and
fade times <= 60000, states 0..2, presets 0..99.

### `Cues\NNNNNNNN.cue` (`lj_cue.h`, kind `cue`)

```
LJ4 cue 1
num=1
name="Very Busy Cue"
type=
cuetime=100
macro=100,100 savemacro=0
transparent=0
bgcue=0 genmacro=0
slot 1 seq=12 on=1 trig=auto dir=fwd spt=1 atpt=1 tol=0 first=0 last=0 loops=0 end=stop goto=0 link=0 time=0 chase=0
slot 2 seq=7 on=1 trig=manual dir=bounce spt=1 atpt=1 tol=0 first=2 last=5 loops=3 end=out goto=0 link=0 time=250 chase=120
```

Only slots with a sequence are written. Words: `trig` auto|manual|audio, `dir`
fwd|rev|bounce|rnd, `end` stop|out|goto|link. Validator: cue time 1..999, slot time
0..999 (0 = CUE), a slot with `seq=0` cannot be `on=1`, `first <= last`, no loop count
in random mode (03 s5.5), chase <= 60000, spt/atpt >= 1.

### `Cues\CuePageInfo.Dat` (`lj_cuepage.h` over the W1 `lj_cue_pages_load/save`, kind `cuepages`)

```
LJ4 cuepages 1
page 1 name="Page 1" cues=1,5,5,9
page 2 name=Specials cues=
```

`lj_cuepages` is the in-memory set with the current tab and cursor. Ranges: `n` 1..255, cue numbers
1..99999999, at most 255 cues per page; a missing file is no pages. `lj_cuepages_add_cue` never
adds a cue already on the page (03 s5.7); duplicates on a page still load from disk.

### `CueLists\NNNNNNNN.Cl2`, `CueLists\AutoSave.Cl2` (`lj_cuelist.h`, kind `cl`)

```
LJ4 cl 1
num=2
name="Show 1"
tracking=1
mark 1=2
line 1 adv=manual cmd=wait_elapsed param=5000 pstr= ref=0 cue=3 trig=0 comment="first, go"
line 2 adv=auto cmd=flash_sequence param=2500 pstr= ref=12 cue=0 trig=0 comment=
line 3 adv=auto cmd=mp_play param=0 pstr=clip.wav ref=0 cue=0 trig=0 comment=
line 4 adv=auto cmd=none param=0 pstr= ref=0 cue=0 trig=2051 comment=
line 5 adv=auto cmd=goto_line param=1 pstr= ref=0 cue=0 trig=0 comment=loop
```

`cmd=` words are the 03 s6.3 commands in snake case, in `lj_cl_cmd_words[]` order
(`none` first; `enum lj_cl_cmd` follows that order). `param=` is the millisecond parameter,
`pstr=` the string one (file name, MTC fps), `ref=` (W3) the record a drag-only command
names, `trig=` the slot bit mask (slots 1-12, 0..4095). Lines are numbered 1..n in file
order; `mark k=line` only when set. A W1 file without `ref=` loads with `ref=0`.

Parameters per command (W3, `model/lj_cuelist.h`):

| `cmd=` | `param=` | `pstr=` / `ref=` / `comment=` |
|---|---|---|
| `wait_elapsed` | ms since the previous line executed (Min 0-99, Sec 0-59, 1/100 s) | - |
| `wait_pc_clock` | ms since local midnight, below 86 400 000 | - |
| `wait_date_change` | - | - |
| `wait_mtc` | ms of H:M:S:F (`lj_cl_hmsf_ms` at the fps) | `pstr` = `24` / `25` / `30` |
| `autoseek_mtc` | - | - |
| `goto_line` | target line 1..n; beyond the last line the list ends (RN v2.6 b7) | - |
| `execute_file` | - | `pstr` = program path, `comment` = its parameters (the "Comment/Exe Parameter" column); runs only with `Main.Ini [CUELIST2] AllowExecute=1` |
| `hotkey_command` | `enum lj_fn` number (`io/lj_actions.h`, append-only) | - |
| `flash_sequence` | duration ms, at most 99 900 | `ref` = sequence |
| `load_background_cue` | - | `ref` = BG cue |
| `link_cuelist` | - | `ref` = cue list |
| CD / DA / MP words | as read | as read |

Load-error lines (design 8.5, ME-11): a line whose word is a CD / DA / MP command, one of
the panel actions `set_current_cue` / `set_cue_trig` (they fill `cue` / `trig`, they are
never stored), or a word this build does not know, loads with `load_error = 1`: kept, shown
red, skipped at execution with a log line, and written back on save with the words it was
read with (an unknown word is held in memory as `param` = word, NUL, then `pstr`). One log
line per such line at load. The file is refused (nothing loaded, `"<path> item N: <why>"`)
for: `num` outside 0..99999999, a line out of order, `param` outside 0..86399999, `ref` or
`cue` outside 0..99999999, `trig` above 4095, a command word longer than 31 characters, a
line that latches a cue and trigs slots (03 s6.1), a Flash Sequence above 99.9 s, a mark
beyond the last line.

### `BGCues\NNNNNNNN.bgc` (`lj_bgcue.h`, kind `bgc`; extension is a PROPOSAL, 18 s2)

```
LJ4 bgc 1
num=5
name=BG
transparent=1
slot 1 seq=10 mode=auto on=1
slot 3 seq=12 mode=noloop on=1
```

`mode` auto|audio|noloop. Add to BG Cue fills the first free slot, else overwrites slot 5
(03 s7).

### `Statics.dat` (`lj_statics.h`, kind `statics`)

```
LJ4 statics 1
group 1 name=Strobes
page 1 name="Page 1" n=20
slot 1 name="Strobe A" type= act=flash rep=solo_group ft=300 level=80 transparent=0 groups=9223372036854775813 kind=recorded link=0,0
scene st=1000 ft=50
ch 1:4=21:F 2:8=100:S
slot 20 name=Link type= act=toggle rep=add ft=50 level=100 transparent=0 groups=0 kind=link link=12,2
```

`act` toggle|flash, `rep` add|slot|page|group|all|solo_slot|solo_page|solo_group|solo_all,
`kind` empty|recorded|link, `groups` the decimal 64-bit group mask (digits only), `link`
sequence number and scene index (0..998). A recorded slot's scene follows its `slot` line as
`scene`/`ch`/`fx` lines (the sequence grammar). Slot indices are 1..64 (`LJ_STATICS2_SLOTS_MAX`,
ME-30); `n=` on the page line is the Statics 2 "Visible Slots" 1..64, default 20 (PROPOSAL rig
14 sB.8 item 2), which Statics Preferences sets on every page together with `Main.Ini [STATICS2]
SlotsVisible`. `group n name=` (n 1..64, the bit of `groups=`) is written only for a name that
differs from `Group n` [14 sB.3], before the pages; `lj_statics_save` (W1) writes no group lines,
`lj_statics_groups_save` writes both. Ranges: `level` 0..100, `ft` 0..60000. Only slots that are
recorded, linked or named are written.

### `Config.dat` (`lj_show.h`, kind `config`)

```
LJ4 config 1
fx 1 kind=builtin id=12 gs2= link=1 addr=17 ipan=0 itilt=0 swap=0 ibo=0 igm=0 lsgm=0 user=MAC500 placed=1 x=120 y=80 tab=0 sub=0
fx 2 kind=gs2 id=0 gs2="My Dimmer.GS2" link=2 addr=10 ipan=0 itilt=0 swap=0 ibo=1 igm=1 lsgm=0 user= placed=0 x=0 y=0 tab=0 sub=3
sub 3 name=Wash mode=2
```

`kind` builtin|gs2 (`id` = exe row for builtin, `gs2` = the `UserFixtureDef\` file name
for gs2), `link` 1..4, `addr` 1..512, the six flags, the user name, icon placement
(`placed x y tab`) and the intensity sub-master (`sub` 0..8). `sub n name= mode=` lines
carry the eight sub-master names and modes (`enum lj_master_mode`). Validator
(`lj_config_validate`): link 1..4, address 1..512, address + footprint - 1 <= 512, no two
fixtures on one channel, builtin profiles known to `lj_fixlib_by_id`, gs2 rows name a
file. `lj_show_load` returns -1 only when `Config.dat` exists and fails this.

W3 (ME-32, Channel Configuration): after an `fx` line, `chcap n text=<caption>` (offset n 1..32, caption at most
5 bytes, quoted when it holds a space) and `chgm=1,3` (the offsets whose Grand Master control is disabled) belong to
that fixture; a `chcap` / `chgm` before any `fx`, an offset outside 1..32 or a longer caption fails the load.
Customize Fixture overrides live per fixture in `UserFixtureDef\Setup.<n>.pal` (n = fixture number, kind
`custompal`): `pal p entry=e rgb=r bmp=<name>` per overridden entry (p 1..6 = Color 1, Color 2, Gobo 1, Gobo 2,
Effects 1, Effects 2; e 1..20; `rgb` 0xRRGGBB as decimal or -1 = none); `lj_show_load` reads them, a bad file is
logged and leaves the profile's palette in force.

### `DefaultOut.dat` (`lj_show.h`, kind `defaultout`)

```
LJ4 defaultout 1
ch 2:10=255 4:512=1
```

`lj_defaultout.h` (over the W1 `lj_defaultout_load/save`). "Use Current Scene" merges every
Snap/Fade channel of the current scene over it; a scene with a preset reference or an active
movement macro is refused [01 s6.3].

### `GroupInfo.dat` (`lj_groups.h`, kind `groups`)

```
LJ4 groups 1
group 1 name=Front members=1,2
group 7 name="SB 54 RBAW" members=
htp 3 name="House warm"
```

`htp n name=` for n in 1..24 is written only for a name that differs from `HTP group n`
(PROPOSAL default) and follows the `group` lines; the W1 reader skips it. HTP membership stays in
the patch (`htp=` mask of the `.GPA` file).

### `CueMacros\NNNNNNNN.CMA` (`lj_genmacro.h`, kind `macro`)

```
LJ4 macro 1
num=7
name=Sine
restart=1
ch 1:17 shape=7 bon=1 base=100 amp=45,-45 cycle=168 dir=rev delay=3
```

One `ch` line per programmed channel (W3: `lj_genmacro_ch_used`, so shape 0 `2*Sine` is written too): shape
index, base on/off and value, positive and negative amplitude, cycle in deciseconds, `dir` fwd|rev|bounce, delay.
Ranges (the loader refuses the whole file on any field outside them, rule 6): `num` 1..9999, `name` up to 30
bytes, `restart` 0..1, `shape` 0..254 (built-ins 0..65, then the User-shapes tab), `bon` 0..1, `base` 0..255, `amp`
two integers -255..255 (required), `cycle` 0..5950 (0 = static, else 0.4..595 s), `delay` 0..8191; an unknown `dir`
word reads as `fwd` (design 5.2 skips unknown words). LSB channels of a 16-bit pair carry no line (CHM 5583).

### The global patch file `.GPA` (`lj_patch.h`, kind `patch`; saved from the Global Patch dialog)

```
LJ4 patch 1
ch 1:17 src=in sl=1 sc=5 mode=htp inv=1 min=10 max=200 htp=8388609
ch 4:512 src=out sl=4 sc=511 mode=abs inv=0 min=0 max=255 htp=0
```

Only channels that differ from the default (`src=none mode=abs inv=0 min=0 max=255 htp=0`)
are written. `src` none|out|in, `mode` abs|lotp|htp|gt0|lt255. Ranges (the loader, `lj_patch.c`):
`sl` 0..4 and `sc` 0..512 (0 only with `src=none`), `inv` 0..1, `min` / `max` 0..255, `htp`
0..16777215 (24 bits). Validator: with `src=out|in` the source is link 1..4, channel 1..512;
min <= max; a channel is never patched from itself; no mask over 24 bits. The file is the one the
user names from the Global Patch dialog's File menu (LJ2's `.GPA` extension; its binary layout is
undecoded, D5); the live patch is written at show save as `<library>\GlobalPatch.GPA` (PROPOSAL rig).

### `Presets\fixtureNNN.pre` (`lj_presets.h`, kind `presets`; NNN = fixture index 1..100)

```
LJ4 presets 1
preset 1 pan=-32768 tilt=32767
preset 99 pan=5 tilt=0
```

### `Palettes\<Control>.Pre` (`lj_palettes.h`, kind `palette`)

```
LJ4 palette 1
entry 1 name="Deep Blue"
ch 1:1=255:F 4:512=1:S
entry 2 name=two
ch 1:1=255:F
```

File names are LJ2's: `Intensity.Pre`, `Movement.Pre`, `Color.Pre`, `Gobo.Pre`,
`Beam.Pre`, `Effects.Pre`, `Levels.Pre` (`lj_palettes_file_name`; lamp, extended,
generic and RGB pack controls have none).

## The library folder (`lj_library.h`, design 5.1)

`<userdir>User\<name>\` with `Sequences`, `Cues`, `CueLists`, `CueMacros`, `BGCues`,
`UserFixtureDef`, `MIDIIn`, `Presets`, `Palettes` and `DirInfo.Ini`. `Default` cannot
be renamed or deleted and `lj_library_open_default` recreates it when the folder is
missing [01 s8]. A library name is one folder name: no `\ / : * ? " < > |`, not `.` or
`..`, under 64 bytes.

Numbering: `lj_library_next_number` reads `DirInfo.Ini [NEXT] <Kind>`, hands that number
out and stores the next one, so a deleted number is never reused, even the highest. When
the key is missing the recovery rule is max existing file + 1 (PROPOSAL, design 5.2).

Backup [F51]: `lj_library_backup` zips the folder (entries relative to the root, `/`
separators) plus `notes.txt` into `<name>.lj4b` with miniz; `lj_library_restore` unzips
into an existing library and refuses entries with `..`, a drive letter or a leading
separator.

## Ini keys (`core/lj_ini`, Delphi-style, CP1252)

| File | Section | Keys | Owner |
|---|---|---|---|
| `DirInfo.Ini` | `[INFO]` | `Name` (display name), `Date` (`M/D/YYYY`, creation) | model (`lj_library_create`) |
| `DirInfo.Ini` | `[NEXT]` | `Seq Cue Cl Bg Macro`: the next number per kind (ours) | model (`lj_library_next_number`) |
| `Main.Ini` | `[ENGINE]` | `TickHz` (ours; default 40, clamped to 10..200 at load, ME-24) | model (`lj_library_tick_hz`) |
| `Main.Ini` | `[OTHER]` | `FirstRun LastRun` | app |
| `Main.Ini` | `[PREFS]` | `ConfineToWindowsDesktop QuickMacro` | app |
| `Main.Ini` | `[MOVEMENT]` | `Res8Bit=255 Res16Bit=32` | controls |
| `Main.Ini` | `[STATICS]` | `Use Fadetime LevelAutoEnable LevelAutoDisable DefaultLevel` | windows |
| `Main.Ini` | `[GLOBALINTENSITY]` | `Fadetime Show Pct` | windows |
| `Main.Ini` | `[FIXTURES]` | `ShowExtraProfiles` (ours, 6.1) | windows |
| `forms.ini` | `[CUE]` | `Left Top Width Height C0..C8 ViewAdvanced DisableChaseFade` | windows |
| `forms.ini` | `[CUELIST2]` | `Width Height ViewEditPanel ViewEditCommand` | windows |
| `forms.ini` | `[ACUESELECT]` | `GWidth GWC0..2` | windows |
| `forms.ini` | `[SEQ]` | `Left Top` | windows |
| `forms.ini` | `[GROUPS]`, `[MOVEPRE]` | window geometry | windows |
| `forms.ini` | `[SAVECUEFORM]` | `AutoClose SaveOnEnter SaveMacro` | windows |
| `Tabs.Ini` | `[TABNAMES]` | `TAB0..` (desktop tab names) | desktop |
| `Tabs.Ini` | `[IMAGE]` | `File Stretch Center` (wallpaper) | desktop |
| `Startup.ini` (per library) | `[STARTUP]` | `AutoLampOn Seq Cue Cl PCUE SeqRun ClRun Minimized ShowNotes` | app |
| `Hardware.Ini` (installation-wide) | `[SACN]` | `Nic Name CID Priority TTL Preview UnlockRate U1..U4 Unicast1..4` (ours) | sacn (`lj_sacn_cfg_load`) |
| `Hardware.Ini` | `[ARTNET]` | `Proto1..4 Addr1..4` (ours, W5; the table below "W5 artnet") | artnet (`lj_artnet_cfg_load`) |
| `Hardware.Ini` | `[DMXIN]` | `Enabled Source Universe Nic Loss FunctionsOff` (ours, W5; meaning in `docs/dmx-in.md`) | dmxin (`lj_dmxin_cfg_load`) |
| `Hardware.Ini` | `[AUDIO]` | `Enabled Device UseFFT NoiseLevelSwitch NoiseLevelValue TrigDecay TrigAggr Gain EqualizerBand1..n ShowInput TriggerFaders SoundCard StayOnTop RunMinimized AutoSaveChanges PassKeys` (W5; LJSound's names where it had one, `docs/audio.md`) | audio (`lj_audio_cfg_load`) |
| `Hardware.Ini` | `[REMOTE]` | `TcpEnabled TcpIp TcpPort SerialEnabled SerialPort Baud` (ours, W5; `docs/remote.md`) | remote (`lj_remote_cfg_load`) |
| `Hardware.Ini` | `[EXTAPP]` | the External Application links (W5; the table below "W5 extapp") | extapp |
| `Main.Ini` | `[CUELIST2]` | `AllowExecute UseColor IgnoreFirstWait LoadCueOnDblClick StopAudio AutoCue AutoSeekAudio Autosave` (ours; `StopAudio AutoSeekAudio` W5, `docs/media.md`) | windows (cue list) |
| `forms.ini` | `[CUELIST2]` | also `TCSource` (the shown time-code source, W5) and `Cmd1..8` (the recent-command buttons) | windows (cue list) |
| `forms.ini` | `[LJMEDIAPLAYER]` | `OnTop ShowControls KeyPass File0..File4` (ours, W5; LJ2 kept them in `LJMediaPlayer.Ini`, `docs/media.md`) | windows (media) |

Files W5 adds (each read as hostile input, every length checked; the grammar is in the feature doc):

| File | Written by | Meaning |
|---|---|---|
| `<library>\DmxIn\DmxIn.dat` (kind `dmxin`) | the DMX In window's Save | the channel functions and the per-channel value tables; a function number outside its own range refuses the file (ME-172) (`docs/dmx-in.md`) |
| `<userdata>\User\ValueTables\<name>.vtr` | Value-translations > Save | one value table in LJ2's own `.vtr` text form, at most 16 KB (`docs/dmx-in.md`) |
| `<library>\LJBImport.txt` | `.LJB` import | the import report: what converted and what did not (`docs/ljb-import.md`) |

Sources for the LJ2 names: 07 s2, 01 s8, 11 E27, 18 s2 (design 5.1).

## W2 additions (contracts seat, 2026-10-21; bodies by the W2 model track)

Rules unchanged. Every numeric key of the kinds below is read through `lj_kv_range` (ME-77):
a value outside its range rejects the line instead of aliasing into range.

### `Notes.txt` (`lj_notes.h`)

Plain CP1252 text, no header, at most 64 KB (PROPOSAL rig: LJ2's own notes file is undecoded;
`System\Notes\*.rtf` are Martin's profile notes, never read).

### `Hotkeys\SetupN.Hot` (`io/lj_hotkeys.h`, kind `hot`; W1 grammar)

N = 1..8; the current setup is `Main.Ini [HOTKEYS] File=SetupN.Hot` (ours; the shipped `File=` is
empty), `Enabled` / `DialogDisable` as shipped. W2 moves the folder under the user library
(`<library>\Hotkeys\`, created by `lj_library_create` since W1) so backups carry the setups (F51).

### `MIDIIn\MIDIIn.Ini` and `MIDIIn\MIDIMap.dat` (`io/lj_midi.h`)

`MIDIIn.Ini` keeps LJ2's shipped keys verbatim (`[SETUP] DeviceCode Note0 FlashOff LatchOff
ListOff BGLatchOff SeqOff`, `[SEQ] SeqEnable SeqChannel`, `[CUE] CueEnable CueChannel`,
`[BGCUE] BGCueEnable BGCueChannel`, `[CUELIST] CueListEnable CueListChannel`, `[FUNCTIONS]
FunctionsEnable FunctionsChannel`, `[OPTIONS] OnTop StartMinimized AutoSave`; channel 16 = All
as shipped). The note tables are ours (kind `midimap`):

```
LJ4 midimap 1
map kind=seq ch=0 note=60 num=12
map kind=fn ch=1 note=61 fn=LJ_FN_BLACKOUT_RESTORE
```

`kind` seq|cue|bgcue|cl|fn, `ch` 0 (All) or 1..16, `note` 0..127, `num` the record number
(seq/cue/bgcue/cl), `fn` the `lj_fn_table` name. Validator: `num` != 0 unless `kind=fn`, `fn`
known.

### Backups `.lj4b` (`lj_library.h`, W1 format)

Unchanged. W2 reads `notes.txt` and the per-kind counts back for the Restore dialog
(`lj_library_backup_notes`, `lj_library_backup_lists`).

### Ini keys added in W2

| File | Section | Keys | Owner |
|---|---|---|---|
| `Main.Ini` | `[PREFS]` | `BitmapCache WarnSeq WarnCue WarnCueList WarnOWSeq WarnOWCue WarnOWCueList WarnBO WarnBlind AllowBlind AllowBlindSeq KeepSeqBlind AllowOffline WarnOffline CloseConfirm ViewSCBar ViewClBar ViewFixBar ViewStatusBar ExtMem ExtMem10 Primary Secondary QuickSeq QuickCue QuickCl QuickMacro MoveId MarkActiveFixtures ConfineToDesktop ConfineToWindowsDesktop SecondaryMonitor` (LJ2's shipped keys; `AllowBlind` = Allow freeze output and `WarnBlind` = Show Freeze Warning are a PROPOSAL mapping) | app (`lj_sysprefs`, `windows/lj_prefs.h`) |
| `Main.Ini` | `[SOLO]` | `Enabled` (Allow fixture solo selection) | app |
| `Main.Ini` | `[DESKTOP]` | `Restore Maximized Left Top Width Height` | app |
| `Main.Ini` | `[HOTKEYS]` | `Enabled DialogDisable File` | app |
| `Main.Ini` | `[MISC]` | `ShowHotkeyWarning ShowNoHardwareWarning SNAPSHOTSETTINGS RTSNAPSHOTPARAM RTSNAPSHOTSCENES SNAPSHOTINSETTINGSIN SANDNET` | app / windows (Snapshot) |
| `Main.Ini` | `[OFFLINE]`, `[CAPTURE]` | `Enable43`, `Enable` (inert External Links boxes, ME-31) | app |
| `Main.Ini` | `[RESTORE]` | `Password AtStartup PassOnRestore` (ours, PROPOSAL) | app |
| `Main.Ini` | `[CUE]` | `DisableChaseFade CueMacroLinked MacroScaleRange MacroFadeIndex HotkeyChangeSeqDir UseGlobalChaseRate` (the exe's; `DisableChaseFade` moves here from `forms.ini [CUE]`; `UseGlobalChaseRate` exe 0x34E59C, default 1 = `LJ_CHASE_GLOBAL_RATE`, the one store of the cue control's menu row and the app, ME-93) | windows (Cue control), app (posts `LJ_CMD_PREFS`) |
| `Main.Ini` | `[SMOKE]` | `UseLevel TimerOnStart Level Duration Cycle HazerLevel HazerFan Reset Top` | windows (Smoke Control) |
| `Main.Ini` | `[STATICS2]` | `Width Height LvlVisible ModeVisible SlotsVisible TabWidth TabHeight Multiline PRWidth` (the exe's Statics 2 keys, ME-30, ME-91): the window size, View Levels / Modes, Visible Slots 1..64 (default 20; 65 clamps to 64), the page button size (0 = Auto) and multiline, the mode + level column width (202). View Slot# has no key among them and holds for the session | windows (Statics 2, Statics Preferences) |
| `Main.Ini` | `[GLOBALPATCH]` | `HTPMasters` (Enable HTP group master intensity control, 11 P8; PROPOSAL key: LJ2's is not in the shipped Main.Ini); the app posts it at start (ME-94) | windows (Global Patch, HTP Groups), app |
| `Main.Ini` | `[STATICS]` | `Use Fadetime LevelAutoEnable LevelAutoDisable DefaultLevel` (W1 row; now read) | windows |
| `Main.Ini` | `[LOOPFORM]`, `[CUETIMEFORM]` | `Top` / `Top Expanded` | windows |
| `Main.Ini` | `[GLOBALINTENSITY]` | `Sub1..Sub8 MasterOnly SyncBump SyncFlags` (the exe's, beside the W1 keys) | windows |

## W2 model track (bodies landed 2026-10-22)

What the bodies do beyond the grammar above; every row has a check in `test_model_w2_records`
or `test_model_w2_library`.

- **ME-77 everywhere.** Every numeric key of every record kind is read through `lj_kv_range`
  (`key=value` fields) or `lj_kv_range_lead` (the leading number of a value: `num=12`,
  `cuetime=250`, `bgcue=3 genmacro=0`, `link=<seq>,<scene>`, the `group` / `htp` / `preset`
  indices). An out-of-range or non-numeric value fails the file with
  `<path> item N: number out of range`; nothing aliases into range. The W1 sites changed with it:
  `.cue` (`cuetime` 1..999, `spt`/`atpt` 1..255, `first`/`last`/`goto` 0..999, `loops` 0..65535,
  `link`/`seq` 0..99999999, `time` 0..999, `chase` 0..60000, flags 0..1) and `Config.dat` (`link`
  0..4, `addr` 0..512, `id` 0..65535, `x`/`y` int16, `tab` 0..255, `sub` 0..8). `Config.dat`
  `link=9` is now a load error, not a validator error.
- **Notes.txt.** Binary read and write: the bytes as typed, CR LF kept, no conversion; over
  64 KB - 1 loads truncated with -1.
- **`.lj4b`.** The backup's own notes are the archive entry `notes.txt` (case-sensitive) and are
  never extracted; the library's `Notes.txt` travels as an ordinary file. Restore checks every
  entry before writing the first byte: an absolute name, a `:` (drive or stream), a `..` anywhere,
  or an uncompressed total over 512 MB (PROPOSAL) refuses the whole archive.
  `lj_library_restore_new` restores into a new library named after the backup, or `name (2)` ..
  `name (99)` when taken (collision rule PROPOSAL rig: LJ2 restores over the library, 01 s8), sets
  `DirInfo.Ini [INFO] Name` to the folder name, keeps `[NEXT]`, and removes the new folder on any
  failure.
- **User libraries.** `lj_library_select` writes the installation-wide `Startup.ini
  [USERDIRECTORY] Start=<name>\` (LJ2's key and value form, 18 s2); deleting the selected library
  resets it to `Default\`. `lj_library_copy` ("copy files") refuses an existing target and sets
  the copy's `DirInfo.Ini` Name.
- **Delete Files.** `lj_library_list_records` enumerates each kind; `lj_library_used_by_cues`
  marks the sequences (slots), BG cues (`bgcue=`) and generic macros (`genmacro=`) a cue on disk
  references ("Check Used Sequences / Mark Unused Sequences", 03 s4.4). HouseLight cues (`HCues\`,
  18 s2) are undecoded and not scanned.

## W3 gdc-fades-followspot (2026-10-22)

### The Generic DMX Control command line (`model/lj_gdc.h`, F35, CHM thegenericdmxcontrol.htm)

Not a file: the keyboard grammar of TGenericMegaForm's `eCommand`, parsed by `lj_gdc_parse` into at most
`LJ_GDC_CMDS_MAX` (64) commands and run by `lj_gdc_run` against the control's selection and the current scene (seam 5 of
`docs/w3-contracts.md`: under `lj_lock`, then `LJ_CMD_CURRENT_SEQ`; no engine command). A line is at most 127 bytes;
spaces between commands are skipped. Commands run left to right, so `#1/3F@50-#2@+10` selects groups 1..3, sets Fade,
programs 50, deselects group 2 and adds 10 to what is left.

| Form | Meaning |
|---|---|
| `n` | select channel n, the selection cleared first (every unsigned term clears: `1+3+5`, `1@50 3@25`) |
| `+n` / `-n` | add / remove channel n |
| `n/m` | channels n..m (swapped when m < n); combines with `+` / `-`: `1/10-4/6+12` = 1,2,3,7,8,9,10,12 |
| `#g`, `+#g`, `-#g`, `#g/h` | groups (1..32); `.` and `,` stand for `#`; `#1/#5` is an input error |
| `##` | every mapped channel |
| `@v`, `@+v`, `@-v` | level / relative level of the selection; `*` stands for `@`; clamped to 0..255 |
| `@@v` | every mapped channel to v (the selection is unchanged) |
| `o O s S f F` | fade state Off / Snap / Fade of the selection |
| empty line | clears the selection when "Empty Command Clears Selection" is on |

Channel n = address code + 1 (1..2048; link 2 starts at 513), PROPOSAL rig F35. Only channels of the Generic DMX Dimmer
and DMX Switch Pack profiles are mapped, minus the hidden ones; selecting anything else is silently ignored. Levels are %
(0..100, nearest to 0..255; above 100 clamps) with "Use % Values", else 8-bit. A level on a channel makes it Fade unless it
is Snap already. Numbers are at most 4 digits. Errors: `input error at column <n>` (1-based, the first byte that does not
fit; LJ2 has no message string for it, PROPOSAL), also for a line over 127 bytes or more than 64 commands.

### `GenericGroups.dat` (`lj_gdc.h`, kind `gdcgroups`; library root, LJ2's own name)

```
LJ4 gdcgroups 1
group 1 name="Front wash"
ch 1:5 groups=3          # bit g-1 = member of group g (decimal mask)
hide 1:7                 # Hide Selected Channels
```

A missing file is no groups. Loaded when the control first opens, saved after every group or hide edit.

### `FollowSpot.dat` (`lj_followspot.h`, kind `follow`; library root, PROPOSAL name; LJ2's `Follow2.Dat` is not imported)

```
LJ4 follow 1
spot 1 used=1 name="Guitar Solo 1" sel=2,5 static=1.3 autoclr=1
ch 1:22=200:F 1:31=128:F
```

`spot n` (1..20) starts a setting; the `ch` lines after it are its recorded channels (non-Off only). `static=p.s` is
the associated Statics 2 entry (page and slot 1..64; absent = none). A missing file is 20 unused settings; every load
is checked with `lj_followspot_validate`. Ini: `Main.Ini [MISC] FollowModePreSelected` = "Use only selected fixtures as
followspots" (key verbatim from the exe, section PROPOSAL). Forms.ini `[GENERIC]` keeps LJ2's keys verbatim for the
Generic DMX Control: `CellWidth CellHight GridBackColor ChanelColor SelectedColor OffColor SnapColor FadeColor
LockCursorKeys EmptyClear AutoScroll ShowCommandLine PctValues ViewGroups ViewLevelFader` (Martin's typos kept).

## W3 gs2: `.GS2`, `.Udf`, `.Udo` (Martin's formats, read and written natively; note 16, oracle `tools/ref/16-gs2_ref.py`)

### `UserFixtureDef\<name>.GS2` (`fixlib/lj_gs2.h`)

One 20,851-byte Delphi record, byte-packed, little-endian, `ShortString[n]` = length byte + n chars with stale tails.
The field table is the oracle's `LAYOUT` (16 s4); `lj_gs2.c` `lj_gs2_xfer` walks the struct-owned subset in file order,
one line per field. Raw-record rule (design 5.3, ME-9): `lj_gs2_write(lj_gs2_parse(x)) == x` for every record, so
reserved blocks, `const04`, `legacy_name` (2609 records), `source_file`, the three OV strings, `rgb_pad`, sub-record
reserved tails, the 22 stray palette bytes and the 17 bytes at 0x266C travel untouched. Messages: a wrong size is
`not a GS2 record: N bytes` (verbatim oracle). `gs2_dump <file>` (`lj4evr_tests`) prints the parsed fields one per line
and `roundtrip ok`.

### `.Udf` / `.Udo` export container (16 s2, re-measured on `udf200`)

```
zlib stream 0      the .GS2 record (20,851 bytes inflated)
zlib streams 1..   icon / palette bitmaps (BM), .Udo also the OV .spt files      (never read by LJ4EVR)
zlib stream last   the file table: one 532-byte entry per file
36-byte footer     u32 crc32(everything before the footer), u32 file count, u32 1,
                   u32 compressed size of the table stream, u32 compressed size of the data streams, u32 1, 12 x 0
```

File-table entry (532 bytes; note 16 s2 says 266, which is one of its two string halves): +0 `ShortString[255]` file
name, +256 `ShortString[255]` destination folder (`User\UserFixtures\`, `Source-Images\Fixtures\`), +512 u32
uncompressed size, +516 u32 compressed size, +520 u32 offset of the stream in the container, +524 u32 crc32 of the
uncompressed file, +528 u32 0x200 (constant in the sample, meaning unknown).

Import (`lj_udf_parse`): the footer CRC is verified first (a mismatch, or a file shorter than the footer, is
`footer CRC mismatch`, the oracle's text), then miniz `tinfl` inflates stream 0 only (zlib header and Adler-32
checked; output beyond 16 MB aborts), then the size check of the record. Export (`lj_udf_export`): stream 0 =
`lj_gs2_write`, a one-entry table (`gs2_file`, `User\UserFixtures\`), the footer; no bitmap stream (D2: the icon
bitmap is the user's file).

## W4 docs-release (WIZARD_STYLES-8): `.bez` shapes and the library's own folders

### `MacroShapes\<name>.bez` (`model/lj_movemacro.h`, kind `bez`, format 1)

One user Bezier shape for movement macros (04 s4.5, 17 s7.2), written by the
Bezier editor, read at startup and on reload (`lj_app_bez_reload`). The folder
name `MacroShapes` is a PROPOSAL (LJ2 lists `.bez` user shapes but its folder is
not recorded). The file name without `.bez` is the name the macro drop-down shows.

```
LJ4 bez 1
name="Figure 8"
node 1 x=-16000 y=0 ix=-16000 iy=-9000 ox=-16000 oy=9000 line=0
node 2 x=0 y=0 ix=-8000 iy=9000 ox=8000 oy=-9000 line=0
node 3 x=16000 y=0 ix=16000 iy=-9000 ox=16000 oy=9000 line=1
```

- `name` up to 30 characters, required.
- 3 to 64 `node` items (`LJ_BEZ_NODES_MAX`, PROPOSAL); the shape is closed (the
  last node joins the first). `x` / `y` is the node, `ix iy` and `ox oy` its
  incoming and outgoing handles, all in editor units -32768..32767 (the full pan /
  tilt span, 17 s1); a missing handle defaults to the node itself. `line=1` makes
  the segment leaving that node straight ("convert segments to lines").
- Out-of-range values, a 65th node, fewer than 3 nodes or a missing name reject the
  file; `lj_bez_set_load` logs and skips a bad file and loads the rest.
- Sampling (`lj_bez_sample`, integer only): each segment gets `4096 / n` points
  (the last one the remainder), cubic Bernstein form in exact 64-bit arithmetic,
  then each axis is normalised to +-32767 around its centre.

### The folders a user fills

All inside the user library (`User\<library>\`; `Export\` is created on the first export). Their
names are PROPOSALs (LJ2 asked with file dialogs or read from the installation),
each recorded in `docs/fidelity.md`.

| Folder | Read / written by | Contents |
|---|---|---|
| `Import\` | Fixture Configuration > Import Profile (`windows/lj_gs2_dialogs.c`) | `.GS2`, `.Udf`, `.Udo` profiles to import; listed in the import dialog, each file untrusted (every length and offset checked, `fixlib/lj_gs2.c`) |
| `Export\` | Create Export File / Export All (`windows/lj_gs2_dialogs.c`) | the `.GS2` / `.Udf` files LJ4EVR writes |
| `Override\` | startup and Generic Macro Editor > Options > Update User Shapes (`app/lj_app_cuelist_host.c`, ME-34) | a user's own `Macros.Mac` (+ `UserMacros.Mac` beside it) that replaces the 66 built-in generic macro shapes; nothing is shipped there |

`Source-Images\` is not in the library: it sits in the data folder (the pref path)
with `Glyphs\`, `Fixtures\`, `Colors\` and `Gobos\` sub-folders of `.bmp` files
that replace our pictures by LJ2 file name (design 7.8, the art track's
`platform/sdl/lj_sdl_srcimg.c`).

## W4 lightcorder-debug: `LightCorderFiles\LCnnnn.LLF` (`model/lj_llf.h`; PROPOSAL format)

LJ2 records a DMX link to `.LLF` files in a `LightCorderFiles` folder (exe 0x1FE558, the TSaveDialog filter
`LightJockey LightCorder Files (*.LLF)`), but no Martin `.LLF` file or format description is known (note 13 s1.3), so
the bytes are ours. The recorder (Cuelist > Tools > Record to LightCorder file) samples our own output frame
(`lj_engine_frame()->out[link - 1]`, copied under `lj_lock` on the UI thread) at 10 / 20 / 40 Hz into an in-memory
"card" of 4 / 8 / 16 / 32 / 64 MB, then Save File writes the whole take as `<library>\LightCorderFiles\LCnnnn.LLF`
(first free nnnn; no platform save dialog yet). Little-endian:

```
offset size
 0     6   "LJ4LLF"
 6     2   version = 1
 8     1   link 1..4
 9     1   flags: bit 0 = compression ("Target File Compression" Enable); other bits 0
10     2   channels: 64 | 128 | 256 | 512 (the first N addresses of the link)
12     2   frequency Hz: 10 | 20 | 40
14     2   0
16     4   card bytes: 4, 8, 16, 32 or 64 MiB
20     4   frame count
24     4   data bytes (<= card bytes)
28     4   user notes bytes (< 65536)
32     4   system notes bytes (< 65536)
36     4   0
40         user notes, then system notes (CP1252, no NUL)
...        frames: 0x00 + <channels> bytes = a full frame;
                   0x01 = the previous frame again (only with compression, never first)
```

The card cap counts encoded bytes: a push that does not fit stops the take with `Lightcorder card memory full -
recording stopped` (exe 0x1FF970). `Max recording time` (exe 0x1FF0DC / 0x1FF0A8) is the uncompressed worst case,
card / ((channels + 1) x Hz). The system notes are the exe's text: `LightJockey Generated Lightcorder DMX Stream`,
`File Created <dd-mm-yyyy> at <hh:mm:ss>`, `Recording Time : m:ss`, `Recorded number of channels : %d`,
`Recording Frequency : %d Hz`, then `LightJockey Link Configuration` and one `%2d. %s (%s) at DMX address %d` line per
fixture of the recorded link inside the recorded channels (number, user name, profile name, address).

Reader (`lj_llf_read` / `lj_llf_parse`), every check before use: magic, version, link, channels, Hz, card size from the
lists above; both notes lengths and the data length bounded; header + notes + data must equal the file size exactly
(shorter = `truncated`, longer = `trailing bytes`); each frame tag valid, a full frame complete, a repeat only with
compression and never first; the frame count must consume the data exactly; frames x channels capped at 256 MiB
decoded. `tests/model/test_llf.c` refuses every prefix of a good file and the hostile variants.

Ini key: `Main.Ini [DEBUG] DebugWindow=1` shows the hidden View > De&bugWindow item (PROPOSAL, the unhide of design 13
item 10; `[DEBUG] MonitorLayer` is the DMX monitor's).

## W4 dlg-files (2026-10-22)

### Restore data `<userdir>RestoreData.lj4d` (`model/lj_restore_data.h`, W4-DLG-2; ours, not LJ2's `LightJockey.Bin`)

The club "show-saver" snapshot of `[06 s4.2]`. One file per installation, beside `Hardware.Ini`. The container
is a `.lj4b` (the zip of `lj_library_backup`, entries relative to the library root, `/` separators) whose
`notes.txt` entry is the manifest:

```
LJ4 restore 1
library="Club Night"
created=2026-10-22T21:04:40
```

- `library=` is quoted like a record string and must be a valid folder name (no `\ / : * ? " < > |`, not `.` or
  `..`); `created=` is local time, information only. A manifest with a format above 1, or none, is not restore data.
- Create (System Preferences, Data Restore tab, Create Restore Data, Continue) writes `RestoreData.lj4d.tmp` and
  renames it over the old file, so a failed create keeps the previous data set.
- Restore (System, Restore Program Data, `Restart & Restore`) sets `Startup.ini [RESTORELIB] RESTORE=1` (LJ2's key)
  and restarts. The next start (`lj_startup_library`, before `Main.Ini` is read) runs `lj_library_replace`: pass 1
  over every entry and every CRC, then the library is erased, then extracted; `RESTORE=0`; `Start=` names the
  restored library. When pass 1 fails nothing is erased, `RESTORE=1` stays, and the first window pass asks
  `Restore data file seems to be corrupt - do you wish to abort the data restore process ?`; Yes deletes the file.
- `Hardware.Ini` and `Startup.ini` are not in the snapshot (LJ2 also stored `Hardware.Ini`, `LJSound.Ini` and
  `Startup.ini`): the data set restores show data, not this PC's network settings.
- **The password is a prompt, not a security boundary.** `Startup.ini [RESTORELIB] Pass=` holds the 32-bit FNV-1a
  of the password's CP1252 bytes as 8 hex digits (empty = none). It only hides the text: anyone who can edit
  `Startup.ini` clears it, and the archive itself is not encrypted. `Main.Ini [RESTORE] Password=1` (the
  `Password protect restore file` box) makes the Data Restore tab ask for it before Create Restore Data.

### Untrusted archives (`.lj4b` and restore data; W2 LORD_C-5, W4-DLG-1)

`lj_library_backup_check` is pass 1 of every restore and writes nothing. It refuses the whole archive when an
entry name is empty, longer than 511 bytes or holds a NUL (miniz's stat would truncate it), starts with `/` or
`\`, contains `:` or `..`, is a file entry ending in `/` or `\`, or makes `root + name` reach `LJ_PATH_CAP`
(1024); when the uncompressed total passes 512 MiB; and when any entry fails `mz_zip_validate_archive` (a
truncated or bit-flipped file). Restore User Library and Data Restore then erase the library before extracting
(`lj_library_replace`, "restoring will completely replace the existing contents" [06 s4.1]); an archive that lies
inside the library it would replace is refused.

### Record format numbers and the update dialogs (F56, W4-DLG-4)

Every LJ4 record kind writes format 1 (`LJ_REC_FORMAT`). The loaders read any format up to their own, so the
update rule is about line 1 only: at start (`lj_file_dlg_startup`, once a window host exists) every file of the
library whose line 1 is `LJ4 <kind> <n>` with `n < LJ_REC_FORMAT` is collected, and

| kind | dialog | action |
|---|---|---|
| `cl` | TUpdateCl2Form `Datafile format update required` | `Ok, do the update now`; `Don't update now, but remindme later` (asked again next start); `Don't update - don't remind me` (`Main.Ini [UPDATE] NoCl2Reminder=1`, ours) |
| `bgc` | TUpdateBGCuesForm `File Format Update Needed` | `Continue Update` / `Cancel Update` |
| `patch` (`GlobalPatch.GPA`) | TPatchUpdateForm `Update to Patch Configuration` | `Update Now` / Cancel |
| any other kind | TUpdateForm `Dataformat has Changed` | OK after `Backup Datafiles first` (`<userdir><library> (before update).lj4b`); Cancel asks `Do NOT update files - are you absolutely sure ?` |

The update rewrites line 1 as `LJ4 <kind> 1` and keeps every other byte; it cannot be undone, and the next start
shows nothing. No LJ4 record has ever been written below format 1, so in v1.0.0 only a synthetic
`LJ4 <kind> 0` file (the tests write them) reaches these dialogs; the first real format change raises the kind's
format and the dialog is already wired. TUpdateProgramForm (installation self-update) and TRebuildSetupForm
(fixture palette rebuild) have no LJ4EVR trigger; they open only through `lj_win_open`.

### Ini keys added by dlg-files

| File | Section | Key | Meaning |
|---|---|---|---|
| `Startup.ini` (userdir) | `[RESTORELIB]` | `RESTORE` | `1` = restore the data set at the next start (LJ2's key) |
| `Startup.ini` (userdir) | `[RESTORELIB]` | `Pass` | FNV-1a of the restore password, 8 hex digits; empty = none (ours) |
| `Main.Ini` | `[UPDATE]` | `NoCl2Reminder` | `1` = `Don't update - don't remind me` was chosen (ours) |

## W4 settings files (ME-151, WIZARD_STYLES-7)

All four are ours (INI grammar of `core/lj_ini`, CP1252, CRLF) and live in the user library. LJ2 kept the same data
in binary or undocumented files that LJ4EVR does not read. Each is untrusted on load: a bad value refuses the whole
file (logged), and the program keeps its defaults, except `RGBPalette.Ini`, which skips a bad entry.

### `Fingers.Ini` (`windows/lj_fingers.h`; LJ2's file name, our keys, PROPOSAL rig 05 s5)

| Section | Key | Value | Range |
|---|---|---|---|
| `[SETUP]` | `Mode` | `0` disabled, `1` Virtual Fingers (the serial box is never selected) | 0..1 |
| `[SETUP]` | `FlickerThreshold` | fader filter of Fingers Setup, default 4 | 2..4 |
| `[FUNCTIONS]` | `S<s>F<k>` | F key `k` (1..10) on shift level `s` (0..3): `fn <function name>` (a hotkey function of `docs/hotkeys.md`, not a Programs row), or `seq` / `cue` / `cl` / `bg` with a record number; empty = unassigned | numbers 1..99999999 |
| `[SLOTS]` | `Row1`..`Row3` | index into the slot-function list (`lj_fingers_slot_functions`, Slot On/Off = 0) | 0..19 |
| `[FADERS]` | `PageA`..`PageD` | index into the page-function list (`lj_fingers_page_functions`) | 0..last |
| `[JOGS]` | `Jog1`..`Jog5` | index into the jog-function list | 0..last |
| `[SCROLL]` | `Set` | index into the up/down-function list | 0..last |
| `[PREFS]` | the Fingers Setup check-box names (`cbShiftLatch` ...) | `0` / `1` | |
| `[VIRTUAL]` | `OnTop`, `FaderHints`, `MouseWheel`, `WheelHints`, `HotZones` | `0` / `1` | |

### `2532Setup.Ini` (`windows/lj_da2532.h`; ours, LJ2's `2532_2.Cfg` is binary and not read)

| Section | Key | Value |
|---|---|---|
| `[SETUP]` | `Version` | `1` (any other value refuses the file) |
| `[SETUP]` | `Page`, `Bank` | the page (1..18) and bank (1..7) shown at the next open |
| `[PAGE1]`..`[PAGE18]` | `Name` | the page tab name, at most 20 characters |
| `[PAGEn]` | `<bank><key>L` (`A1L`..`G8L`) | a latch key: `fn <function name>`, `cue <n>`, `cl <n>` or `bg <n>` |
| `[PAGEn]` | `<bank><key>F` (`A1F`..`G8F`) | a flash key: `fn <function name>` or `seq <n>` |
| `[PAGEn]` | the key name + `T` (`A1LT`) | the key's text, at most 20 characters |
| `[USER]` | `U1`..`U18` | a user key: `fn <function name>` only; `U<n>T` its text |

Record numbers are 1..99999999. Programs rows of the function table are refused in a `fn` value. The window's
preferences (`TabWidth`, `TabHeight`, `AutoWidth`, `AutoHeight`, `M_ClickAutoFlash`, `M_TabColorEnable`) are in
`Main.Ini [2532]`.

### `RGBPalette.Ini` (`windows/lj_config_dialogs.c`, TRGBPaletteForm; PROPOSAL file, 04 s5.3)

    [RGBPALETTE]
    Entry0=GENERIC|Red|FF0000

`Entry<i>` counts up from 0 and the first missing number ends the list (at most 192 entries). The value is
`<type>|<name>|RRGGBB`: type and name at most 31 characters each, the colour exactly six hex digits. A malformed
entry is skipped. With no file, or no valid entry, the palette starts from the built-in GENERIC colours and the
first save writes the file.

### `Main.Ini [CUSTOMSEL]` (`desktop/lj_selection.h`; Fan Out's saved custom selections)

| Key | Value |
|---|---|
| `Count` | the number of saved selections, 0..99 (outside that range it is clamped) |
| `Name<n>` | selection `n` (1..Count): its name, at most 25 characters |
| `Fix<n>` | its fixture order, comma-separated fixture numbers 1..100 (`3,7,12`); anything else in the list is skipped |

Clear All sets `Count=0`; the old `Name` / `Fix` keys stay and are overwritten by the next save.

## W5 extapp (2026-10-23)

### The `ext` word of a scene in `Sequences\NNNNNNNN.seq` (format 1, additive)

```
scene st=1000 ft=50
ch 2:1=0:S
ext 5 cmd=TRIG!1
ext 7 cmd="say ""hi"" #13"
```

`ext <fixture> cmd=<string>` follows the scene's `ch` / `fx` lines. There is one line per External Application/Device
fixture (built-in profile 90) that has a command string in that scene, at most 4 (`LJ_SCENE_EXT`). `fixture` is
1..100 and appears once per scene. `cmd` is 1..32 characters, ASCII 32..127, quoted by the common rule. It is the raw
string; the `#` / `!` grammar is applied at send time (docs/extapp.md). Absent = no string, so a v1.0.0 file loads
unchanged. An older build skips the key. A bad `ext` line rejects the file like any other bad line.

### Hardware.Ini `[EXTAPP]` (ours)

Per link slot n = 1..4: `Fixture<n>` (1..100, 0 = unused; a fixture named twice keeps the first slot),
`Transport<n>` (0 off, 1 serial, 2 TCP), `SerialPort<n>`, `Baud<n>` (110..921600, else 9600), `Host<n>` (dotted
IPv4), `Port<n>` (1..65535), `Definition<n>` (the definition file path shown by TExternalAppSetupForm). A value out of
range turns that slot's transport off. A fresh Hardware.Ini has every slot off (W5-X-OFF).

### Definition files (`[LJ4EVR]` section, ours)

`TRANSPORT=SERIAL|TCP`, `SERIALPORT`, `BAUD`, `HOST`, `PORT`, in an ini of at most 16 KB. LJ2's `[IDENTIFIERS]` keys
are shown and never acted on (docs/extapp.md).

## W5 artnet (2026-10-23)

### `Hardware.Ini [ARTNET]` (`io/lj_artnet.h`, `lj_artnet_cfg_load / save`; ours, additive, installation-wide)

| Key | Value |
|---|---|
| `Proto1`..`Proto4` | the protocol of link n: `0` sACN (also the value of a missing key or any other number), `1` Art-Net |
| `Addr1`..`Addr4` | the Art-Net Port-Address of link n, 1..32767 (15 bits: Net 0-127, Sub-Net 0-15, Universe 0-15); missing or out of range = n (PROPOSAL rig W5-ART-4) |

`In` is no longer a key (ME-178): the Art-Net input Port-Address is derived at run time from `[DMXIN]` (enabled, source
Art-Net, its universe) and never written; an older file's `In` is ignored, so it can no longer bind UDP 6454 with
DMX-in off.

A file from v1.0.0 has no `[ARTNET]` section and loads as sACN on every link. The Hardware Setup OK button writes all
eight keys. Two Art-Net links with one Port-Address, or Port-Address 0 on an Art-Net link, are refused by the page
(`lj_artnet_cfg_validate`); a hand-edited file with either keeps the offending link silent and logs it.
