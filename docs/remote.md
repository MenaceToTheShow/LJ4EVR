# Remote control: the RS-232 / TCP 3-byte protocol

LightJockey 2 shipped a separate program, `LJ_RS232.exe`, that let another device drive the show over a serial cable.
LJ4EVR builds the same protocol in, over a serial port and over TCP. Both are off until you turn them on.

## Turning it on

Open the **RS232 Settings** window: Setup > RS-232 / TCP Remote ... (window id `remote_setup`; `lj4evr --script` can
open it with `open remote_setup`).
The top half is LJ2's window: **COM Port** (COM 1 to COM 4; on macOS and Linux the serial devices found under `/dev`)
and **Baud Rate** (2400, 4800, 9600, 14400, 19200, 38400, 57600). The format is fixed at 8 data bits, 1 stop bit, no
parity, no flow control. 14400 baud is not available on Linux; choosing it there logs a refusal.

The **Remote** group below is new in LJ4EVR:

- **RS-232 remote enabled** opens the chosen port.
- **TCP remote enabled** opens a TCP listener. **Interface** is `127.0.0.1 (local)` unless you pick the address
  of one of this computer's network interfaces. **Port** defaults to 7232 (a proposal: LJ2 had no TCP).

**OK** saves the settings to `Hardware.Ini` and reopens the remote at once. The log names every port and address it
opens.

**There is no authentication.** On `127.0.0.1` only programs on this computer can connect. On a network interface,
anyone on that network can drive the show. Pick a network interface only on a network you control.

At most 4 TCP connections are served at a time; a fifth is closed as soon as it arrives. Each connection is read with
a budget of 1024 bytes per pass of the 10 ms loop, so a flood cannot stall the program.

## The protocol

Every command is three bytes, all sent within 0.2 seconds:

    [command 128-255] [data 1, 0-127] [data 2, 0-127]

The number a command carries is `data 1 x 128 + data 2` (0 to 16383). A byte of 128 or more always starts a new
command: a half-sent one is dropped. So is a half-sent command older than 0.2 seconds. Nothing is ever sent back.

| Command | Data 1 | Data 2 | Does |
|---|---|---|---|
| 128 | number high | number low | Loads cue list n, at its top line (as a Cuelist hotkey). 0 stops the current cue list. |
| 129 | number high | number low | Loads cue n. 0 clears the current cue. |
| 130 | number high | number low | Loads background cue n. 0 clears it. |
| 131 | number high | number low | Flashes sequence n on. 0 releases the flash. |
| 132 | 0 | 0 / 1 | Smoke off / on (as the Smoke button held). |
| 132 | 1 | 0 / 1 | Smoke timer off / on. |
| 132 | 2 | 1 / 0 | Blackout / restore. |
| 132 | 3 | 0 / 1 | Fade in / fade out, with the Master Intensity fade time. |
| 133 | static 1-20 | 1 / 0 | Turns that static entry of the current Statics page on / off. |
| 134 | 1 | 1 / 2 | Go / Back on the current cue list, only while it waits on a manual line. |
| 135 | slot 1-12 | 1 / 0 | Enables / disables that cue slot. |
| 136 | slot 1-5 | 1 / 0 | Enables / disables that background cue slot. |

Example: load cue 130 = `129 1 2`.

Anything else (commands 137-255, a static, slot or data value out of range, Go / Back while the cue list is not
waiting on a manual line) is ignored. The log records ignored commands at most once a second, with a count of the
ones it skipped.

Commands 132-136 set a state rather than flip it: sending "on" to something already on does nothing.

## Testing without hardware

- From a script: `remote 129 1 2` (docs/ui-script.md) feeds the same bytes through the same code as a connection.
- Over TCP, with any tool that sends raw bytes to `127.0.0.1:7232` once TCP is enabled.
- The CTest rows: `test_remote` (every command, framing, the settings keys, loopback TCP), `fuzz_remote` (20000
  mutated byte streams), `test_remote_form` (the settings window), and on Linux and macOS `test_serial_pty` (a
  pseudo terminal standing in for the serial cable).

## Files

`Hardware.Ini` section `[REMOTE]` (ours, additive):

| Key | Default | Meaning |
|---|---|---|
| `TcpEnabled` | 0 | 1 = listen on TCP |
| `TcpIp` | 127.0.0.1 | listen address; anything that is not 127.0.0.1 or an address of an interface that is up is refused at start (0.0.0.0 never listens) |
| `TcpPort` | 7232 | 1-65535 |
| `SerialEnabled` | 0 | 1 = open the serial port |
| `SerialPort` | COM1 | `COM1`-`COM255` on Windows, a `/dev/...` terminal device elsewhere |
| `Baud` | 9600 | one of the seven rates above; anything else falls back to 9600 |

## Differences from LJ2

- LJ2's remote was a separate program with its own window (last cue list, cue, background cue and command received,
  a byte counter and a data list). LJ4EVR keeps only the settings window; the log carries the rest.
- TCP and the enable switches are LJ4EVR's.
- Go / Back is accepted while the cue list waits on a manual line; LJ2's help says "manual cue lists only" without
  saying more (PROPOSAL, to be checked on the reference rig).
