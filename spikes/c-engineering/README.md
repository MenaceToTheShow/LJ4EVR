# Spike: architect "c-engineering" (SDL3 + clone-drawn classic widgets)

One C11 program that opens a window titled `Sequence` with the client size of
`TSequenceForm` (466 x 163 from the DFM, plus a 19 px menu bar the clone draws),
paints the three labels, the three read-only `clSilver` edits and `Panel1` with
its children in Windows-classic chrome (`clBtnFace` #C0C0C0, white / #808080 /
black bevels), with text on 13 px rows, runs for 2 s and exits 0.

Files

| File | What |
|---|---|
| `CMakeLists.txt` | C11, SDL3 `release-3.4.18` via FetchContent (static, unneeded subsystems off), `/W4 /WX`, two MSVC workarounds (below) |
| `main.c` | the program; `--shot file.bmp` also writes its own back buffer, `--ms N` overrides the 2000 ms run time |
| `seqform_dfm.h` | `TSEQUENCEFORM.dfm.txt` transcribed into a widget table (the shape a DFM-to-C generator will emit) |
| `font13.h` | generated 1-bit font, 95 glyphs, 13-row cells (placeholder metrics, see the proposal's font section) |
| `tools/genfont.py` | generates `font13.h` from Pillow's bundled open-licence face |
| `shot.ps1` | starts the exe, finds its window by process id, grabs it with System.Drawing |
| `build-*.log` | configure and build logs of the runs recorded below |

Build and run (what was executed on this PC, 2026-10-21)

```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
powershell -ExecutionPolicy Bypass -File shot.ps1
```

Results are in the design vault (not published)
section 16 and summarised here:

1. First attempts failed inside SDL3's own compilation on this machine's MSVC
   (19.44.35213 / toolset 14.44.35207, Windows SDK 10.0.26100): `C1001` internal
   compiler error in `objidl.h` through SDL's precompiled header (Debug), and
   `CL.exe exited with code -1073741819` during code generation of a 20-file
   batch (Release, with and without the PCH). Every one of the 20 files compiles
   cleanly when CL is given one file at a time with the identical flags
   (bisect script in the session scratchpad; results in the proposal).
2. Fix, both in `CMakeLists.txt`: `set(CMAKE_DISABLE_PRECOMPILE_HEADERS ON)` and
   `set(CMAKE_VS_GLOBALS "UseMultiToolTask=true" ...)` so MSBuild runs one
   CL.exe per source. With these, the two commands above build `build/Release/seqform.exe`
   (1,835,008 bytes) with zero warnings. The Ninja generator (ships inside VS)
   with the same `cl` also builds it (`build-ninja.cmd`, `build-ninja/seqform.exe`):
   Ninja already runs one CL.exe per source, so only the PCH switch mattered there.
3. `shot.ps1`: window found 50 ms after launch, client rect 466 x 182, exit code 0.
   `seqform-backbuffer.bmp` / `.png` (`-2x.png` enlarged) is the renderer's own frame;
   `seqform-screen.png` is the OS window grabbed with `PrintWindow(PW_RENDERFULLCONTENT)`
   (a plain `CopyFromScreen` at the window rect returned the desktop wallpaper on this
   four-monitor PC, DPI-aware or not).

Nothing in this folder is Martin material: geometry and captions are facts from
the DFM; the font and all pixels are drawn by the spike.
