/* lj_app_cuelist_host.h - the app's host half (was lj_app_w3.h + lj_app_w4.h, ME-173; docs/w3-contracts.md section 2, w4-contracts
 * section 4). The lj_app.h W3 block (lj_app_cuelist_step, lj_app_load_cuelist, the reloads, lj_app_followspot) is defined in lj_app_cuelist_host.c; this
 * header carries what the app, the UI script and the tests need beside it. SDL-free, UI thread only (design 2.3:
 * the cue list runs here, inside the 10 ms idle pass, never in the engine). */
#ifndef LJ_APP_APP_CUELIST_HOST_H
#define LJ_APP_APP_CUELIST_HOST_H

#include <stddef.h>
#include <stdint.h>
#include "app/lj_app.h"

void     lj_app_cuelist_host_start(void);        /* after lj_app_seam_start: runner (ctx->cl), editor record, gdc state, shape table, user profiles, follow spots, the W3 actions and the cue-list lj_fn rows no window bound */
void     lj_app_cuelist_host_stop(void);         /* after lj_engine_destroy, before lj_show_free: frees the editor record, the shape override, a linked-away list */
uint32_t lj_app_cuelist_steps(void);   /* lj_app_cuelist_step calls so far (the once-per-loop-pass check) */
int      lj_app_load_cuelist_path(const char *path, uint32_t number);   /* a .Cl2 as current_cl (number 0 keeps the file's num=); the runner unpinned from the old list first; 0 / -1 + log */
void     lj_app_cuelist_prog(uint32_t number);                        /* LJ_FN_PROG_CUELIST (hotkey, MIDI): load + Top */
void     lj_app_mtc_feed(uint8_t qf_data);                            /* an F1 quarter-frame off the MIDI queue (lj_app_midi_dispatch) */
int      lj_app_gs2_import(const char *path, char *err, size_t cap);  /* .GS2 / .Udf / .Udo -> UserFixtureDef\<base>.GS2 + lj_app_gs2_reload; 0 / -1 with the importer's message */

void     lj_app_cuelist_host_windows_start(void);   /* after lj_app_cuelist_host_start: binds LJ_ACT_FIXTURE_INFO / NOTES, AUTO_ARRANGE, DESKTOP_GRID / IMAGE / TABS,
                                            * ICON_STYLE, NEXTPREV_SETTINGS, RESTORE_DATA, HELP, SELECT_HELPFILE, WINDOWS_MEMORY, DEBUG_WINDOW,
                                            * FINGERS_SETUP and DMX_IN to their W4 windows */
void     lj_app_cuelist_host_idle(uint64_t now_us);  /* every UI loop pass after lj_app_cuelist_step: lj_lightcorder_step (the recorder copies frames here) */
/* W6 LORD_ENGINE-9: the idle pass's middle, in its order: lj_app_io_idle (DMX-in, audio, remote, External Application,
 * and the MediaPlayer clock), then lj_app_cuelist_step, then lj_app_cuelist_host_idle. The media clock and the inputs
 * are stepped first, so an MP wait releases in the same pass the clip crosses its point. lj_app.c calls this. */
void     lj_app_cuelist_pass(uint64_t now_us);

#endif /* LJ_APP_APP_CUELIST_HOST_H */
