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

### `Cues\CuePageInfo.Dat` (`lj_cue.h`, kind `cuepages`)

```
LJ4 cuepages 1
page 1 name="Page 1" cues=1,5,5,9
page 2 name=Specials cues=
```

### `CueLists\NNNNNNNN.Cl2`, `CueLists\AutoSave.Cl2` (`lj_cuelist.h`, kind `cl`)

```
LJ4 cl 1
num=2
name="Show 1"
tracking=1
mark 1=2
line 1 adv=manual cmd=wait_elapsed param=5000 pstr= cue=3 trig=0 comment="first, go"
line 2 adv=auto cmd=goto_line param=1 pstr= cue=0 trig=0 comment=
line 3 adv=auto cmd=none param=0 pstr= cue=0 trig=2051 comment=
```

`cmd=` words are the 03 s6.3 commands in snake case, in `lj_cl_cmd_words[]` order
(`none` first; W3's enum follows that order). `param=` is the millisecond parameter,
`pstr=` the string one (file name, hotkey name), `trig=` the slot bit mask. Lines are
numbered 1..n in file order; `mark k=line` only when set. Validator: a line latches a cue
or trigs slots, not both (03 s6.1); marks inside 1..n.

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
page 1 name="Page 1" n=20
slot 1 name="Strobe A" type= act=flash rep=solo_group ft=300 level=80 transparent=0 groups=9223372036854775813 kind=recorded link=0,0
scene st=1000 ft=50
ch 1:4=21:F 2:8=100:S
slot 20 name=Link type= act=toggle rep=add ft=50 level=100 transparent=0 groups=0 kind=link link=12,2
```

`act` toggle|flash, `rep` add|slot|page|group|all|solo_slot|solo_page|solo_group|solo_all,
`kind` empty|recorded|link, `groups` the 64-bit group mask, `link` sequence number and
scene index. A recorded slot's scene follows its `slot` line as `scene`/`ch`/`fx` lines
(the sequence grammar). Pages are 20 slots (14 sB.8 PROPOSAL); only slots that are
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

### `DefaultOut.dat` (`lj_show.h`, kind `defaultout`)

```
LJ4 defaultout 1
ch 2:10=255 4:512=1
```

### `GroupInfo.dat` (`lj_groups.h`, kind `groups`)

```
LJ4 groups 1
group 1 name=Front members=1,2
group 7 name="SB 54 RBAW" members=
```

### `CueMacros\NNNNNNNN.CMA` (`lj_genmacro.h`, kind `macro`)

```
LJ4 macro 1
num=7
name=Sine
restart=1
ch 1:17 shape=7 bon=1 base=100 amp=45,-45 cycle=168 dir=rev delay=3
```

Per channel with a shape: shape index, base on/off and value, positive and negative
amplitude, cycle in deciseconds, `dir` fwd|rev, delay.

### The global patch file (`lj_patch.h`, kind `patch`; saved from the Global Patch dialog)

```
LJ4 patch 1
ch 1:17 src=in sl=1 sc=5 mode=htp inv=1 min=10 max=200 htp=8388609
ch 4:512 src=out sl=4 sc=511 mode=abs inv=0 min=0 max=255 htp=0
```

Only channels that differ from the default (`src=none mode=abs inv=0 min=0 max=255 htp=0`)
are written. `src` none|out|in, `mode` abs|lotp|htp|gt0|lt255. Validator: source
link:channel in range, min <= max, a channel is never patched from itself.

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
| `Hardware.Ini` | `[MIDI]` | device (W5) | - |

Sources for the LJ2 names: 07 s2, 01 s8, 11 E27, 18 s2 (design 5.1).
