/* lj_app_w4.h - the W4 half of the app seam (docs/w4-contracts.md section 4). Binds the desktop actions whose window is
 * a W4 family (lj_windows.h W4 block) and runs the per-pass W4 work on the UI thread. SDL-free. The tracks never edit
 * this file: a family answers through its own behaviour file (windows/lj_<family>.c) and its bind_fns. */
#ifndef LJ_APP_APP_W4_H
#define LJ_APP_APP_W4_H

#include <stdint.h>

void lj_app_w4_start(void);              /* after lj_app_w3_start: binds LJ_ACT_FIXTURE_INFO / NOTES, AUTO_ARRANGE, DESKTOP_GRID / IMAGE / TABS,
                                          * ICON_STYLE, NEXTPREV_SETTINGS, RESTORE_DATA, HELP, SELECT_HELPFILE, WINDOWS_MEMORY, DEBUG_WINDOW,
                                          * FINGERS_SETUP and DMX_IN to their W4 windows */
void lj_app_w4_idle(uint64_t now_us);    /* every UI loop pass after lj_app_cuelist_step: lj_lightcorder_step (the recorder copies frames here) */

#endif /* LJ_APP_APP_W4_H */
