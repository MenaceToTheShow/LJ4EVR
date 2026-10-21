# Spike b-lazy: TSequenceForm in SDL3 + SDL_Renderer (2026-10-21)

What it proves: one SDL3 window per LightJockey window, Windows-classic chrome drawn with
filled rects (clBtnFace #C0C0C0, highlight #FFFFFF, shadow #808080, dark #000000), DFM
geometry used verbatim (TSEQUENCEFORM.dfm.txt: client 466x163, labels, three read-only
clSilver edits, Panel1 354x106 with its bevel, 25x25 speed buttons, time edits, up/down
spinners, check boxes). Text is SDL3's built-in 8x8 debug font on a 13 px line pitch; the
real font strategy is in the proposal (stb_truetype + a bundled OFL/Apache font).

Ran on this PC (Windows 11, VS 2022 17.14, MSVC 19.44.35213, CMake 4.1.0-rc2):

    cmake -S . -B build -G "Visual Studio 17 2022" -A x64        # fetches SDL3 release-3.2.16
    cmake --build build --config Release --parallel
    build\Release\seqwin.exe                                     # runs 2 s, exit 0, writes seqwin.bmp

Result: built, ran, exit code 0, stdout "window pixels 466x182 (LJ client 466x163 + 19 px
menu bar)". seqwin.png (converted from the renderer readback seqwin.bmp) is the screenshot.
An external PowerShell/System.Drawing grab was attempted twice but the agent's shell has no
interactive desktop (FindWindow never sees the window), so the in-app capture is the evidence.

Gotcha found and worked around: MSVC 19.44 crashes (CL.exe exit 0xC0000005) compiling
SDL's src/render/software/SDL_blendfillrect.c at /O2 when SDL's precompiled header is on; the
same file compiles standalone at /O2. Fix: SDL_PRECOMPILED_HEADERS=OFF (CMakeLists.txt).
Also turned off the GPU/Vulkan/D3D12 renderers (unused, faster build).
