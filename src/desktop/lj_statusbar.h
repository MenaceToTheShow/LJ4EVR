/* lj_statusbar.h - the 17-px status bar of the main window ([07 s4.8], [01 s2.4], F10):
 * sunken panels clock | Running.. | memory % bar | Master 100% | Seq. Modified |
 * Cue Modified | CueList Modified | Trig. Public API: lj_desktop_status_set / _flag in
 * lj_desktop.h. Widget name "sb_status" (panels are not widgets; docs/ui-script.md's
 * sb_master etc. resolve through lj_statusbar_panel_text). */
#ifndef LJ_DESKTOP_STATUSBAR_H
#define LJ_DESKTOP_STATUSBAR_H

#include "desktop/lj_desktop.h"

enum { LJ_SB_H = 17 };   /* statusbar.gif 400x17 (1:1), rig item */

void        lj_statusbar_build(lj_widget *root);       /* alBottom under root */
void        lj_statusbar_refresh(void);                /* the 100 ms hook: clock, Master %, flags from lj_ctx */
void        lj_statusbar_show(int show);
int         lj_statusbar_visible(void);
const char *lj_statusbar_panel_text(enum lj_status_field f);   /* what the panel shows ("" when blank) */

#endif /* LJ_DESKTOP_STATUSBAR_H */
