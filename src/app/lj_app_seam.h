/* lj_app_seam.h - the SDL-free half of the app (design 2.3 rules 2-6; docs/w2-contracts.md seams 1-3):
 * the record-loading seam, the pending-load servicing, MIDI dispatch on the UI thread, System
 * Preferences consequences, hotkey setups, snapshot, Striking Lamps and the W2 desktop actions.
 * The public lj_app_* entry points it defines stay declared in app/lj_app.h (one API for the app);
 * lj_app.c keeps argv, the platform host, startup / shutdown order and the idle loop. Split out
 * so lj4evr_tests links it without platform/ (tests/app/test_uiscript_w2.c). Every function here
 * runs on the UI thread. */
#ifndef LJ_APP_APP_SEAM_H
#define LJ_APP_APP_SEAM_H

#include <stdint.h>
#include "app/lj_app.h"
#include "engine/lj_engine.h"
#include "ui/lj_window.h"

void lj_app_seam_start(void);              /* lj_ctx filled (show, engine, lib, main_ini, hotkeys): prefs, MIDI map + device, hotkey setup, lj_fn rows, W2 actions, window views */
void lj_app_seam_stop(void);               /* shutdown, before the engine thread stops (ME-23): MIDI port closed, the map saved when Auto Save */
void lj_app_seam_idle(uint64_t now_us);    /* every idle pass after lj_app_midi_dispatch: snapshot serving, real-time recording, Striking Lamps, Auto LampOn */
void lj_app_key(const lj_key_ev *ev, lj_window *win);   /* the platform key hook: lj_desktop_key, in_dialog from lj_win_is_dialog of the window that has focus */
int  lj_app_key_passes(const lj_window *win);           /* ME-182: 0 when win is the Audio Analyzer / LJ MediaPlayer and its Pass Keypresses is off (the side window keeps the key) */
void lj_app_repin_current_cue(void);       /* a slot of the working cue changed: under lj_lock the engine re-pins every runner from the tables (the Cue control's repin hook) */
int  lj_app_hotkey_setup(int setup);       /* <library>Hotkeys/SetupN.Hot -> ctx->hotkeys (missing file: Setup 1 = lj_hotkeys_default, 2..8 empty, no log line); 0 / -1 */
int  lj_app_post_locked(const lj_cmd *cmd);  /* W3 (ME-102): caller holds lj_lock; lj_engine_cmd, 0 / -1 when the array is full (logged once): the caller then keeps the record the post was to replace */

#endif /* LJ_APP_APP_SEAM_H */
