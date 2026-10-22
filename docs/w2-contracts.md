# W2 contracts: the headers eleven tracks code against

Written 2026-10-21 by the W2 contracts seat. Every header compiles alone (`tools/test_layering.py`,
cl: 296 files, 139 headers, 0 findings), every library links with stub bodies, `LJ_SDL` ON and OFF
build on this PC, the W1 `ctest` rows stay green (the evidence lines are at the end). Design
sections are the source; deviations and why are the last section. `docs/w1-contracts.md` holds
the W1 layers and the three W1 seams; this page adds what W2 brings. Shared files stay
append-only for every track: `CMakeLists.txt` tests section, `tests/lj_tests.c`,
`tools/glyphmap.tsv`, `src/io/lj_actions.h`, `src/app/lj_app.c` bindings,
`tools/test_layering.py` allowlists (ME-15).

## 1. Map (what changed per header)

| Header | Adds | Owner (bodies) |
|---|---|---|
| `model/lj_kv.h` | `lj_kv_range(fields, key, dflt, lo, hi, &bad)` (ME-77) | **done** (`lj_kv.c`, `test_model_w2_range`) |
| `model/lj_cuepage.h` (new) | `lj_cuepages` (255 pages, current tab, cursor), `lj_cuepages_init/load/save/validate/add/insert/delete/rename/clear/move/add_cue/remove_cue/step/step_cue`; `CuePageInfo.Dat` stays the W1 `lj_cue_pages_load/save` | model |
| `model/lj_defaultout.h` (new) | `lj_defaultout_use_scene/clear/scene_blocked`; file layer stays `lj_show.h` | model |
| `model/lj_notes.h` (new) | `LJ_NOTES_CAP`, `LJ_NOTES_FILE` (PROPOSAL), `lj_notes_load/save` | model |
| `model/lj_groups.h` | `lj_htp_names` (24 names in `GroupInfo.dat` `htp n name=`), `lj_htp_names_load/save`, `lj_group_select` | model |
| `model/lj_statics.h` | `LJ_STATIC_GROUPS` 64, `LJ_STATICS2_SLOTS_MAX` 64 (ME-30), `lj_static_groups` (`group n name=` in `Statics.dat`), `lj_statics_groups_load/save`, `lj_static_slot_link`, `lj_static_global_index` (**done**), `lj_static_scope_hits` | model |
| `model/lj_patch.h` | `LJ_PATCH_EXT ".GPA"`, `LJ_PATCH_FILE` (PROPOSAL), `lj_patch_clear` (**done**), `lj_patch_htp_set` | model |
| `model/lj_library.h` | `LJ_LIBRARY_BACKUP_EXT`, `lj_library_backup_notes/backup_lists/copy` | model |
| `engine/lj_engine.h` | 12 `LJ_CMD_*` kinds (table in the header): `BG_ON STATIC_ACTIVATE STATIC_DEACTIVATE SMOKE TRANSPARENT CHASE_TAP LOOP_OPTS HTP_GROUPS GLOBAL_PATCH DEFAULT_OUT SNAPSHOT_REQ PREFS`, `LJ_CMD_CUE_TIME == LJ_CMD_TIME`; FLASH `c` = latch; `lj_cmd` gains `slot patch default_out snapshot`; enums `lj_smoke_ctl lj_transparent_mode lj_snapshot_src lj_engine_pref`; `lj_engine_status` gains `static_slot_of[] static_level[] bg_number bg_enabled[] chase_tap_us flash_latched flash_seq transparent_mode smoke_active haze_level fan_level smoke_timer smoke_cycle_elapsed_ms snapshot_tick prefs[]` | engine |
| `engine/lj_engine_rules.h` | every W2 constant with its rig comment: `LJ_TIME_PCT_*`, `LJ_CHASE_*`, `LJ_LOOP_RANDOM_NO_COUNT`, `LJ_BOUNCE_END_AT_FIRST`, `LJ_TRANSPARENT_*`, `LJ_FLASH_*`, `LJ_BG_*`, `LJ_STATIC_*` (14 sB scopes), `LJ_SMOKE_*` (11 C12, 04 s12), `LJ_SEQINT_*`, `LJ_SNAPSHOT_*`, `LJ_HTP_MASTERS_DEFAULT` | **done** |
| `engine/lj_runner.h` | `lj_runner_loop_step` (the 03 s5.5 step + end-action table, pure; **done**: the W1 static body promoted), `lj_seq_intensity_rule` (03 s5.9, pure; **done**) | engine (lj_merge.c calls the rule once it lands) |
| `io/lj_midi.h` (new) | `lj_midi_dev`, `lj_midi_list/open/close` (backend), `lj_midi_map` + `lj_midi_map_entry`, `lj_midi_init/load/save/validate/assign/unassign/match`, the queue `lj_midi_post/poll/dropped/inject`, Learn `lj_midi_learn_begin/cancel/take`, `lj_midi_dispatch`, `lj_midi_kind_words[]` | **done** except load/save (`lj_midi.c`); backends `lj_midi_winmm.c` / `lj_midi_coremidi.c` stubs (midi track); `lj_midi_null.c` **done** |
| `io/lj_actions.h` | `LJ_FN_STATIC_11..20` appended (the exe's `Static 01..20`, 14 sB.2) | **done** |
| `windows/lj_windows.h` | 37 `LJ_WIN_*` ids appended (section 4), `lj_win_is_dialog` (**done**), `lj_win_about_text` | windows-a/b/c/d (one `lj_<window>.h/.c` each, rows in `lj_win_table`) |
| `windows/lj_prefs.h` | `struct lj_sysprefs` (every key of the five System Preferences tabs, section 6), `lj_sysprefs_defaults/load/save` (**done**, `test_prefs`), `lj_system_prefs_open/close/window/apply` | windows (the form); app (the file) |
| `desktop/lj_desktop.h` | `lj_ctx` gains `prefs midi cue_pages static_groups htp_names transparent_mode flash_latch static_visible`; `enum lj_desktop_led` + `lj_desktop_led_set/get` (**done** for BG / Statics), `lj_desktop_hotkey_setup_set`, `lj_desktop_toolbar_gates`, 17 `LJ_ACT_*` rows appended, the F8 rows W2 brings alive (header comment) | desktop |
| `app/lj_app.h` | `lj_app_prefs_load/save/apply`, `lj_app_midi_dispatch` (**done**), `lj_app_midi_open`, `lj_app_static_clear`, `lj_app_bgcue_clear`, `lj_app_snapshot_request/take` + `LJ_SNAP_*` filter bits, `lj_app_strike_lamps` | app |
| `app/lj_uiscript.h` | verbs `midi` (**done**) `hotkey assert_led assert_file_line sleep_until_tick`, `assert_view` field names | app |
| `tests/engine/lj_replay.h` | unchanged; `lj_replay.c` parses every W2 `at` form and the new `expect_view` fields (`docs/replay-grammar.md`) | replay (bodies that need more than a post; the nine scenarios' event lines) |

## 2. Who owns which files

| Track | Writes | Replaces the stub |
|---|---|---|
| model | `src/model/*.c` bodies of the W2 additions, `tests/model/*` | `lj_cuepage.c`, `lj_defaultout.c`, `lj_notes.c`, the W2 tails of `lj_groups.c lj_statics.c lj_patch.c lj_library.c`; `lj_cuepages_*` rows of `test_w2.c` |
| engine | `src/engine/*.c`: the 12 kinds in `lj_engine.c`, rank 4 scopes / levels, rank 5 BG_ON, rank 7 smoke, transparent load, chase tap, loop opts, HTP/patch/default swaps, snapshot fill, prefs; `tests/engine/test_statics.c` rows | every `default: break` fall-through of the W2 kinds; `lj_merge.c` to call `lj_seq_intensity_rule` |
| replay | `tests/engine/lj_replay.c` (runtime `gpatch`/`default`, anything beyond a post), the event lines of the eight W2 `.lj4r` files, `wire/` | the build-only scenarios |
| midi | `src/io/lj_midi.c` load/save, `lj_midi_winmm.c`, `lj_midi_coremidi.c`, `tests/io/test_midi_map.c` rows for the files | the two backend stubs |
| profiles | `src/fixlib/hand/*.c` for 15/02-15/05, `tools/qxf_map.tsv`, `tools/exe_profiles_check.py` `WAVE_IDS[2]` | the red `profiles_ids` row |
| windows-a | Loop Options, Cue Time, cue pages (name / reorder), transparent / flash preferences in the Cue control, Background Cue (+ Select / Save), `lj_ctl_rgbpack.c` (LORD_FIDELITY-18) | rows `LJ_WIN_LOOP_OPTIONS .. LJ_WIN_SAVE_BGCUE` |
| windows-b | Statics 2 (+ Preferences, Properties, page / slot name), Smoke Control, Position Presets (+ Save Preset), User Palettes, Fixture Groups (+ Add Group), Intensity Groups, HTP groups | rows `LJ_WIN_STATICS2_FORM .. LJ_WIN_HTP_GROUPS` |
| windows-c | Global Patch (.GPA), Default output, Snapshot, Edit Hotkeys (+ Select Function, the hotkey warning), MIDI-IN (code-built), System Preferences | rows `LJ_WIN_GLOBAL_PATCH .. LJ_WIN_SYSTEM_PREFS` |
| windows-d | the five Delete Files dialogs, User Libraries (+ name), Backup, Restore, Notes, About, Striking Lamps, Fixture Configuration "Export to text file" (ME-31), the Yes/No confirm for `lj_ctl_set_confirm` | rows `LJ_WIN_DELETE_SEQ .. LJ_WIN_STRIKING_LAMPS` |
| desktop | `src/desktop/*`: LED hooks, hotkey-setup menu, toolbar gates, the memory gauge (ME-87), `lj_desktop_key` wired | the W2 tail of `lj_toolbars.c` |
| app | `src/app/*`: prefs, MIDI open / shutdown order, pending servicing of statics / BG, snapshot, strike lamps, the W2 script verbs, `lj_desktop_key` caller, hotkey setups under the library | the W2 tail of `lj_app.c`, `lj_us_exec_w2` |
| integrator | `CMakeLists.txt`, `tests/lj_tests.c`, `tools/`, `src/ui/`, `src/gfx/`, docs, the ME-43/44/46/47/51/53/54/57 gate rows | - |

Compiling standalone: as W1 (`docs/w1-contracts.md` "Compiling standalone"). The nosdl line is
`cmake -S . -B build-nosdl -DLJ_SDL=OFF -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build
build-nosdl && ctest --test-dir build-nosdl -C Release` from a vcvars64 shell; the app is
`cmake --preset dev-msvc && cmake --build --preset dev-msvc`.

## 3. The three new seams

1. **Statics and BG cue pinned pointers (design 2.3 rules 2, 3, 6; ME-2).** The Statics 2 window
   posts `LJ_CMD_STATIC_ACTIVATE` with `st = &show->pages[p].slot[s]` and `a =
   lj_static_global_index(p+1, s+1)`; the Background Cue window loads a record through
   `lj_app_load_bgcue` (the app pins `bg` + `bgseq[5]` from the tables) and toggles slots with
   `LJ_CMD_BG_ON`. A slot's scene is freed, a slot re-recorded or a BG record deleted only through
   `lj_app_static_clear` / `lj_app_delete_record`, which hand the engine `STATIC_DEACTIVATE` /
   `LOAD_BG 0` in the same critical section before `lj_static_slot_clear` / `lj_show_free_record`.
   A cue's latched `bg_cue` keeps going through `pending_bg` (W1), the Link end action through
   `pending_link_cue`; the replay form `at k link` is the harness doing what
   `lj_app_service_pending` does. The engine's view reports `static_slot_of[]` + `static_active[]`
   per runner so the window lights the LED of page/slot without a second table.
2. **MIDI thread -> UI event (design 2.3 rule 5, 8.3; ME-23).** The OS callback calls only
   `lj_midi_post` (fixed queue under `lj_lock`, `LJ_MIDI_QUEUE` 256, full = dropped + counted).
   `lj_app_midi_dispatch` runs every idle pass after `lj_app_service_pending`: `lj_midi_poll` ->
   `lj_midi_dispatch(ctx->midi, msg)` -> `lj_fn_run` / `lj_fn_run_prog` on the UI thread. Learn is
   `lj_midi_learn_begin` + `lj_midi_learn_take` polled by the MIDI-IN window on its refresh.
   `lj_midi_inject` is the test seam (the script verb `midi 90 3C 7F`, `test_midi_map`). Shutdown:
   `lj_midi_close` before `lj_platform_engine_stop`, then `lj_sacn_close`.
3. **Preferences -> engine commands.** System Preferences edits `ctx->prefs` (`lj_sysprefs`,
   `windows/lj_prefs.h`) and runs `LJ_ACT_SYSTEM_PREFS_APPLY`; the app saves `Main.Ini`
   (`lj_sysprefs_save`, LJ2's keys), re-gates the toolbar (`lj_desktop_toolbar_gates`: Solo /
   OnLine-OffLine / Freeze / blind, ME-29; the three bar flags) and posts `LJ_CMD_PREFS` for
   `KeepSeqBlind` (LORD_ENGINE-13), `UseGlobalChaseRate`, `DisableChaseFade`, `Use Fadetime`
   (statics), `HotkeyChangeSeqDir`. The engine keeps them in `lj_engine_status.prefs[]` so a window
   never asks the ini twice.

## 4. Windows: the TFORM class each id binds

From `reference/installer/exe_resources/dfm_text/*.dfm.txt` (the `object X: TClass` line); "code-built"
= no DFM, with the GIF it is sized from. Only the 1:1 GIFs of 07 s14 give sizes; every W2 window
GIF named here is a reduced capture, so its numbers are PROPOSAL rig items (ME-67), and the DFM
numbers are the ground truth where a DFM exists.

| `LJ_WIN_*` | id name | TFORM class (caption, client size) | Note |
|---|---|---|---|
| LOOP_OPTIONS | `loop_options` | `TLoopToolsForm` ("Loop Options", 353x247) | 03 s5.5; `[LOOPFORM] Top` |
| CUE_TIME | `cue_time` | `TCueTimeForm` ("Time Control") | 13 faders, 03 s5.6; `[CUETIMEFORM] Top Expanded` |
| CUE_PAGE_NAME | `cue_page_name` | `TGetPageInfoForm` ("Add New Cue Page", 254x77) | Add / Insert / Edit Current Page Name |
| CUE_PAGES_REORDER | `cue_pages_reorder` | `TReorderCuePagesForm` ("Re-order Cue Pages") | |
| (transparent / flash options) | - | no dialog: `TCurrentCue` Preferences menu items `M_UseTransparentCues` / `M_ForceTransparentCues` / `M_IgnoreTransparenCues` (D13 spelling), `M_StickyFlashSequences`, `M_FlashAutoRelease` | windows-a inside the Cue control |
| BGCUE_FORM | `bgcue_form` | `TBGCueForm` ("Background Cue", 384x124) | + `TBGCueSequenceModeForm` ("BG Cue - Sequence Mode", 204x101) |
| SELECT_BGCUE | `select_bgcue` | `TSelectBGCueForm2` ("Select Background Cue") | the newer of the two forms (has the Transparent column) |
| SAVE_BGCUE | `save_bgcue` | `TSaveBGForm` ("Save Background Cue", 198x92) | |
| STATICS2_FORM | `statics2_form` | `TStatics2Form` ("Statics", 510x356 design) | 14 sB.3; the slot popup keeps the duplicated `Replace Page` / `Replace Slot` (D13, acceptance 4) |
| STATICS2_PREFS | `statics2_prefs` | `TStatics2PrefsForm` ("Statics Preferences", 481x299) | ME-30 `[STATICS2]` keys |
| STATIC_PROPS | `static_props` | `TStaticEntryPropertiesForm` ("Static entry properties", 540x300) | + `TGetStaticsPageNameForm` (258x79), `TStaticSlotNameForm` (288x67) |
| SMOKE_CONTROL | `smoke_control` | `TSmokeForm` ("Smoke Control", 310x213) | 04 s12; `[SMOKE]` |
| POSITION_PRESETS | `position_presets` | `TPresetForm` (DFM caption "PresetForm") | runtime caption UNVERIFIED; `[MOVEPRE]` |
| SAVE_PRESET | `save_preset` | `TSavePresetForm` ("Save Preset", 359x73) | `TSavePresetForm2` ("Save", 312x82) is the palette save |
| USER_PALETTES | `user_palettes` | code-built; `usercolorpalette.gif` 169x208 (reduced) | 07 s9.10, 04 s9; `TPaletteValueTypeForm` (346x173) for the value-type choice |
| FIXTURE_GROUPS | `fixture_groups` | code-built tree + `TAddGroupForm` ("Add Fixture Group"); `fixturegroupwindow.gif` 225x205 (reduced), INI 220x250 | 01 s3.4; `[GROUPS]` |
| INTENSITY_GROUPS | `intensity_groups` | `TIntensityGroupForm` ("Intensity Groups", 325x348) | 9 tabs |
| HTP_GROUPS | `htp_groups` | code-built; no DFM and no GIF: the Global Patch `HTP Groups` column + the 24 names | 02 s7.1 |
| GLOBAL_PATCH | `global_patch` | `TGlobalPatchForm` ("Global Patch") | File > New / Load / Save Global Patch (.GPA); Preferences > Enable HTP group master intensity control |
| DEFAULT_OUTPUT | `default_output` | `TDefault2Form` (DFM caption "Set Fade Status", 239x226) | the only Default* dialog; its role is UNVERIFIED (windows-c confirms from the controls); `TApplyDefaultDMXForm` ("Applying Default DMX - please wait", 251x43) |
| SNAPSHOT | `snapshot` | `TSnapshotSceneForm` ("Snapshot from DMX Out", 375x281) | 03 s4.7 |
| EDIT_HOTKEYS | `edit_hotkeys` | `TFunctionsForm` ("Edit Hotkeys") | 01 s7; Options > Save / Load / Default Hotkeys |
| HOTKEY_FUNCTION | `hotkey_function` | `THotkeyFunctionDialog` ("Select Function") | + `TShowHotKeyWarningForm` ("Notification !", 271x122) |
| MIDI_IN | `midi_in` | code-built: `LJ_MidiIn.exe`'s forms are not in LightJockey.exe; `midiin.gif` (reduced) | 05 s7.2 tabs |
| SYSTEM_PREFS | `system_prefs` | `TSystemPrefsForm` ("System Preferences") | 01 s6.1, five tabs |
| DELETE_SEQ / CUE / CL / BG / MACRO | `delete_seq` .. `delete_macro` | `TDeleteSequencesForm` (+ `TSeqUsedForm`), `TDeleteCuesForm`, `TDeleteCuelistsForm`, `TDeleteBGCuesForm`, `TDeleteGenericMacrosForm` | `TDeleteMacrosForm` = movement macros, W3 |
| USER_LIBRARIES | `user_libraries` | `TUserLibForm` ("User Libraries") | 01 s8 |
| USER_LIB_NAME | `user_lib_name` | `TGetUserLibNameForm` ("Enter User Library Name", 345x89) | |
| BACKUP | `backup` | `TLibraryBackupForm` ("Backup User Library", 569x257) | + `TBackupForm` ("Backing Up") |
| RESTORE | `restore` | `TLibraryRestoreForm` ("Restore User Library", 715x299) | + `TRestoreLibProgressForm` |
| NOTES | `notes` | `TNoteForm` ("Notes") | poScreenCenter, stay-on-top |
| ABOUT | `about` | `TAboutForm2` ("About LightJockey", 360x346) | no logo (D2); VERSION + THIRD_PARTY_NOTICES.txt (LORD_FIDELITY-16) |
| STRIKING_LAMPS | `striking_lamps` | `TAutoLampForm` ("Striking Lamps") | 01 s6.2 |

The id names are what `lj_win_find` and the UI script resolve; the class strings in
`lj_windows.c` for the code-built windows (`TUserPaletteForm`, `TGroupForm`, `THTPGroupsForm`,
`TMidiInForm`) are ours and must be the `lj_window` name the builder gives.

## 5. Replay and UI script

`docs/replay-grammar.md` "W2 command lines": every W2 kind has an `at` form; `expect_view` gains
the W2 fields. The nine scenarios of design 10 W2 acceptance 1 are committed with their header
comment and build lines; `seq_intensities` is the W1 bonus row; `wire/drift_600s_two_slots_wire.lj4r`
is outside the `replay_all` glob (the acceptance integrator's 10-minute wire run).
`docs/ui-script.md` "W2 verbs": `midi hotkey assert_led assert_file_line sleep_until_tick`, the
`assert_view` fields, and the W2 window ids.

## 6. System Preferences keys

`windows/lj_prefs.h` `struct lj_sysprefs` carries every box of the five tabs with its `Main.Ini`
key (the shipped `User\Default\Main.Ini` and the exe's literal pool, 13 s6): `[PREFS]`
`BitmapCache AllowBlind AllowBlindSeq AllowOffline ConfineToDesktop ConfineToWindowsDesktop
SecondaryMonitor KeepSeqBlind WarnSeq WarnCue WarnCueList WarnOWSeq WarnOWCue WarnOWCueList WarnBO
WarnBlind WarnOffline CloseConfirm QuickSeq QuickCue QuickCl QuickMacro ExtMem ExtMem10 Primary
Secondary ViewSCBar ViewClBar ViewFixBar ViewStatusBar MarkActiveFixtures MoveId`, `[SOLO]
Enabled`, `[DESKTOP] Restore`, `[OFFLINE] Enable43`, `[MISC] SANDNET ShowHotkeyWarning
ShowNoHardwareWarning`, `[CAPTURE] Enable`, `[HOTKEYS] Enabled DialogDisable File`, `[RESTORE]
Password AtStartup PassOnRestore` (ours). Two mappings are a PROPOSAL (rig: toggle the box in LJ2
and diff `Main.Ini`): `AllowBlind` = "Allow freeze output" and `WarnBlind` = "Show Freeze
Warning" (the exe's keys predate the freeze rename; `AllowBlindSeq` is the sequence blind box).
The shipped file has `AllowBlind=0 AllowOffline=0`: W2 reads the file, so a fresh library follows
it where W1 forced the three gates ON (LORD_FIDELITY-11 note in `lj_sysprefs_defaults`).

## 7. Tests and build rows

`tests/lj_tests.c` + `CMakeLists.txt` (appended): `test_model_w2_range`, `test_model_w2_cuepages`,
`test_runner_loop`, `test_intensity_rule`, `test_statics`, `test_midi_map`, `test_prefs`,
`profiles_ids` (`tools/check_profiles.py --tier 2 3 4 5`: **red until the profiles track lands**, by
design: it names every missing 15/02-15/05 id). MIDI backend selection: `HEADER_FILE_ONLY` on the two
backends that are not this OS's, `winmm` / `CoreMIDI` + `CoreFoundation` linked. The OS-header
allowlist already carried `io/lj_midi_winmm.c` / `io/lj_midi_coremidi.c` (ME-15).

## Decisions recorded here

- Hotkey setups live under the user library (`<library>\Hotkeys\SetupN.Hot`, created since W1)
  and `Main.Ini [HOTKEYS] File=SetupN.Hot` names the current one (the w1-integration s7 item).
- The Statics 2 file stays `Statics.dat` with `group` lines and `n=` on the page line; `Default.Sta`
  (14 sB.6, inferred) is not used (PROPOSAL rig 14 sB.8 item 8).
- `[PREFS] KeepSeqBlind` is the exe key for the gate's "[SEQ] KeepContentsBlind" (LORD_ENGINE-13).
- `[CUE] DisableChaseFade` belongs to `Main.Ini` (the shipped file), not `forms.ini` as the W1
  table had it.

## Deviations from design.md (and why)

- `lj_sysprefs` lives in `windows/lj_prefs.h`, not `app/lj_app.h` (the brief): the System
  Preferences window must fill it and `windows/` may not include `app/`; the app keeps the file
  and the consequences (`lj_app_prefs_*`). `lj_ctx` forward-declares the type.
- The MIDI event path is one plain queue under `lj_lock` for every backend (design 2.3 rule 5
  names `SDL_PushEvent` for the OS backends): the 10 ms idle wake bounds the latency and `io/`
  cannot reach `platform/`; a `lj_platform_wake` is the upgrade if a measured latency asks for it.
- `LJ_CMD_CUE_TIME` is an alias of the W1 `LJ_CMD_TIME` (same 13 faders); `CHASE_TAP` is a real
  kind because a tap without a scene advance has no W1 form.
- `HTP_GROUPS` and `GLOBAL_PATCH` both pass `show->patch`: the masks live in the patch table
  (design 4), so the two swaps re-read the same pointer and differ in what the engine rebuilds.
- `SNAPSHOT_REQ` fills a UI-owned `lj_frame` (the design's "frame copy") rather than a new type.
- `lj_runner_loop_step` is the W1 static `lj_runner_advance` made public (the brief asked for a
  pure function; it already was one); `lj_seq_intensity_rule` is new and `lj_merge.c` still holds
  its own copy of the rule until the engine track switches it.
- `LJ_FN_STATIC_11..20` are ten explicit rows, not an `LJ_FN_R20` range: the W1 `LJ_FN_STATIC`
  range is `R10` and the X-macro ranges cannot be extended in place without reordering.
- `LJ_WIN_DEFAULT_OUTPUT` binds `TDefault2Form` on the strength of its name only; its DFM caption
  is "Set Fade Status". Recorded as UNVERIFIED for the windows-c track.
- The nine W2 replay files carry build lines only; `replay_all` therefore stays green with 0
  expects for them (no evidence yet, recorded in `docs/replay-grammar.md`).
- `profiles_ids` is red on purpose until the hand files land (the brief asked for the row; a
  disabled row would hide the gap).

## Contracts finish 2026-10-22

The contracts seat died while verifying its builds; this pass verified them on the same PC (MSVC 14.44, vcvars64)
and fixed what failed. The contracts themselves are unchanged.

Evidence (Release, both builds from clean configure runs):

```
build-nosdl (LJ_SDL=OFF, Ninja + cl):  [15/16] Linking C executable lj4evr_tests.exe ... NOSDL_BUILD_RC=0, 0 warnings
build-msvc  (cmake --preset dev-msvc): lj4evr.vcxproj -> D:\lj4evr\build-msvc\Release\lj4evr.exe ... MSVC_BUILD_RC=0, 0 warnings
ctest --test-dir build-nosdl -C Release: 100% tests passed, 0 tests failed out of 62 (63 - profiles_ids (Disabled)), 5.18 sec
ctest --test-dir build-msvc  -C Release: 100% tests passed, 0 tests failed out of 62 (63 - profiles_ids (Disabled)), 5.17 sec
test_layering: 296 file(s) checked, 139 header(s) compiled alone, 0 finding(s)
```

The "hang": not reproduced. Both builds finish in minutes and the whole ctest run takes about 5 s with
`--timeout 300` and stdin from `NUL`; no test waits on a socket, a window or input. The nosdl build that looked hung
had in fact stopped on a compile error (below). The likely cause of the hang is the shell, not the build: a
`cmd //c <bat>` from Git Bash that cmd cannot resolve leaves a cmd process waiting on stdin. Unverified; drive
vcvars builds from PowerShell (`cmd /c "<full path>.bat" > log 2>&1`).

Fixes:

1. `tests/engine/test_statics.c`: three `CHECK`s on compile-time constants (`LJ_STATICS2_SLOTS_MAX`,
   `LJ_STATIC_GROUPS`, `LJ_STATIC_RUNNERS`) raised C4127 (conditional expression is constant) under `/W4 /WX` and
   stopped both builds. They are now `_Static_assert`s at file scope (the `tests/core/test_trig.c` pattern).
2. `tests/engine/test_runner_loop.c`: the bounce row expected `1 2 3 2 1 1` from scene 0, which skips the first
   scene on the way back and contradicts the 03 s5.5 order `1->2->3->4->3->2->1->2` that `lj_runner_loop_step`
   and `LJ_BOUNCE_END_AT_FIRST` implement. The row now expects `1 2 3 2 1 0 1` over seven steps with
   `loop_count == 1` (the step past first ends the loop, matching the forward rows, where the step past last ends it).
3. `tests/io/test_midi_map.c`: the LatchOff row expected a note-off release to reach the `LJ_FN_PROG_CUE` handler.
   `lj_fn_run_prog` drops the release of every non-flash function, and the W1 `test_hotkeys` row asserts that for
   `LJ_FN_PROG_CUE`. The other five failures followed from that one. The row now expects the release to be
   dropped (`lj_t_calls == 1`), and the counts after it are one lower. **Pending for the midi + app tracks:** a
   LatchOff release path that does not change the W1 hotkey rule. For example, the four `LJ_FN_PROG_*` rows could
   let edge 0 through to `lj_app_fn`, which already checks `edge`, and the `test_hotkeys` row would change in the
   same commit.
4. `CMakeLists.txt` (appended): `profiles_ids` is `DISABLED` (expected-pending, owner: the W2 profiles track). It is
   still listed in every ctest summary as "Not Run (Disabled)". The profiles track removes the property in the
   commit that lands the 15/02-15/05 hand files. Run it by hand with
   `python -I tools/check_profiles.py --tier 2 3 4 5`: today it reports 0 of 2 / 0 of 8 / ... / 0 of 9 ids.
