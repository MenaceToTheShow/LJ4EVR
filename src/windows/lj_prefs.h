/* lj_prefs.h - Startup Options (TSTARTUPFORM, [01 s6.2]; Main.Ini [STARTUP] AutoLampOn
 * Seq Cue Cl PCUE SeqRun ClRun Minimized ShowNotes, -1 = none [18 s2]) and the four
 * 2.209 preferences check boxes of TSYSTEMPREFSFORM's External Links tab (External
 * Links (ver 0.1), External Links Memory (ver 1.0), Run as Primary / Secondary
 * Instance): present, persisted to Main.Ini, inert (ME-31, LORD_FIDELITY-8). */
#ifndef LJ_WINDOWS_PREFS_H
#define LJ_WINDOWS_PREFS_H

#include <stdint.h>
#include "core/lj_ini.h"
#include "windows/lj_windows.h"

typedef struct { int auto_lamp_on /* 0 none, 1..5 minutes */; int32_t seq, cue, cl, pcue /* -1 = none */; uint8_t seq_run, cl_run, minimized, show_notes; } lj_startup_opts;
void lj_startup_opts_load(lj_ini *main_ini, lj_startup_opts *o);     /* missing keys = LJ2's shipped defaults (AutoLampOn=0 Seq=-1 Cue=-1 Cl=-1 PCUE=-1, flags 0) */
void lj_startup_opts_save(lj_ini *main_ini, const lj_startup_opts *o);

lj_window *lj_startup_options_open(void);
void       lj_startup_options_close(void);
lj_window *lj_startup_options_window(void);
int        lj_startup_options_apply(void);                           /* the Ok button: page -> ctx->main_ini; 0 / -1 */

/* the four inert boxes: the System Preferences owner calls these at open / Ok */
void lj_prefs_inert_load(lj_window *system_prefs, lj_ini *main_ini);  /* [PREFS] ExternalLinks01 ExternalLinksMem10 PrimaryInstance SecondaryInstance (ours) */
void lj_prefs_inert_save(lj_window *system_prefs, lj_ini *main_ini);

#endif /* LJ_WINDOWS_PREFS_H */
