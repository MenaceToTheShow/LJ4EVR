# Building LJ4EVR

C11, CMake >= 3.25, Ninja or Visual Studio. SDL3 (`release-3.4.18`) is fetched
by CMake at configure time; nothing else is downloaded. Build directories are
`build-*/` under the repo and are gitignored (design 9.1).

## Presets (`CMakePresets.json`)

| Preset | Host | Generator | SDL | What it is for |
|---|---|---|---|---|
| `dev-msvc` | Windows | Visual Studio 17 2022 x64 | on | day-to-day on the work PC |
| `dev-ninja-msvc` | Windows | Ninja + `cl` (inside a `vcvars64` shell) | on | faster incremental builds |
| `dev-mac` | macOS | Ninja + clang | on | day-to-day on a Mac |
| `ci-linux-asan` | Linux | Ninja + gcc, Debug, ASan+UBSan | off | tests + headless renderer |
| `ci-linux-clang` | Linux | Ninja + clang, Release | off | tests + headless renderer |
| `release-win` | Windows | as `dev-ninja-msvc` (Ninja + `cl`, vcvars64 shell), `build-release-win/` | on | NSIS installer; what CI builds (hosted runners carry whichever VS is newest, so no VS version is hard-wired) |
| `release-mac` | macOS | as `dev-mac`, universal arm64+x86_64, min macOS 12 | on | DragNDrop `.dmg` |

Every preset: `cmake --preset <p> && cmake --build --preset <p> && ctest --preset <p>`.

## Windows (MSVC 2022 Community)

    cmake --preset dev-msvc
    cmake --build --preset dev-msvc
    ctest --preset dev-msvc

Executables land in `build-msvc/Release/`. The Ninja preset needs `cl` on PATH:
open "x64 Native Tools Command Prompt for VS 2022" (or run `vcvars64.bat`) first,
then `cmake --preset dev-ninja-msvc`.

Installer: `cd build-release-win && cpack -G NSIS` (NSIS 3 on PATH;
`choco install nsis`). Produces `LJ4EVR-<ver>-win64-setup.exe`, a per-user
install under `%LOCALAPPDATA%\Programs\LJ4EVR`, no admin prompt (ME-19).

**MSVC 19.44 and SDL3**: the root `CMakeLists.txt` carries two workarounds
(`CMAKE_DISABLE_PRECOMPILE_HEADERS ON`, `CMAKE_VS_GLOBALS UseMultiToolTask=true`)
plus a subsystem trim. If the SDL part of the build dies with C1001 or
`CL.exe exited with code -1073741819`, read `docs/msvc-sdl-build.md` before
touching anything.

## macOS (12+, Xcode command-line tools)

    brew install cmake ninja
    cmake --preset dev-mac
    cmake --build --preset dev-mac
    ctest --preset dev-mac

The app is `build-mac/lj4evr.app`. Installer: `cmake --preset release-mac`,
build, then `cd build-release-mac && cpack -G DragNDrop`. The release workflow
ad-hoc signs the bundle first (`codesign --force --deep -s -`); a local build is
unsigned and Gatekeeper will ask for right-click > Open.

## What the installers carry (ME-36, W4-HELP, W4-MAC)

The install / CPack section of `CMakeLists.txt` puts three things in both
installers and nothing else:

| Item | Windows (NSIS, per-user) | macOS (`.dmg`) |
|---|---|---|
| the program | `lj4evr.exe` (static CRT: no `vcruntime` DLL, ME-19) | `LJ4EVR.app` (built as `lj4evr.app`, renamed on install), our `lj4evr.icns`, universal arm64 + x86_64, macOS 12 minimum |
| `THIRD_PARTY_NOTICES.txt` | beside `lj4evr.exe` | `LJ4EVR.app/Contents/Resources/` (inside the bundle, so the signature covers it) |
| the help topics | `help\*.txt` beside `lj4evr.exe` | `LJ4EVR.app/Contents/Resources/help/` |

plus, on the `.dmg`, the `Applications` link CPack DragNDrop adds.
`THIRD_PARTY_NOTICES.txt` is written at configure time: `NOTICE` followed by
`third_party/miniz/LICENSE`, `third_party/qlcplus-fixtures/NOTICE` and
`third_party/qlcplus-fixtures/LICENSE` (the full Apache-2.0 text); SDL3's zlib
text is inside `NOTICE`. The program reads its help compiled in
(`src/windows/lj_help_topics_gen.c`); the `.txt` copies are for reading outside it.

Check an installed tree (an NSIS install directory, a mounted `.dmg`, or
`cmake --install <build> --prefix <dir>`):

    python -I tools/help_check.py --install-tree <dir>

It fails unless the notices file carries the Apache-2.0 terms, the QLC+ NOTICE,
the miniz MIT text and the SDL3 zlib text, and every topic matches `help/`.
`release.yml` runs it on the silently installed NSIS tree and on the mounted
`.dmg`.

## Linux (tests and the headless renderer only; no app)

    sudo apt install gcc clang cmake ninja-build python3     # or use docker/Dockerfile
    cmake --preset ci-linux-asan     # or ci-linux-clang
    cmake --build --preset ci-linux-asan
    ctest --preset ci-linux-asan

`LJ_SDL=OFF` builds every SDL-free library, `lj4evr_tests` and `lj4evr_render`.
The CI container (`docker/Dockerfile`, debian:trixie) is the reference Linux
environment: `docker build -t lj4evr-ci docker && docker run --rm -v "$PWD:/w" -w /w lj4evr-ci sh -c 'cmake --preset ci-linux-clang && cmake --build --preset ci-linux-clang && ctest --preset ci-linux-clang'`.

## Options

| Cache variable | Default | Meaning |
|---|---|---|
| `LJ_SDL` | ON | build `platform/sdl` and the `lj4evr` app |
| `LJ_WARNINGS_AS_ERRORS` | ON | `/WX` / `-Werror` |
| `LJ_SANITIZE` | off | `address,undefined` or `thread` |
| `FETCHCONTENT_SOURCE_DIR_SDL3` | unset | use an existing SDL checkout instead of cloning (CI caches one) |

Warnings are errors everywhere: `/W4 /WX /permissive- /utf-8 /fp:strict` on
MSVC, `-Wall -Wextra -Wpedantic -Wshadow -Wvla -Werror -ffp-contract=off` on
gcc/clang. Compiling one file by hand: `docs/w0-contracts.md` rule 3.

## Version

`VERSION` (one line, `x.y.z`) is the single source; `cmake/version.cmake` turns it
into `version.h` (`LJ_VERSION_STR`, `LJ_GIT_HASH`), the CPack version and the
bundle version. See `docs/releasing.md`.

## Tooling (not at build time)

`tools/*.py` run under Python 3.12 with `python -I`; the work PC has a venv at
`.venv/`. CTest runs `tools/test_layering.py` through whichever `python3` CMake
finds. `packaging/icons/make_icons.py` (needs Pillow) regenerates the icon set.
