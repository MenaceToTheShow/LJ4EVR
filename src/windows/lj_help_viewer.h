/* lj_help_viewer.h - W4 window family HELP (docs/w4-contracts.md section 3), owned by the docs-release track. lj_windows.c
 * dispatches every id below through these four functions (the family table), so the track never edits lj_windows.c;
 * it replaces the stub bodies of lj_help_viewer.c and adds what it needs below the contract block.
 *   LJ_WIN_HELP  THelpViewer: every Help leaf (LJ_ACT_HELP, F1): the bundled text viewer, code-built (design 13 item 12)
 */
#ifndef LJ_WINDOWS_HELP_VIEWER_H
#define LJ_WINDOWS_HELP_VIEWER_H

#include "windows/lj_windows.h"

lj_window *lj_help_viewer_open(enum lj_win id);      /* creates (bindings, forms.ini position, lj_win_show) or raises; NULL + log */
void       lj_help_viewer_close(enum lj_win id);     /* saves the position, destroys; no-op when closed */
lj_window *lj_help_viewer_get(enum lj_win id);       /* NULL when closed */
void       lj_help_viewer_refresh(enum lj_win id);   /* repaint from the model / engine view copy (seam 3); no-op when closed */
void       lj_help_viewer_bind_fns(void);            /* lj_fn rows and lj_desktop actions this family answers; called from lj_win_set_host */
int        lj_help_viewer_topic(const char *topic); /* opens the viewer at a topic file name (help/<topic>.txt); NULL / "" = the index; 0 / -1 */

#endif /* LJ_WINDOWS_HELP_VIEWER_H */
