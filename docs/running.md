# Running lj4evr (W0)

What the three executables do, their switches, and how the W0 screenshots were
taken. Build first: `cmake --preset dev-msvc && cmake --build --preset dev-msvc`
(Windows) or `cmake --preset dev-mac && cmake --build --preset dev-mac`; the
MSVC-specific SDL workarounds are in `docs/msvc-sdl-build.md`.

## lj4evr (the app)

`build-msvc/Release/lj4evr.exe`. Opens the stub main window (799x606 clBtnFace,
sizeable, the generated `TLJMainForm` menu bar), then `TSEQUENCEFORM` and
`TCURRENTCUE` from the generated tables as owned windows (one taskbar button,
design 2.8), starts the engine thread at `[ENGINE] TickHz` (default 40) and runs
the UI loop. On close it writes window positions to `forms.ini`.

Files live in `SDL_GetPrefPath("LJ4EVR", "LJ4EVR")`:
`%APPDATA%\LJ4EVR\LJ4EVR\` on Windows, `~/Library/Application Support/LJ4EVR/LJ4EVR/`
on macOS. `Main.Ini` is read, not written, in W0:

```ini
[CHROME]
Zoom=1        ; 2 = every window doubled, fat pixels (SDL_BlitSurfaceScaled NEAREST)
Native=0      ; 1 = SDL's bordered windows instead of the self-drawn classic frame
[ENGINE]
TickHz=40
```

| Switch | Effect |
|---|---|
| `--ms <n>` | quit after n milliseconds (what `tools/shot.ps1` uses) |
| `--save <file.bmp>` | write the main window surface (frame + menu bar + client) as a 24-bit BMP |
| `--selftest [ref.bmp]` | paint `TSEQUENCEFORM` on screen, read the OS window surface back and compare it with the image `lj4evr_render` paints for the same form (or with `ref.bmp`); prints one line ending `OK` or `FAIL`, exit 0/1 |
| `--explode` | design 7.6: a container declared after the controls it covers (Panel1 on `TSEQUENCEFORM`) is moved below them and the client grows, so the covered rows are visible |
| `--native` | same as `[CHROME] Native=1` |
| `--zoom <1\|2>` | same as `[CHROME] Zoom` |

`lj4evr` is a WIN32-subsystem program, so its stdout only shows up when
redirected: `lj4evr.exe --selftest > selftest.txt` or
`Start-Process ... -RedirectStandardOutput`. The log sink writes to stderr the
same way.

Windows: the self-drawn frame drags by its caption and resizes at its border
through `SDL_SetWindowHitTest` driven by `lj_window_hit()`; the caption buttons
and the system-menu icon are ordinary clicks handled by the ui layer, which asks
the platform to minimise/maximise/restore through the `lj_window_host` callbacks.
Minimising the main window hides the controls, as LJ2's `TApplication` does.

## lj4evr_render (headless, no SDL)

```
lj4evr_render <out dir>                        one BMP per generated form, <CLASS>.bmp upper-cased
                                               (TSEQUENCEFORM.bmp ...) plus MAINWINDOW.bmp (the stub main window)
lj4evr_render --font-proof <face> <out.bmp>    every CP1252 glyph 32..255 of sans13 | sans13b | arial14 |
                                               arial14b | arialblack16 in a 16-column grid
```

Prints `lj4evr_render: N written, M failed -> <dir>` and exits 1 when any form
failed. The W0 goldens are written to `tests/ui/render/`.

## lj4evr_tests

`lj4evr_tests <subcommand>`; `ctest --preset dev-msvc` runs every subcommand.
`lj4evr_tests render [tmp.bmp]` is this track's check: every generated form and
the stub main window instantiate with no bindings, paint to a surface at least
their client size, and `lj_bmp_write` produces exactly
`54 + h * ((w * 3 + 3) & ~3)` bytes.

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
