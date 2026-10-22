# CI

Three workflows under `.github/workflows/`, design 9.3 and 9.5.

| Workflow | Trigger | Runs on |
|---|---|---|
| `ci.yml` | push to `main`, tags `v*`, `pull_request` | self-hosted (Linux, in the container) + `windows-latest` + `macos-14` |
| `release.yml` | tags `v*` | hosted only (`docs/releasing.md`) |
| `docker.yml` | push to `main` touching `docker/**` or itself, manual | self-hosted; pushes the CI image to GHCR |

## `ci.yml` jobs

| Job | Runner | Does | Artifact |
|---|---|---|---|
| `generate-check` | `[self-hosted, lj4evr]`, container | `tools/test_layering.py`; every `tools/{qxf2c,fontpack,artpack,gen_vectors}.py --check` that exists (they land with their waves, design 9.2) | - |
| `linux-gcc-asan` | same | preset `ci-linux-asan` (gcc, ASan+UBSan, `LJ_SDL=OFF`), `ctest`, `lj4evr_tests dump-frames` | `frames-linux` |
| `linux-clang` | same | preset `ci-linux-clang`, `ctest` | - |
| `windows-msvc` | `windows-latest` | preset `release-win`, `ctest`, `lj4evr --selftest`, `dump-frames`, the static-CRT check; then preset `debug-win`: the whole Debug tree, `ctest` (ME-70: `/RTC1` and the ME-16 asserts only live here) and the Debug `--selftest` (no `no binding for handler`, no `unknown event` line, ME-74); `cpack -G NSIS` | `frames-windows`, `installer-windows` |
| `macos-clang` | `macos-14` | preset `release-mac`, `ctest`, `--selftest`, `dump-frames`, `cpack -G DragNDrop` | `frames-macos`, `installer-macos` |
| `determinism` | container, needs the three above | `cmp` the three `frames-*.bin`; one byte differs = red | - |

Hosted jobs cache the SDL3 source checkout (`actions/cache`, key
`sdl3-src-<GIT_TAG from CMakeLists.txt>`) and hand it to CMake through
`FETCHCONTENT_SOURCE_DIR_SDL3`, so a configure never clones SDL on a cache hit.
Changing the SDL tag in `CMakeLists.txt` changes the key automatically.

`concurrency: ci-<ref>` with cancel-in-progress: a new push to the same branch
cancels the previous run. `release.yml` does not cancel (a half-made release is
worse than a slow one).

## Runner security model (project.md, LORD_C-13)

Four self-hosted runners (`lj4evr-ci2-1..4`) on **atlas-ci2**, Ubuntu 24.04,
labels `[self-hosted, lj4evr]`, Docker 29 for the runner user, **no cmake on the
host**: every Linux job runs inside `ghcr.io/unbannablegt/lj4evr-ci`.

Rules the workflows enforce, in this order of defence:

1. Every self-hosted job carries `if: github.event_name != 'pull_request'`. A
   pull request, from a fork or a branch, never schedules work on atlas-ci2;
   only `push` to `main` and `v*` tags do. Installers and the two hosted
   `--selftest` runs still cover PRs.
2. Repo Actions setting: "Require approval for all outside collaborators"
   before the public flip, so a fork PR does not even run the hosted jobs
   until a maintainer looks at it.
3. Workflow permissions default to read. `ci.yml` asks for `contents: read,
   packages: read` (pull the image); `docker.yml` for `packages: write`;
   only the `release` job in `release.yml` gets `contents: write`.
4. The container runs as root. It ran as uid 1000 (`ci`) at first, but the runner
   writes its file commands (`_temp/_runner_file_commands/save_state_*`) as the
   host runner user and a container user with another uid gets EACCES on every
   step (seen on atlas-ci2 at the first push). Root inside a throwaway container
   with the workspace mounted is the GitHub-documented arrangement.

## The CI image

`docker/Dockerfile`: `debian:trixie` (ME-20; trixie's gcc, clang, cmake,
ninja-build and python3 >= 3.12, the exact versions are printed by the
workflow's smoke-test step), `git`, `ca-certificates`, root. `docker.yml` builds
it on a self-hosted runner, smoke-tests the tool versions, and pushes
`ghcr.io/unbannablegt/lj4evr-ci:<short-sha>` and `:latest`, logging in with
`GITHUB_TOKEN` (`github.actor`). The package is private because the repo is;
`ci.yml` pulls it with the same token through `container.credentials`.

**First run / package link**: GHCR packages pushed with `GITHUB_TOKEN` from a
workflow are linked to the repo automatically. If the pull in `ci.yml` is
denied, open the package on github.com > Package settings > "Manage Actions
access" and add `UnbannableGT/LJ4EVR` with read.

**Pin by digest after the first push** (ME-20). `docker.yml` prints the digest
in its step summary, e.g. `ghcr.io/unbannablegt/lj4evr-ci@sha256:abcd...`. In
`ci.yml` replace every `image: ghcr.io/unbannablegt/lj4evr-ci:latest` with that
string (four jobs; `container:` cannot read `env`, hence the repetition). After
a Dockerfile change: merge, let `docker.yml` push, take the new digest, bump
`ci.yml`. `:latest` is for bootstrap only.

## Running the lint locally

    pip install pyyaml                       # into .venv
    python -c "import yaml,glob; [yaml.safe_load(open(f)) for f in glob.glob('.github/workflows/*.yml')]"
    actionlint -no-color                    # rhysd/actionlint; .github/actionlint.yaml declares the lj4evr label

## W1

- `replay_all` runs the 16 scenarios under `tests/engine/replay/` on every
  job; `replay_dump` writes `replay-frames.bin` (103.8 MB) into the build
  directory and nothing compares it yet (ME-78, W2; done in W2, below): the `determinism` job
  compares the 400-tick `dump-frames` streams of the three OSes only.
- `windows-msvc` is the one job that runs a Debug `ctest` (ME-70). The Linux
  ASan job is Debug too, but gcc has no `/RTC1`.
- `generate-check` runs `qxf2c`, `fontpack`, `artpack` and `gen_vectors`
  `--check` (W1; W2 adds `exe_profiles_check`, below).
- Green run of the fix pass: see `docs/w1-integration.md` section 10.

## W2

- `generate-check` runs `qxf2c`, `fontpack`, `artpack`, `gen_vectors` and
  `exe_profiles_check` `--check` (ME-76). Without `reference/` the last one checks the
  committed `binding` column against `src/fixlib/hand/*.c` and `tools/qxf_map.tsv`.
- `replay_all` runs the 24 scenarios under `tests/engine/replay/` (W1 15, W2 9);
  `wire/drift_600s_two_slots_wire.lj4r` is its own row (`replay_wire_drift`), outside the
  glob. `replay_dump` writes `replay-frames.bin` (116,538,720 bytes) into each build
  directory. The Linux ASan, Windows Release and macOS jobs each publish its sha256 as a
  job output (`replay_sha`), and `determinism` prints the three and fails unless they are
  equal (ME-78). It still compares the 400-tick `dump-frames` files byte for byte too.
- The desktop, controls and windows libraries call each other, so on Linux CMake links
  them as a static cycle. GNU ld reads archives once, in order; the W2 pre-integration
  run failed to link `lj4evr_render` on Linux before this. macOS and Windows get no
  cycle: their linkers resolve across archives, and ld64 warns about repeated libraries.

## W3

- `linux-clang` also builds the two libFuzzer targets (`-DLJ_FUZZ=ON`, `tools/fuzz/`) in
  `build-fuzz` with the whole tree compiled `-fsanitize=fuzzer-no-link,address,undefined`,
  so `lj_fixlib` and `lj_core` carry coverage, and runs `fuzz_gs2` and `fuzz_udf` for 60 s
  each (LORD_C-17). `fuzz_gs2` also aborts when an accepted record does not write back
  byte-exact. `fuzz_udf` (ME-114) rewrites the footer CRC with `mz_crc32` of the body
  before parsing, so mutations reach the inflate and record paths, starts from
  `udf-corpus/` (the `lj_w3_synthetic.Udf` that `test_uiscript_w3` exports in the ctest
  step) and runs with `-max_len=32768`. Run 39867308177 (fix pass r1): `fuzz_gs2` DONE cov 708 (1,303,376
  runs); `fuzz_udf` INITED cov 1216 from the seed, DONE cov 1463, 164 corpus entries in 8,156 runs
  (was cov 175 without the CRC rewrite), above `fuzz_gs2`'s 708.
- The 66 generic-macro shapes (`src/engine/lj_shapes_gen.c`) are checked by the CTest rows
  `gen_vectors_shapes_check` (`gen_vectors --shapes --check`) and `gen_vectors_shapes_mutated`
  (WILL_FAIL) in every ctest job; `generate-check` keeps `gen_vectors --check` for
  `tests/vectors.h` and `lj_trig_gen.c`.
- `gs2_check` (the 48 shipped `.GS2` and the sampled `.Udf` against the Python oracle) is a
  CTest row that prints SKIP without `LJ4EVR_REFERENCE_DIR`; it only compares on the
  developer PC, where `reference/` exists.
