# External Application string output

LJ2 drove a plug-in program (LJ-RS232Send) from the "Other / External Application/Device" fixture profile
(built-in 90, tag `EXTERNAL`, 32 channels). LJ4EVR keeps the profile and the per-scene command string. A built-in
serial or TCP transport replaces the plug-in, and no program is ever started (W5-EXT-1..4; note 05 s10.2; the
LJ-RS232Send "Command Syntax" topic).

## Programming a string

1. Patch an External Application/Device fixture (Fixture Configuration, manufacturer Other).
2. Make it the master fixture and press Fixture toolbar button 7. This opens the Extended window, here
   `TSpecialSettings_ExternalApp` "External Application": group "ZString Interface" with the "External Application
   Command" edit.
3. Type the string and press **Set Command**. The current scene of the current sequence now carries it, and the
   sequence is marked modified. An empty edit removes the string. **Get Command** reads the scene's string back into
   the edit. The bottom line shows the link: "Link: Active" (connected), "Link:-" (configured, not connected), or
   "Application Link Undefined" (no transport).

Each scene holds strings for up to 4 External Application fixtures (`LJ_SCENE_EXT`, PROPOSAL rig W5-EXT-4).

## The grammar (05 s10.2)

- ASCII 32..127, at most 32 characters. A longer string, or one with any other byte, is refused.
- Plain characters go out as their ASCII bytes. Trailing spaces are removed before sending.
- `#123` is one decimal byte, 0..255. `#X7F` (or `#x7f`) is one hex byte. The digits are read greedily, so `#1234`
  and `#256` are refused. A `#` with no digits is refused. `#` itself is sent as `#35`, `!` as `#33`.
- `!` ends the part that is sent. A string goes out only when it changes, so `TRIG!1` in one scene and `TRIG!2` in
  the next send `TRIG` twice.
- A refused string is logged and counted (`refused`), and nothing is sent. The edit refuses it as well.

## Linking a transport: Define Application Link

Right-click the fixture icon, choose **Define Application Link** (`TExternalAppSetupForm`, "External Application
Link"), type the path of a definition file into the field, and press OK. LJ4EVR has no file browser yet, so the folder
button only says so. The list shows the file's LJ2 `[IDENTIFIERS]` keys, as LJ2 did, plus our transport keys. OK
stores the link in Hardware.Ini `[EXTAPP]`. An empty path unlinks the fixture.

A definition file is an ini of at most 16 KB, and every value is range-checked. Add our section to an LJ2 plug-in
file, or write a new file:

```
[IDENTIFIERS]
NAME=Projector shutter
COMMANDINTERFACE=ZSTRING
[LJ4EVR]
TRANSPORT=TCP          ; or SERIAL
HOST=192.0.2.10        ; dotted IPv4 only (no name lookup, so nothing can block); TCP only
PORT=7000              ; 1..65535; TCP only
SERIALPORT=COM3        ; SERIAL only (POSIX: /dev/ttyUSB0)
BAUD=9600              ; SERIAL only, 110..921600, default 9600 (8N1 fixed)
```

A file without `[LJ4EVR] TRANSPORT` is still stored and shown, but nothing is sent. LJ-RS232Send's own
`[PORTSETUP]` keys are not read, because its BAUDRATE is an index whose table we do not have.

## Sending (W5-X-OFF, W5-X-THREAD, W5-X-ENGINE)

- Each tick, the engine copies into the published frame (`lj_frame.ext`) the string of the topmost running scene for
  every built-in profile-90 fixture. Topmost follows the merge order: cue slots, flash, current sequence, statics,
  BG slots. A scene with no string for that fixture leaves the string of the layer below. The UI loop reads its copy
  of the frame and sends from outside `engine/`.
- Nothing opens until a string for a linked fixture has to go out. Serial uses `io/lj_serial.h`. TCP is a client that
  connects to `HOST:PORT`. Every handle is non-blocking. Replies are read and dropped, so a peer close is noticed.
- Failures: a port that does not open, a connect that fails or takes longer than 2 s, a send error or a peer close
  each count once in `failed`. The first failure of an outage is logged. Failures are never fatal.
- Reconnect rule (PROPOSAL rig W5-EXT-3, `LJ_EXTAPP_RECONNECT_US` = 2 s): after a failure the link stays closed
  for 2 s. The latest unsent string is kept and goes out once the link is back. A partial write counts as a failure,
  and the whole string is resent on reconnect.
- UDP is not offered. It would need a second socket kind for no case anyone has asked for.

## Not reproduced (fidelity rows)

- **Auto-launch application / Auto-close application** stay greyed. LJ4EVR starts no plug-in program.
- **DMX Output channel 1-4 / DMX Input channel 1-4** stay greyed. LJ2 handed up to 4 DMX-out and 4 DMX-in channel
  values to the plug-in, and the bundled LJ-RS232Send disabled these too ("does not implement transport of DMX
  output/input channels"). LJ4EVR sends only the string.
- The control window has no LJ2 capture, so its geometry is PROPOSAL rig W5-EXT-1. Its captions are the exe strings.

## Checks

`test_extapp` (grammar, settings, engine copy, loopback TCP with a dead peer), `test_extapp_seq` (the `ext` word),
`test_extapp_ui` (both entries, goldens `CTL_EXTERNAL_EXTENDED`, `W5X_EXTAPP_SETUP`).
