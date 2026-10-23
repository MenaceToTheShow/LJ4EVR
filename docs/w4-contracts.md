Track 0 (ME-125) landed first as `97fe7b4607ba6cbaf2a48f3a3591c02328ab1508` "W4: track 0 (ME-125)"; `git log --reverse v0.3.0..HEAD` lists it before the commit that adds this file.

# W4 contracts: the plan, the measurements and the seams nine tracks code against

Written 2026-10-22 by the W4 planner + contracts seat. Design section 10 W4 is the scope and acceptance; `docs/w4-gates.md`
is the gate list this page serves (committed with it). Every header compiles alone (`tools/test_layering.py`, cl: 0 findings,
now with the ME-44 pairing / identifier / OS_ALLOW checks and the ME-129 counts), `LJ_SDL` ON and OFF build with 0 warnings,
ctest is 101 of 101 in both trees (`build-w4`, `build-w4-nosdl`, Ninja + cl 19.44, Release) and the Debug app's
`--selftest` returns 0 with no `no binding`, `unknown event` or `no behaviour file` line. Shared files stay append-only
for every track: `CMakeLists.txt` tests section, `tests/lj_tests.c`, `tools/glyphmap.tsv`, `src/io/lj_actions.h`,
`src/app/lj_app.c`, `tools/gen_vectors.py`, `tools/test_layering.py` allowlists, `docs/file-formats.md`,
`tests/ui/golden/REVIEWED.tsv` (ME-15).

## 0. What landed before the fork

| Item | Commit | Evidence |
|---|---|---|
| ME-125 / ME-43 full align | track 0 | `lj_widget_layout` orders each align group as `TWinControl.AlignControls` inserts it (alTop by Top, alBottom by bottom edge descending, alLeft by Left, alRight by right edge descending, alClient in creation order) inside a new `vt->client` (AdjustClientRect: TPanel bevels / border / bsSingle, TGroupBox text height + 2, TODPanel / TODPanel2 frames); TTabSheet takes the DFM page rect (`rows * 18 + 6` top, 4 px at the sides: PageControl height - TabSheet height is 28 with one row in 11 forms, 46 with two in 4). `test_align`: 110 forms at their design client, 211 aligned controls, **0 misplaced**, 1 named exclusion (TIntensityGroupForm tab rows: comctl32 minimum tab width, W4-FRAME); the 98 derived-client forms are counted, not gated (80 misplaced there, all from the estimated client). Nine goldens re-rendered with REVIEWED rows; Statics 2 keeps its code-built rows and now has the splitter between the columns, as its DFM does. |
| ME-125 / ME-44 | track 0 | conventions sentence for `lj_widgets_basic.h` / `lj_widgets_lists.h`; `test_layering.py` pairing (the 18 headerless widget files resolve through the group headers, `PAIR_EXEMPT` names the 7 others), header identifier scan with a planted-header self check, phantom `OS_ALLOW` rows (2 removed from `io/lj_sacn.c`). Design 2.0's copy of the sentence is the design owner's (ME-124). |
| ME-125 / ME-46 | track 0 | `dfm2c.py` reads `lj_wev_names` from `src/ui/lj_widget.c`; an unknown `On*` is an unknown row; selftest feeds `OnFoo`. |
| ME-125 / ME-47 | track 0 | truncated blobs read whole from `10_<FORM>.bin`, bounds-checked and matched in DFM order: `grep -c truncated tools/dfm2c_blobs.txt` 113 -> **0**; 8 TListView `Items.Data` decoded; synthetic 158-byte stream at offset 716 in the selftest. |
| ME-125 / ME-53 | track 0 | `widthcorpus.py` deleted; `widths.tsv` / `font_census.tsv` frozen (`docs/generated-files.md`). |
| ME-125 / ME-54 | track 0 | `lj_w_sbar_fit` / `lj_w_sbar_reveal`; `grep -c "static .* fit_sb" src/ui/widgets/*.c` = **0**. |
| ME-125 / ME-57 | track 0 | sans13 0x85 0x89 0x8C 0x99 0x9C drawn to the 3-px harvested advance; `text` row checks non-box ink; `fontpack --check` current. |
| ME-129 | contracts | `lj_ctx_scene()` (NULL past the end, never clamps; the shrink sites already clamp) replaces six private getters; `lj_window_set_text()` replaces four set-caption copies; the two `lib()` and two `ctx()` copies are gone; `test_layering` prints `scene getters 0, set-text/lib/ctx copies 0`; `test_ctx_scene` shrinks a 3-scene sequence and checks every reader agrees. |
| ME-37 constant | contracts | `LJ_MAIN_CAPTION_FMT` is the only caption literal (`lj_win_caption_main` formats it); the fallback `LJ4EVR (User lib: %s)` is named in its comment. Joey's view and `docs/provenance.md` stay RELEASE rows. |

## 1. Measurements (on `97fe7b4` + this commit; commands as run)

**(a) Forms.** `grep -o "^const lj_form_desc lj_form_T[A-Z0-9_]*" src/ui/forms/lj_forms_gen.c` = 208; a form is wired when
`lj_form_<NAME>` is referenced outside `src/ui/forms/`: **121 unwired** (the gate file's census, unchanged by track 0).
This commit gives **101 of them a W4 window id** (plus `LJ_WIN_HELP`, the code-built help viewer, so 102 new ids), keeps
`TSAVEPRESETFORM` on its existing `LJ_WIN_SAVE_PRESET`, and names the **19 that never open** (fidelity rows, section 2):
TConfigPrintPreviewForm (Print Configuration greyed), TPrint2532Form (greyed), the 2510 family (TBinary2510Form,
TCreate2510Form, TD2510Form, TDownload2510Form, TMemoryTest2510Form), the 2518 family (TCreate2518MemStatusForm,
TD2518Form, TForm2518_2, TSetup2518Form), TMCXSetupForm, TMCICDForm2, TSandNetStatusForm, TCaptureForm,
TLJManagerPrefsForm, TDATCScaleForm, TExternalAppSetupForm (W5) and TUSBWarningForm. Today 105 ids log
`has no behaviour file yet` (the 102 W4 ids, `please_wait`, `save_preset`, `default_output`); ME-131 wants 0 at the RC.
The census is re-run with the same grep; the per-id entry points are in section 3.

**(b) Art.** `ls art/glyphs | wc -l` = **114** (target 210, the `Glyphs\` count of note 07's fact-check); `ls art/icons` =
**2** (`dimmer`, `movinghead`; target 58); `tools/glyphmap.tsv` 95 rows naming 57 glyphs; `tools/dfm2c_blobs.txt` 157
blobs: 44 resolved to a glyph name, **105 `dropped:no-glyphmap-row`** (92 Glyph.Data, 6 Picture.Data, 4 Icon.Data,
3 TImageList Bitmap), 8 decoded `Items.Data`; `art/gels.txt` absent (0 gel swatches), 0 gobo tiles, 2 family
silhouettes (`lj_fixture_icon_bitmap` picks `movinghead` or `dimmer`).

**(c) Captions.** New: UI script verb `dump_captions <path>` (`src/app/lj_uiscript.c`), script
`tests/ui/scripts/w4_captions.lj4s`, tool `tools/caption_diff.py` (`--check`, `--selftest`; CTest `caption_diff_selftest`
plants a "fixed" typo and must fail), reference copy at `reference/research/13-dfm-captions.tsv` (gitignored). Run on
the Release app in a backed-up `%APPDATA%\LJ4EVR`: 64 window ids opened (5 need a selection: `warnings`, `close_prompt`,
`position_presets`, `customize_fixture`, `channel_config`), 2686 runtime rows, **59 of the 207 captioned forms
implemented, 90 differences in 22 forms, 0 excluded, 63 absent**. The differences are mostly runtime-filled labels
whose DFM text is a placeholder (`lFixType`, `HTP Gropus:8888`, `0000`), the F19 runtime captions (`CUE [0000 No Name]`),
the About text and three `Not available in LJ4EVR` hints; each needs a fidelity-row exclusion or a fix. The 63 absent
are mostly TTabSheet captions (`lj_w_caption` does not cover TTabSheet: the captions owner extends the dump) and hints
behind ShowHint = False.

**(d) Goldens.** 54 BMPs in `tests/ui/golden/`, `REVIEWED.tsv` 87 data rows (78 + the 9 of track 0), every BMP has at
least one row; 10 goldens are named by DFM class (ui_golden), 44 by window id. Target: 208 forms + the code-built windows.

**(e) Fidelity.** `docs/fidelity.md` has contract rows for F8, F19, F20, F24, F25, F30, F31, F35, F38-F40, F45-F48, F51,
F52, F55 (18) and I2-I5 (4). Missing: **42 F rows** (F1-F7, F9-F18, F21-F23, F26-F29, F32-F34, F36, F37, F41-F44, F49,
F50, F53, F54, F56-F60) and **9 I rows** (I1, I6-I13); the design 1 sacrifice list is in README.md only.

**(f) Docs set.** Present: README.md, LICENSE (MIT), NOTICE, CONTRIBUTING.md, CHANGELOG.md, docs/building.md,
generated-files.md, file-formats.md, fidelity.md, running.md. **Missing: docs/provenance.md, SECURITY.md, a user guide**
(`docs/user-guide.md`). Private details in the tree (`git grep` for LAN IPs, host and user names, local paths): 57 lines,
45 of them in `docs/w1..w3-integration.md`, the rest `atlas-ci2` in ci.yml / release.yml / Dockerfile / docs/ci.md /
fidelity.md / releasing.md, `D:/lj4evr` in tools/ref headers, `tools/dfm2c.py` usage, `spikes/` logs and the
`LJ4EVR_REFERENCE_DIR` help string; test IPs (192.168.10.5, 192.168.1.x) are synthetic. History has one author identity
(the GitHub noreply address).

**(g) Installers.** `CMakeLists.txt` installs `NOTICE` as `THIRD_PARTY_NOTICES.txt` at the install root: beside
`lj4evr.exe` in the NSIS tree, but at the `.dmg` root on macOS, **not inside `LJ4EVR.app`**. Its SDL3 zlib text is
complete; miniz's MIT text and the QLC+ Apache-2.0 licence and NOTICE are only pointed at (`third_party/*/LICENSE`, not
shipped). ME-36 is open on both counts.

## 2. Tracks (disjoint write paths)

Nine tracks. "Writes" is exclusive unless marked append-only. Every track: reads design 1, 2.0, 2.3, 7.3-7.10, 10 W4, 12
and its notes in full; binds every OnXxx its DFMs declare (ME-16, Debug `--selftest` clean); uses real widgets (ME-17);
renders a golden per window it wires and appends a `REVIEWED.tsv` row in the KING_DAVE-20 form (DFM anchors, reference
GIF and whether it is 1:1, pixel delta), which the goldens integrator reviews; returns its fidelity rows as text for the
fidelity integrator (`docs/fidelity.md` is not a track file); puts every prompt of `[12 Q9]` its windows raise in verbatim
(the DFM wins where note and DFM differ); keeps CP1252, integer ms, C11, no float in SDL-free dirs.

| Track | Writes | Owns (forms / items) | Gates |
|---|---|---|---|
| dlg-config | `src/windows/lj_config_dialogs.c` (+ `.h` below the contract block), `lj_fixture_config.c`, `lj_profile_editor_form.c`, `lj_gs2_dialogs.c`, `src/fixlib/lj_profile_editor.c`, `src/fixlib/lj_gs2.c` (LORD_FIXTURES-3 only), `tests/ui/test_config_dialogs.c` (new), `tests/fixlib/test_profile_editor.c` (append) | the 25 CONFIG ids (section 3); TConfigPrintPreviewForm stays greyed (fidelity row) | W4-DLG-5, W4-DLG-0 (export warning, old-style name, import-modified, colour scroller, RGB pack, palette value type, Copy From / Clone with the F44 copy filter, file exists), W4-DLG-3 (TConfigWarnForm `Save Ayway`), ME-126, LORD_FIDELITY-7 / LORD_FIXTURES-5, LORD_FIDELITY-9, LORD_FIXTURES-3, the Martin locate button, the seven Fixture Configuration Q9 prompts, F41, F43, F44, ME-9 |
| dlg-show | `src/windows/lj_show_dialogs.c` (+ `.h`), `lj_seq_form.c`, `lj_cue_form.c`, `lj_cuelist_form.c`, `lj_bgcue_form.c`, `lj_statics2_form.c`, `lj_statics2_prefs.c`, `lj_movemacro_editor.c`, `lj_position_presets.c`, `tests/ui/test_show_dialogs.c` (new), `tests/ui/test_movement_w3.c` (append) | the 16 SHOW ids + `LJ_WIN_SAVE_PRESET` (row or deleted with a fidelity row), the BG slot Edit Sequence item | W4-DLG-7, W4-DLG-0 (select startup cue list, sort / offset time code with the ME-120 items alive, position options; the "ponytail, W4" rows closed), ME-127, LORD_FIDELITY-8, LORD_FIDELITY-10/-11 (statics2-popup.png with both Replace Page rows, Fan Out auto-close script row), W3 integration s7 cue-list rows (Action-linked items, TC source, SyncFrames, `lj_cuelist_form_save` copy), cue-slot and Statics 2 Q9 prompts, ME-11 |
| dlg-files | `src/windows/lj_file_dialogs.c` (+ `.h`), `lj_user_libraries.c`, `lj_backup_restore.c`, `lj_system_prefs.c`, `src/model/lj_restore_data.c/.h` (new, our format), `src/model/lj_library.c`, `src/app/lj_startup.c`, `docs/file-formats.md` (append), tests `tests/ui/test_file_dialogs.c`, `tests/model/test_restore_data.c` (new) | the 19 FILES ids + `LJ_WIN_PLEASE_WAIT` (the startup status window) | W4-DLG-1 (+ Restore folder tree, archive input untrusted), W4-DLG-2 (format ours and documented, "the password is not a security boundary"), W4-DLG-4 (F56, NIT-28), ME-130, W2 LORD_C-5 (.lj4b path length / directory entries), W4-WIN please_wait, restart / password / startup status / Delete Files Q9 prompts |
| dlg-desktop | `src/windows/lj_desktop_dialogs.c` (+ `.h`), `lj_warnings.c`, `lj_prefs.c` (Startup Options: auto-launch, Select Startup Cuelist button), `src/desktop/lj_desktop_tabs.c`, `lj_selection.c`, `tests/ui/test_desktop_dialogs.c` (new) | the 13 DESKTOP ids + `LJ_WIN_DEFAULT_OUTPUT` (row or deleted) | W4-DLG-6, W4-DLG-3 (`Notification !` x2, TUSBWarningForm fidelity row text), W4-LASER, W4-WIN default_output, LORD_FIDELITY-12 (warnings centred, LStatus table), F13 |
| hw-dmxin | `src/windows/lj_dmx_in.c` (+ `.h`), `lj_global_patch.c`, `lj_hardware_setup.c`, `src/desktop/lj_toolbars.c`, `src/desktop/lj_main_window.c`, `tests/ui/test_dmx_in.c` (new), `tests/ui/test_desktop.c` (append) | the 5 DMXIN ids; the entry points of the 17 dropped-hardware forms (2510, 2518, MC-X, MCI-CD, SandNet, Capture, LJ-Manager prefs, DA TC scale, External Application W5, USB warning) present, greyed, hint `Not available in LJ4EVR` | W4-DMX (I7 "window W4, input W5"; no sACN receive code), W4-HW-0, F8 (CD / MC-X / 2518 greyed, DMX-in opens), I13, ME-11 |
| da2532-fingers | `src/windows/lj_da2532.c`, `lj_fingers.c` (+ `.h`), `src/io/lj_actions.h` + `lj_actions.c` (append LJ_FNF_EXT rows), `src/io/lj_midi.c` (learn for those rows), `tests/io/test_midi_map.c` (append), `tests/ui/test_da2532_fingers.c` (new) | the 7 DA2532 and 10 FINGERS ids; TPrint2532Form greyed (fidelity row) | W4-2532, W4-FINGERS, I8, I9, F8 (2532 opens TDA2532FORM_3), ME-23 (MIDI posts to the UI thread; no `lj_engine_` / `lj_sacn_` in `io/lj_midi_*`) |
| lightcorder-debug | `src/windows/lj_lightcorder.c`, `lj_debug_form.c` (+ `.h`), `src/model/lj_llf.c/.h` (new), `docs/file-formats.md` (append .LLF), `tests/model/test_llf.c`, `tests/ui/test_debug_form.c` (new) | the 2 LIGHTCORDER and 4 DEBUG ids | W4-LC, W4-DBG (unhide PROPOSAL, hardware tabs greyed, no CLUBLITE_EXTERNALMAP row), ME-24, ME-25..28 (layer_of[] in the Debug window), F20, design 13 item 10, ME-2 |
| art | `art/**`, `tools/artpack.py`, `tools/glyphmap.tsv` (append), regenerated `src/gfx/lj_art_gen.c`, `src/ui/forms/lj_forms_gen.c`, `tools/dfm2c_blobs.txt` (through dfm2c), `src/gfx/lj_glyphs.c/.h`, one new `src/platform/sdl/lj_sdl_srcimg.c` (+ `.h`), `src/desktop/lj_fixture_icons.c`, palette tile lookup in `src/controls/lj_ctl_colors.c` / `lj_ctl_gobos.c`, `tests/fonts/test_art.c` (append), `tests/gfx/test_srcimg.c` (new) | 210 glyphs, 58 icons, `art/gels.txt` swatches, 31x30 gobo tiles, family silhouettes, Smoke bar and button art | W4-ART-1, W4-ART-2, W4-SRCIMG, ME-18 |
| docs-release | `src/windows/lj_help_viewer.c` (+ `.h`), `help/*.txt` (new), `src/windows/lj_about_form.c`, README.md, NOTICE, LICENSE (after the ruling), CONTRIBUTING.md, SECURITY.md, `docs/provenance.md`, `docs/user-guide.md`, `docs/building.md`, `docs/releasing.md`, `docs/generated-files.md` (append), `CMakeLists.txt` install / CPack section only, `packaging/**`, `.github/workflows/release.yml`, `tools/help_check.py` (new) | `LJ_WIN_HELP`; THIRD_PARTY_NOTICES in both installers; the public docs set | W4-HELP, W4-MAC, ME-36 (both rows), ME-37 provenance line, design 13 item 1 text (LICENSE / About / NOTICE / CPack agree with Joey's ruling), WIZARD_STYLES-8 (`.bez`, Override\, Import\, Export\ in file-formats.md), LORD_FIDELITY-10 About MIT tagged constant, ME-19 |

Not tracks (integrators, the design owner, Joey): ME-124 (design owner, vault); ME-128 rank-6 16-bit delta, ME-131
census at the RC, W4-CAP / W4-Q9 tally (captions), W4-GOLD (goldens), W4-FID (fidelity.md, every track's rows), W4-FRAME
(gfx: checkerboard, popup metrics, caption glyphs, focus rect, secondary monitor, poDesigned, comctl32 minimum tab width),
LORD_ENGINE-8 / -9, LORD_C-7 / -9 / -10, W2 LORD_C-4 / -8, LORD_FIXTURES-8, LORD_C-8 Execute File, LORD_FIDELITY-19
restart re-exec, WIZARD_STYLES-12, W2 integration s7 / s9, ME-66, the caret row, LORD_C-14 / WIZARD_STYLES-11, design 13
items 2, 6, 7, 11, 13 records, every REL row and the HONOUR checks. Joey: the licence ruling, his ME-37 view, ME-45
(non-admin install), ME-21 (Mac run), the history-rewrite ruling (REL-PRIVATE) and the go for REL-FLIP.

### Checks each track leaves and the GIFs it compares against

- **dlg-config**: `test_config_dialogs` (each id opens from its entry point headless with ME-16 clean; F43 address rows;
  the F44 copy filter; `Save Ayway` through `assert_prompt`), the ME-126 row on the synthetic v1 record in
  `test_profile_editor`, fuzz_gs2 60 s clean. GIFs: addfixturesdialog, autoaddressing, clearfixturedata,
  configurationwarnings, copyfixture, copyfixture2, findprofiledialog, fixtureinfo, colorscrollersetup, scanner1defdialog
  (+ scanner1*dialog), submasterpatchdialog, selectbitmapdialog2, userbitmaplist, rgbpalette, rgbpaletteentry,
  selectingpalettetypes, applydefaultprompt, importprofilefilefilter, exportlist, userdeffixtureinfotab, special.
- **dlg-show**: `test_show_dialogs`; a script row per Q9 prompt; W3MM_MOVEMACRO re-rendered for ME-127 (preview 128x128 at
  (3,3), cross at 63) with its REVIEWED row. GIFs: cuebuilder, cuebuildertrigbuttons, deletescenesdialog, insertdialog,
  seqslotlabel, seqslotlabeldialog, staticcontrol, staticsreplacematrix, cuelistoffsettimecode, cuelisttimecodecontrols,
  movementoptions, movementmacro (1:1, 447x243), bgcue-sequencemode, listofbgcues.
- **dlg-files**: `test_file_dialogs`, `test_restore_data` (create, change password, restore round trip; truncated and
  hostile archives refused), a synthetic older-format record that raises each format-update dialog, ME-130's fresh-library
  row. GIFs: backupdatadialog, backupdatapswdialog, datarestoreerrordialog, userlibbackup, userlibrestore.
- **dlg-desktop**: `test_desktop_dialogs`; `assert_prompt` rows for `Notification !`, `Warning!` and
  `Enable laser output ?`. GIFs: desktopgrid, desktopwithimage, desktop2, iconsondesktop, icontrans, iconnontrans,
  nextprevfixturedialog, nextprevfixturecustom, nextprevfixturebuttons, saveselectiondialog, mainpopdesktop, config desktop.
- **hw-dmxin**: `test_dmx_in` (opens from `LJ_ACT_DMX_IN` and the Global Patch source; every input control disabled with
  the hint), `test_desktop` rows for each greyed hardware entry, `git grep` shows no receive socket. GIFs: dmxindialog,
  dmxintranslation, dmxintranslationedit, dmxinicon, dmxincue, dmxinflashseq, globalpatch, globalpatchdmxindisableat0,
  cuelisttoolbar, capturestatus, ljmanagerprefs, a2510*, 2518*, datcscaling, cdcontrol.
- **da2532-fingers**: a `midi` script row latching and flashing a 2532 key; `test_midi_map` rows for the new EXT rows;
  keyboard rows; Arial Black on the Fingers form (`arialblack18`). GIFs: 2532colorscheme, 2532control_1, 2532matrixinfo,
  2532matrixkeys, 2532pic, 2532userkeys, a2532print, a2532small, directaccessicon, fingers, fingersalternativeskin,
  fingerssetup*, fingersstatus*, fingers*buttons, fingersfaders.
- **lightcorder-debug**: `test_llf` (round trip, the card-size cap with the exe message, truncation refused),
  `replay_dump` sha unchanged, `test_sacn_rate` unchanged while recording, a drop-counter row in `test_debug_form`. No CHM
  GIF exists (both features are undocumented); DFM numbers only.
- **art**: `artpack --check`, `art_table`, `test_art` rows (every built-in profile resolves to an icon, every palette
  entry to a swatch or tile: 0 unresolved), `dfm2c_blobs.txt` `no-glyphmap-row` 105 -> 0, `test_srcimg` (synthetic BMPs,
  no folder, a truncated BMP). Sizes from the 1:1 GIFs of note 07 s14 only; nothing traced (ME-18).
- **docs-release**: `test_desktop` shows every Help leaf bound and the viewer opening; `help_check` CTest (no 8+ word run
  from the reference help texts, no FR/DE); the macOS CI log carries `lipo -info`, `codesign -dv` and `plutil -p`;
  THIRD_PARTY_NOTICES.txt inside `LJ4EVR.app/Contents/Resources` and beside `lj4evr.exe`.

## 3. The seams

1. **Window families.** `enum lj_win` gains 102 ids (`lj_windows.h`, W4 block, each with its DFM class and entry point).
   `lj_windows.c` keeps the W1-W3 `lj_win_table` and adds a family table: an id with no table row dispatches by id through
   its family's `open / close / get / refresh(enum lj_win)` in one behaviour file per family; `lj_win_set_host` calls each
   family's `*_bind_fns`. The ten stubs log `lj_win_open: <id> has no behaviour file yet` and open nothing. A track replaces
   its stub bodies and may add declarations below its header's contract block; it never edits `lj_windows.c`. The typing
   dialogs of `lj_win_is_dialog` are listed there already (a track that finds another tells the integrator).
2. **Entry points (pre-routed, `src/app/lj_app_w4.c`).** `LJ_ACT_FIXTURE_INFO` / `FIXTURE_NOTES` (arg = fixture,
   `lj_config_dlg_target` first), `AUTO_ARRANGE`, `DESKTOP_GRID`, `DESKTOP_IMAGE`, `DESKTOP_TABS`, `ICON_STYLE`,
   `NEXTPREV_SETTINGS`, `RESTORE_DATA`, `HELP`, `SELECT_HELPFILE` (only English ships), `WINDOWS_MEMORY`, and three new
   actions: `LJ_ACT_DEBUG_WINDOW` (View > De&bugWindow, hidden in the DFM: the debug track unhides it with
   `lj_menu_set_visible` from its own file, method PROPOSAL), `LJ_ACT_FINGERS_SETUP` (&Fingers > Setup, no longer greyed),
   `LJ_ACT_DMX_IN` (C-L toolbar DMX In, still disabled until hw-dmxin enables it; also the Global Patch source).
   `LJ_FN_2532_CONTROL`, `LJ_FN_VIRTUAL_FINGERS` and `LJ_FN_FINGERS_STATUS_PANEL` are already routed by the desktop and
   are bound by the da2532-fingers track's `*_bind_fns`. Cuelist Tools > Record to LightCorder file opens
   `LJ_WIN_LIGHTCORDER`. Every other entry point lives inside a window its own track owns.
3. **LightCorder frame tap (ME-2, ME-24).** `lj_app_w4_idle(now)` runs `lj_lightcorder_step(now)` on every UI loop pass
   after `lj_app_cuelist_step`. The recorder copies `lj_engine_frame(ctx->engine)` under `lj_lock` at its chosen rate
   (10 / 20 / 40 Hz) and writes `<library>\LightCorderFiles\*.LLF` on the UI thread. The engine gains nothing.
4. **Debug counters.** `lj_engine_status` already carries `tick_us_last`, `tick_us_max`, `drops`, `overruns`, `tick_no`
   and `lj_frame.layer_of[]`; the Debug window reads the app's view copy and a frame copy under the lock. A counter the
   DFM shows that is not there is requested from the integrator (engine files are not track files); until then its label
   reads `n/a` with a fidelity row.
5. **2532 and Fingers on keyboard / MIDI (D16).** The track appends `LJ_FNF_EXT` rows to the `LJ_FN_LIST` X-macro (2532
   latch 1-8, flash 1-8, user keys 1-18, page and bank; Fingers F1-F10, slots, faders with the `LJ_FN_MASTER_LEVEL`
   level convention, bumps, GM, B-O, jog wheels) and binds them with `lj_fn_bind`; the MIDI-IN map binds a note or CC to
   them, so `lj_app_midi_dispatch` reaches them on the UI thread (ME-23). EXT rows stay out of Edit Hotkeys.
6. **Help.** `lj_help_viewer_topic(topic)` opens the bundled viewer (`LJ_WIN_HELP`, code-built) at `help/<topic>.txt`,
   NULL = the index. Topics are our own text, installed by both installers.
7. **Captions (W4-CAP).** `dump_captions` + `tools/caption_diff.py` + `tools/caption_diff_excl.tsv` (form, object,
   property, the fidelity row; created by the captions owner). Runtime captions are compared after the window's
   behaviour has run.
8. **Source-Images.** `platform/sdl` decodes `<userdata>/Source-Images/{Glyphs,Fixtures,Colors,Gobos}/*.bmp` with
   `SDL_LoadBMP` and hands `lj_surface`s to `gfx/lj_glyphs.c`, which substitutes them by LJ2 file name with the
   bottom-left key; `gfx` keeps no decoder (design 7.8). All in the art track.
9. **Product name (ME-37).** `LJ_MAIN_CAPTION_FMT` only. The About string is `lj_win_about_text` (docs-release).

## 4. Window ids by family (entry points)

**CONFIG** (`windows/lj_config_dialogs.c`, `lj_config_dlg_*`, track dlg-config): `add_fixtures` TAddFixForm2 (Fixture Configuration bAdd / Add Fixture(s) [02 s2.3]); `apply_default_dmx` TApplyDefaultDMXForm (progress box while Default DMX output is applied (Setup > Default Output) [02 s7]); `apply_multi` TApplyMultiForm (Fixture Configuration: apply a user profile to the selected fixtures [02 s2]); `auto_address` TAutoAddressForm2 (Fixture Configuration Calculate addresses (Auto) [02 s2.6, F43]); `cleanup` TCleanUpForm (Fixture Configuration Clear Fixture Data [02 s2.10, F44]); `clone` TCloneForm (Fixture Configuration Copy Fixture [02 s2.9, F44]); `copy_from` TCopyFromForm (Fixture Configuration Copy From Fixture (F44 copy filter) [02 s2.9]); `config_warn` TConfigWarnForm (Fixture Configuration Save with conflicts: Warnings / 'Save Ayway' [F41, 02 s2.8]); `export_warning` TFixtureExportWarningDialog (Fixture Configuration Create Export File / Export All warning [02 s5]); `file_exists` TFileExistsForm (Export when the target file exists [02 s5]); `find_profile` TFindProfileForm (Fixture Configuration profile tree Find profile.. [02 s2]); `fixture_info` TFixtureInfoForm (icon popup Fixture Info (LJ_ACT_FIXTURE_INFO, arg = fixture) [02 s2.13]); `fixture_notes` TFixtureNotesForm (icon popup Fixture Notes (LJ_ACT_FIXTURE_NOTES, arg = fixture) [02 s2.13]); `fixture_special` TFixtureSpecialForm (Fixture Configuration Special [02 s2]); `hidden_fixture` THiddenFixtureForm (Fixture Configuration hidden-fixture flag [02 s2]); `intensity_patch` TInternalIntensityPatchForm (Patch Sub Masters (Fixture Configuration / Master Intensity) [04 s11]); `scanner1_defs` TScanner1DefsForm (icon popup Define Generic Scanner1 profile [02 s5.5]); `color_scroller_defs` TColorScrolDefForm (icon popup Define Color Scroller profile [02 s5.6]); `rgbpack_defs` TRGBPackDefForm (icon popup RGB pack configuration [02 s5.7]); `palette_value_type` TPaletteValueTypeForm (Customize Fixture palette value type [02 s4.3]); `profile_name_oldstyle` TProfileNameOldStyleForm (Generic Fixture Profile on a format-0/1 .GS2 (ME-126) [16 s2]); `profile_import_modified` TProfileImportModifiedFilesForm (Import Profile when a modified file exists [02 s5]); `bitmap_picker` TBitMapPickerForm2 (palette editors Select Bitmap (31x30 library) [07 s12]); `get_bitmap` TGetBitmapForm (Generic Fixture Profile Icon Bitmap Set [16 s4]); `rgb_palette` TRGBPaletteForm (RGB pack Colors > RGB Palette [04 s5.3])

**SHOW** (`windows/lj_show_dialogs.c`, `lj_show_dlg_*`, track dlg-show): `cue_stack` TCueStackForm (Cue control menu Cue Builder, hotkey Cue Builder [03 s5.8]); `modal_select_cue` TModalSelectCueForm (Cuelist edit panel Set Current Cue / cue pick [03 s6]); `delete_scene` TDeleteSceneForm (Sequence window Delete (LJ_SB_DELETE) [03 s4.3]); `insert_scenes` TInsertScenesForm (Sequence window Insert [03 s4.3]); `set_sequence` TSetSequenceForm (Sequence window Start Sequence at : [03 s4]); `seq_used` TSeqUsedForm (Delete Sequences Check Used Sequences / Mark Unused [01 s8]); `select_bgcue_old` TSelectBGCueForm (BG slot Edit Sequence / cue-list Load BG Cue picker [03 s7]); `save_static` TSaveStaticForm (Statics Control Save [03 s8, 14 sB.2]); `statics_control` TStaticsControlForm (View > Controls Statics Control [03 s8, 14 sB.2]); `static_slot_name` TStaticSlotNameForm (Statics 2 slot popup Name [14 sB.3]); `statics_replace` TStaticsReplaceForm (Statics 2 Replace matrix [14 sB.3]); `name_slots` TNameSlotsForm (Cue control Sequence Slot Labels [03 s5]); `select_cuelist_modal` TSelectCuelist2ModalForm (Startup Options Select Startup Cuelist [01 s6.2]); `sort_timecode` TSortTimeCodeForm (Cuelist Tools Sort Waits (ME-120) [03 s6]); `timecode_offset` TTimeCodeOffsetForm (Cuelist Tools Offset TimeCode (ME-120) [03 s6]); `movement_options` TMovementOptionsForm (Movement window Options > Position Options [04 s4]); existing ids `LJ_WIN_SAVE_PRESET`

**FILES** (`windows/lj_file_dialogs.c`, `lj_file_dlg_*`, track dlg-files): `backup_progress` TBackupForm (Backup User Library progress 'Backing Up' [01 s8]); `restore_lib_progress` TRestoreLibProgressForm (Restore User Library progress [01 s8]); `copying_files` TCopyingFilesForm (User Libraries copy progress [01 s8]); `rename_file` TRenameFileForm (User Libraries Rename [01 s8]); `progress` TProgressForm (generic file progress [01 s8]); `print_wait` TPrintWaitForm (print spool wait (Print stays greyed: fidelity row) [01 s8]); `restore_data` TRestoreDataForm (System > Restore Program Data (LJ_ACT_RESTORE_DATA), System Preferences Data Restore tab [06 s4.2]); `create_data` TCreateDataForm (Creating Auto Restore Data (System Preferences Data Restore) [06 s4.2]); `compress_restore` TCompressRestoreDataForm (Compressing Restore Data progress [06 s4.2]); `restore_progress` TRestoreProgressForm (Restoring Files progress [06 s4.2]); `new_password` TNewPasswordForm (System Preferences Data Restore Enter New Password [06 s4.2]); `update` TUpdateForm (first start on an older record format 'Dataformat has Changed' [F56, 12 Q9]); `update_file` TUpdateFileForm (format update progress 'Updating Files' [F56]); `update_program` TUpdateProgramForm (installation update progress [F56]); `update_bgcues` TUpdateBGCuesForm (older BG cue files 'File Format Update Needed' [F56]); `update_cl2` TUpdateCl2Form (older .Cl2 'Datafile format update required' [F56, 12 Q9]); `patch_update` TPatchUpdateForm (older patch 'Update to Patch Configuration' [F56, 12 Q9]); `rebuild_setup` TRebuildSetupForm (setup rebuild 'Please Wait...' [F56]); `init_form` TInitForm1 (first-start splash TInitForm1 [F56]); existing ids `LJ_WIN_PLEASE_WAIT`

**DESKTOP** (`windows/lj_desktop_dialogs.c`, `lj_desktop_dlg_*`, track dlg-desktop): `auto_arrange` TAutoArrangeForm (desktop popup Auto-Arrange Icons (LJ_ACT_AUTO_ARRANGE) [01 s3.5]); `desktop_grid` TGridForm (desktop popup Desktop Grid (LJ_ACT_DESKTOP_GRID) [01 s3.5, F13]); `icon_style` TIconStyleForm (desktop popup Select Icon Style (LJ_ACT_ICON_STYLE) [01 s3.5]); `desktop_image` TImageSetupForm (desktop popup Desktop Image (LJ_ACT_DESKTOP_IMAGE) [01 s3.5, F13]); `desktop_tabs` TLayerManagerForm (desktop popup Desktop tabs (LJ_ACT_DESKTOP_TABS) [01 s3.5]); `nextprev_selection` TNextPrevSelectionForm (right-click prev/next (LJ_ACT_NEXTPREV_SETTINGS) [01 s3.3]); `custom_order` TCustomOrderForm (Next/Previous Fixture Selection Custom order [01 s3.3]); `save_selection` TSaveSelectionForm (Fan Out named orders / Custom order Save [01 s3.3, 17 s5.2]); `auto_launch` TAutoLaunchForm (Startup Options auto-launch programs, behind the Execute File rule (LORD_C-8) [01 s6.2]); `hotkey_warning` TShowHotKeyWarningForm (Edit Hotkeys / Select Function 'Notification !' (Main.Ini [MISC] ShowHotkeyWarning) [01 s7]); `hardware_warning` TShowHardwareWarningForm (startup without transmitter hardware 'Notification !' [12 Q9]); `laser_warning` TLaserWarningForm (laser output active 'Warning!' Laser(s) emitting! [12 Q9]); `unblock_lasers` TUnblockLasersForm ('Enable laser output ?' before laser output opens [12 Q9]); existing ids `LJ_WIN_DEFAULT_OUTPUT`

**DMXIN** (`windows/lj_dmx_in.c`, `lj_dmx_in_*`, track hw-dmxin): `dmx_in` TDmxInForm (C-L toolbar DMX In (LJ_ACT_DMX_IN), Global Patch DMX-in source [04 s15, 05 s3]); `dmxin_translation` TDMXInValueTranslationForm (Dmx In translation tables [05 s3]); `translation_edit` TTranslationEditForm (DMX-In translation Edit [05 s3]); `value_table_error` TValueTableImportErrorForm (DMX-In translation import error [05 s3]); `export_table` TExportTableForm (DMX-In translation Export Value tables (.vtr) [05 s3])

**DA2532** (`windows/lj_da2532.c`, `lj_da2532_*`, track da2532-fingers): `da2532` TDa2532Form_3 (C-L toolbar 2532 (LJ_FN_2532_CONTROL) [05 s4, D16]); `da2532_functions` TButtonFunctions2532Form (2532 Direct Access key function list [05 s4]); `da2532_colors` TColorScheme2532Form (2532 Direct Access Color Scheme [05 s4]); `da2532_tab_colors` TDA2532_TabColorsForm (2532 Direct Access Tab Colors [05 s4]); `da2532_tab_height` TDa2532TabHeight (2532 Direct Access Set Tab Size [05 s4]); `da2532_text` TEdit2532TextForm (2532 Direct Access Edit 2532 Text [05 s4]); `da2532_page_names` TName2532PagesForm (2532 Direct Access 2532 Page Names [05 s4])

**FINGERS** (`windows/lj_fingers.c`, `lj_fingers_*`, track da2532-fingers): `fingers` TLJFingersMainForm (&Fingers > Virtual Fingers (LJ_FN_VIRTUAL_FINGERS) [04 s17, 05 s5, D16]); `fingers_setup` TFingersSetupDialog (&Fingers > Fingers Setup (LJ_ACT_FINGERS_SETUP) [05 s5]); `fingers_status` TFingersStatus (&Fingers > Fingers status panel (LJ_FN_FINGERS_STATUS_PANEL) [05 s5]); `fingers_error` TFingersErrorForm (Fingers error box [05 s5]); `fingers_skin` TFingersSkinSelectForm (Virtual Fingers Select Skin [05 s5]); `fingers_functions` TFingersFunctionListForm (Fingers Setup F1-F10 function list [05 s5]); `fingers_jog_functions` TFingersJogFunctionListForm (Fingers Setup Datawheel function list [05 s5]); `fingers_page_functions` TFingersPageFunctionListForm (Fingers Setup fader page function list [05 s5]); `fingers_slot_functions` TFingersSlotFunctionListForm (Fingers Setup Cue Slot functions [05 s5]); `fingers_ud_functions` TFingersUDFunctionListForm (Fingers Setup Scroll button function list [05 s5])

**LIGHTCORDER** (`windows/lj_lightcorder.c`, `lj_lightcorder_*`, track lightcorder-debug): `lightcorder` TLightCorderDialog (Cuelist > Tools Record to LightCorder file (M_RecordtoLightCorderClick) [13 s1.2]); `save_lightcorder` TSaveLightCorderFileDialog (Record to LightCorder file Save [13 s1.2])

**DEBUG** (`windows/lj_debug_form.c`, `lj_debug_form_*`, track lightcorder-debug): `debug` TDebugForm (View > De&bugWindow, hidden (LJ_ACT_DEBUG_WINDOW; unhide method PROPOSAL) [13 s1.2, design 13 item 10]); `statics_debug` TStaticsDebugForm (Debug window Statics debug [13 s1.2]); `msg_debug` TMsgDebugForm (Debug window WM_CPOPYDATA DEBUG [13 s1.2]); `memory_view` TMemoryViewForm (View > System Memory (ViewControlClick Tag 7, LJ_ACT_WINDOWS_MEMORY) [01 s4])

**HELP** (`windows/lj_help_viewer.c`, `lj_help_viewer_*`, track docs-release): `help` THelpViewer (every Help leaf (LJ_ACT_HELP, F1): the bundled text viewer, code-built (design 13 item 12))

## 5. Deviations and open points of this seat

- **Push.** The brief authorised `git push`; this session's permission system refused it for the track-0 commit, so both
  commits (`97fe7b4` track 0 and the contracts commit) are local on `main` until someone with push rights runs
  `git push origin main`. Nothing else depends on the push; the order rule is checked with `git log`.
- **design.md** is the vault's and read-only here: the ME-44 design 2.0 sentence and every ME-124 sentence (including
  "the DMX-in window opens in W4 with its inputs greyed; the input path is W5" for 10 W4 and 12 F8) are the design
  owner's.
- **ME-129 before the fork** was an integrator ENTRY row; it landed here because six of the files it edits are W4 track
  files. ME-128 and ME-130 did not: ME-130 is now dlg-files' (it owns `lj_library.c` and `lj_startup.c`), ME-128 stays
  with the integrator.
- **`LJ_WIN_PLEASE_WAIT`** is in the FILES family (the startup status window lives in `app/lj_startup.c`, which dlg-files
  owns); `LJ_WIN_DEFAULT_OUTPUT` in DESKTOP and `LJ_WIN_SAVE_PRESET` in SHOW. Each track gives its id a body or deletes
  it with a fidelity row (W4-WIN); deleting an id is the one edit of `lj_windows.h` / `.c` a track may make, through the
  integrator.
- **Startup Options** (`windows/lj_prefs.c`) is dlg-desktop's; the Select Startup Cuelist button there opens
  `LJ_WIN_SELECT_CUELIST_MODAL`, whose body is dlg-show's.
- **The captions measurement** ran the Release app against the developer's own library with `%APPDATA%\LJ4EVR` backed
  up and restored afterwards (`U:<name>` did not switch the library in that run; dlg-files' ME-130 covers the `U:`
  path). It is a measurement, not a gate: W4-CAP's 0 is the captions integrator's.
