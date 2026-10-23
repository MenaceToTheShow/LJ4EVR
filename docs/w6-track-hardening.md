# W6 track: hardening

Branch `w6/hardening`, from `origin/main` @ `2668de8` ("W6: gates"). Rows from `docs/w6-gates.md` with owner
`hardening`. Toolchain: MSVC cl 19.44 + Ninja (vcvars64), SDL3 from the `release-3.4.18` source. Each row below quotes
the output of its Verify line, run on this branch. No row is closed in writing.

Trees (all in the worktree, gitignored):

| Tree | Settings | Build | ctest |
|---|---|---|---|
| `build-h-rel` | Release, `LJ_SDL` ON | 592 steps, 0 warnings, 0 errors | `100% tests passed, 0 tests failed out of 153` |
| `build-h-nosdl` | Release, `LJ_SDL` OFF | 352 steps, 0 warnings, 0 errors | `100% tests passed, 0 tests failed out of 151` |
| `build-h-dbg` | Debug, `LJ_SDL` ON | 592 steps, 0 warnings, 0 errors | `100% tests passed, 0 tests failed out of 153` |
| `build-h-asan` | Debug, OFF, `LJ_SANITIZE=address` (the new CI step's settings) | 0 warnings, 0 errors | `100% tests passed, 0 tests failed out of 151` |
| `build-h-asan200k` | as `build-h-asan`, plus `/DLJ_FUZZ_N=200000` | 352 steps, 0 warnings, 0 errors | `100% tests passed, 0 tests failed out of 151` |

    test_layering    463 file(s) checked, 221 header(s) compiled alone, noop handlers 0, ini ternaries 0, scene getters 0,
                     set-text/lib/ctx copies 0, noop Help handlers 0, hint/edit_int/seam copies 0, popup_at 2, 0 finding(s)
    fidelity_check   73 F/I id(s) with a row of 73, sacrifice paragraphs equal, 0 finding(s)

Every fuzz row under MSVC ASan at `LJ_FUZZ_N=200000` (`build-h-asan200k`, each rc 0, no ASan report):

    fuzz_sacn_rx: 200000 iterations, PASS
    fuzz_vtr: 200000 iterations, PASS
    fuzz_artnet: 200000 iterations, PASS
    fuzz_remote: 200000 iterations, PASS
    fuzz_ljb: 200000 iterations, PASS
    fuzz_ljb index: 200000 iterations, 66348 opened, PASS

Proof that the new rows catch their defect: with `src/gfx/lj_glyphs.c`, `src/model/lj_ljb.c` and `src/model/lj_show.c`
taken back to `origin/main`, and the stale-copy drop deleted from `lj_cuelist_form_save`, the four rows fail
(`build-h-asan`):

    FAIL tests/ui/test_cuelist_form.c:407 lj_table_get(&ctx->show->cuelists, 7) == NULL
    FAIL tests/model/test_ljb.c:293 !lj_ljb_path_ok("", "COM\xB9.txt") && ... && !lj_ljb_path_ok("", "CON .txt") && ...
    FAIL tests/model/test_model_w2_library.c:163 log_lines == 0   (fresh-library save_config logged 2 line(s))
    FAIL LORD_C-11: a BMP taller than its cell must give our art
    FAIL LORD_C-11: a BMP narrower than its cell must give our art

The CI half of three rows (LORD_C-7 INITED lines, LORD_C-12 step log, generate-check) can only be read from a CI run on
`main`. `ci.yml` runs on pushes to `main` and tags, and on pull requests, so a push of this branch runs nothing. Those
lines are for the integrator's RC run. The local stand-ins are quoted under each row.

## Rows

| Id | Mode | Status | Change |
|---|---|---|---|
| W5/LORD_C-7 | LAND | landed; CI half at the RC run | second fuzz mode in the CTest row and a second libFuzzer target |
| W5/LORD_C-8 | LAND | landed | `comp_ok` trims stem spaces, counts CP1252 superscript 1, 2, 3 |
| W5/LORD_C-12 | LAND | landed; CI half at the RC run | MSVC ASan `LJ_SDL=OFF` Debug ctest step in `windows-msvc` |
| W4/LORD_C-11 | LAND | landed | `lj_srcimg_get` returns NULL on a size mismatch with a cell |
| W4/LORD_C-12-part | LAND | landed; CI green at the RC run | `logged` under `NDEBUG`; four `snprintf(key)` return-checked |
| W4/WIZARD_STYLES-14 | LAND | landed; generate-check at the RC run | the `if [ -f ]` guard deleted |
| W4/WIZARD_STYLES-18 | LAND | landed | per-topic sha256 in the generated banner |
| W3/INT-s7-CLSAVE | LAND | landed | `test_cuelist_form` row |
| W2/INT-s9-GROUPINFO | LAND | landed | `lj_fs_exists` guard; `test_model_w2_library` row |
| ME-66 | DONE | verified | none |

### W5/LORD_C-7: `fuzz_ljb` second mode

`tests/model/test_ljb.c`: after the container loop, the `fuzz_ljb` row runs `fuzz_ljb_index`. It inflates the seed's
index, mutates the 3 x 532 records (the length is held at n x 532 so the size rule passes), deflates them again into
the same container (`ljb_reindex`, trailer size and CRC rewritten), then opens and extracts. The row fails unless some
mutated index opens. `lj4evr_tests fuzz_ljb --seed-index <dir>` writes a container with its index stored raw.

`tools/fuzz/fuzz_ljb.c` is built twice. With `LJ_FUZZ_LJB_INDEX` (CMake target `fuzz_ljb_index`, appended) it reads
that raw-index container, deflates the index and rewrites the trailer before `lj_ljb_open`, so libFuzzer mutates the
inflated records. The `ci.yml` linux-clang step builds it and runs it 60 s from the `--seed-index` corpus.

Verify, MSVC ASan, 20000 (`build-h-asan`, `lj4evr_tests fuzz_ljb`, rc 0):

    fuzz_ljb: 20000 iterations, PASS
    fuzz_ljb index: 20000 iterations, 6598 opened, PASS

Local stand-in for the CI INITED lines: both libFuzzer modes built with MSVC `/fsanitize=address /fsanitize=fuzzer`
(`fuzz_ljb.c` and `lj_ljb.c` instrumented, the `build-h-asan` libraries linked; the build is scratch, in
`build-h-lf`), 60 s each from the two seeds, rc 0:

    fuzz_ljb        #3  INITED ft: 70 corp: 1/265b     ...  #3799127 DONE ft: 106 corp: 11/1507b
    fuzz_ljb_index  #3  INITED ft: 79 corp: 1/1744b    ...  #385231  DONE ft: 302 corp: 75/119Kb

The index mode reaches 302 features against 106 for byte mutation: the per-record checks behind the inflate are now
fuzzed. CI half: the linux-clang log of the RC run shows an INITED line for `fuzz_ljb` and one for `fuzz_ljb_index`.

### W5/LORD_C-8: `comp_ok`

`src/model/lj_ljb.c` `comp_ok`: trailing spaces are trimmed from the stem before the device-name compare, and a fourth
character of CP1252 0xB9, 0xB2 or 0xB3 counts as a COM / LPT digit. Verify (`test_ljb`, rows at `test_ljb.c:293-294`):
`COM\xB9.txt`, `lpt\xB2`, folder `COM\xB3\`, `CON .txt` and `com1  .ini` refused; `COM\xB4.txt` and `CONX .txt`
accepted.

    ljb container: 0 failures after the refusal rows
    test_ljb: PASS (0)

### W5/LORD_C-12: MSVC ASan ctest in `windows-msvc`

`ci.yml` `windows-msvc` gains a step after the main one: vcvars64, then `cmake -S . -B build-asan-win -G Ninja
-DCMAKE_C_COMPILER=cl -DCMAKE_BUILD_TYPE=Debug -DLJ_SDL=OFF -DLJ_SANITIZE=address`, build, `ctest --test-dir
build-asan-win --output-on-failure`. `docs/ci.md` names it (job table and the "W6 hardening" section).

    $ grep -n LJ_SANITIZE .github/workflows/ci.yml
    154:          cmake -S . -B build-asan-win -G Ninja -DCMAKE_C_COMPILER=cl -DCMAKE_BUILD_TYPE=Debug -DLJ_SDL=OFF -DLJ_SANITIZE=address || exit /b 1

Line 154 is inside `windows-msvc`. The same configure locally (`build-h-asan`): `100% tests passed, 0 tests failed
out of 151`. CI half: the windows-msvc log of the RC run shows the step and `100% tests passed`. The hosted runner
gets `clang_rt.asan_dynamic-x86_64.dll` on `PATH` from vcvars64, as the local run does.

### W4/LORD_C-11: odd-sized Source-Images BMP

`src/gfx/lj_glyphs.c` `lj_srcimg_get`: with a cell given, a decode whose height differs from the cell, or that is
narrower than it, is freed and cached as a miss, so `lj_glyph_get` returns our art. Verify (`test_srcimg`): the
in-memory loader serves `Cancel.bmp` at 18 x 20 for the 18 x 18 cell and `DeleteScene.bmp` at 8 x 16 for the 16 x 16
cell; both give our art and `lj_srcimg_get` NULL.

    srcimg: in-memory loader, 7 loader calls, 0 failures

`test_srcimg_sdl` (the real-BMP half) passes in `build-h-rel` and `build-h-dbg`.

### W4/LORD_C-12-part

`tests/ui/test_window.c`: `logged` and its sink are declared inside `#ifdef NDEBUG`, the only block that reads them
(the `(void)sink` else branch is gone). `src/io/lj_sacn.c`: the four `snprintf(key, ...)` calls (`U%d`, `Unicast%d`, in
load and save) use the repo idiom `if (snprintf(...) >= (int)sizeof key) continue;`. Verify: the code, and the five
trees above at 0 warnings. gcc 16 / clang 23 are not on this PC; the CI images do not move in this track (digest
unchanged, `sha256:731edd12...`). CI half: green at the RC run.

### W4/WIZARD_STYLES-14

`ci.yml` generate-check: the loop runs every `--check` tool and exits 1 on a failure; the `if [ -f ]` guard is deleted
(all five tools exist).

    $ grep -c 'if \[ -f "tools/' .github/workflows/ci.yml
    0

The five tools of the loop, `--check` locally, each rc 0 (last line of each):

    qxf2c               Martin-Rush-MH3.qxf  1  1002  Color  31
    fontpack            src/gfx/lj_fonts_gen.c is current (5 fonts)
    artpack             census: 210 LJ2 Glyphs names -> our stems, 316 glyph sheets, 58 icons, 108 gobo tiles, 133 gel rows
    gen_vectors         gen_vectors --check: tests\vectors.h up to date
    exe_profiles_check  exe_profiles_check: 201 rows, 86 hand ids, 45 map ids, 0 finding(s)

CI half: generate-check green at the RC run.

### W4/WIZARD_STYLES-18

`tools/help_check.py`: `load_topics` keeps the sha256 of each LF-normalised `help/*.txt`, and `render` writes one banner
line per topic. `src/windows/lj_help_topics_gen.c` regenerated with `--write`: the banner has 18 lines, one per topic,
e.g. `*   aefe1f13...5f33  help/cues.txt`. The selftest checks that the banner carries the topic's sha256 and that an
edited topic is caught.

    help_check --check (tree)                  help_check: 18 topics          rc 0
    help_check --check (copy, cues.txt edited) FAIL lj_help_topics_gen.c is stale: run tools/help_check.py --write   rc 1
    help_check --selftest                      help_check selftest: OK        rc 0

### W3/INT-s7-CLSAVE

`tests/ui/test_cuelist_form.c`, new block before the ME-16 block. The block puts a stale `show->cuelists` copy of
number 7 in the table, then saves the window's list as 7 (`lj_cuelist_form_save(2, 7)`) into a scratch library. The
copy is dropped (`lj_table_get(..., 7) == NULL`), and the file loads back with the window's name and line count. A
table entry that is the window's own list is kept across a save. No code change (`lj_cuelist_form.c:859` already drops
the copy).

    test_cuelist_form: 0 failure(s)

### W2/INT-s9-GROUPINFO

`src/model/lj_show.c` `lj_show_save_config`: `htp` is zeroed (an empty name saves as the default), and
`lj_htp_names_load` runs only when `lj_fs_exists(path)`, as `lj_show_load_config` does. Verify
(`test_model_w2_library`): a fresh library (no `GroupInfo.dat`, asserted) is saved through a counting log sink, and
the file then reads back with the default names.

    test_model_w2_library: fresh-library save_config logged 0 line(s)
    test_model_w2_library: 0 failure(s)

### ME-66

    $ git grep -n find_form -- src tests tools
    (no output)  rc=1

`lj_win_find` (`src/windows/lj_windows.h:216`) is the one resolver. DONE, as the gate table says.

## Standing rules over this diff

- No listener, dependency, OS file or workflow trigger added. No `self-hosted`, `pull_request_target` or secret beyond
  `GITHUB_TOKEN` in `ci.yml`.
- Inputs: the fuzz targets and test seeds are synthetic (`ljb_build`); no `reference/` file was read or junctioned.
- No Martin byte: sha1 of the 17 changed paths against the 18,923 `reference/` files: 0 equal.
- REL-PRIVATE: the diff and the commit message were scanned for the user name, the host and work-PC names, the LAN
  prefixes, `Users\` and the Dropbox root: 0 hits.
