# Running lj4evr (W0)

What the three executables do, their switches, and how the W0 screenshots were
taken. Build first: `cmake --preset dev-msvc && cmake --build --preset dev-msvc`
(Windows) or `cmake --preset dev-mac && cmake --build --preset dev-mac`; the
MSVC-specific SDL workarounds are in `docs/msvc-sdl-build.md`.

## lj4evr (the app)

`build-msvc/Release/lj4evr.exe`. W1 startup ([01 s6.2], `src/app/lj_app.h`):
pref path -> `Startup.ini` / `U:<folder>` -> the user library (`Default`
recreated when missing) -> `Main.Ini` + `forms.ini` from the library,
`Hardware.Ini` from the pref path (sACN config, CID generated once) -> the main
window with the caption `Please Wait..` and the startup status window while
`Config.dat`, `DefaultOut.dat`, groups, statics and presets load -> engine bound,
`LJ_CMD_CURRENT_SEQ` + `LJ_CMD_SACN_CFG` posted -> caption
`LJ4EVR (User lib: Default)` -> Sequence and Cue windows at their
`forms.ini` positions -> `Main.Ini [STARTUP]` actions -> engine thread at
`[ENGINE] TickHz` (clamped 10..200) -> the UI loop (`SDL_WaitEventTimeout(10)`;
every pass services the engine's pending loads and the script driver, every
100 ms repaints the engine-mirroring windows). Closing the main window asks
`Close Lightjockey ?`; yes stops and joins the engine thread, closes the sACN
sender, closes every window with its position saved, frees the show and writes
`forms.ini`, `Main.Ini` (`[OTHER] LastRun=LJ4EVR x.y.z`) and `Hardware.Ini`.

Files: `SDL_GetPrefPath("LJ4EVR", "LJ4EVR")` is the installation folder
(`%APPDATA%\LJ4EVR\LJ4EVR\` on Windows, `~/Library/Application Support/LJ4EVR/LJ4EVR/`
on macOS; kept as UTF-8 bytes, ME-65) holding `Startup.ini`, `Hardware.Ini` and
`User\<library>\` with `Main.Ini`, `forms.ini`, `Config.dat`, `Sequences\`, `Cues\` ...

```ini
; Main.Ini (per library)
[CHROME]
Zoom=1        ; 2 = every window doubled, fat pixels (SDL_BlitSurfaceScaled NEAREST)
Native=0      ; 1 = SDL's bordered windows instead of the self-drawn classic frame (the documented macOS default until ME-21 is ticked)
[ENGINE]
TickHz=40     ; clamped to 10..200 (ME-24)
[STARTUP]
Seq=0         ; sequence loaded at startup; SeqRun=1 starts it
Cue=0         ; cue loaded at startup
PCUE=0        ; background cue (PROPOSAL: LJ2's key meaning is UNVERIFIED)
Cl=0          ; cue list (W3), ClRun
AutoLampOn=0  ; minutes (W2)
Minimized=0   ; Run Minimized
ShowNotes=0   ; W2
[OTHER]
LastRun=LJ4EVR 0.1.0
; Startup.ini (installation)
[USERDIRECTORY]
Start=Default\
```

| Switch | Effect |
|---|---|
| `--ms <n>` | quit after n milliseconds (what `tools/shot.ps1` uses) |
| `--save <file.bmp>` | write the main window surface (frame + menu bar + client) as a 24-bit BMP |
| `--selftest [ref.bmp]` | prints the startup line (`Please Wait..` caption and the status window seen, LORD_FIDELITY-20), then paints `TSEQUENCEFORM` on screen, reads the OS window surface back and compares it with the image `lj4evr_render` paints for the same form (or with `ref.bmp`); W2 adds the MIDI line (`selftest MIDI-IN: <n> input device(s) listed, injected 90 3C 7F polled back: OK`: the OS backend lists its inputs and the inject path runs through the UI-thread queue); lines end `OK` or `FAIL`, exit 0/1 |
| `--script <file>` | run a UI script (`docs/ui-script.md`) one line per loop pass; exit 0 on `quit` / EOF, 1 on the first failing line (`SCRIPT FAIL line n: ...` on stderr, nothing saved to `forms.ini`); `tests/ui/scripts/w1_acceptance.lj4s` is the W1 walkthrough |
| `U:<folder>` | start in that user library instead of `Startup.ini`'s ([01 s6.2]) |
| `--explode` | design 7.6: a container declared after the controls it covers (Panel1 on `TSEQUENCEFORM`) is moved below them and the client grows, so the covered rows are visible |
| `--native` | same as `[CHROME] Native=1` |
| `--zoom <1\|2>` | same as `[CHROME] Zoom` |
| `--version` | prints `LJ4EVR x.y.z` and exits |

`lj4evr` is a WIN32-subsystem program, so its stdout only shows up when
redirected: `lj4evr.exe --selftest > selftest.txt` or
`Start-Process ... -RedirectStandardOutput`. The log sink writes to stderr the
same way.

Windows: the self-drawn frame drags by its caption and resizes at its border
through `SDL_SetWindowHitTest` driven by `lj_window_hit()`; the caption buttons
and the system-menu icon are ordinary clicks handled by the ui layer, which asks
the platform to minimise/maximise/restore through the `lj_window_host` callbacks.
Minimising the main window hides the controls, as LJ2's `TApplication` does.
`tools/taskbar-count.ps1` counts the LJ4EVR taskbar buttons through UI Automation
(ME-7 / ME-59 evidence: one).

`LJ4EVR_USERDATA=<folder ending in \ or />` replaces the per-user data folder (`%APPDATA%\LJ4EVR\LJ4EVR\`, macOS
`~/Library/Application Support/LJ4EVR/LJ4EVR/`) for that run: libraries, `Main.Ini`, `Hardware.Ini`. The W4 captures
of User Libraries use it so no account path shows (ME-148).

Cue lists, Execute File: the line runs only with `Main.Ini [CUELIST2] AllowExecute=1` (default 0). AllowExecute=1 lets a cue list start any program through the shell; enable it only for cue lists you wrote.

### Network listeners (W5)

Every listener is off on a fresh `Hardware.Ini`; each one logs the address it binds when it opens (the security view is
`SECURITY.md` "Network listeners").

| Listener | Opens when | Binds | Filter |
|---|---|---|---|
| UDP 5568, sACN DMX-in | Hardware Setup > Enable DMX in, source sACN (`[DMXIN] Enabled=1`) | every interface (multicast) | with `[DMXIN] Nic` set, packets from off that subnet are dropped |
| UDP 6454, Art-Net | an output link on Art-Net, or DMX-in from Art-Net | the output interface (every interface when none is chosen) | ArtPollReply only from that subnet, at most 4 rows per address; Art-Net DMX-in from off the subnet dropped |
| TCP 7232, remote | Setup > RS-232 / TCP Remote > TCP enabled (`[REMOTE] TcpEnabled=1`) | 127.0.0.1 unless you choose an interface; never 0.0.0.0 | at most 4 connections; one silent for 60 s is closed |
| serial, remote | the RS-232 remote enabled (`[REMOTE] SerialEnabled=1`) | the chosen port only | none |
| External Application | a patched External Application fixture with its link set | no listener: a TCP client to the definition file's host and port, or a serial port | none |

The remote has no authentication: on a LAN interface anyone on that network can drive the show. Keep it on 127.0.0.1
unless the controller sits on a lighting network you trust.

## lj4evr_render (headless, no SDL)

```
lj4evr_render <out dir>                        one BMP per generated form, <CLASS>.bmp upper-cased
                                               (TSEQUENCEFORM.bmp ...) plus MAINWINDOW.bmp (the stub main window)
lj4evr_render --font-proof <face> <out.bmp>    every CP1252 glyph 32..255 of sans13 | sans13b | arial14 |
                                               arial14b | arialblack18 in a 16-column grid
```

Prints `lj4evr_render: N written, M failed -> <dir>` and exits 1 when any form
failed. The W0 goldens are written to `tests/ui/render/`.

## lj4evr_tests

`lj4evr_tests <subcommand>`; `ctest --preset dev-msvc` runs every subcommand.
`lj4evr_tests render [tmp.bmp]` is this track's check: every generated form and
the stub main window instantiate with no bindings, paint to a surface at least
their client size, and `lj_bmp_write` produces exactly
`54 + h * ((w * 3 + 3) & ~3)` bytes. `lj4evr_tests test_uiscript` (W1, app
track) parses every script verb, rejects a bad line with its number, drives a
headless `TSEQUENCEFORM` (click a check box, type into an edit, `shot` to PNG),
recreates a missing `Default` library, writes `LastRun`, reads `','` under a
comma locale (ME-52) and round-trips `forms.ini` under a non-CP1252 folder name
(ME-65).

## tools/shot.ps1 (Windows screenshots)

```
powershell -ExecutionPolicy Bypass -File tools\shot.ps1 -Exe build-msvc\Release\lj4evr.exe `
    -Title Sequence -Out docs\screenshots\w0\seqform.png -Taskbar -TaskbarOut docs\screenshots\w0\taskbar.png
```

Starts the exe with `--ms 4000` (plus `-Args '--native'` etc.), finds the window
with that title in that process, brings it to the front and grabs it with
`PrintWindow(PW_RENDERFULLCONTENT)`, which reads the window's own surface:
`CopyFromScreen` at the window rect returns the wallpaper on this four-monitor
PC (spikes/c-engineering). `-Taskbar` also grabs `Shell_TrayWnd` (PrintWindow,
screen copy as fallback) so the single taskbar button for main + Sequence + Cue
is on record. The W0 captures are in `docs/screenshots/w0/`; they are our own
renderings and are committed.

## W0 evidence on the work PC (2026-10-21, build-platform/, MSVC 19.44)

```
lj4evr.exe --selftest
selftest TSEQUENCEFORM: screen 472x207 vs render 472x207, 0 mismatching pixel(s): OK   (exit 0)

tools\shot.ps1 -Title Sequence ... -Taskbar
  docs\screenshots\w0\seqform-classic.png : 472x207 PrintWindow=True
  docs\screenshots\w0\taskbar.png : 2560x48 PrintWindow=True     (one LJ4EVR button with main + Sequence + Cue open)
tools\shot.ps1 -Extra '--native'   -> seqform-native.png 482x221 (OS frame)
tools\shot.ps1 -Extra '--explode'  -> seqform-explode.png (Panel1 moved below the ESeq* row)
tools\shot.ps1 -Title Cue           -> cueform-classic.png 620x300
tools\shot.ps1 -Title LJ4EVR        -> mainwindow-classic.png 807x652

lj4evr_render tests/ui/render        -> 47 of 208 forms written, then an access violation inside
                                        lj_window_paint() of TDA2532FORM_3 (ui track; the renderer and
                                        lj4evr_tests render stop there until it is fixed)
```
