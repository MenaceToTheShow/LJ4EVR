/* lj_tests.c - the CTest driver: lj4evr_tests <subcommand> [args]. No SDL.
 * A track adds a row here and the same name to LJ_TEST_SUBCOMMANDS in CMakeLists.txt.
 * A subcommand returns 0 on pass, non-zero on fail, and prints its own evidence. */
#include <stdio.h>
#include <string.h>

int lj_test_widget(int argc, char **argv);
int lj_test_widgets_lists(int argc, char **argv);
int lj_test_render(int argc, char **argv);
int lj_test_ui_golden(int argc, char **argv);
int lj_test_draw_goldens(int argc, char **argv);
int lj_test_bmp(int argc, char **argv);
int lj_test_text(int argc, char **argv);
int lj_test_encoding(int argc, char **argv);
int lj_test_ini(int argc, char **argv);
int lj_test_textrec(int argc, char **argv);
int lj_test_lock(int argc, char **argv);
int lj_test_clock_monotonic(int argc, char **argv);
int lj_test_trig(int argc, char **argv);
int lj_test_engine_stub(int argc, char **argv);
int lj_test_dump_frames(int argc, char **argv);
int lj_test_font_corpus(int argc, char **argv);
int lj_test_font_cells(int argc, char **argv);
int lj_test_art_table(int argc, char **argv);
int lj_test_forms_tables(int argc, char **argv);
int lj_test_window(int argc, char **argv);
int lj_test_menu(int argc, char **argv);
int lj_test_formsini(int argc, char **argv);
int lj_test_hittest(int argc, char **argv);
int lj_test_widgets_lj(int argc, char **argv);
int lj_test_widgets_basic(int argc, char **argv);
/* W1 rows (append-only, one block per track; add the name to LJ_TEST_SUBCOMMANDS in CMakeLists.txt):
 * model: test_model_*  fixlib: test_fixlib_*  engine: test_engine_*  replay: replay (file args)  sacn: test_sacn_*  actions: test_actions */
int lj_test_model_table(int argc, char **argv);
int lj_test_replay(int argc, char **argv);
int lj_test_desktop(int argc, char **argv);
int lj_test_controls_c(int argc, char **argv);
int lj_test_fixlib_profiles(int argc, char **argv);
int lj_test_windows_b(int argc, char **argv);
int lj_test_windows_b2(int argc, char **argv);   /* W2 windows-b */
int lj_test_engine_fade(int argc, char **argv);
int lj_test_engine_ownership(int argc, char **argv);
int lj_test_engine_perf(int argc, char **argv);
int lj_test_engine_post(int argc, char **argv);
int lj_test_uiscript(int argc, char **argv);
int lj_test_controls_a(int argc, char **argv);
int lj_test_model_records(int argc, char **argv);
int lj_test_model_library(int argc, char **argv);
int lj_test_model_scene(int argc, char **argv);
int lj_test_replay_dump(int argc, char **argv);
int lj_test_replay_grammar(int argc, char **argv);
int lj_test_sacn_packets(int argc, char **argv);
int lj_test_sacn_rate(int argc, char **argv);
int lj_test_sacn_loopback(int argc, char **argv);
int lj_test_sacn_cfg(int argc, char **argv);
int lj_test_hotkeys(int argc, char **argv);
int lj_test_controls_b(int argc, char **argv);
int lj_test_windows_c(int argc, char **argv);
int lj_test_windows_a(int argc, char **argv);
int lj_test_windows_a_w2(int argc, char **argv);   /* W2 windows-a */
int lj_test_ui_golden_ctl(int argc, char **argv);
/* W2 contracts (docs/w2-contracts.md): model / engine / io / ui rows, appended */
int lj_test_model_w2_range(int argc, char **argv);
int lj_test_model_w2_cuepages(int argc, char **argv);
int lj_test_runner_loop(int argc, char **argv);
int lj_test_intensity_rule(int argc, char **argv);
int lj_test_statics(int argc, char **argv);
int lj_test_midi_map(int argc, char **argv);
int lj_test_prefs(int argc, char **argv);
int lj_test_desktop_w2(int argc, char **argv);   /* W2 desktop track */
int lj_test_hotkey_setups(int argc, char **argv);   /* midi track */
int lj_test_uiscript_w2(int argc, char **argv);    /* app track */
int lj_test_replay_w2(int argc, char **argv);   /* W2 replay track */
int lj_test_model_w2_records(int argc, char **argv);   /* W2 model track */
int lj_test_model_w2_library(int argc, char **argv);   /* W2 model track */

int lj_test_windows_d(int argc, char **argv);   /* W2 windows-d */
int lj_test_chase(int argc, char **argv);
int lj_test_lock_hold(int argc, char **argv);
int lj_test_engine_w2(int argc, char **argv);
int lj_test_profiles_w2(int argc, char **argv);   /* W2 profiles track */
/* W3 contracts (docs/w3-contracts.md section 7), appended */
int lj_test_cuelist(int argc, char **argv);
int lj_test_cuelist_run(int argc, char **argv);
int lj_test_genmacro(int argc, char **argv);
int lj_test_movemacro(int argc, char **argv);
int lj_test_movement_w3(int argc, char **argv);   /* W3 movement: the five windows headless (tests/ui/test_movement_w3.c) */
int lj_test_gdc_grammar(int argc, char **argv);
int lj_test_gs2(int argc, char **argv);
int lj_test_udf_crc(int argc, char **argv);
int lj_test_gs2_dump(int argc, char **argv);
int lj_test_profile_editor(int argc, char **argv);
int lj_test_profile_editor_form(int argc, char **argv);   /* profile-editor: the three windows headless (tests/ui/test_profile_editor_form.c) */
int lj_test_followspot(int argc, char **argv);
int lj_test_gdc_fades_fs(int argc, char **argv);   /* W3 gdc-fades-followspot windows */

int lj_test_cuelist_form(int argc, char **argv);   /* W3 cuelist-window */
int lj_test_uiscript_w3(int argc, char **argv);    /* W3 desktop-app */
int lj_test_profiles_w3(int argc, char **argv);   /* W3 profiles */
int lj_test_genmacro_editor(int argc, char **argv);   /* W3 genmacro: the editor and its dialogs headless */
/* W4 (appended) */
int lj_test_align(int argc, char **argv);   /* W4 track 0, ME-43 */
int lj_test_ctx_scene(int argc, char **argv);   /* W4 entry, ME-129 */
int lj_test_help_viewer(int argc, char **argv);   /* W4 docs-release */
int lj_test_dmx_in(int argc, char **argv);   /* W4 hw-dmxin */
int lj_test_llf(int argc, char **argv);   /* W4 lightcorder-debug */
int lj_test_debug_form(int argc, char **argv);   /* W4 lightcorder-debug */
int lj_test_desktop_dialogs(int argc, char **argv);   /* W4 dlg-desktop */
int lj_test_show_dialogs(int argc, char **argv);   /* W4 dlg-show */
int lj_test_art_census(int argc, char **argv);   /* W4 art */
int lj_test_srcimg(int argc, char **argv);       /* W4 art */
int lj_test_config_dialogs(int argc, char **argv);   /* W4 dlg-config */
int lj_test_da2532_fingers(int argc, char **argv);   /* W4 da2532-fingers */
int lj_test_file_dialogs(int argc, char **argv);   /* W4 dlg-files */
int lj_test_restore_data(int argc, char **argv);   /* W4 dlg-files */
/* W5 contracts (docs/w5-contracts.md seam 11; append-only) */
int lj_test_engine_w5(int argc, char **argv);
int lj_test_socket(int argc, char **argv);
int lj_test_extapp(int argc, char **argv);      /* W5 extapp (append-only) */
int lj_test_extapp_seq(int argc, char **argv);
int lj_test_extapp_ui(int argc, char **argv);
int lj_test_fuzz_sacn_rx(int argc, char **argv);
int lj_test_fuzz_vtr(int argc, char **argv);
int lj_test_dmxin_rules(int argc, char **argv);       /* W5 dmxin (append-only) */
int lj_test_dmxin_vtr(int argc, char **argv);
int lj_test_dmxin_functions(int argc, char **argv);
int lj_test_dmxin_loopback(int argc, char **argv);
int lj_test_fuzz_artnet(int argc, char **argv);
int lj_test_artnet(int argc, char **argv);   /* W5 artnet */
int lj_test_fuzz_remote(int argc, char **argv);
int lj_test_fuzz_ljb(int argc, char **argv);
int lj_test_uiscript_w5(int argc, char **argv);
/* W5 media (append-only) */
int lj_test_media(int argc, char **argv);
int lj_test_media_form(int argc, char **argv);
int lj_test_audio(int argc, char **argv);          /* W5 audio */
int lj_test_audio_form(int argc, char **argv);     /* W5 audio */
int lj_test_remote(int argc, char **argv);        /* W5 remote */
int lj_test_serial_pty(int argc, char **argv);    /* W5 remote */
int lj_test_remote_form(int argc, char **argv);   /* W5 remote */
int lj_test_ljb(int argc, char **argv);           /* W5 ljb */
int lj_test_ljb_sample(int argc, char **argv);    /* W5 ljb */
static const struct { const char *name; int (*fn)(int, char **); } lj_subcommands[] = {
    { "font_corpus", lj_test_font_corpus },
    { "font_cells", lj_test_font_cells },
    { "art_table", lj_test_art_table },
    { "widget", lj_test_widget },
    { "widgets_lists", lj_test_widgets_lists },
    { "forms_tables", lj_test_forms_tables },
    { "window", lj_test_window },
    { "menu", lj_test_menu },
    { "formsini", lj_test_formsini },
    { "hittest", lj_test_hittest },
    { "render", lj_test_render },
    { "ui_golden", lj_test_ui_golden },        /* the six reviewed W0 goldens, byte-exact (tests/ui/golden) */
    { "draw_goldens", lj_test_draw_goldens },
    { "bmp", lj_test_bmp },
    { "text", lj_test_text },
    { "encoding", lj_test_encoding },
    { "ini", lj_test_ini },
    { "textrec", lj_test_textrec },
    { "lock", lj_test_lock },
    { "clock_monotonic", lj_test_clock_monotonic },
    { "trig", lj_test_trig },
    { "engine_stub", lj_test_engine_stub },
    { "dump-frames", lj_test_dump_frames },      /* dump-frames [path], default frames.bin (CI determinism job) */
    { "widgets_lj", lj_test_widgets_lj },
    { "widgets_basic", lj_test_widgets_basic },
    /* ---- W1 rows: append below, never reorder (contract: docs/w1-contracts.md) ---- */
    { "test_model_table", lj_test_model_table },   /* model: lj_table + lj_words (W1 contracts) */
    { "replay", lj_test_replay },                  /* replay <file.lj4r>...: the CTest row replay_all passes every .lj4r under tests/engine/replay */
    { "test_desktop", lj_test_desktop },           /* desktop: tests/ui/test_desktop.c (W1 desktop track) */
    { "test_windows_b", lj_test_windows_b },     /* windows-b: Cue control, Save Cue, List of Cues, Master Intensity */
    { "test_engine_fade", lj_test_engine_fade },             /* engine: lj_fade_value table, snap rules, 16-bit pair through the engine (3.3, ME-22) */
    { "test_engine_ownership", lj_test_engine_ownership },   /* engine: design 2.3 ownership contract (load error swap, 44 drops, pending_* once) */
    { "test_engine_perf", lj_test_engine_perf },             /* engine: 100 fixtures, 12+5 slots, 20 statics, generic macro on 2048 ch, one tick < 2 ms (logged; gated at 10x in Release, LORD_C-12) */
    { "test_engine_post", lj_test_engine_post },             /* engine: S3-before-S2 (Example 3/6), SWITCH, blackout, HTP, overwrite, S7 patch, layer ranks, freeze clock, interrupted fade, macros */
    { "test_controls_a", lj_test_controls_a },     /* controls-a: Intensity / Levels / Lamp / Extended windows, O/S/F, locale caption, lj_prog_* (tests/ui/test_controls_a.c) */
    { "test_model_records", lj_test_model_records },   /* model: every 5.2 record round-trips, validators, 999-scene load < 200 ms */
    { "test_model_library", lj_test_model_library },   /* model: library tree, Default rule, [NEXT], backup/restore, ME-24 TickHz */
    { "test_model_scene", lj_test_model_scene },       /* model: F29 scene rules */
    { "--dump-frames", lj_test_replay_dump },      /* replay: --dump-frames <out.bin> <file.lj4r>...: every scenario's frames, per-scenario header (CI determinism cmp) */
    { "test_replay_grammar", lj_test_replay_grammar },   /* replay: bad lines rejected with their line number, expects on unpatched channels fail */
    { "test_sacn_packets", lj_test_sacn_packets }, /* sacn: Appendix B vectors byte-exact (tests/vectors.h) */
    { "test_sacn_rate", lj_test_sacn_rate },       /* sacn: ME-6 / ME-24 send mechanics with a fake clock */
    { "test_sacn_loopback", lj_test_sacn_loopback }, /* sacn: unicast 127.0.0.1 receiver: 3 packets per change, keep-alive, remap and close terminations (ME-23) */
    { "test_sacn_cfg", lj_test_sacn_cfg },         /* sacn: lj_sacn_cfg_validate clamps/rejects, load/save round trip */
    { "test_hotkeys", lj_test_hotkeys },           /* actions: lj_actions table, lj_hotkeys rules + .Hot, lj_fn edges (tests/io/test_hotkeys.c) */
    { "test_controls_b", lj_test_controls_b },     /* controls-b: the Movement control + the 17 position vectors (tests/ui/test_controls_b.c) */
    { "test_windows_c", lj_test_windows_c },       /* windows-c: fixture config tree/DIP/Config.dat, monitor colours + search, sACN page Apply, warnings/close prompt, startup opts (tests/ui/test_windows_c.c) */
    { "test_windows_a", lj_test_windows_a },       /* windows-a: Sequence control, Save Sequence, List of Sequences (tests/ui/test_windows_a.c) */
    { "test_uiscript", lj_test_uiscript },         /* app: the --script driver headless, startup library + LastRun, ME-52 locale, ME-65 UTF-8 pref path */
    { "test_fixlib_profiles", lj_test_fixlib_profiles },   /* fixlib: 201 rows, hand > qxf, window tables, ME-25/27, lj_layout_derive */
    { "test_controls_c", lj_test_controls_c },     /* controls-c: Colors / Gobos / Beam / Effects windows (tests/ui/test_controls_c.c) */
    { "ui_golden_ctl", lj_test_ui_golden_ctl },     /* integrator: the code-built control-window goldens, MAC 500 + dimmer (tests/ui/test_golden_ctl.c) */
    /* W2 (appended) */
    { "test_model_w2_range", lj_test_model_w2_range },       /* model: lj_kv_range (ME-77) rejects out-of-range numbers (tests/model/test_w2.c) */
    { "test_model_w2_cuepages", lj_test_model_w2_cuepages }, /* model: CuePageInfo.Dat round trip; lj_cuepages_* rows by the model track */
    { "test_runner_loop", lj_test_runner_loop },             /* engine: lj_runner_loop_step, the 03 s5.5 loop / end-action table (tests/engine/test_runner_loop.c) */
    { "test_intensity_rule", lj_test_intensity_rule },       /* engine: lj_seq_intensity_rule, Martin's 12-cell table (tests/engine/test_intensity_rule.c) */
    { "test_statics", lj_test_statics },                     /* engine/model: Statics 2 indices and caps; scope / LTP rows by the engine track (tests/engine/test_statics.c) */
    { "test_midi_map", lj_test_midi_map },                   /* midi: map, queue, inject, dispatch, Learn, drops (tests/io/test_midi_map.c) */
    { "test_prefs", lj_test_prefs },                         /* app/windows: lj_sysprefs Main.Ini round trip with LJ2's keys (tests/ui/test_prefs.c) */
    { "test_desktop_w2", lj_test_desktop_w2 },               /* desktop: F8 order, LEDs from the view, ME-29 gates from Main.Ini, every W2 menu leaf reaches its window, hotkey setups, memory gauge (tests/ui/test_desktop_w2.c) */
    { "test_hotkey_setups", lj_test_hotkey_setups },         /* midi: hotkey setups 1..8 under the library, setup switch, fallbacks (tests/io/test_hotkey_setups.c) */
    { "test_uiscript_w2", lj_test_uiscript_w2 },             /* app: W2 script verbs parse + run headless, MIDI dispatched on the UI thread, pending_bg round trip (tests/app/test_uiscript_w2.c) */
    { "test_replay_w2", lj_test_replay_w2 },                 /* replay: W2 line forms (seed, ack_bg, nobg, runtime gpatch/default) rejected with their line number (tests/engine/test_replay_w2.c) */
    { "test_model_w2_records", lj_test_model_w2_records },   /* model: every W2 record round-trips byte-equal, ME-77 / ME-30 rejections, scopes, CP1252 0xE9 (tests/model/test_model_w2_records.c) */
    { "test_model_w2_library", lj_test_model_w2_library },   /* model: backup/restore into a new library, traversal refused, [NEXT] Bg= (ME-14), copy, select, used-by-cue (tests/model/test_model_w2_library.c) */
    { "test_windows_a_w2", lj_test_windows_a_w2 },   /* W2 windows-a: Loop Options, Cue Time Control, cue pages, transparent / flash prefs, W2A_* goldens (tests/ui/test_windows_a.c) */
    { "test_chase", lj_test_chase },                         /* engine: the global chase timer, 1 ms taps, chase_030ms grid (tests/engine/test_chase.c) */
    { "test_lock_hold", lj_test_lock_hold },                 /* engine: one tick under lj_lock < tick / 4 with 4 cues, 20 statics, BG (ME-81, tests/engine/test_chase.c) */
    { "test_engine_w2", lj_test_engine_w2 },                 /* engine: the W2 kinds through the tick: ranks 2-7, transparent, flash latch, smoke, swaps, snapshot, blind, link (tests/engine/test_engine_w2.c) */
    { "test_windows_b2", lj_test_windows_b2 },             /* windows-b W2: Background Cue, Statics 2 (popup verbatim, STATIC_ACTIVATE, LEDs), Smoke Control, goldens (tests/ui/test_windows_b.c) */
    { "test_windows_d", lj_test_windows_d },               /* W2 windows-d: System Preferences, About, Delete Files, User Libraries, Backup/Restore, Notes, Striking Lamps, Edit Hotkeys, MIDI-IN, W2D_* goldens (tests/ui/test_windows_d.c) */
    { "test_profiles_w2", lj_test_profiles_w2 },           /* profiles: every 15/02-15/05 id by lj_fixlib_by_id, footprints, MAC 2000 pan pair, 812 gobo slots, ME-25/27 (tests/fixlib/test_profiles_w2.c) */
    /* W3 (appended) */
    { "test_cuelist", lj_test_cuelist },   /* cuelist-core: command enum / captions / inert families / time code packing; .Cl2 load_error + ref= rows (tests/model/test_cuelist.c) */
    { "test_cuelist_run", lj_test_cuelist_run },   /* cuelist-core: the UI-thread runner at 10 ms steps (tests/app/test_cuelist_run.c) */
    { "test_genmacro", lj_test_genmacro },   /* genmacro: step / shapes vs vectors.h, ME-34 override, .CMA (tests/engine/test_genmacro.c) */
    { "test_movemacro", lj_test_movemacro },   /* movement: positions vs 17-position_ref.py, Bezier, Fan Out, Auto Delay (tests/engine/test_movemacro.c) */
    { "test_gdc_grammar", lj_test_gdc_grammar },   /* gdc-fades-followspot: the F35 command line (tests/model/test_gdc.c) */
    { "test_gs2", lj_test_gs2 },   /* gs2: synthetic record + LJ4EVR_REFERENCE_DIR 48-file oracle compare, SKIP line when unset (tests/fixlib/test_gs2.c) */
    { "test_udf_crc", lj_test_udf_crc },   /* gs2: .Udf container, footer CRC message verbatim, truncation (tests/fixlib/test_gs2.c) */
    { "test_profile_editor", lj_test_profile_editor },   /* profile-editor: tab captions + field table bounds; editor rows (tests/fixlib/test_profile_editor.c) */
    { "test_profile_editor_form", lj_test_profile_editor_form },   /* profile-editor: Generic Fixture Profile, Customize Fixture, Channel Configuration (ME-16, ME-32, NIT-26, goldens W3PE_*) */
    { "test_followspot", lj_test_followspot },   /* gdc-fades-followspot: follow spot record / apply, sequence fades (tests/model/test_gdc.c) */
    { "gs2_dump", lj_test_gs2_dump },   /* gs2: gs2_dump <file.GS2|.Udf|.Udo>, the flat field list tools/gs2_check.py compares with 16-gs2_ref.py */
    { "test_cuelist_form", lj_test_cuelist_form },   /* cuelist-window: TCuelist2Form enable table, greyed CD / DA / MP rows, ListPop order, red load-error line, golden (tests/ui/test_cuelist_form.c) */
    { "test_profiles_w3", lj_test_profiles_w3 },   /* profiles: every 15/06-15/09 id by lj_fixlib_by_id, footprints, Atomic classes, MAC 550 pan pair, RoboColor palettes, ME-25/27 (tests/fixlib/test_profiles_w3.c) */
    { "test_uiscript_w3", lj_test_uiscript_w3 },   /* desktop-app: W3 script verbs parse + run headless, every W3 menu / toolbar leaf bound, runner step count, MP line red, ME-102 full-array paths (tests/app/test_uiscript_w3.c) */
    { "test_gdc_fades_fs", lj_test_gdc_fades_fs },   /* gdc-fades-followspot: Generic DMX Control, Set Fade Status, follow spot windows headless, ME-16, W3GDC_* goldens (tests/ui/test_gdc_fades_fs.c) */
    { "test_movement_w3", lj_test_movement_w3 },   /* movement: Movement Macro + Bezier, Delete Macros, Fan Out, Auto Delay, Cue Macro Control headless; ME-16, LORD_FIDELITY-12, goldens W3MM_* (tests/ui/test_movement_w3.c) */
    { "test_genmacro_editor", lj_test_genmacro_editor },   /* genmacro: Generic Macro Editor + Save / Select Macro headless, ME-16, F19 caption, F39 shapes, golden W3GM_EDITOR (tests/ui/test_genmacro_editor.c) */
    /* W4 (appended) */
    { "test_align", lj_test_align },   /* ME-43: the full align pass reproduces every DFM at its design client, 0 misplaced (tests/ui/test_align.c) */
    { "test_ctx_scene", lj_test_ctx_scene },   /* ME-129: one current-scene getter, NULL past the end, the clamp once where the sequence shrinks (tests/ui/test_ctx_scene.c) */
    { "test_help_viewer", lj_test_help_viewer },   /* docs-release: every Help leaf opens LJ_WIN_HELP, the topics, About notices / trademark line, golden W4HELP_VIEWER (tests/ui/test_help_viewer.c) */
    { "test_dmx_in", lj_test_dmx_in },   /* hw-dmxin: Dmx In from the toolbar and the Global Patch source, inputs greyed with the hint, the four DMX-in sub-windows, dropped Hardware Setup tabs, goldens W4DMX_* (tests/ui/test_dmx_in.c) */
    { "test_llf", lj_test_llf },   /* lightcorder-debug: .LLF round trip from a replay, card cap, truncated / hostile files refused, the recorder + save dialogs headless, W4LC_* goldens (tests/model/test_llf.c) */
    { "test_debug_form", lj_test_debug_form },   /* lightcorder-debug: the Debug window (unhide, drops, layer_of ME-28, hardware tabs greyed), Statics / Msg debug, System Memory, W4DBG_* goldens (tests/ui/test_debug_form.c) */
    { "test_desktop_dialogs", lj_test_desktop_dialogs },   /* dlg-desktop: the 13 DESKTOP ids from their entry points, ME-16, 'Notification !' / 'Warning!' / 'Enable laser output ?' through assert_prompt, LORD_FIDELITY-12, W4DD_* goldens (tests/ui/test_desktop_dialogs.c) */
    { "test_show_dialogs", lj_test_show_dialogs },   /* dlg-show: the 16 SHOW ids from their entry points, ME-16, ME-120 Offset / Sort, ME-127, the cue-slot and Statics 2 Q9 prompts through assert_prompt, W4SD_* goldens (tests/ui/test_show_dialogs.c) */
    { "art_census", lj_test_art_census },   /* art: W4-ART-1 / -2 counts, every profile -> an icon, every named palette entry -> a tile or gel, 0 unresolved (tests/fonts/test_art.c) */
    { "test_srcimg", lj_test_srcimg },   /* art: W4-SRCIMG substitution through an in-memory loader (tests/gfx/test_srcimg.c); lj4evr_srcimg_test runs the SDL half */
    { "test_config_dialogs", lj_test_config_dialogs },   /* dlg-config: the 25 CONFIG ids from their entry points, ME-16, F41 warnings + Save  Anyway, F43 address rows, F44 copy / clear, the Fixture Configuration Q9 prompts through assert_prompt, ME-126 old-style name, W4C_* goldens (tests/ui/test_config_dialogs.c) */
    { "test_da2532_fingers", lj_test_da2532_fingers },   /* W4 da2532-fingers: the 7 DA2532 + 10 FINGERS windows headless, 2532 latch / flash / pages / Record / file, the midi script row, EXT Learn, Fingers fader CC / bump / GM / F keys / slots / jog, keyboard EXT row, goldens W4DA_* (tests/ui/test_da2532_fingers.c) */
    { "test_file_dialogs", lj_test_file_dialogs },   /* dlg-files: the 19 FILES ids + please_wait open, the Startup Status countdown, Data Restore from the prefs tab and System menu (create, change, restore), password prompts, the F56 update dialogs on synthetic older records, Rename / Delete / Restore folder tree, goldens FD_* (tests/ui/test_file_dialogs.c) */
    { "test_restore_data", lj_test_restore_data },   /* dlg-files: restore-data round trip, password, truncated / CRC / hostile archives refused (LORD_C-5), ME-130 U:<new folder>, F56 header rewrite (tests/model/test_restore_data.c) */
    /* W5 contracts (append-only) */
    { "test_engine_w5", lj_test_engine_w5 },   /* contracts: LJ_CMD_DMX_IN / OVERWRITE_ENABLE into S1, LJ_TRIG_AUDIO_EVENT with Audio-Trigs / Trig (tests/engine/test_engine_w5.c) */
    { "test_socket", lj_test_socket },         /* contracts: io/lj_socket.h on loopback, UDP + TCP, non-blocking (tests/io/test_socket.c) */
    { "fuzz_sacn_rx", lj_test_fuzz_sacn_rx },  /* dmxin (pending): sACN receiver fuzz row (tests/io/test_dmxin.c) */
    { "fuzz_vtr", lj_test_fuzz_vtr },          /* dmxin (pending): .vtr parser fuzz row (tests/io/test_dmxin.c) */
    { "dmxin_rules", lj_test_dmxin_rules },          /* dmxin: E1.31 receiver rules, merge, loss, tables, emulation, Art-Net input, loopback unicast (tests/io/test_dmxin.c) */
    { "dmxin_vtr", lj_test_dmxin_vtr },              /* dmxin: .vtr parse / limits / names / linear / round trip, SYSTEM tables, DmxIn.dat (tests/io/test_dmxin.c) */
    { "dmxin_functions", lj_test_dmxin_functions },  /* dmxin: the 05 s3.2 threshold and edge rules on synthetic frames (tests/io/test_dmxin.c) */
    { "dmxin_loopback", lj_test_dmxin_loopback },    /* dmxin: W5-DMXIN-1 unicast to 127.0.0.1:5568, our own CID ignored (tests/io/test_dmxin.c) */
    { "fuzz_artnet", lj_test_fuzz_artnet },    /* artnet (pending): ArtDmx / ArtPoll / ArtPollReply fuzz row (tests/io/test_artnet.c) */
    { "fuzz_remote", lj_test_fuzz_remote },    /* remote: 3-byte frame parser + exec fuzz row (tests/io/test_remote.c) */
    { "fuzz_ljb", lj_test_fuzz_ljb },          /* ljb: .LJB container fuzz row (tests/model/test_ljb.c) */
    { "test_uiscript_w5", lj_test_uiscript_w5 },   /* contracts: app/lj_app_io.c headless (W5-X-OFF, Emulate Audio Trig), the four W5 script verbs' checks (tests/ui/test_uiscript_w5.c) */
    /* W5 media (append-only) */
    { "test_media", lj_test_media },             /* media: io/lj_media.c clock / formats / states, the runner's MP lines sample-exact, .Cl2 MP round trip (tests/io/test_media.c) */
    { "test_media_form", lj_test_media_form },   /* media: the LJ MediaPlayer window + golden W5MED_MEDIA_PLAYER, the Cuelist window's MP rows live (tests/io/test_media.c) */
    /* W5 extapp (append-only) */
    { "test_extapp", lj_test_extapp },         /* extapp: W5-EXT-2 grammar, [EXTAPP] / definition files hostile, the engine frame copy, loopback TCP with a dead peer (tests/io/test_extapp.c) */
    { "test_extapp_seq", lj_test_extapp_seq }, /* extapp: W5-EXT-4 the scene record's ext word round trip, v1.0.0 files unchanged, bad ext lines refused (tests/io/test_extapp.c) */
    { "test_extapp_ui", lj_test_extapp_ui },   /* extapp: W5-EXT-1 both entries headless, goldens CTL_EXTERNAL_EXTENDED / W5X_EXTAPP_SETUP (tests/io/test_extapp.c) */
    { "test_audio", lj_test_audio },           /* W5 audio: the analyzer on synthetic PCM (120 BPM = 2 trigs/s, silence, noise, decay / sensitivity / gain, FFT bands + EQ, clip, ring, [AUDIO] keys), W5-AUD-6 inputs (tests/io/test_audio.c) */
    { "test_audio_form", lj_test_audio_form }, /* W5 audio: the Audio Analyzer window, &Audio menu, Hardware Setup tsAudio page, status-bar Trig, audio_inject, golden W5AU_ANALYZER (tests/ui/test_audio_form.c) */
    { "test_artnet", lj_test_artnet },         /* artnet: W5-ART-1..6 vectors, readers, [ARTNET], Hardware Setup rows, discovery / expiry, rate, bind failure, loopback wire (tests/io/test_artnet.c) */
    { "test_remote", lj_test_remote },         /* W5 remote: 05 s10.1 commands 128-136 incl. the 0 meanings, framing / 0.2 s / resync, [REMOTE] keys hostile, loopback TCP bounded + budget (tests/io/test_remote.c) */
    { "test_serial_pty", lj_test_serial_pty }, /* W5 remote: the POSIX serial backend on a pty driving the same parser; SKIP on Windows (tests/io/test_remote_pty.c) */
    { "test_remote_form", lj_test_remote_form }, /* W5 remote: TRS232SettingsForm by UI script, [REMOTE] saved + reopened, the windows-layer state query (tests/ui/test_remote_form.c) */
    { "test_ljb", lj_test_ljb },               /* W5 ljb: W5-LJB-1 refusals, W5-LJB-2 hostile names, W5-LJB-3 synthetic import read back, W5-LJB-4 report + Restore window entry, LORD_C-9 (tests/model/test_ljb.c) */
    { "ljb_sample", lj_test_ljb_sample },      /* W5 ljb: W5-LJB-5, the note 18 sample under LJ4EVR_REFERENCE_DIR; SKIP without it (tests/model/test_ljb.c) */
};

int main(int argc, char **argv)
{
    int i;
    if (argc < 2) {
        fprintf(stderr, "usage: lj4evr_tests <subcommand>\n");
        for (i = 0; i < (int)(sizeof lj_subcommands / sizeof lj_subcommands[0]); i++) fprintf(stderr, "  %s\n", lj_subcommands[i].name);
        return 2;
    }
    for (i = 0; i < (int)(sizeof lj_subcommands / sizeof lj_subcommands[0]); i++)
        if (strcmp(argv[1], lj_subcommands[i].name) == 0) return lj_subcommands[i].fn(argc - 1, argv + 1);
    fprintf(stderr, "unknown subcommand %s\n", argv[1]);
    return 2;
}
