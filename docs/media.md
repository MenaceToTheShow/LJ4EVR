# The LJ MediaPlayer (W5 media track)

Design 8.5, W5 gates W5-MED-1..5 and ME-11 (`docs/w5-gates.md`), seam 7 of `docs/w5-contracts.md`. LJ2 shipped the player
as a separate program (`LJMediaPlayer.exe`, a wrapper round Windows Media Player 6+); LJ4EVR builds it in. One clip
plays at a time and it is the one time-code source the cue list reads ("only one instance returns timecode", H2.100).

## What plays

- **WAV only.** SDL3 decodes WAV (`SDL_LoadWAV`) and nothing else, and W5 adds no dependency (W5-X-DEP). Any other
  extension is refused before a byte is read, with the message `Only WAV files can be played (LJ4EVR plays audio through
  SDL3, without codecs)`. A file with a `.wav` name that SDL cannot decode is refused with `Not a playable WAV file`.
- **Size cap.** A file over 64 MB (`LJ_MEDIA_FILE_MAX`, PROPOSAL) is refused before it is decoded, with
  `The file is too large to load`. The WAV is decoded inline on the UI thread, so the cap keeps that stall under a
  250 ms budget (measured on the work PC: 64 MB in 19 ms from the OS cache; a cold read adds the disk, about 160 ms
  at 400 MB/s). 64 MB is about 6 minutes of 44.1 kHz 16-bit stereo and 3.9 minutes of 48 kHz 24-bit stereo; split a
  longer show track over several MP Load File lines.
- **Local files only.** An MP Load File path is refused before any file call when it starts with two slashes or
  backslashes (a network share: opening the cue list would connect to that machine), contains a `:` other than a
  drive letter's, names a device (`CON`, `COM1`, `LPT1` ...) or does not end in `.wav`; the line loads red. A relative
  path is read from the library folder, not from wherever LJ4EVR was started (ME-169).
- **No video, ever.** SDL3 cannot decode video and no codec library is allowed, so the player is audio-only. The black
  video area of the LJ2 window is kept (empty) and every video option is greyed with the hint `Not available in LJ4EVR`:
  Startup > Display Size (Custom, FullScreen, FullScreen Monitor 1-4), Media Clip Size, Full Screen, Custom Display Size.
- **No playback files.** LJ2's `.mps` playback definitions (window position, size, full screen, loop) only describe how a
  video window looks, so the whole Playback File menu, Save / Load Playback Definition and Recent Playback Files are
  greyed. `.mps` files started as extra player instances through Execute File are not reproduced.

## The clock

The clip clock counts **sample frames the audio device has consumed**, never the wall clock: `cs = frames * 100 / rate`,
shown as `HH:MM:SS:hh` (hundredths, as LJ2's "elapsed hours:minutes:seconds:1/100 seconds"). `io/lj_media.c` is SDL-free
and integer only; `platform/sdl/lj_sdl_media.c` reports the consumed count as bytes fed to the SDL stream minus bytes
still queued, under the stream lock. The device's own buffer (one SDL period, typically 10-20 ms) is counted before it
reaches the speaker, so the clock leads the sound by that much; MP Wait times programmed by ear absorb it.

## Cue list commands (the nine MP rows)

| Command | What LJ4EVR does |
|---|---|
| MP Load File | Closes the current clip and loads the line's file, paused at 0. LJ2 waited up to 5 s; the WAV load is synchronous, so the list never waits. A file that is not there when the list loads shows the line red (load error): kept, skipped, written back unchanged. Picking a file with the `..` button clears the red. |
| MP Play | Plays from the current position. A clip that ran to its end starts again from 0 (PROPOSAL). |
| MP Seek | Moves to H:M:S:hh, clamped to the clip's length; playing or paused stays as it was. Never waits (LJ2: up to 5 s). |
| MP Stop | Stops and returns to the start ("Stop playback and set position to beginning of file"). |
| MP Pause | Pauses. |
| MP Wait | Waits until the clip clock reaches H:M:S:hh. No clip loaded = waits until Go, like Wait MTC with no time code. Ignored on a manual line, like every Wait. |
| MP Close Clip | Stops and clears the clip. |
| MP Start | Opens the LJ MediaPlayer window. |
| MP Close | Closes the window and the clip. |

**MP Wait frame bound.** The help says MP Wait takes "Frames (0-74)" while MP Seek takes "Frames (0-99)" and the
player's time code is in 1/100 s. The 0-74 text is the CD row's (75 fps) repeated: the edit panel's `UpDown28` on
`eWaitMPFrame` has `Max = 99` in the 2.209 DFM (TClEditPanels), as `UpDown32` on MP Seek does. LJ4EVR takes 0-99 for both.

`.Cl2` files: the MP words are unchanged (`mp_load_file pstr=<path>`, `mp_seek` / `mp_wait param=<ms>`), so a v1.0.0
file with MP lines now loads with them live and saves byte-for-byte as read. CD and DA lines stay red, kept, skipped and
written back unchanged (ME-11).

## The Cuelist window

- The `Mediaplayer Commands` and `LJ Mediaplayer` ListPop families, `MP Load File`, and the eight MP command-panel
  actions are live; the MP edit pages (`pCueMP`, `pSeekMPTime`, `pWaitMPTime`) write the line (fields clamped to the
  DFM's UpDown bounds).
- `sbMPTime` (Show Media Timecode) is live: it shows the clip clock in the time display and opens the player window
  (the help: "click [the media player icon] in the cue list control"). The chosen source now also reaches the runner, so
  Log time code stamps the source shown (MIDI included).
- Offset Mediaplayer Timecodes and Sort Media Player wait times work on MP Wait lines (`pMP` panel of TTimecodeOffsetForm).
- Preferences: **Stop Audio/Media Playback when Cuelist stops** sends MP Stop when the list is stopped by Stop or
  Stop and Clear (not when it runs out of lines). **Auto seek audio tracks/media file on Run**: Run from line n seeks
  the clip to the time of the last MP Wait at or above line n (PROPOSAL rig 05 s8.3: LJ2 does not say which wait).
- CD and DA rows, `sbCDTime` / `sbDATime`, and the CD / DA offset and sort items stay greyed with the hint (ME-11).

## The player window

Built from the 2.209 `LJMediaPlayer.exe` form (TLJMediaForm, read from the executable's TPF0 resource; geometry,
captions, hints, colours and menu tree as the DFM gives them; our own glyphs). Differences, each tagged in
`src/windows/lj_media_form.c`:

- LJ2's window has no caption bar (moved by dragging the video area, resized with Shift-drag). LJ4EVR frames it with a
  normal caption, `LightJockey MediaPlayer`.
- LJ2 draws File / Preferences / Playback File / Help as a 23-px tool bar; LJ4EVR draws them as the window's menu bar,
  so the client is 371 x 181 instead of 371 x 204.
- The four scan / frame buttons stay disabled as the DFM has them (Windows Media Player enabled them per format).

Kept: Play / Pause / Stop, the time code, the clip position bar (click or drag to seek), Load Media File (the OS file
dialog, WAV filter), the five recent media files, Minimum Size (Audio) and Default Size, Toggle Controls, Startup >
Show Controls, Stay on top (applied when the window next opens, as LJ2's "requires a restart"), Pass keypresses to
LightJockey (stored; in LJ4EVR the player is a window of the same program, so hotkeys reach LightJockey from it as
from any other window). Settings and the recent list are in `forms.ini [LJMEDIAPLAYER]` (`ShowControls`, `OnTop`,
`KeyPass`, `File0`..`File4`; defaults from the shipped `LJMediaPlayer.Ini`: 1, 1, 1).

## Checks

`lj4evr_tests test_media` (the clock, the refusals, the state machine, the runner sample-exact at 44.1 kHz: Wait
00:00:02:00 fires at frame 88200 and not 88199, pause and seek, Stop on list stop, Auto seek on Run, the `.Cl2` round
trip) and `lj4evr_tests test_media_form` (the window, golden `tests/ui/golden/W5MED_MEDIA_PLAYER.bmp`, the Cuelist MP rows).
`test_cuelist_run` line 5 (MP Play) now executes. The replay harness has no media host, so MP lines in `.lj4r`
scenarios stay skipped there.
