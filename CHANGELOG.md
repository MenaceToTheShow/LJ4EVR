# Changelog

Format: Keep a Changelog; versions are semver tags `vX.Y.Z` and `VERSION` is the
single source of the number.

## [Unreleased]

## [0.0.1] - W0 Foundation

The build skeleton. Nothing drives a light yet.

- CMake project: C11, one static library per `src/` layer, presets for
  Windows (MSVC, Ninja+cl), macOS, Linux CI and the two release builds;
  SDL3 `release-3.4.18` by FetchContent with the MSVC 19.44 workarounds
  (`docs/msvc-sdl-build.md`).
- W0 contract headers for core, gfx, ui, forms, engine and platform
  (`docs/w0-contracts.md`); `ui/lj_widget.c` with its test.
- `tools/test_layering.py`: include graph, naming, OS-header, float/atomics
  checks and each header compiling alone, under CTest.
- `VERSION` -> `version.h`, CPack version, bundle version.
- Packaging: per-user NSIS template (no admin), macOS `Info.plist`,
  Windows UTF-8 manifest, our own icon set (`packaging/icons/`).
- CI: `docker/Dockerfile` (debian:trixie) and `docker.yml`; `ci.yml` with
  generate-check, linux-gcc-asan, linux-clang, windows-msvc, macos-clang and
  determinism; `release.yml` tag-driven Windows + macOS installers with
  `SHA256SUMS`.
- Docs: building, ci, releasing, conventions, contributing.
