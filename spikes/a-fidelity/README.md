# Spike a-fidelity: TSequenceForm drawn Windows-classic on SDL3 + own rasteriser

What it proves (run 2026-10-21 on the work PC, MSVC 14.44, CMake 4.1.0-rc2, Windows 11 at 96 dpi):

- A C11 program with SDL3 (window/events/blit only, everything else drawn by hand) opens a window
  titled `Sequence` with the exact TSEQUENCEFORM client size 466x163 (+19 px self-drawn menu bar).
- Text height is 13 px: GDI reports `tmHeight=13 tmAscent=11 tmDescent=2` for the system raster
  font `MS Sans Serif` at `-11`, and the six AutoSize label widths in the DFM (26, 80, 76, 83, 78, 64)
  are reproduced to the pixel, so the font Delphi laid the forms out with is the one being drawn.
- Runs for 2 s and exits 0; `--explode` lowers Panel1 by 40 px so the labels/edits it covers are visible;
  `--save file.bmp` dumps the drawn surface.

Commands:

    cmake -S . -B build -G "Visual Studio 17 2022" -A x64
    cmake --build build --config Release
    build\Release\spike_sequence.exe
    powershell -ExecutionPolicy Bypass -File screenshot.ps1 -Out screenshot.png
    powershell -ExecutionPolicy Bypass -File screenshot.ps1 -Out screenshot-explode.png -ExtraArgs "--explode"

Build note: with SDL3's default option set, CL.exe 14.44 crashed (exit -1073741819 / C1001) at a moving
position inside SDL's hidapi/joystick/gpu sources on eight consecutive incremental attempts. Turning off the
subsystems the clone does not use (joystick, haptic, camera, gpu, dialog, process, hidapi, offscreen, render,
vulkan, sensor, power, tray, opengl, directx) built clean on the first try. Keep that option set.

Outputs here: `screenshot.png`, `screenshot-explode.png` (PrintWindow captures), `surface.bmp`,
`surface-explode.bmp` (the program's own surface dumps), `run.log`. `build/` is gitignored.
