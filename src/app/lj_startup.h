/* lj_startup.h - the SDL-free pieces of the startup sequence ([01 s6.2, s8],
 * LORD_FIDELITY-20, design 9.4) so tests/ui/test_uiscript.c can run them
 * without a window: which user library to open (U:<folder> beats Startup.ini
 * [USERDIRECTORY] Start=, Default recreated when missing), the startup status
 * window, and Main.Ini [OTHER] LastRun. The rest of the sequence (platform,
 * engine, windows, [STARTUP] actions) is lj_app.c. */
#ifndef LJ_APP_STARTUP_H
#define LJ_APP_STARTUP_H

#include "core/lj_ini.h"
#include "model/lj_library.h"
#include "ui/lj_window.h"

#define LJ_STARTUP_PLEASE_WAIT "Please Wait.."   /* TLJMainForm's design caption until the library is open [13 captions, TLJMAINFORM row]; D2 verbatim */

int        lj_startup_library(const char *userdir, const char *u_arg, lj_library *out);   /* 0 / -1; on -1 out->root = userdir so Main.Ini and forms.ini still resolve (W0 layout) */
void       lj_startup_last_run(lj_ini *main_ini);                                        /* [OTHER] LastRun=LJ4EVR x.y.z, FirstRun=0 (LJ2's keys [13 s6]) */
lj_window *lj_startup_status_create(void);                                               /* the generated TStartupStatusWindow with its handlers bound; used when lj_win_open(LJ_WIN_PLEASE_WAIT) has no body */

#endif /* LJ_APP_STARTUP_H */
