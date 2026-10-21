# W0 integration log

Integrator session, 2026-10-21, work PC (MSVC 19.44, CMake 4.1, VS 17 2022 x64).
Commands and outputs recorded as run; resolutions of the track blockers at the end.

## 1. Pre-integration commit

    git status            -> 361 untracked/modified paths; reference/ and build-*/ ignored (.gitignore)
    spikes/README.md      -> written (design-phase evidence, not built by root CMake)
    .gitignore            -> + spikes/**/*.bmp (build*/ already matched at any depth)
    git add -A && git commit "W0: contracts, tracks landed (pre-integration)"   -> d855140
    git push origin main  -> a7b573d..d855140

Staged check: `git diff --cached --name-only | grep "^reference\|\.bmp$"` -> nothing.

## 2. Full-tree build

    cmake -S . -B build-nosdl -G "Visual Studio 17 2022" -A x64 -DLJ_SDL=OFF     -> configured (Python3 3.14 found for CTest)
    cmake --build build-nosdl --config Release                                   -> every library compiled clean (0 warnings);
        link errors: unresolved lj_font_arialblack16 (lj_render_main.obj, lj_ui.lib: lj_window.obj font_of, lj_widgets_lists.obj);
        LNK4006 lj_form_TSEQUENCEFORM / lj_forms_all defined twice (lj_form_example.obj vs lj_forms_gen.obj)

Resolutions (design.md wins; each is the smallest change):

| Blocker (track) | Resolution |
|---|---|
| `lj_font_arialblack16` vs the harvested `arialblack18` (gfx, fonts, draw x5, pack, ui, app) | ME-18 names faces by harvested cell: renamed to `arialblack18` / `LJ_FONT_ARIALBLACK18` in `src/gfx/lj_font.h`, `src/ui/forms/lj_form.h`, `src/ui/lj_window.c`, `src/ui/widgets/lj_widgets_lists.c`, `src/app/lj_render_main.c`, `tools/dfm2c.py` (+ regenerated `lj_forms_gen.c`, `dfm2c --check: OK`), `docs/running.md`, `docs/w0-contracts.md`; `tests/fonts/test_corpus.c` lost its local extern |
| `lj_font` needs `overhang` (fonts pack: sans13b 200 mismatches) | `int overhang` added after `descent` in `lj_font.h`; `lj_text_width` returns `w ? w + overhang : 0`; `tools/fontpack.py` emits `.overhang = %d,` (the ponytail line retired); `fontpack.py` re-run, `--check` current; `tests/gfx/test_text.c` positional initializer gained the field |
| `src/ui/forms/lj_form_example.c` collides with `lj_forms_gen.c` (dfm2c) | `git rm` (the contracts doc said so) |
| `_CRT_SECURE_NO_WARNINGS` per file (core, gfx) | left as is: the per-file define is what lets rule-3 standalone `cl` compiles work; six copies is cheaper than a CMake-only knob |
| nothing defines `LJ_DEBUG` (core) | left as is: Debug configs measure lock hold time via `!defined(NDEBUG)`, Release does not; `test_lock` adapts |
| `tools/ref/16-gs2_ref.py` 17 unlisted bytes at 0x266C (core) | noted for the W2 `lj_gs2_parse` author: treat 0x266C as `reserved_266C` raw 17 (vault note, not editable here) |
| `lj_widgets_lj.h` missing `lj_w_dipswitch.h` / `lj_w_updown.h` (seen mid-flight by three tracks) | already landed; `test_layering` 0 findings |
| no widget calls `lj_glyph_get()` (pack) / glyphmap rows inert until dfm2c looks them up (pack) | W1: button glyphs stay absent in the W0 goldens (noted in REVIEWED.tsv) |
| `lj_window_last_event` proposal (basic widgets -> frame) | W1 (no W0 form needs the mouse button) |

Second build, both trees:

    cmake --build build-nosdl --config Release        -> lj4evr_tests.exe, lj4evr_render.exe, 0 warnings
    cmake --preset dev-msvc && cmake --build --preset dev-msvc   (LJ_SDL=ON, SDL3 release-3.4.18 fetched)
                                                      -> lj4evr.exe, lj4evr_tests.exe, lj4evr_render.exe, rc=0, 0 warnings/errors

## 3. Tests

    ctest --test-dir build-nosdl -C Release    -> 100% tests passed, 0 tests failed out of 26 (test_layering 65 s)
    ctest --test-dir build-msvc  -C Release    -> 100% tests passed, 0 tests failed out of 27 (after ui_golden was added)
    lj4evr_tests font_corpus   -> font_corpus: 826 rows, 34 skipped (face not packed), 0 mismatches   (was 200 before the overhang field)
    lj4evr_tests font_cells    -> font_cells: 5 faces, 0 failures
    lj4evr_tests art_table     -> art_table: 59 entries, 0 failures
    lj4evr_tests forms_tables  -> forms 208, rows 3632, menu items 1081, forms.ini-persisted 73, unknown rows 0, fails 0
    lj4evr_tests render        -> render: 208 form(s) + main window, 0 failure(s)
    lj4evr_tests dump-frames f1.bin ; dump-frames f2.bin ; cmp f1.bin f2.bin   -> identical, md5 283aa5c0229866868bc4678af4f46392 (400 ticks, 1641600 bytes)
    tools/test_layering.py --src src --cc <cl.exe> --gen build-nosdl/generated -> 154 file(s) checked, 66 header(s) compiled alone, 0 finding(s)
    tools/fontpack.py --check / artpack.py --check / gen_vectors.py --check / dfm2c.py --check -> all current

Test fixes found by the full run:

- `tests/ui/test_widgets_basic.c`: the "sans13 has no inked glyphs" probe drew black text on the still-black surface, so every text-ink
  assertion was skipped as NOINK even with the inked font. The probe now fills BTNFACE first; all ink checks count and pass.
- `tests/ui/test_widgets_lists.c:266`: the active tab is now painted last with its 2-px overhang over the neighbour (classic look),
  so the expectation at x = 2 + tw1 is the active tab's black edge, not the inactive tab's white one.

## 4. Renders and goldens

    lj4evr_render build-nosdl/render   -> lj4evr_render: 209 written, 0 failed   (208 forms + MAINWINDOW.bmp)

Reviewed at 2x/4x/8x with Read against the DFM numbers and `reference/screenshots/lj-Help_chm_images/` (sequencecontrol.gif,
saveseqdialog.gif, cuecontrol.gif, systempreferencesdialog.gif, fixtureconfiguration.jpg). Paint bugs found and fixed:

1. `src/ui/widgets/lj_widgets_lj.c` `lj_w_spin_pair`: the down arrow sat one row too high (`+ p2` instead of `+ 1 + p2`);
   spike A's `updown()` and `gfx/lj_spinner` have the `+1`. Found by the spike-A pixel diff.
2. `src/ui/widgets/lj_w_pagecontrol.c`: `MultiLine = True` was ignored, so TSYSTEMPREFSFORM's fifth tab ("Data Restore") was
   clipped. Implemented the comctl32 TCS_MULTILINE rule: greedy wrap, rows balanced (equal counts, extras on the last rows),
   every row justified to the strip width, the active tab's row rotated to the bottom; strip height = rows * (font + 6) + 2.
   Six DFMs use it (TSYSTEMPREFSFORM, 3 other TPageControls, 2 TTabControls).
3. `src/ui/widgets/lj_w_listview.c`: a column without `Width` in the DFM drew 0 px wide ("Special" on TLINKCONFIGFORM3);
   TListColumn's default is 50. `AutoSize` columns now follow LVSCW_AUTOSIZE_USEHEADER: the last one fills the remaining
   width, others fit their header text.

Spike A comparison (`spikes/a-fidelity/surface.bmp`, 466x182 = client + 19-px menu bar, against the render's client area at
offset (3,22) inside the 472x207 classic frame), measured by a PIL script:

    compared 466x182, mismatching pixels: 2508 (2.96%), rows 5..118
    classified: 2508 black/face/white swaps (glyph shapes; plus the spike measuring "&Preferences" with GetTextExtentPoint32A,
                which counts the '&' and shifts "Snapshot"/"Help" right by one '&' advance), 0 pixels of any other colour pair
    = geometry identical pixel for pixel; only glyph ink differs (acceptance 2)

Goldens: `tests/ui/golden/{TSEQUENCEFORM,TSAVESEQFORM,TCURRENTCUE,TSYSTEMPREFSFORM,TLINKCONFIGFORM3}.bmp` (+ `REVIEWED.tsv`,
KING_DAVE-20), gate `lj4evr_tests ui_golden` (`tests/ui/test_golden.c`, byte-exact file compare, in LJ_TEST_SUBCOMMANDS):

    ui_golden: 5 form(s), 0 failure(s)

## 5. The app on screen (build-msvc/Release/lj4evr.exe, MSVC 19.44, Windows 11)

    lj4evr.exe --selftest   -> selftest TSEQUENCEFORM: screen 472x207 vs render 472x207, 0 mismatching pixel(s): OK   (rc 0)

    tools/shot.ps1 -Title Sequence -Taskbar
      docs/screenshots/w0/seqform-classic.png : 472x207 PrintWindow=True      (self-drawn classic frame, menu bar, client)
      docs/screenshots/w0/taskbar.png         : 2560x48 PrintWindow=True      (+ taskbar-button.png, the 2x crop around x=1899)
    UI Automation count of Shell_TrayWnd buttons (scratch script), main + Sequence + Cue open:
      taskbar buttons before: 32; while lj4evr runs: 33; new: 1  -> "lj4evr.exe - 1 running window", rect 1899,1392 44x48   (ME-7: one button)
    ShowWindow(main, SW_MINIMIZE) from PowerShell:
      before: main visible=True Sequence visible=True Cue visible=True
      after SW_MINIMIZE: main iconic=True Sequence visible=False Cue visible=False      (2.8 ownership rule)
      docs/screenshots/w0/taskbar-minimised.png (the one button stays; a desktop copy is not evidence on this PC: CopyFromScreen returns the wallpaper, spikes/c-engineering)
    [CHROME] Native=1 written to %APPDATA%\LJ4EVR\LJ4EVR\Main.Ini, repeated:
      docs/screenshots/w0/seqform-native.png  : 482x221 PrintWindow=True      (OS frame around the same client)
      docs/screenshots/w0/taskbar-native.png  : one button (UIA: new: 1, "lj4evr.exe - 1 running window")
      SW_MINIMIZE under Native=1: Sequence visible=False Cue visible=False
    Main.Ini removed again afterwards (it did not exist before).

## 6. CI

    git commit "W0: integrated build, tests green locally" -> 65ebf4d, push
    gh run list --workflow ci.yml   -> 39657660090 failure (all five jobs)

Round by round (each: `gh run view <id> --log-failed`, fix, commit, push, `gh run watch --exit-status`):

| Run | Result | Cause | Fix (commit) |
|---|---|---|---|
| 39655094340 (pre-integration push) | all red | container `:latest` not yet pushed; `windows-latest` has no "Visual Studio 17 2022" instance; clang: `size_t` undeclared, `/*` inside a block comment; link errors | - |
| 39657660090 | all red | Linux: container user uid 1000 gets EACCES on the runner's `_runner_file_commands` (runner user on atlas-ci2 is not 1000); Windows VS generator; macOS clang errors | 7b00546: `<stddef.h>` in `gfx/lj_draw.c`, `core/lj_log.h`, `ui/widgets/lj_w_dragbar.h`; `lj_font.h` comment; Dockerfile runs as root; `release-win` preset = Ninja + cl, workflows call `vcvars64.bat` found by `vswhere` in a `cmd` step |
| 39658388291 | Windows green; Linux EACCES (still pulled the old `:latest`: `docker push :latest` re-pushed the previous image); macOS `-Wmissing-field-initializers` | 2f11615: `ci.yml` pinned to `ghcr.io/unbannablegt/lj4evr-ci@sha256:6c05ca17c8ed0cb03da1f6db73931b0319f622513c60af2a975a795c79084406` (tag 7b00546, docker.yml run 39658388295); `docker.yml` retags `:latest` server-side with `buildx imagetools create` and reports the sha tag's digest; designators in `lj_frame.c`, two tests |
| 39659233316 | generate-check + Windows green; gcc/clang: four positional `lj_ctl_desc` rows in tests, gcc `-Wformat-truncation` in `test_encoding.c` | c314217: `.tag = 0` on the four rows, `path[2 * 64 + 8]` |
| 39659718859 | Linux x2 (ctest 27/27 under ASan+UBSan and clang) + Windows green; macOS: `--selftest` "SDL_GetWindowSurface: Window framebuffer support not available" | fcdddec: `SDL_RENDER ON` on APPLE (Cocoa has no native window framebuffer; the surface is renderer-backed, Metal with software fallback). Windows keeps the design 9.1 trim (GDI framebuffer is native) |
| **39660165446** | **all six jobs green** | | |

Green run: https://github.com/UnbannableGT/LJ4EVR/actions/runs/39660165446 (main @ fcdddec)

    generate-check  test_layering (gcc) 0 findings; fontpack/artpack/gen_vectors --check current
    linux-gcc-asan  100% tests passed, 0 tests failed out of 27   (Debug, -fsanitize=address,undefined)
    linux-clang     100% tests passed, 0 tests failed out of 27
    windows-msvc    100% tests passed out of 27; selftest TSEQUENCEFORM: screen 472x207 vs render 472x207, 0 mismatching pixel(s): OK; NSIS installer artifact
    macos-clang     100% tests passed out of 27; selftest TSEQUENCEFORM: screen 472x207 vs render 472x207, 0 mismatching pixel(s): OK; DragNDrop artifact
    determinism     frames-linux.bin = frames-macos.bin = frames-windows.bin, 1641600 bytes,
                    sha256 64236b688ba924a576a68a7d8678b0a1ce7eb0a9128fd721ba5b351e272f8d76 on all three

Note for the next Dockerfile change: merge, let docker.yml push, copy the digest from its step summary (now the sha tag's
digest, `:latest` is retagged from it), bump the four `image:` lines in ci.yml.

## 7. W0 acceptance (design section 10, W0)

| # | Item | Status |
|---|---|---|
| 1 | `tests/fonts/test_corpus` 0 mismatches over every AutoSize label of the 208 DFMs, on Windows, macOS, Linux, from the in-repo font | **done**: `font_corpus: 826 rows, 34 skipped (face not packed), 0 mismatches` in all four ctest jobs (the 34 skipped rows use faces outside the design's five: Courier, Arial -12/-13/-15/-16, MS Sans Serif -13/-16/-19; harvested, no sheet yet) |
| 2 | `lj4evr_render` writes 208 BMPs; TSEQUENCEFORM, TSAVESEQFORM, TCURRENTCUE, TSYSTEMPREFSFORM, TLINKCONFIGFORM3 reviewed and committed as goldens; TSEQUENCEFORM matches spike A's `surface.bmp` pixel for pixel except glyph shapes | **done**: 208 forms + MAINWINDOW written (209), 0 failed; goldens in `tests/ui/golden/` + `REVIEWED.tsv`, gate `ui_golden` green on every OS; spike A diff 2508 px = 2.96%, every one a glyph-ink swap (black/face/white), 0 geometry pixels |
| 3 | `lj4evr` opens TSEQUENCEFORM in a self-drawn classic frame on Windows (PrintWindow screenshot) and on `macos-14` (`--selftest`); `Native=1` shows the OS frame; one taskbar button with main + TSEQUENCEFORM + TCURRENTCUE; minimising the main window hides both; same under `Native=1` | **done on Windows** (seqform-classic.png, taskbar.png / taskbar-button.png, UIA: 1 new button, SW_MINIMIZE hides Sequence + Cue; seqform-native.png + taskbar-native.png + minimise under Native=1 identical); **macOS `--selftest` OK on macos-14** (0 mismatching pixels). Not checked: the macOS frame / Spaces behaviour on a real Mac (needs Joey's Mac; the design's R1 escape hatch is `Native=1`) |
| 4 | `release.yml` on `v0.0.1` produces a `.dmg` and an NSIS `.exe` whose About text reads `0.0.1` | **waits for the tag**: `VERSION` = 0.0.1; both installers already build as ci.yml artifacts (`installer-windows`, `installer-macos`) with the same presets; the About box text is W1 (no About form is wired in W0) |
| 5 | `determinism` green on an engine stub frame dump; `test_layering` green | **done**: three identical 1641600-byte dumps (sha256 64236b68...), `test_layering` 0 findings with gcc (CI) and cl (local, 154 files, 66 headers alone) |

Measured numbers: corpus 826 rows / 0 mismatches; 208 forms rendered (+ main window); 3632 control rows, 1081 menu items, 73
forms.ini-persisted forms; spike A diff 2508 glyph-class px, 0 geometry px; taskbar 1 button (main + Sequence + Cue, classic and
native); frame dump 400 ticks, 1641600 bytes, identical on three OSes; 27 CTest rows green on four toolchains.

Left for W1 (recorded, not blocking): button glyphs through `lj_glyph_get()` (art is packed, nothing draws it), glyphmap lookup in
dfm2c for the TSEQUENCEFORM/TCURRENTCUE toolbars, `lj_window_last_event` for right-click O/S/F, the 34 corpus rows outside the five
faces, `tools/ref/16-gs2_ref.py` 0x266C reserved 17 for `lj_gs2_parse`, the macOS Spaces/Mission Control check on a real Mac.
