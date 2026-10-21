/* lj_prefs.h - the ini-backed preferences the app reads at startup and writes at
 * close (design 5.1, 7.10, 8.1; ME-24, ME-23): Main.Ini [ENGINE] TickHz,
 * Hardware.Ini [SACN] (installation-wide, CID generated once), and window
 * positions routed to forms.ini or Main.Ini by lj_formsini_section. SDL-free. */
#ifndef LJ_APP_PREFS_H
#define LJ_APP_PREFS_H

#include "core/lj_ini.h"
#include "io/lj_sacn.h"
#include "ui/lj_window.h"

int  lj_prefs_tick_hz(lj_ini *main_ini);                                        /* [ENGINE] TickHz clamped to LJ_TICK_HZ_MIN..MAX (ME-24); default LJ_TICK_HZ_DEFAULT */
int  lj_prefs_sacn(lj_ini *hardware_ini, const char *path, lj_sacn_cfg *cfg);  /* [SACN] -> cfg; a missing CID is generated and the file saved; 0 = validated, -1 = rejected (logged, not to be posted) */
int  lj_prefs_window_pos(lj_ini *forms_ini, lj_ini *main_ini, const lj_window *win, int *x, int *y);   /* the persisted Left/Top; -1 = never persisted or not yet saved (centre on the owner) */
void lj_prefs_window_save(lj_ini *forms_ini, lj_ini *main_ini, const lj_window *win, int x, int y);    /* Left/Top (+ Width/Height when sizeable) into the right ini; no-op for never-persisted forms */

#endif /* LJ_APP_PREFS_H */
