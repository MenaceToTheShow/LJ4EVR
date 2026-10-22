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
