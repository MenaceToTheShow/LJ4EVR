# LJ4EVR user guide

LJ4EVR is a lighting controller for Windows and macOS that looks and works like
Martin LightJockey 2 (the 2.209 feature set) and drives the rig over the network
with sACN (E1.31). The same text, split into topics, is in the program under
**Help** (every Help item opens it). This page adds what a new user needs before
the program is running.

LightJockey is a trademark of Martin Professional; LJ4EVR is not affiliated with or endorsed by Martin Professional or HARMAN.

## Install

| | Windows 10 (1903) or later, 64-bit | macOS 12 or later (Intel or Apple Silicon) |
|---|---|---|
| Download | `LJ4EVR-<version>-win64-setup.exe` from the GitHub release page | `LJ4EVR-<version>-macos.dmg` from the same page |
| Install | run it; it installs for your account only, asks for no administrator rights, and goes to `%LOCALAPPDATA%\Programs\LJ4EVR` with a Start menu entry | open the `.dmg` and drag **LJ4EVR.app** onto **Applications** |
| First start | SmartScreen warns about an unsigned program: **More info > Run anyway** | the app is ad-hoc signed, not notarised: right-click it > **Open**, or in Terminal `xattr -d com.apple.quarantine /Applications/LJ4EVR.app` |
| Uninstall | Settings > Apps > LJ4EVR | drag the app to the Bin |

Compare the download with `SHA256SUMS` on the release page if you want to be
sure it is the file the build produced. `THIRD_PARTY_NOTICES.txt` (the licences
of SDL3, miniz and the QLC+ fixture definitions) sits beside `lj4evr.exe` and in
`LJ4EVR.app/Contents/Resources`, next to the `help` folder.

## Where your data lives

Uninstalling does not touch your data. It is kept in the LJ4EVR data folder:

- Windows: `%APPDATA%\LJ4EVR\LJ4EVR\`
- macOS: `~/Library/Application Support/LJ4EVR/LJ4EVR/`

Inside it, `Hardware.Ini` holds the network output settings and `User\<name>\`
holds each user library: `Config.dat` (the fixtures), `Main.Ini` and `forms.ini`
(preferences and window positions), and one folder per kind of record
(`Sequences\`, `Cues\`, cue lists, background cues, macros ...). Every record is a
small text file; the layouts are in [`file-formats.md`](file-formats.md). Three
folders inside a library are yours to fill: `Import\` (fixture profiles to
import), `Export\` (profiles you export) and `Override\` (your own `Macros.Mac`).
A `Source-Images\` folder in the data folder replaces built-in pictures by file
name.

To move a show to another computer, make a backup under System > User Libraries
and restore it there; the backup is one `.lj4b` file.

## The first show

1. **Output.** Setup > Hardware Setup: pick the network card, give each link a
   universe, and choose multicast or a list of node addresses. LJ4EVR appears on
   the network as the sACN source `LJ4EVR <computer name>`.
2. **Fixtures.** Setup > Fixture Configuration: choose a profile, how many and the
   start address, add them, then Calculate addresses if you want them packed.
   Save. The new fixtures wait in View > Unassigned Fixtures; drag each onto the
   desktop.
3. **Program.** Click an icon (Ctrl adds more), open its control windows from the
   fixture toolbar and set values. Each channel row stores nothing until you
   switch it to S (snap) or F (fade).
4. **Sequence.** In the Sequence window add the scene, change values, add the next
   scene, set fade and scene times, and Save Sequence.
5. **Cue.** Add the sequence to a slot of the Cue window, Save Cue, press Go.
6. **Run the show.** Put cues, waits and links in a cue list (View > Controls >
   Cuelist Control) and run it, or map hotkeys and MIDI controls to Go, flash and
   master functions (Setup > Hotkeys, the MIDI In button on the C-L toolbar).

## Command line

`lj4evr U:<library>` starts in that user library. `--native` uses the operating
system's window frames instead of the classic ones (`[CHROME] Native=1` in
`Main.Ini`, the recommended setting on macOS for now), `--zoom 2` doubles every
window, and `--version` prints the version. The rest are developer switches,
listed in [`running.md`](running.md).

## What this version leaves out

Since 1.1.0 LJ4EVR includes the Audio Analyzer and audio triggers, DMX input over
sACN and Art-Net (with the DMX-in value tables), the RS-232 / TCP remote protocol,
string output to external applications, the `.LJB` importer, the LJ MediaPlayer
and Art-Net output; each has its page in this folder (`artnet.md`, `audio.md`,
`dmx-in.md`, `extapp.md`, `ljb-import.md`, `media.md`, `remote.md`). Still left
out, greyed with the hint "Not available in LJ4EVR": audio CD and Winamp
playback, video in the MediaPlayer (it plays WAV files only) and the Global
Patch's per-channel "Use / Ignore DMX In Enable" items. The Martin hardware that
LightJockey drove directly (2510 and 2518 controllers, MC-X, CD control, SandNet,
the USB and PCI interfaces) is replaced by network output. Every other known
difference from LightJockey 2.209 is listed with its reason in
[`fidelity.md`](fidelity.md).

Art-Net(TM) Designed by and Copyright Artistic Licence.

## Getting help

Help in the program covers each part of the main window. Bugs and questions go
to the GitHub issues of the repository; security problems go through the private
report described in [`SECURITY.md`](../SECURITY.md).
