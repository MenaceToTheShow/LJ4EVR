# W3 contracts: the headers nine tracks code against

Written 2026-10-22 by the W3 contracts seat. Every header compiles alone (`tools/test_layering.py`, cl: 386 files,
184 headers, 0 findings), every library links with stub bodies, `LJ_SDL` ON and OFF build on this PC with 0 warnings,
and the W2 `ctest` rows stay green (evidence at the end). Design sections 2.2-2.5, 2.9, 3.4, 3.6, 3.7, 4, 5.2, 5.3,
6.1-6.2, 8.3-8.5 and 10 W3 are the source; `docs/w3-gates.md` is the gate list this page serves. Deviations and why
are the last section. Shared files stay append-only for every track: `CMakeLists.txt` tests section,
`tests/lj_tests.c`, `tools/glyphmap.tsv`, `src/io/lj_actions.h`, `src/app/lj_app.c` bindings, `tools/gen_vectors.py`,
`tools/test_layering.py` allowlists (ME-15).

**Gate order note.** `docs/w3-gates.md` makes ME-100 and ME-101 PRE-FORK ("the commit precedes
`docs/w3-contracts.md`"). Neither is in `git log` as of this writing (HEAD `63d1f3a`, v0.2.0). These contracts are
uncommitted; the integrator lands ME-100 / ME-101 first and commits this set after them. Nothing here depends on
either (no file of this set touches `lj_window.h`, `lj_ini.c`, the widget group headers or `lj_cuepage.c`).

## 1. Map (what changed per header)

| Header | Adds | Owner (bodies) |
|---|---|---|
| `model/lj_model.h` | `lj_cl_line.ref` (sequence / bg cue / cue list of a drag-only command) and `.load_error` (design 8.5); `lj_gm_param` (the generic-macro channel struct, was anonymous; layout unchanged); `lj_fixture.ch_caption[32][6]`, `.ch_gm_off` (ME-32); `lj_gdc_groups`; `lj_show.gs2 / ngs2` (parsed user profiles), `.bez` (sampled Bezier shapes), `.gdc` | **done** (data) |
| `model/lj_cuelist.h` | `enum lj_cl_cmd` (in `lj_cl_cmd_words` order, `_Static_assert`ed), `lj_cl_cmd_caption[]` (TCuelist2Form action / menu captions) and `lj_cl_cmd_grid[]` (exe pool grid texts), `lj_cl_adv_caption[]`, families + `lj_cl_cmd_inert` / `lj_cl_cmd_is_wait`, `LJ_CL_NOT_AVAILABLE`, time code packing `lj_cl_hmsf_ms` / `lj_cl_ms_hmsf` / `lj_cl_tc_text`, `lj_cl_status` (what the runner publishes), `enum lj_cl_state / lj_cl_tc_src / lj_cl_line_state / lj_cl_op`, `LJ_CL_OP_ARG`; the parameter encoding per command in the header comment | tables and helpers **done** (`test_cuelist`); `ref=` / `load_error` in load / save / validate: cuelist-core |
| `model/lj_genmacro.h` | `LJ_GM_SAMPLES / SHAPES / SHAPES_MAX / FULL / DELAY_MAX / CYCLE_MAX_DS`, `lj_genmacro_ch_used` (**done**), `lj_gm_shapes` (the shape table record), `lj_gm_shapes_load / free` (Macros.Mac layout of 14 sA.6, ME-34 override), `LJ_GM_OVERRIDE_DIR`, `enum lj_gm_delay_style / lj_delay_order`, `lj_genmacro_auto_delay` | genmacro |
| `model/lj_movemacro.h` (new) | `LJ_MM_TYPE_*` (0 none, 1..6 system in exe order, 7+ Bezier), `lj_mm_type_names[]` (**done**, verbatim), `enum lj_mm_param` (the fstate rows), `lj_bez_node / lj_bez / lj_bez_table / struct lj_bez_set`, `lj_bez_load / save / validate / sample`, Fan Out `lj_fan_out_offsets / lj_fan_out / lj_fan_spread` (17 s5.2), Auto Delay `lj_mm_auto_delay`, `lj_delay_order` | movement |
| `model/lj_followspot.h` (new) | `LJ_FOLLOWSPOT_FILE` (`FollowSpot.dat`, PROPOSAL), load / save / validate, `lj_followspot_record`, `lj_followspot_apply` (into the current scene, all Snap, defaults for unprogrammed channels) | gdc-fades-followspot |
| `model/lj_gdc.h` (new) | the F35 grammar as `enum lj_gdc_op`, `lj_gdc_cmd / lj_gdc_prog / lj_gdc_state`, `lj_gdc_parse / lj_gdc_is_generic / lj_gdc_run`, `LJ_GDC_GROUPS_FILE` + groups load / save | gdc-fades-followspot |
| `model/lj_seqfades.h` (new) | `enum lj_attr` (the 8 check boxes), `enum lj_sfx` (the 9 RG1 transforms), both caption tables (**done**), `lj_seqfades`, `lj_ch_attr`, `lj_seqfades_apply` | gdc-fades-followspot |
| `model/lj_show.h` | `lj_show_profile` / `lj_show_gs2` (**done**), `lj_show_gs2_load_all / free` (free **done**, `lj_show_free` calls it), `lj_fixture_custom_load / save` (`Setup.<slot>.pal`) | gs2 (load_all); profile-editor (custom palettes, `chcap` / `chgm` in `lj_config_load/save`) |
| `engine/lj_engine.h` | kinds `GENMACRO_ON GENMACRO_RESTART GM_SHAPES MACRO_FADE` (table in the header), `LOAD_GENMACRO c = enum lj_gm_src` (cue / editor), `enum lj_gm_src`, `enum lj_mm_ctl`, `lj_cmd.gm_shapes`; `lj_engine_status` gains `gm_engine_on gm_slot_on gm_cue_number gm_editor_number mm_amp_pct mm_speed_pct mm_fade mm_frozen mm_idx_pan[100] mm_idx_tilt[100]` (defaults set in create) | engine parts of genmacro / movement |
| `engine/lj_engine_rules.h` | every W3 constant with its rig comment (`LJ_GENMACRO_*` incl. `AMP186_EXAMPLE`, `RANK_ABOVE_BG`, `16BIT_SCALE`, `EDITOR_AFTER_CUE`; `LJ_MOVEMACRO_PCT_MAX / FADE_LINEAR / DELAY_UNIT / STEP_MAX / BEZ_EQUAL_TIME`; `LJ_FOLLOWSPOT_ALL_SNAP`; `LJ_CL_STEP_MS / MANUAL_IGNORES_WAITS / GOTO_BEYOND_IGNORED`; `LJ_MTC_TIMEOUT_MS / LATE_FRAMES`) | **done** |
| `engine/lj_genmacro.h` | `lj_genmacro_shape_in(sh, shape, k)` and the pure `lj_genmacro_step(sh, param, idx, ref, wide)` (bodies **done** over the W2 arithmetic; the shapes are the stub's sine fallback until `lj_shapes_gen.c` lands) | genmacro |
| `engine/lj_shapes_gen.h` (new) | `extern const lj_gm_shapes lj_gm_shapes_builtin` from `lj_shapes_gen.c` (**stub**, n = 0) | genmacro (`gen_vectors.py --shapes`) |
| `engine/lj_movemacro.h` | the pure `lj_movemacro_pos(state, macro, bez, amp_pct, base, &pan, &tilt)` (system types **done** through `lj_movemacro_offset`, Bezier types by the track) | movement |
| `fixlib/lj_profile.h` | `lj_profile.gs2_file[64]` | **done** |
| `fixlib/lj_gs2.h` | `LJ_GS2_ERR_SIZE` / `LJ_GS2_ERR_CRC` (the oracle's assert texts), `LJ_UDF_FOOTER`, `LJ_GS2_VERSION`, `LJ_GS2_LEGACY_MANUFACTURER` (ME-33), `lj_gs2_read(buf, len)`, `lj_gs2_load / save`, `lj_udf_parse(buf, len)`, `lj_udf_export`, `lj_gs2_free` (**done**); lj_fixlib now links miniz | gs2 |
| `fixlib/lj_profile_editor.h` (new) | `enum lj_pe_tab` + `lj_pe_tab_caption[]`, `enum lj_pe_kind`, `lj_pe_field` and the complete field table `lj_pe_fields[]` (81 rows: every located control of 16 s4.2-4.12 with offset, width and DFM name; **done**, `test_profile_editor`), `lj_profile_edit` (struct + the raw record), `lj_pe_begin / refresh / set_u8 / set_str / validate`, `lj_pe_field_find` (**done**) | profile-editor |
| `core/lj_str.h` | `lj_str_rtf_strip` (design 5.3, LORD_FIDELITY-17) | gs2 |
| `io/lj_midi.h` | `lj_mtc` + `lj_mtc_reset / feed / time` (quarter-frame MTC), `lj_midi_feed` (LORD_C-6 byte-stream parser) | cuelist-core |
| `windows/lj_windows.h` | 22 `LJ_WIN_*` ids appended (section 4); `lj_win_is_dialog` covers the W3 typing dialogs (**done**) | see section 2 |
| `windows/lj_cuelist_form.h` `lj_genmacro_editor.h` `lj_movemacro_editor.h` `lj_gdc_form.h` `lj_profile_editor_form.h` `lj_gs2_dialogs.h` (new) | open / close / get / refresh per window id + one `*_bind_fns` per file; the `lj_win_table` rows, class strings, id strings and the `lj_win_set_host` calls are already filled, so no track edits `lj_windows.c` | see section 2 |
| `desktop/lj_desktop.h` | `lj_ctx` gains `cl` (runner status) `gm_edit` `gm_shapes` `gdc` `followspot`; 11 `LJ_ACT_*` appended (`LOAD_CUELIST CUELIST_OP LOAD_GENMACRO GENMACRO_APPLY GENMACRO_TO_CUE GM_SHAPES_RELOAD BEZ_RELOAD GS2_RELOAD GS2_EXPORT_ALL FOLLOWSPOT_APPLY FIXTURE_APPLY`) | desktop-app (binds them) |
| `app/lj_cuelist_run.h` (new) | `lj_cl_host` (every effect the runner has), `lj_cl_run`, `lj_cl_run_init / op / step / unpin`; the line semantics in the header comment | cuelist-core |
| `app/lj_app.h` | idle-pass order with `lj_app_cuelist_step`, the cue-list hotkey mapping, `lj_app_load_cuelist / gs2_reload / gm_shapes_reload / bez_reload / followspot` (stubs in `lj_app_seam.c`) | desktop-app |
| `app/lj_uiscript.h` | the eight W3 verbs (comment; `docs/ui-script.md` "W3 verbs") | desktop-app |

## 2. Who owns which files

| Track | Writes | Replaces the stub |
|---|---|---|
| cuelist-core | `model/lj_cuelist.c` (load / save / validate W3 rules), `app/lj_cuelist_run.c`, the app's `lj_cl_host` (in `lj_app_seam.c` or a new `app/lj_cuelist_host.c`), `io/lj_midi.c` MTC block + `lj_midi_feed`, `tests/model/test_cuelist.c` (append), `tests/app/test_cuelist_run.c`, replay `cuelist_run` + the harness `cl` / `clline` / `at cl_*` / `expect_cl` forms; ME-103, LORD_ENGINE-7 / -9, LORD_C-6 | `lj_cl_run_*`, `lj_mtc_*`, `lj_midi_feed`; enables `test_cuelist_run` |
| cuelist-window | `windows/lj_cuelist_form.c` (TCuelist2Form, Save / Select / Set Cue Trig / Log TimeCode, TclEditPanels, the CD / DA / MP rows greyed with `LJ_CL_NOT_AVAILABLE`, red load-error lines, Lock / Browse, marks, recent commands), `hotkey_function` behaviour file (W2 LOW row), cuelist goldens | the five rows of `lj_cuelist_form.c` |
| genmacro | `model/lj_genmacro.c` (shape 0 + bounce fixes, `lj_gm_shapes_load`, auto delay), `engine/lj_genmacro.c` (editor source, GENMACRO_ON / RESTART / GM_SHAPES, view fields), `tools/gen_vectors.py --shapes` (append) -> `engine/lj_shapes_gen.c`, `windows/lj_genmacro_editor.c`, `tests/engine/test_genmacro.c`, replays `genmacro_sine_amp186` `genmacro_vs_bgcue`; ME-34, ME-35 rule line, LORD_ENGINE-14, ME-28 | enables `test_genmacro`, `gen_vectors_shapes_check` |
| movement | `model/lj_movemacro.c`, `engine/lj_movemacro.c` (Bezier types, MACRO_FADE, view phases), `windows/lj_movemacro_editor.c` (Movement Macro + Bezier canvas, Delete Macros, Fan Out, Auto Delay, Cue Macro Control), `tests/engine/test_movemacro.c`, `tests/vectors.h` rows via `gen_vectors.py` (append), replays `movemacro_six_shapes` `fanout_auto_delay`; LORD_FIDELITY-9 / -12, save_preset fidelity row | enables `test_movemacro` |
| gdc-fades-followspot | `model/lj_gdc.c`, `lj_seqfades.c`, `lj_followspot.c`, `windows/lj_gdc_form.c`, `tests/model/test_gdc.c`; NIT-27 | enables `test_gdc_grammar`, `test_followspot` |
| gs2 | `fixlib/lj_gs2.c`, `core/lj_str.c` RTF stripper, `model/lj_show.c` `lj_show_gs2_load_all`, `windows/lj_gs2_dialogs.c`, Fixture Configuration Export All / Reload (ME-31) in `lj_fixture_config.c`, `tests/fixlib/test_gs2.c`, the two fuzz targets (LORD_C-17); ME-33, ME-9, LORD_FIXTURES-9 / -14, LORD_FIDELITY-17 | enables `test_gs2`, `test_udf_crc` |
| profile-editor | `fixlib/lj_profile_editor.c` bodies, `windows/lj_profile_editor_form.c` (Generic Fixture Profile, Customize Fixture, Channel Configuration), `model/lj_show.c` `chcap` / `chgm` + `lj_fixture_custom_*`, the S3 per-channel GM rule (ME-32, with the engine track's one line in `lj_post.c`), ME-105, NIT-26, the Martin locate button | appends to `test_profile_editor` |
| profiles | `fixlib/hand/*.c` for 15/06-15/09, `tools/qxf_map.tsv`, `exe_profiles_check.py` `WAVE_IDS` from `check_profiles.py TIERS` (ME-106) | enables `profiles_ids_w3` |
| desktop-app | `app/lj_app.c` / `lj_app_seam.c` (the W3 actions, `lj_app_cuelist_step` call, pending_genmacro, reloads, follow spot), `app/lj_uiscript.c` W3 verbs, `desktop/*` (tb_fixture_18 Follow spot, the cue-list toolbar), ME-102 / -104 / -45 / -21 and the W2 LOW rows of `w3-gates.md` | `lj_app_*` W3 stubs |
| integrator | `CMakeLists.txt`, `tests/lj_tests.c`, `tools/`, `src/ui/` (widgets W3 forms need: Bezier canvas, string grids, ME-17), `src/gfx/`, docs, ME-100 / ME-101 (PRE-FORK) | - |

Compiling standalone: as W1 (`docs/w1-contracts.md`). From a vcvars64 shell (drive it from PowerShell, w2-contracts
"hang"): `cmake -S . -B build-nosdl -DLJ_SDL=OFF -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build-nosdl &&
ctest --test-dir build-nosdl -C Release`; the app is `cmake --preset dev-msvc && cmake --build --preset dev-msvc`.

## 3. The seams

1. **Cue list runner <-> engine (design 2.3 rules 2-4).** `app/lj_cuelist_run.c` is a state machine over a pinned
   `lj_cuelist`, stepped by `lj_app_cuelist_step` on every 10 ms UI wake (`LJ_CL_STEP_MS`), never in the engine. It
   reaches nothing directly: every effect goes through `lj_cl_host` (`load_cue` = `lj_app_load_cue`, `post` =
   `lj_ctx_post` for the Set Cue Trig `LJ_CMD_TRIG`s with `b = 0xFF`, `flash` pins and releases a Flash Sequence,
   `load_bg`, `link`, `fn` = `lj_fn_run(fn, 1)`, `exec` gated by `[CUELIST2] AllowExecute`, `clock`, `timecode`). The
   replay harness supplies its own host at synthetic time, so `cuelist_run` and the app run the same code. The window
   reads `ctx->cl` (`lj_cl_status`) and acts through `LJ_ACT_CUELIST_OP`; the hotkeys `LJ_FN_CUELIST_*` map to the
   same ops. MTC: the MIDI path decodes quarter-frames into `lj_mtc` on the UI thread (`lj_app_midi_dispatch`, ME-23)
   and the runner's `timecode(LJ_TC_MIDI)` reads `lj_mtc_time`. CD / DA / MP lines are `load_error`: kept, skipped,
   saved back; their timecode sources return -1 and their buttons are greyed with `LJ_CL_NOT_AVAILABLE`.
2. **Generic macro pinned pointers (design 2.3 rules 2, 3, 6; ME-2, ME-102).** Two rank-6 sources: the cue's macro
   (`LOAD_CUE.gm` or `LOAD_GENMACRO c = LJ_GM_SRC_CUE`) and the editor's working record `ctx->gm_edit`
   (`LOAD_GENMACRO c = LJ_GM_SRC_EDITOR`, applied after the cue's, `LJ_GENMACRO_EDITOR_AFTER_CUE`). A cue whose
   `gen_macro` the UI could not pin arrives with `gm = NULL` and the engine writes `pending_genmacro`; the app's
   `lj_app_service_pending` loads `CueMacros\NNNNNNNN.CMA` and posts it; the old record is freed only after
   `lj_engine_cmd` returned 0 (ME-102). Parameter edits in the editor happen in place under `lj_lock` (one model
   mutation, design 3.1) and need no post; a changed channel set or restart flag runs `LJ_ACT_GENMACRO_APPLY`. The
   shape table is a third pinned pointer: `LJ_CMD_GM_SHAPES` hands the engine the user's override (ME-34) or NULL
   (the regenerated built-ins); the app frees the old table after the post.
3. **GS2 raw bytes <-> profile struct (design 5.3, 6.2; ME-9, ME-31, ME-33).** A user profile is parsed once into
   `show->gs2[]` with its 20,851 stream-0 bytes in `gs2_raw` (heap, freed by `lj_gs2_free`). `lj_gs2_write` copies
   `gs2_raw` and overwrites only the struct-owned fields, i.e. `serialize(parse(x))` of `16-gs2_ref.py`; export of a
   built-in starts from a zeroed record. A GS2 fixture finds its profile with `lj_show_gs2(show, fixture.gs2_file)`
   and the engine gets it through `lj_engine_bind_profile`. Reload: `lj_show_gs2_load_all` outside the lock, swap +
   rebind every GS2 fixture under it, free the old array after (2.3 rule 6). Untrusted input: every length / offset
   checked; oracle messages verbatim (`LJ_GS2_ERR_SIZE`, `LJ_GS2_ERR_CRC`).
4. **Editor working copy <-> fixlib.** The profile editor's working copy is the raw record (`lj_profile_edit.raw`):
   a control writes its bytes at the `lj_pe_fields[]` offset, `lj_pe_refresh` re-parses into `.p` (so struct and bytes
   never disagree and fields the struct does not own, the Offline Visualizer strings, are edited directly). Commit =
   `lj_pe_validate` + `lj_gs2_save` + `LJ_ACT_GS2_RELOAD`. Customize Fixture and Channel Configuration are
   per-fixture (`lj_fixture.custom[6]`, `ch_caption`, `ch_gm_off`), saved through `LJ_ACT_FIXTURE_APPLY`
   (Config.dat + `Setup.<slot>.pal`, `lj_engine_rebind`).
5. **UI-side scene writers (not engine commands).** Generic DMX Control, Sequence Fades, Follow spot recall, Fan Out,
   Auto Delay and the Movement Macro window write the current scene under `lj_lock` and post `LJ_CMD_CURRENT_SEQ`,
   like every fixture control (design 3.4: movement macros are not a layer; CHM: the Generic DMX control "adds data
   into the current scene"; 04 s13: follow spot "applies the stored parameters into the current sequence"). Bezier
   shapes reach the engine as `show->bez`, read per tick and swapped whole under the lock (`LJ_ACT_BEZ_RELOAD`).

## 4. Windows: the TFORM class each id binds

From `reference/installer/exe_resources/dfm_text/*.dfm.txt` (`object X: TClass`); sizes are DFM client sizes.
"code-built" = no DFM; its GIF is named and, not being in 07 s14's 1:1 list, gives PROPOSAL sizes only (rule 9).

| `LJ_WIN_*` | id | File (owner) | TFORM class (DFM caption, client) | Sub-forms the same file owns |
|---|---|---|---|---|
| CUELIST_FORM | `cuelist_form` | `lj_cuelist_form.c` (cuelist-window) | `TCuelist2Form` ("Cuelist", 545x506, min 545x350, `#6B8DCB`) | `TclEditPanels`, `TCuelistLinkOrImportForm` (202x73), `TTimeCodeOffsetForm` (378x399), `TSortTimeCodeForm` (250x33) |
| SAVE_CUELIST | `save_cuelist` | same | `TSaveCuelist2Form` ("Save Cuelist", 273x79) | `TSaveAsCuelistForm` ("Save as Cuelist#") |
| SELECT_CUELIST | `select_cuelist` | same | `TSelectCuelist2Form` ("Select Cuelist") | - |
| CL_SET_CUE_TRIG | `cl_set_cue_trig` | same | `TClSetCueTrigForm` ("Set Cue Trig", 169x86) | - |
| LOG_TIMECODE | `log_timecode` | same | `TLogTimeCodeForm` ("Log TimeCode", 145x253) | - |
| GENMACRO_EDITOR | `genmacro_editor` | `lj_genmacro_editor.c` (genmacro) | `TCueMacroControlForm` (DFM "Generic 8 bit Macro Editor", runtime "Generic Macro Editor [...]", 564x478) | - |
| SAVE_GENMACRO | `save_genmacro` | same | `TSaveMacroDialog` ("Save Macro", 326x120) | `TSaveAsMacroDialog` ("Save as Macro#", 220x70) |
| SELECT_GENMACRO | `select_genmacro` | same | `TLoadMacroList` ("Select Macro") | - (Delete Generic Macros stays W2 `delete_macro`) |
| MOVEMACRO_EDITOR | `movemacro_editor` | `lj_movemacro_editor.c` (movement) | code-built `TMovementMacroForm` ("Movement Macro"; `movementmacro.gif` 447x243 and `beziershapeeditor.gif` 400x145 reduced: PROPOSAL rig) | `TBezNameForm` (256x66) |
| DELETE_MOVEMACRO | `delete_movemacro` | same | `TDeleteMacrosForm` ("Delete Macros", 189x400) | - |
| FAN_OUT | `fan_out` | same | `TFanOutForm` ("Fan Out") | `TCustomOrderForm` ("Custom Fixture Selection Order"), `TSaveSelectionForm` (197x74) |
| AUTO_DELAY | `auto_delay` | same | `TAutoDelayForm` ("Pan/Tilt Macro Auto Delay", 391x201) | - |
| CUE_MACRO_CONTROL | `cue_macro_control` | same | `TCueMacroForm` ("Cue Macro Control", 172x256) | - |
| GDC_FORM | `gdc_form` | `lj_gdc_form.c` (gdc-fades-followspot) | `TGenericMegaForm` ("Generic DMX Control") | `TMegaGridSizeForm` (181x76), `TMegaGridColorSchemeForm` (144x173), `TMegaGroupCreateForm` (206x84) |
| FOLLOWSPOT_FORM | `followspot_form` | same | `TFollowSpotForm` ("Edit FollowSpot Settings", 432x398) | - |
| FOLLOWSPOT_LIST | `followspot_list` | same | `TFollowListForm` ("Select Followspot Setting") | - |
| SEQ_FADES | `seq_fades` | same | `TDefault2Form` (DFM "Set Fade Status", 239x226) | - |
| PROFILE_EDITOR | `profile_editor` | `lj_profile_editor_form.c` (profile-editor) | `TScanner2DefsForm` ("Generic Fixture Profile", 575x391, 11 tabs) | `TPaletteValueTypeForm`, `TBitMapPickerForm2`, `TQuickRGBDefForm`, `TFixtureSpecialForm`, `TFixtureNotesForm`, `TFixtureInfoForm` (358x99) |
| CUSTOMIZE_FIXTURE | `customize_fixture` | same | `TFixtureSetupForm` ("Customize Fixture") | `TCopySetupForm` ("Copy Palettes from", 237x353) |
| CHANNEL_CONFIG | `channel_config` | same | `TChannelDefForm` ("Channel Configuration", 356x207) | - |
| GS2_IMPORT | `gs2_import` | `lj_gs2_dialogs.c` (gs2) | `TFixtureImportForm` ("Import Fixture Profile") | `TProfileImportModifiedFilesForm` ("File Exists"), `TProfileNameOldStyleForm` |
| GS2_EXPORT | `gs2_export` | same | `TExportFixtureForm` ("Exporting User profile", 381x396) | `TFixtureExportWarningDialog`, `TExportAllForm` |

`LJ_WIN_DEFAULT_OUTPUT`'s class string is now `""`: W2 had parked `TDefault2Form` on it by name only and the windows-c
track settled that it has no window (`windows/lj_default_output.h`); the class is the Sequence Fades dialog, so
`lj_win_find("TDefault2Form")` resolves to `seq_fades`. The CD / DA / MP rows of TCuelist2Form, the `ListPop` items
`CD Commands`, `Digital Audio Commands`, `Mediaplayer Commands`, `LJ Mediaplayer`, `MP Load File` and the `sbCDTime` /
`sbDATime` / `sbMPTime` buttons are present verbatim, greyed, hint `Not available in LJ4EVR` (design 8.5, ME-11).

## 5. Replay and UI script

`docs/replay-grammar.md` "W3 lines": the build lines `cl` / `clline`, `bez` / `node`, `follow`, `gdcgroup`,
`gm_shapes`, `patch ... gs2=`; the command lines for every W3 kind and every UI-side writer; the 10 ms cue-list
stepping rule; `expect_cl`, `expect_mm` and the new `expect_view` fields. The five scenarios `cuelist_run`,
`genmacro_sine_amp186`, `genmacro_vs_bgcue`, `movemacro_six_shapes`, `fanout_auto_delay` are committed with header
comments and W2-grammar build lines (0 expects). `docs/ui-script.md` "W3 verbs": `cl_load cl_goto cl_execute
cl_assert_line assert_pos gs2_import assert_prompt mm_set` and the 22 window ids.

## 6. Tests and build rows

`tests/lj_tests.c` + `CMakeLists.txt` (appended): `test_cuelist` (live: tables, families, time code packing at every
frame of 24 / 25 / 30 / 75 / 100 fps), `test_profile_editor` (live: tab captions, the 81-row field table inside the
record and non-overlapping), and DISABLED until their track lands (each prints PENDING and fails if enabled early;
the track deletes its name from the `set_tests_properties` line): `test_cuelist_run`, `test_genmacro`,
`test_movemacro`, `test_gdc_grammar`, `test_gs2` (SKIP line when `LJ4EVR_REFERENCE_DIR` is unset), `test_udf_crc`,
`test_followspot`, `profiles_ids_w3` (`check_profiles.py --tier 1 2 3 4 5 6 7 8 9`), `gen_vectors_shapes_check`
(`gen_vectors.py --shapes --check`, which exits 1 "not implemented yet" until the genmacro track writes it, so it can
never pass by accident). `lj_fixlib` links `lj_miniz` (tinfl for `.Udf`); `src/app/lj_cuelist_run.c` is linked into
`lj4evr_tests`.

## Decisions recorded here

- Cue list parameters: waits in `param_ms` (ms; MTC frames packed by `lj_cl_hmsf_ms`, fps in `pstr`), Goto in
  `param_ms`, Hotkey Command = `enum lj_fn` in `param_ms` (the X-macro list is append-only, so the number is stable),
  Flash Sequence duration in `param_ms` and its sequence in the new `ref` field, Load background cue / Link Cuelist
  numbers in `ref`, Execute File path in `pstr` with its parameters in `comment` (the "Comment/Exe Parameter" column).
  `set_current_cue` / `set_cue_trig` are panel actions that fill `cue` / `trig_mask`, never a stored `cmd`.
- Execute File runs only with `Main.Ini [CUELIST2] AllowExecute=1` (ours, default 0, PROPOSAL): a show file must not
  start programs on another machine unasked. A `docs/fidelity.md` row by cuelist-core.
- Follow spot settings live in `FollowSpot.dat` (PROPOSAL; LJ2's `Follow2.Dat` is binary and not imported, design 5.3),
  Generic DMX groups in `GenericGroups.dat`, Bezier shapes in `<library>\MacroShapes\*.bez`, the Macros.Mac override in
  `<library>\Override\` (all PROPOSAL names, fidelity rows by their tracks).
- Generic DMX Control channel n = address code + 1 (1..2048), PROPOSAL rig F35: LJ2's map numbering is unverified.
- The editor's generic macro runs live at rank 6 next to the cue's (14 sA.7 "Fixture Out" display, "Disable Macros ...
  this includes any macro running from the cue"); `LJ_GENMACRO_EDITOR_AFTER_CUE` PROPOSAL.

## Deviations from design.md / the brief (and why)

- **No `LJ_CMD_FOLLOWSPOT`, `LJ_CMD_GDC_EXEC`, `LJ_CMD_SEQ_FADES` or per-fixture `LJ_CMD_MOVEMACRO`** (the brief asked
  for them). The sources make all four UI-side writes into the current scene: CHM thegenericdmxcontrol "adds data into
  the current scene in the current sequence like any other fixture control"; 04 s13 / 11 F22 follow spot "applies the
  stored parameters into the current sequence with all channels snapped"; 04 s1.4 Sequence fades "changes only fade
  state" of the sequence; design 3.4 / 3.6 "movement macros are not a layer: a runner's scene evaluation produces the
  macro-overlaid pan/tilt". An engine command would bypass the scene, make the result unsaveable and break the ME-35
  "runners only read the index" rule. They are model functions plus `LJ_CMD_CURRENT_SEQ` (seam 5); the view's
  follow-spot state is `ctx->followspot` (UI state), not engine state.
- **No `lj_followspot.h` stage / fixture geometry fields** (the brief): 04 s13 and the CHM topic describe a recorded
  scene fragment + selection + static association, and nothing geometric; `lj_followspot` in `lj_model.h` (W1) already
  carries exactly that, so the header adds its file, record and apply functions only.
- **`GENMACRO_ON` covers On and Off; no `GENMACRO_AMP` / `GENMACRO_SPEED`** (the brief named four). LJ2 has
  Engine Off / On / Toggle and Slot On / Off for generic macros (14 sA.9) and no amplitude / speed master for them;
  the amplitude / speed faders of the Cue macro control are movement-macro scales, already `LJ_CMD_MACRO_CTL`, now
  joined by `LJ_CMD_MACRO_FADE` for its Fade out / in, Off / On and Freeze / Run buttons. `GENMACRO_RESTART`
  (Restart Macro) and `GM_SHAPES` (ME-34) are added because the sources need them.
- **MTC is live, not stubbed "none" with greyed buttons** (the brief). Design 8.5 greys only the CD / DA / MP families;
  design 8.3 keeps MTC decode and design 12 I3-I5 says "MTC kept". The contract gives the decoder (`lj_mtc`) and the
  runner's `timecode(LJ_TC_MIDI)`; if the cuelist-core track runs out of time the stub returns -1 and the waits simply
  never resolve, which is the brief's behaviour without greying a button LJ2 has.
- **`lj_gs2_parse` keeps its W1 fixed-size signature**; the brief's `lj_gs2_parse(buf, len, out, errbuf)` is
  `lj_gs2_read(buf, len, out, err, cap)` (the length check with the oracle's message, the `gs2_raw` copy, then the
  parse). Changing the W1 prototype would touch every caller for no gain.
- **`lj_udf_import(path)` stays for the file; `lj_udf_parse(buf, len)` is the boundary function** the tests and the
  fuzz target (LORD_C-17) call, so fuzzing needs no file system.
- **Shapes table size.** `lj_shapes_gen.c` as design 3.6 describes it is 66 x 8192 int16, about 1 MB of data and some
  6 MB of C text. The contract fixes only `lj_gm_shapes_builtin`; the genmacro track may emit the shapes as integer
  generator code plus small tables (same samples, same `--check`) if the text size is unwelcome. Recorded, not decided.
- **ME-32 field names** follow the gate (`ch_caption[32][6]`, `ch_gm_off`); the design 4 struct does not have them yet
  (design owner, ME-50 style sentence).

## Found while reading (for the owning tracks)

- `model/lj_genmacro.c` `lj_genmacro_save` writes only channels with `shape != 0`, so every `2*Sine` channel (shape 0)
  is lost on save; `lj_genmacro_validate` rejects `dir = bounce`, which 14 sA.2 allows and the engine implements. Both
  are named in `lj_genmacro.h` for the genmacro track (`lj_genmacro_ch_used` is the save condition).
- `lj_cue.macro_amp_pct` / `macro_speed_pct` are `uint8_t` and `lj_cue_validate` caps them at 100 %, while 03 s5.11
  allows 1-999 % with the preference; the engine's `MACRO_CTL` clamps at 255. The movement track widens the two fields
  (append-safe `uint16_t` pair or a separate field) and the validator; the `.cue` grammar `macro=a,s` is unchanged.
- `lj_layout_derive` mallocs window tables that are never freed (W1 comment: "the profile lives as long as the
  process"). W3's Reload profile data (ME-31) re-derives on every reload; on LSan builds that is a reported leak per
  reload. The gs2 track frees them in `lj_gs2_free` (a `lj_layout_free` beside `lj_layout_derive`) or records why not.

## Evidence (this PC, MSVC 14.44 vcvars64, Release)

```
build-nosdl (LJ_SDL=OFF, Ninja + cl): [240/241] Linking C executable lj4evr_tests.exe, RC 0, 0 warnings
build-msvc  (cmake --preset dev-msvc): lj4evr.vcxproj -> build-msvc\Release\lj4evr.exe, lj4evr_render.exe, lj4evr_tests.exe, 0 warnings
ctest --test-dir build-nosdl -C Release: 100% tests passed, 0 tests failed out of 80 (89 rows, 9 Not Run (Disabled): the W3 pending rows)
ctest --test-dir build-msvc  -C Release: 100% tests passed, 0 tests failed out of 80
test_layering: 386 file(s) checked, 184 header(s) compiled alone, 0 finding(s)
lj4evr_tests replay <the five W3 stubs>: 5 scenario(s), 0 failed (0 ticks, 0 expects each)
test_cuelist: 0 failure(s); test_profile_editor: 81 fields, 0 failure(s)
```
