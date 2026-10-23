# Security policy

## Reporting a vulnerability

Please report security problems privately through GitHub's
**Security > Report a vulnerability** form on this repository (private
vulnerability reporting). Do not open a public issue for them. Include the
version (`lj4evr --version` or Help > About), the operating system, and the
smallest file or steps that show the problem.

If the form is not offered yet (it is switched on when the repository becomes
public), open an issue titled `security contact` with no details in it; the
maintainer answers there with a private channel.

You will get an answer within 14 days. A confirmed problem is fixed on `main`
and shipped in the next tagged release, with credit in `CHANGELOG.md` unless
you ask otherwise.

## Supported versions

Only the latest release receives fixes. Pre-1.0 tags (`v0.x`) are not supported.

## What is in scope

LJ4EVR reads files that may come from other people: show records in a user
library, `.GS2` / `.Udf` / `.Udo` fixture profiles, `.Cl2` cue lists, `.CMA`
macros, `Macros.Mac` shape files, `.lj4b` backups, restore-data archives,
bitmaps in `Source-Images`, LightJockey 2 backups (`.LJB`), DMX-in value tables
(`.vtr`), DMX-in assignments (`DmxIn.dat`), WAV files for the MediaPlayer and
External Application definition files. It also reads network and serial input:
sACN (E1.31) and Art-Net packets for DMX-in, ArtPollReply packets from Art-Net
nodes, and the remote-control bytes (TCP or RS-232). Every length, count, offset
and index in such input is checked before use (design rule 6); a crash, hang,
out-of-bounds read or write, or path escape from any of them is a vulnerability.
libFuzzer targets (`tools/fuzz/`, CMake option `LJ_FUZZ`) cover the `.GS2` and
`.Udf` parsers and the five W5 parsers: `fuzz_sacn_rx`, `fuzz_artnet`,
`fuzz_remote`, `fuzz_vtr` and `fuzz_ljb`.

A MediaPlayer path from a cue list (`MP Load File`) is refused before any file
call when it is a network path (it starts with two slashes or backslashes), names
a device or is not a `.wav` file, so opening a cue list never connects to another
machine (`docs/media.md`).

## Network listeners

Every listener is off on a fresh install, and each one logs the address it binds
when it opens.

- **UDP 5568 (sACN DMX-in).** Open only while Hardware Setup > Enable DMX in is on
  with source sACN. It binds every interface so multicast arrives; when a
  network interface (`[DMXIN] Nic`) is set, packets from outside that interface's
  subnet are dropped. The DMX-in functions act like a show remote (cue latch,
  Blackout, Grand Master, Cuelist Go), so set the interface.
- **UDP 6454 (Art-Net).** Open only while an output link uses Art-Net or DMX-in
  uses Art-Net. It binds the chosen output interface (every interface when none
  is chosen). An ArtPollReply is accepted only from that interface's subnet, and
  one address holds at most 4 subscriber rows, so a forged reply from elsewhere
  cannot make LJ4EVR send DMX to a third host. Art-Net DMX-in from outside the
  subnet is dropped too.
- **TCP 7232 (remote control).** Off by default. It listens on 127.0.0.1 unless
  you choose a network interface in Setup > RS-232 / TCP Remote, and it never
  listens on 0.0.0.0. At most 4 connections. The remote has no authentication:
  on a LAN interface anyone on that network can drive the show.
- **RS-232 remote.** Off by default; it opens only the serial port you choose.
- **External Application (TCP client).** It opens no listener. While a fixture
  of the External Application profile is patched and its link is set, it
  connects out to the host and port in the definition file (or to a serial port).

LJ4EVR also sends sACN (E1.31) to multicast or to the unicast addresses you
configure, and Art-Net DMX by unicast to subscribed nodes only. MIDI input is
read from local devices.

## Out of scope

- Cue-list Execute File lines. `Main.Ini [CUELIST2] AllowExecute` is 0 by
  default. AllowExecute=1 lets a cue list start any program through the shell; enable it only for cue lists you wrote.

- The Restore Program Data password. It guards against accidents, not
  attackers, and the archive is not encrypted (`docs/file-formats.md`).
- sACN and Art-Net have no authentication: anyone on the lighting network can send
  or read DMX, and with DMX-in on, drive the DMX-in functions. The remote has no
  authentication either. Keep the lighting network separate from untrusted networks.
- The installers are not code-signed (the macOS app is ad-hoc signed only).
  Verify downloads against `SHA256SUMS` on the release page.
