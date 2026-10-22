# MIDI in

W2 (design 8.3, 2.3 rule 5; note 05 s7; inventory I2 Keep). LJ2's `LJ_MidiIn.exe` was a
separate process; here it is `io/lj_midi.*` plus the code-built MIDI-IN window
(`LJ_WIN_MIDI_IN`, windows-c). What MIDI does is LJ2's, word for word from the help: note
on / note off only, to flash sequences, latch cues, latch background cues, load and start
cue lists and run hotkey functions, per MIDI channel 1-16 or All, with Learn. One marked
extension: control change (off by default).

## Threads

```
OS MIDI thread (WinMM MIM_DATA / CoreMIDI read proc)
    -> cb(ud, msg[3])  == lj_midi_post            (lj_lock, append, never the engine / sender / model)
UI loop, every idle pass after lj_app_service_pending
    -> lj_app_midi_dispatch: lj_midi_poll -> lj_midi_dispatch(ctx->midi, msg)
    -> lj_fn_run / lj_fn_run_prog                 (UI thread only, ME-2 / ME-23)
```

`lj_midi_inject` is the same post: the UI script verb `midi 90 3C 7F` and
`tests/io/test_midi_map.c` drive the real path without a device. Shutdown:
`lj_midi_close` before the engine stops, and never under `lj_lock` (WinMM's
`midiInReset` waits for a running callback, which may be waiting for the lock).

## Backends

| File | OS | Calls | Device index |
|---|---|---|---|
| `io/lj_midi_winmm.c` | Windows | `midiInGetNumDevs`, `midiInGetDevCapsA`, `midiInOpen(CALLBACK_FUNCTION)`, `midiInStart`; close = `midiInStop`, `midiInReset`, `midiInClose` | WinMM input id |
| `io/lj_midi_coremidi.c` | macOS | `MIDIGetNumberOfSources`, `MIDIGetSource`, `kMIDIPropertyDisplayName`, `MIDIClientCreate` (once), `MIDIInputPortCreate`, `MIDIPortConnectSource`; close = disconnect + `MIDIPortDispose` | CoreMIDI source index |
| `io/lj_midi_null.c` | Linux, CI | none; `lj_midi_list` = 0, `lj_midi_open` = -1 | - |

CMake compiles one (`HEADER_FILE_ONLY` on the other two) and links `winmm` or the
CoreMIDI + CoreFoundation frameworks; `tools/test_layering.py` `OS_ALLOW` lets only these
two files include `windows.h` + `mmsystem.h` / `CoreMIDI/CoreMIDI.h`. Device names are
CP1252 (`midiInGetDevCapsA`; `CFStringGetCString(..., kCFStringEncodingWindowsLatin1)`).
The CoreMIDI read proc splits each packet into 3-byte channel messages, follows running
status, skips real-time bytes and SysEx. 2-byte messages (program change, channel
pressure) carry 0 as the third byte. The CoreMIDI file is compiled only on the macOS CI
job; it has not run against a device yet (ME-21: Joey's Mac).

## The queue rule

`lj_midi_post` ignores system messages (status `0xF0`-`0xFF`: clock, active sensing;
MTC quarter-frame `0xF1` is W3 and lands here) and drops, counting in
`lj_midi_dropped`, a malformed message (status `< 0x80`, a data byte `> 0x7F`) or one
that finds the 256-entry queue full. `lj_midi_dispatch` applies the same check, so a
malformed message is never dispatched by either entry.

## Dispatch

| Message | Kind gate | Fires |
|---|---|---|
| `9n kk vv`, vv > 0 (or Note0 off) | the kind is enabled and its channel is 16 (All) or n+1 | seq: `LJ_FN_PROG_SEQ_FLASH num` edge 1; cue: `LJ_FN_PROG_CUE num`; bgcue: `LJ_FN_PROG_BGCUE num`; cl: `LJ_FN_PROG_CUELIST num`; fn: `lj_fn_run(fn, 1)` |
| `8n kk vv`, or `9n kk 00` with Note0=1 | same | fn: `lj_fn_run(fn, 0)` (flash release). seq / cue / bgcue / cl: only with that kind's Release box set and only for the note that last fired the kind: Release Seq = `LJ_FN_PROG_SEQ_FLASH num` edge 0; Release Cue = `LJ_FN_NEW_CLEAR_CUE`; Release BGCue = `LJ_FN_CLEAR_BGCUE`; Release Cuelist = `LJ_FN_CUELIST_STOP_AND_CLEAR`. Otherwise ignored (the latch holds). |
| `Bn cc vv` | kind cc enabled + channel | the cc entry for controller cc: `lj_fn_run_prog(fn, (vv*100+63)/127, 1)` |
| anything else | - | nothing |

The map entry that fires is the exact channel first, then the All entry (`ch=0`). The
release rule follows the help (CHM `midiin.htm`): "clear the flashed sequence when a
note off command corresponding to the last sequence note on is received", "clear the
latched cue", "clear the latched background cue", "stop the latched cue list and clear
the current cue". Mapping those to the three clear functions keeps the W1 rule of
`lj_fn_run_prog` (a release reaches flash functions only) and closes the LatchOff item
the contracts finish left pending.

**CC extension** (05 s7 clone verdict: "add what LJ never had ... clearly marked as
non-original"). Kind `cc`, default off. `LJ_FN_MASTER_LEVEL` (flag `LJ_FNF_EXT`, appended
to `io/lj_actions.h`) is its first target: CC 7 (Channel Volume) mapped to it sets the
Grand Master to 0..100 %. Its handler belongs beside the other master rows
(`windows/lj_master_intensity.c` `fn_master`: `lj_master_intensity_set(0, (prog *
LJ_GM_DOMAIN + 50) / 100)`), not written by this track.

**Learn.** `lj_midi_learn_begin`; the next note on is captured (not dispatched) and
Learn ends; `lj_midi_learn_take(&ch, &note)` reports it once. Notes after it dispatch
normally, so a second key press cannot overwrite the first. The MIDI-IN window polls
`take` on its 100 ms refresh and calls `lj_midi_assign` with the current sequence / cue /
BG cue / cue list number (the help's Learn) or the dropped function.

## Files (per user library, `MIDIIn\`)

`MIDIIn.Ini`: LJ2's shipped keys, read and written verbatim (`User\Default\MIDIIn\MIDIIn.Ini`
of the 2.209 installer):

| Section | Keys | Field |
|---|---|---|
| `[OPTIONS]` | `OnTop StartMinimized AutoSave` | Stay on Top, Run Minimized, Auto Save Changes |
| `[SETUP]` | `DeviceCode Note0 FlashOff LatchOff BGLatchOff ListOff SeqOff` | device; Treat velocity = 0 as note off; Release Seq / Cue / BGCue / Cuelist on Note Off |
| `[SEQ] [CUE] [BGCUE] [CUELIST] [FUNCTIONS]` | `SeqEnable SeqChannel`, `CueEnable CueChannel`, `BGCueEnable BGCueChannel`, `CueListEnable CueListChannel`, `FunctionsEnable FunctionsChannel` | enable + channel (16 = All) |
| `[CC]` (ours) | `CCEnable CCChannel` | the extension; written only once it is used |

`SeqOff` has no box of its own in the help; `FlashOff` is taken as Release Seq (both are
1 shipped). `SeqOff` is range-checked and kept, never driven. PROPOSAL rig M-midi-1: tick
each Setup box in LJ2 and diff the file. Save updates the file in place, so `[FORM]`
`Top`/`Left` and any key we do not own survive. A missing file = the shipped defaults; a
present value out of range or not a number (`CueChannel=17`, `Note0=x`) refuses the file
(ME-77) and leaves the defaults.

`MIDIMap.dat`: the note tables, ours, design 5.2 grammar:

```
LJ4 midimap 1
map kind=cue ch=0 note=60 num=12
map kind=fn ch=2 note=61 fn=LJ_FN_BLACKOUT_RESTORE
map kind=cc ch=0 note=7 fn=LJ_FN_MASTER_LEVEL
```

`kind` seq|cue|bgcue|cl|fn|cc, `ch` 0 (All) or 1..16, `note` 0..127 (the controller for
cc), `num` 1..2^31-1 for seq/cue/bgcue/cl, `fn` an `lj_fn_table` name for fn/cc. Every
number goes through `lj_kv_range`; a bad line refuses the file. Unknown keys and lines are
skipped. LJ2's own `MIDIIn.Dat` (2562 bytes shipped: a 2-byte header then, by its size,
5 tables x 128 notes x a 4-byte number, all `FFFFFFFF` = unassigned; inferred, never
imported, design 5.2) is left alone, so a library copied from LJ2 keeps it.

## Checks

`lj4evr_tests test_midi_map` (CTest row): defaults; assign / replace / unassign / match;
`90 3C 7F` through inject -> `LJ_FN_PROG_CUE` num 1; note off ignored by a latched cue,
LatchOff clears it once, flash release for the last flash note only, Note0 both ways;
channel gates; CC 7 127 / 64 / 1 / 0 -> 100 / 50 / 1 / 0 %, other controllers and notes
ignored, nothing while cc is off; Learn takes the next note only; three malformed
messages dropped and counted, never dispatched; clock / active sensing ignored; a full
queue drops 5 of 261; the shipped `MIDIIn.Ini` loads to the defaults; a full
`MIDIIn.Ini` + `MIDIMap.dat` round trip keeps `[FORM]` and `SeqOff`; nine refused or
accepted files.
