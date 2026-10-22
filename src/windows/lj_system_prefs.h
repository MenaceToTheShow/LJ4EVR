/* lj_system_prefs.h - the System Preferences window (TSystemPrefsForm, five tabs [01 s6.1], F52);
 * open / close / window / apply are declared with the record in windows/lj_prefs.h. The page edits
 * a copy of ctx->prefs: OK copies it back and runs LJ_ACT_SYSTEM_PREFS_APPLY (the app saves Main.Ini
 * with LJ2's keys, re-gates the toolbar and posts LJ_CMD_PREFS, seam 3); Cancel and the frame
 * discard it. The two "Open controls on ..." boxes and Primary / Secondary Instance are exclusive
 * pairs; the External Links boxes are inert (ME-31), Offline Visualizer / SandNet / Capture greyed
 * (D12); the Data Restore buttons are W4. */
#ifndef LJ_WINDOWS_SYSTEM_PREFS_H
#define LJ_WINDOWS_SYSTEM_PREFS_H

#include "windows/lj_prefs.h"

void lj_system_prefs_refresh(void);   /* the pending close */
int  lj_system_prefs_box_count(void); /* the check boxes bound to lj_sysprefs fields (tests walk them) */
const char *lj_system_prefs_box(int i, const char **sec, const char **key);   /* DFM name of box i and its Main.Ini section/key; NULL out of range */

#endif /* LJ_WINDOWS_SYSTEM_PREFS_H */
