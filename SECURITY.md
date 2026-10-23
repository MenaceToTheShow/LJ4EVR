# Security policy

## Reporting a vulnerability

Please report security problems privately through GitHub's
**Security > Report a vulnerability** form on this repository (private
vulnerability reporting). Do not open a public issue for them. Include the
version (`lj4evr --version` or Help > About), the operating system, and the
smallest file or steps that show the problem.

You will get an answer within 14 days. A confirmed problem is fixed on `main`
and shipped in the next tagged release, with credit in `CHANGELOG.md` unless
you ask otherwise.

## Supported versions

Only the latest release receives fixes. Pre-1.0 tags (`v0.x`) are not supported.

## What is in scope

LJ4EVR reads files that may come from other people: show records in a user
library, `.GS2` / `.Udf` / `.Udo` fixture profiles, `.Cl2` cue lists, `.CMA`
macros, `Macros.Mac` shape files, `.lj4b` backups, restore-data archives and
bitmaps in `Source-Images`. Every length and offset in such a file is checked
before use (design rule 6); a crash, hang, out-of-bounds read or write, or path
escape from any of them is a vulnerability. The `.GS2` and `.Udf` parsers have
libFuzzer targets (`tools/fuzz/`, CMake option `LJ_FUZZ`).

On the network LJ4EVR only sends: sACN (E1.31) to multicast or to the unicast
addresses you configure. It opens no listening socket for sACN. MIDI input is read
from local devices.

## Out of scope

- The Restore Program Data password. It guards against accidents, not
  attackers, and the archive is not encrypted (`docs/file-formats.md`).
- sACN itself has no authentication: anyone on the lighting network can send or
  read DMX. Keep the lighting network separate from untrusted networks.
- The installers are not code-signed (the macOS app is ad-hoc signed only).
  Verify downloads against `SHA256SUMS` on the release page.
