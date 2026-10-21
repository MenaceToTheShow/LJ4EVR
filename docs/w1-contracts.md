# W1 contracts: the headers fourteen tracks code against

Written 2026-10-21 by the W1 contracts seat. Every header below compiles alone
(`tools/test_layering.py`, cl: 0 findings), every library links with stub bodies,
`ctest` is green (29 rows incl. the new `test_model_table` and `replay_all`),
`LJ_SDL` ON and OFF build on this PC. Design sections are the source; deviations and
why are at the end. `docs/w0-contracts.md` still holds the core/gfx/ui/platform
contracts and the three rules for the tracks (where you write, what you may include,
how to compile one file standalone); this page adds the W1 layers.

## Map

| Header | Types | Functions / constants | Owner (bodies) |
|---|---|---|---|
| `model/lj_model.h` | limits enum (adds `LJ_BG_FILES .. LJ_FADE_TIME_DEFAULT_MS`, `LJ_NAME_CAP`, `LJ_NUMBER_MAX`; the six engine-facing ones stay in `core/lj_types.h`), `lj_fstate`, enums `lj_trig lj_dir lj_end_action lj_bg_mode lj_activation lj_replace lj_static_kind lj_fixture_kind lj_patch_src lj_in_mode lj_cl_adv`, structs `lj_movemacro lj_scene lj_sequence lj_cue_slot lj_cue lj_cl_line lj_cuelist lj_bgcue lj_static_slot lj_static_page lj_fixture lj_group lj_preset lj_user_palette lj_patch_ch lj_genmacro lj_followspot lj_show` (design 4 verbatim) | `lj_addr_code(link, addr)` | model (types only, no .c) |
| `model/lj_table.h` | `lj_table_entry`, `lj_table` (sorted array) | `lj_table_init/get/put/remove/count/at/free` | **done** (`lj_table.c`, test `test_model_table`) |
| `model/lj_words.h` | - | `lj_trig_words[] lj_dir_words[] lj_end_words[] lj_bg_mode_words[] lj_act_words[] lj_replace_words[] lj_static_kind_words[] lj_fixture_kind_words[] lj_patch_src_words[] lj_in_mode_words[] lj_fstate_words[]`, `lj_word`, `lj_word_index` | **done** (`lj_words.c`) |
| `model/lj_scene.h` | - | `lj_scene_init/copy/is_programmed`, `lj_sequence_alloc/free_scenes/add/insert/delete/new` (F29 rules) | model |
| `model/lj_sequence.h` | - | `lj_sequence_load/save/validate/free/new_rec` | model |
| `model/lj_cue.h` | `lj_cue_page` | `lj_cue_load/save/validate/new_rec/free/add_sequence/find_free`, `lj_cue_pages_load/save` | model |
| `model/lj_cuelist.h` | - | `lj_cuelist_load/save/validate/new_rec/free/insert/delete` (W3 bodies) | model |
| `model/lj_bgcue.h` | - | `lj_bgcue_load/save/validate/new_rec/free/add_sequence` | model |
| `model/lj_statics.h` | - | `lj_statics_load/save/validate/free/add_page`, `lj_static_slot_record/clear` | model |
| `model/lj_patch.h` | - | `lj_patch_load/save/validate/init/is_default` | model |
| `model/lj_groups.h` | - | `lj_groups_load/save/validate` | model |
| `model/lj_presets.h` | - | `lj_presets_load/save/validate` | model |
| `model/lj_palettes.h` | `lj_palette_list` | `lj_palettes_load/save/validate/add/delete/free/file_name` | model |
| `model/lj_library.h` | `lj_library`, `enum lj_rec_kind`, `LJ_PATH_CAP`, `LJ_LIBNAME_CAP` | `lj_library_open/create/open_default/list/delete/rename/is_default/path/record_path/next_number/list_records/delete_record` | model |
| `model/lj_show.h` | - | `lj_show_new/free` (**done**), `lj_config_load/save/validate`, `lj_defaultout_load/save`, `lj_show_load/save_config`, `lj_show_fixture_count/base/at/add_fixture/remove_fixture` | model |
| `fixlib/lj_profile.h` | `LJ_PROFILE_CH LJ_GS2_REC_SIZE LJ_PAL_ENTRIES LJ_PALETTES`, `enum lj_ch_class` (+ SWITCH SMOKE HAZE HAZE_FAN, ME-25), `lj_dyn lj_gs2_sfunc lj_pal_entry lj_palette`, `enum lj_ctl_kind` (`LJ_CTL_*`), `enum lj_ctl_elem` (`LJ_W_*`), `lj_ctl_button lj_ctl_row` (+ `states_allowed`, ME-27) `lj_ctl_window`, `lj_profile` (6.2 verbatim + `pair_lsb[]`), `enum lj_profile_binding`, `lj_exe_row` | `lj_fixlib_by_id/count/row`, `lj_profile_window` (**done**), `lj_layout_derive`, `lj_gs2_parse/write`, `lj_udf_import` (W3; stubs -1) | fixlib |
| `engine/lj_engine_rules.h` | `enum lj_layer` (`LJ_L_*`), `enum lj_post_stage` (`LJ_POST_*`), `enum lj_master_mode`, `enum lj_seq_int_mode` | every design 3.3 constant with its rig comment + `LJ_TICK_HZ_MIN/MAX LJ_SNAP_TEST_AFTER_SCALE LJ_GM_FADE_MIN/MAX_MS LJ_SWITCH_UNSCALED LJ_CATCHUP_BOUNDED LJ_GENMACRO_SHAPE_N LJ_STATIC_RUNNERS LJ_ENGINE_CMD_CAP`; `extern const uint8_t LJ_LAYER_ORDER[8]` (lj_merge.c), `LJ_POST_ORDER[9]` (lj_post.c) | **done** |
| `engine/lj_engine.h` | `lj_engine` (opaque), `lj_frame`, runner ids `LJ_RUN_*`, `enum lj_cmd_kind` (24 kinds, table in the header), `lj_cmd` (a b c + pinned `cue seq[12] bg bgseq[5] gm st sacn`), `LJ_SLOT_*`, `lj_runner`, `lj_engine_status` (the design's `lj_engine_view` struct), `lj_post_ctx`, `lj_fade_value()` | `lj_engine_create/destroy/bind/bind_profile/rebind/cmd/tick/frame/view/runner/post_ctx/tick_us/set_tick_us`, thread hooks `lj_engine_is_offline/take_sacn_cfg/set_sacn_stats` | engine (`lj_engine.c` keeps the W0 pattern body until the runner lands) |
| `engine/lj_merge.h` | - | `lj_merge` | engine |
| `engine/lj_post.h` | - | `lj_post_s1_patch_src .. s9_freeze_offline`, `lj_post_s7_pairs`, `lj_post_run` | engine |
| `io/lj_sacn.h` | `LJ_SACN_*` constants, `lj_sacn_cfg` (8.1 verbatim), `lj_sacn_stats` | `lj_sacn_open/set/pump/close/stats_get`, `lj_sacn_cfg_validate/default/load/save`, `lj_sacn_cid_new`, `lj_sacn_pack_data/pack_discovery` | sacn |
| `io/lj_net.h` | `lj_nic` | `lj_net_list_ipv4/hostname/startup/cleanup` | sacn |
| `io/lj_actions.h` | `enum lj_fn_group`, `LJ_FNF_*`, `LJ_FN_LIST(X)` (X-macro, every row of 01 s7 + the 05 s11.2 families the design names), `enum lj_fn`, `lj_fn_desc` | `lj_fn_table[]`, `lj_fn_group_names[]`, `lj_fn_caption`, `lj_fn_by_name`, `lj_fn_is_flash` | **done** (`lj_actions.c`); actions track adds `test_actions` |
| `io/lj_hotkeys.h` | `lj_hotkey`, `lj_hotkeys`, `.Hot` grammar in the header | `lj_hotkeys_init/load/save/validate/setup_path/assign/clear_key/clear_fn/find_fn/match/combo_text` | actions |
| `io/lj_fn.h` | `lj_fn_handler` | `lj_fn_run`, `lj_fn_run_prog`, `lj_fn_bind`, `lj_fn_bound`, `lj_fn_unbind_all` | **done** (`lj_fn.c`) |
| `desktop/lj_desktop.h` | `lj_ctx` (the process context), `enum lj_modified lj_toolbar lj_status_field lj_sel_mode lj_desktop_popup` | `lj_ctx_get/set_modified/set_scene/post`, `lj_desktop_create/destroy/window/refresh/key`, `lj_desktop_toolbar_*`, `lj_desktop_seqcue_fields`, `lj_desktop_status_set/flag`, `lj_desktop_tab*`, `lj_desktop_icons_refresh/icon_at/place_icon/unassign_icon`, `lj_sel_*`, `lj_unassigned_*`, `lj_desktop_popup` | desktop (`lj_ctx_get/post` **done**; W0 `lj_main_window.h` stays until the table lands) |
| `controls/lj_ctl.h` | `LJ_PROG_KEEP` | `lj_ctl_open/close/close_all/toggle/open_only/window_get/refresh/caption`, `lj_ctl_row_build/row_refresh/value_text`, `lj_prog_get/set/set16/set_sel/set_sel16/set_state_sel/cycle_state/master_ch` | controls-a/b/c |
| `windows/lj_windows.h` | `enum lj_win` (14 ids), `lj_win_host`, `enum lj_warning` | `lj_win_open/close/toggle/get/refresh/refresh_all/close_all/find/class/id_name/set_host`, captions `lj_win_caption_seq/cue/cuelist/main` (**done**), `lj_win_warning`, `lj_win_close_prompt`, `lj_win_message` | windows-a/b/c |
| `app/lj_app.h` | - | `lj_app_main`, `lj_app_quit/refresh/service_pending`, `lj_app_load_cue/bgcue/genmacro/sequence`, `lj_app_save_sequence/cue`, `lj_app_delete_record`; the startup sequence is the header comment | app |
| `app/lj_uiscript.h` | - | `lj_uiscript_load/step/active/line/fail` (docs/ui-script.md) | app |
| `tests/engine/lj_replay.h` | `lj_replay_result` | `lj_replay_run` (docs/replay-grammar.md) | replay |

Build: `CMakeLists.txt` already globs `src/<dir>/**/*.c` per library (W0), so a new
`.c` needs no CMake edit. New in W1: `list(APPEND LJ_TEST_SUBCOMMANDS ...)` is the
append point for test names; `replay_all` runs `lj4evr_tests replay` over every
`tests/engine/replay/*.lj4r` (CONFIGURE_DEPENDS glob; zero files = pass);
`LJ4EVR_REFERENCE_DIR` (cache PATH, default empty) reaches `lj4evr_tests` as a string
define for developer-only tests that read `reference/` and skip when it is empty.

## Who owns which files

| Track | Writes | Replaces the stub |
|---|---|---|
| model | `src/model/*.c` (bodies of the twelve headers; `lj_genmacro_data.c` is W2), `tests/model/*` | every `src/model/lj_*.c` except `lj_table.c`, `lj_words.c`, and `lj_show_new/free` |
| fixlib | `src/fixlib/lj_profile.c lj_gs2.c lj_layout.c lj_exe_profiles.c lj_profiles_gen.c hand/*.c`, `tools/qxf2c.py qxf_map.tsv`, `tests/fixlib/*` | `lj_profile.c` (keep `lj_profile_window`); new files need matching `.h` (`lj_gs2.h`, `lj_layout.h`, `lj_exe_profiles.h`) that re-declare nothing: they may be one-line headers that include `lj_profile.h` |
| engine | `src/engine/lj_engine.c lj_runner.c lj_merge.c lj_post.c lj_masters.c lj_movemacro.c lj_genmacro.c lj_shapes_gen.c`, `tests/engine/test_ownership.c` | the tick body, `lj_merge`, every `lj_post_*` |
| replay | `tests/engine/lj_replay.c`, `tests/engine/replay/*.lj4r`, `dump-frames` extension | `lj_replay.c` |
| sacn | `src/io/lj_sacn.c lj_net.c`, `tests/sacn/*` | both |
| actions | `src/io/lj_hotkeys.c`, `tests/io/test_actions.c` (row `test_actions`) | `lj_hotkeys.c`; `lj_actions.c` and `lj_fn.c` are done, extend only |
| desktop | `src/desktop/*` | `lj_desktop.c` (split into the design's files; keep `lj_ctx_get/post` somewhere under desktop/) |
| controls-a/b/c | `src/controls/lj_ctl_common.c` + one file per kind | `lj_ctl.c` (delete it with the first real file) |
| windows-a/b/c | `src/windows/lj_<window>.c` + `.h` each | `lj_windows.c` open/close/refresh bodies; keep the caption helpers |
| app | `src/app/lj_app.c lj_uiscript.c lj_prefs.c lj_startup.c main.c`, `tools/taskbar-count.ps1` | the `lj_app_*` stubs at the end of `lj_app.c`, `lj_uiscript.c` |
| integrator | `CMakeLists.txt`, `tests/lj_tests.c` (append-only for everyone else), `tools/`, `src/ui/`, `src/gfx/`, docs | - |

Shared files, append-only, never reorder: `tests/lj_tests.c` (the W1 block at the end
of the table), `CMakeLists.txt` (`list(APPEND LJ_TEST_SUBCOMMANDS ...)`),
`tools/glyphmap.tsv`.

## Compiling standalone

As W0 (`docs/w0-contracts.md` rule 3): one `.c` through `cl /nologo /c /std:c11 /W4
/WX /permissive- /utf-8 /fp:strict /I D:\lj4evr\src /I D:\lj4evr\build-msvc\generated
<file>`, a header through a one-line TU that includes it. `python -I
tools/test_layering.py --src src --cc cl --gen build-msvc/generated` is what CTest runs
(the include graph, OS-header allowlist, float/atomics/volatile, `lj_table_*` outside
engine/, names and guards, every header alone). Full: `cmake -S . -B build-nosdl
-DLJ_SDL=OFF && cmake --build build-nosdl --config Release && ctest --test-dir
build-nosdl -C Release` (SDL-free, what CI's Linux jobs do), `cmake --preset dev-msvc &&
cmake --build --preset dev-msvc` (the app).

## The three cross-track seams

1. **model <-> engine: pinned pointers (design 2.3 rules 2, 3, 6).** The engine never
   looks a record up and never loads. The UI thread (app) loads a cue and its
   sequences from disk outside `lj_lock`, inserts them into `show->sequences` /
   `show->cues` (`lj_table_put`) under the lock, and in the same critical section posts
   `LJ_CMD_LOAD_CUE` with `cue`, `seq[12]`, `bg`, `bgseq[5]`, `gm` pointing at those
   records (`seq[i] = NULL` for a sequence that failed to load = `load error`). The
   runner keeps `seq_no` as identity and the pointer until the next load swaps it. A
   record is freed only after the engine was handed a replacement or NULL for every
   runner that pointed at it (`lj_app_delete_record`). The current sequence works the
   same way: `LJ_CMD_CURRENT_SEQ` pins `show->current_seq` with the scene index and the
   blind flag; the UI edits that scene's `val[]/state[]` under the lock and the engine
   reads it by pointer on the next tick. Engine-initiated loads (Link to cue, latched bg
   cue / generic macro) are requests in `lj_engine_status.pending_*` that
   `lj_app_service_pending` turns into ordinary loads. `tests/engine/test_ownership.c`
   is the check.
2. **controls <-> current scene (`controls/lj_ctl.h`).** A control window is built from
   the master fixture's `lj_ctl_window` table (`lj_sel_master_profile()` ->
   `lj_profile_window(p, kind)`); every row's widget binds to a channel of the current
   scene (`ctx->show->current_seq->scenes[ctx->current_scene]`) through
   `lj_prog_get` / `lj_prog_set*` (under `lj_lock`, selection-wide variants apply to
   every selected fixture of the master's profile id at `base + offset`). Fade state
   lives on the scene channel, so two buttons bound to one channel in two windows
   repaint from one state (ME-27); `states_allowed` on the row limits the O/S/F cycle
   (`lj_prog_cycle_state`, D10). `lj_ctl_refresh()` runs on the 100 ms timer and
   rebuilds when `lj_sel_serial()` changed.
3. **windows <-> engine view (`windows/lj_windows.h`, `engine/lj_engine.h`).** A window
   never reaches into the engine: in `lj_win_refresh` it copies `lj_engine_status`
   (`lj_engine_view`, which also clears `pending_*`, so only the app's
   `lj_app_service_pending` may call it; windows read the app's copy through
   `lj_ctx`... see below) and `lj_frame` under `lj_lock` and paints from the copy: the
   Cue grid's `1:2` and status, the Sequence window's scene/fade progress bars
   (`cur_*_elapsed_ms`), the DMX Output Monitor cells (`out[]`, `layer_of[]` only behind
   `[DEBUG]`, ME-28), the Master Intensity faders (`gm`, `sub[]`), the warning boxes.
   It acts only through `lj_ctx_post()` (`LJ_CMD_GO`, `_STOP`, `_TRIG`, `_SET_MASTER`,
   `_OVERWRITE`, ...). To keep `pending_*` single-reader, the app calls
   `lj_engine_view` once per idle pass and keeps the copy; a window that needs the view
   between passes copies `lj_engine_frame()` itself and reads the app's last status
   copy (the app track exposes it through `lj_ctx`: add `lj_engine_status view;` there
   when the first window needs it; the field is reserved by this sentence).

Context: `desktop/lj_desktop.h`'s `lj_ctx` is the one process context (show, engine,
library, ini files, sACN config, hotkeys, current scene, blind, modified flags). The
app fills it before any window opens; windows and controls include `desktop/` (they
may not include `app/`).

## Decisions recorded here

- LORD_FIDELITY-18 (RGB pack control window): **W2**. W1 fixlib ships the generic RGB
  profile (`LJ_CTL_RGBPACK` exists in the enum and `lj_fixlib_by_id` returns it); the
  window lands with `lj_ctl_rgbpack.c` in W2, so no W1 golden or O/S/F row for it.
- W0-gs2-266C: the 17 bytes at `.GS2` offset 0x266C are carried only by
  `lj_profile.gs2_raw` and travel untouched through `lj_gs2_write`; the comment sits on
  `lj_gs2_parse` in `fixlib/lj_profile.h`. Nothing in W1 decodes them.
- sACN settings are installation-wide in `Hardware.Ini [SACN]` (ours; design 5.2 names
  the file), loaded by `lj_sacn_cfg_load`; the CID is generated once and saved.
- Hotkey setups: `<userdir>Hotkeys\SetupN.Hot` in the `.Hot` grammar of
  `io/lj_hotkeys.h` (PROPOSAL: LJ2's binary `.Hot` is undecoded).

## Deviations from design.md (and why)

- `enum lj_ctl_row_kind` (design 6.4) is **`enum lj_ctl_elem` with `LJ_W_*`** in
  fixlib: W0 gave the name to the DFM class census in `ui/forms/lj_form.h` and
  controls/ includes both. `CTL_INTENSITY ...` are `LJ_CTL_*` (the `LJ_` rule).
- `lj_profile` gains `uint8_t pair_lsb[LJ_PROFILE_CH]` (non-pan/tilt 16-bit pairs,
  `LJ_PAIR16_NON_PT_AS_ONE`): the design says such pairs are treated as one value but
  gives no field to declare them; `pt16` + `pan_ofs/tilt_ofs` + `pt_patch` cover pan/tilt
  only.
- `lj_exe_row.binding` names the tier (none / hand / qxf) so the Fixture Configuration
  tree can grey a row without a second lookup.
- `lj_cmd` carries `c`, `bgseq[5]`, `st` and `sacn` beyond the design's `a, b, cue,
  seq[], bg, gm`: a background cue has five sequences to pin, a static has its slot
  record, SET_MASTER needs level + mode, and the sACN config crosses as `const void*`
  because engine/ may not include io/. `lj_engine_take_sacn_cfg` /
  `lj_engine_is_offline` / `lj_engine_set_sacn_stats` are the engine-thread hooks
  ME-23 and S9 need; the design's loop pseudo-code reads `offline` and `map[]` without
  saying where from.
- `LJ_CMD_*` kinds beyond the design's list: `LOAD_BG`, `LOAD_GENMACRO`, `CHASE`,
  `SOLO`, `HTP_GROUP`, `SEQ_INT`, `MACRO_CTL`; each is a UI action the design describes
  elsewhere (3.4, 3.5, 03 s5.5/5.9/5.11) with no command named.
- `LJ_LAYER_ORDER` / `LJ_POST_ORDER` are `extern const uint8_t[]` (the brief) rather
  than the design's `static const` in the rules header, so tests read the order the
  engine runs; upper-case names for variables are the one exception to the `lj_`
  rule, as the brief asked.
- `enum lj_master_mode` / `enum lj_seq_int_mode` live in `engine/lj_engine_rules.h`
  (the brief); the model stores them as `uint8_t` with a comment, since model/ cannot
  include engine/.
- `LJ_STATICS2_SLOTS_PER_PAGE` is a macro spelled identically in `lj_engine_rules.h`
  and `lj_model.h` (`#ifndef` guarded): design 4 uses it in `lj_static_page` while
  design 3.3 defines it in the rules header, which is above the model.
- `LJ_STATIC_RUNNERS 64` (PROPOSAL): the design says "one runner per active static
  slot" without a bound; 64 pages x 20 slots is too many fixed arenas.
- `lj_table_free` takes a destructor; `lj_table_remove` returns the record (the
  design lists only init/get/put/remove/free).
- `lj_library` paths are `LJ_PATH_CAP = 1024` bytes; `lj_library_next_number` returns
  the number (0 on error) rather than int 0/-1 because the number is the result.
- `lj_sacn_stats` is the TYPE and `lj_sacn_stats_get` the function (one C namespace,
  as with `lj_engine_status`). `lj_sacn_pack_data/discovery` are added so
  `test_packets.c` checks bytes without a socket.
- `lj_nic` is ours (`name, ipv4, mask, up, loopback`); `lj_net_hostname/startup/cleanup`
  added for the default source name and WSAStartup.
- The function table is an X-macro (`LJ_FN_LIST`) so the enum, the names and the
  captions cannot drift; numbered families keep one `%d` caption and `n`.
- `lj_fn_run` keeps the design signature; `lj_fn_run_prog(fn, prog_num, edge)` carries
  the bound program number; the binding table is file-static in `lj_fn.c` (the design
  declares `lj_fn_run` without a context, and there is one UI thread).
- `lj_ctx` (desktop) holds `current_scene`, `blind` and the three modified flags rather
  than `lj_show` (design 4 kept verbatim) or `app/` (not includable from windows/).
- Window files under `windows/` are `lj_seq_form.c` etc. (the layering test requires
  the prefix; the design wrote `seq_form.c`).
- `lj_engine_status` keeps the W0 rename of the design's `lj_engine_view` struct.
- `lj_engine.c` still runs the W0 pattern tick (`>> 8` on the signed table, ME-60):
  the engine track replaces the body and regenerates the determinism sha in the same
  commit; `engine_stub` stays green until then.
