/* lj_app.h - lj4evr startup, the UI loop's idle pass and shutdown (design 2.9,
 * 3.1 rule 4, [01 s6.2], 5.1). SDL-free header; lj_app.c is the only file that
 * also includes platform/.
 *
 * Startup sequence (lj_app_main):
 *  1. argv: --save <bmp> --selftest [ref] --explode --ms <n> --native --zoom <1|2>
 *     --version --script <file> (docs/ui-script.md) U:<folder> (library, [01 s6.2]).
 *  2. main.c already did setlocale(LC_ALL, "C") then setlocale(LC_NUMERIC, "")
 *     once (ME-52, D6: the separator reaches displayed text only through
 *     lj_locale_decimal; INI and records use %d / strtol).
 *  3. pref path (lj_platform_pref_path, UTF-8 bytes kept as-is, ME-65) ->
 *     Startup.ini [USERDIRECTORY] Start=Default\ or U:<folder> -> lj_library_open
 *     (Default recreated when missing; lj_startup.c) -> Main.Ini, forms.ini
 *     (library) and Hardware.Ini (installation; CID generated and saved once,
 *     lj_prefs.c); [OTHER] LastRun written now, saved at close.
 *  4. lj_platform_init; the "Please Wait.." status window (LORD_FIDELITY-20)
 *     while the show loads: lj_show_new + lj_show_load, current sequence =
 *     one all-off scene, lj_ctx filled.
 *  5. lj_engine_create, lj_engine_set_tick_us (Main.Ini [ENGINE] TickHz clamped
 *     10..200, ME-24), lj_engine_bind under lj_lock, LJ_CMD_CURRENT_SEQ and
 *     LJ_CMD_SACN_CFG posted.
 *  6. lj_desktop_create (main window, LJ_WF_MAIN), lj_win_set_host, the W1
 *     windows reopened at their forms.ini positions, lj_fn bindings registered
 *     by desktop/windows/controls.
 *  7. Startup Options (Main.Ini [STARTUP] AutoLampOn Seq Cue Cl PCUE SeqRun
 *     ClRun Minimized ShowNotes): loads posted through lj_app_load_*.
 *  8. lj_platform_engine_start, then lj_platform_run(lj_app_idle).
 * Idle pass (every wake, SDL_WaitEventTimeout(10)): lj_uiscript_step when a
 * script runs; lj_app_service_pending; the cue list (W3); lj_app_refresh once
 * 100 ms have passed. Shutdown: "Close Lightjockey ?" -> lj_platform_engine_stop
 * (join) -> lj_sacn_close (the engine thread's last act; the app's call after
 * the join is the no-op backstop, ME-23) -> windows closed with positions saved
 * -> lj_engine_destroy -> lj_show_free -> ini files saved (not after a failed
 * script). The windows reach the screen through the lj_win_host the app
 * installs: open = platform open + forms.ini/Main.Ini position or centre on
 * the main window, close = save the position + platform close (7.10). */
#ifndef LJ_APP_APP_H
#define LJ_APP_APP_H

#include <stdint.h>

int  lj_app_main(int argc, char **argv);     /* the whole program; returns the process exit code */

void lj_app_quit(void);                      /* ends lj_platform_run after the current pass (the close prompt said yes, or the script quit) */
void lj_app_refresh(void);                   /* the 100 ms hook: lj_desktop_refresh, lj_win_refresh_all, lj_ctl_refresh */
void lj_app_service_pending(void);           /* every idle pass: lj_engine_view -> pending_link_cue / pending_bg / pending_genmacro -> lj_app_load_* (design 2.3 rule 3) */

/* UI-thread loads (design 2.3 rule 2): records built from disk outside lj_lock,
 * inserted into the lj_tables and handed to the engine with pinned pointers
 * under it; a sequence that fails to load leaves seq[i] NULL = `load error`. */
int  lj_app_load_cue(uint32_t number, int edit);     /* Cues\NNNNNNNN.cue + its sequences, bg cue, generic macro -> LJ_CMD_LOAD_CUE; show->current_cue replaced; 0 / -1 + log */
int  lj_app_load_bgcue(uint32_t number);             /* -> LJ_CMD_LOAD_BG */
int  lj_app_load_genmacro(uint32_t number);          /* -> LJ_CMD_LOAD_GENMACRO (W2) */
int  lj_app_load_sequence(uint32_t number);          /* into show->current_seq (the old one freed after the engine got the new pointer) -> LJ_CMD_CURRENT_SEQ */
int  lj_app_save_sequence(int as_new);               /* assigns a number on first save (lj_library_next_number), writes, inserts/replaces in the table, clears Seq. Modified; 0 / -1 */
int  lj_app_save_cue(int as_new);
int  lj_app_delete_record(int kind /* enum lj_rec_kind */, uint32_t number);   /* file + table; runners pointing at it are handed NULL first (2.3 rule 6) */


/* ---- W2 (appended) ----
 * Preferences (System Preferences, five tabs; windows/lj_prefs.h lj_sysprefs in ctx->prefs): the app loads Main.Ini at
 * startup, saves at close and on every Apply, and turns the record into consequences: ctx->allow_freeze / allow_offline /
 * allow_solo and the bar visibility (lj_desktop_toolbar_gates), the warning-box gates, the close confirm, quick save/load,
 * and LJ_CMD_PREFS for KeepSeqBlind / UseGlobalChaseRate / DisableChaseFade / StaticsUseFadetime / HotkeyChangeSeqDir (seam 3). */
int  lj_app_prefs_load(void);                /* Main.Ini -> ctx->prefs, then lj_app_prefs_apply; also [STATICS2] SlotsVisible (ME-30), [CUE] DisableChaseFade, the hotkey setup ([HOTKEYS] File) */
int  lj_app_prefs_save(void);                /* ctx->prefs -> Main.Ini (the file is written at close with the other ini files, and here on Apply) */
void lj_app_prefs_apply(void);               /* the consequences above; idempotent */
/* MIDI (seam 2): the OS thread posted 3-byte messages into io/lj_midi's queue; every idle pass drains it here and runs
 * lj_midi_dispatch on the UI thread (lj_fn_run / lj_fn_run_prog). Shutdown order (ME-23): lj_midi_close before the engine
 * thread is stopped, then lj_sacn_close after the join. */
void lj_app_midi_dispatch(void);             /* called from the idle pass right after lj_app_service_pending */
int  lj_app_midi_open(int device);           /* [SETUP] DeviceCode: opens through lj_midi_open with lj_midi_post as the callback; -1 closes; 0 / -1 */
/* Pinned-pointer seam for statics and BG cues (design 2.3 rules 3 and 6): the windows post LJ_CMD_STATIC_ACTIVATE /
 * LJ_CMD_LOAD_BG themselves with pointers into show->pages / the bgcues table; only these two app entries may clear a
 * slot or free a record, and they hand the engine NULL (STATIC_DEACTIVATE / LOAD_BG 0) in the same critical section. */
int  lj_app_static_clear(int page, int slot);   /* Clear Slot: deactivate if active, lj_static_slot_clear under lj_lock, Statics.dat saved; 0 / -1 */
int  lj_app_bgcue_clear(void);                  /* Clear BGCue: LOAD_BG with bg NULL; the record stays in the table */
/* Snapshot / real-time recording [03 s4.7]: LJ_CMD_SNAPSHOT_REQ with a UI-owned lj_frame; when the view's snapshot_tick
 * matches, the frame is folded into the current scene per the Snapshot window filters (selected only, non-default only,
 * DMX-in only, as Snap / as Fade), optionally as a new scene ("Auto add scene"); real-time recording repeats it at
 * 1..20 scenes/s for 2..999 scenes and then replaces the current sequence. */
int  lj_app_snapshot_request(void);          /* posts the request; 0 / -1 */
int  lj_app_snapshot_take(const uint8_t *filters /* lj_snapshot_filters bits */, int add_scene);   /* folds the last served frame; -1 when none is ready */
enum { LJ_SNAP_SELECTED_ONLY = 1, LJ_SNAP_NON_DEFAULT_ONLY = 2, LJ_SNAP_DMXIN_ONLY = 4, LJ_SNAP_AS_FADE = 8 };   /* TSnapshotSceneForm cbSelectedOnly cbNonDefaultOnly cbDMXinOnly cbAsFade (cbAsSnap = !cbAsFade) */
/* Striking Lamps [01 s6.2]: Lamp On one fixture at a time ~1 s apart with the TAutoLampForm progress; Stop cancels. */
int  lj_app_strike_lamps(int from_startup);

#endif /* LJ_APP_APP_H */
