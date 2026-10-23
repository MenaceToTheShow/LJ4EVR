/* lj_remote_form.h - W5 window family REMOTE (docs/w5-contracts.md seam 9), owned by the remote track. lj_windows.c dispatches
 * its ids here; the track replaces the stub bodies of lj_remote_form.c and adds what it needs below the contract block.
 *   LJ_WIN_REMOTE_SETUP  TRS232SettingsForm (LJ_RS232.exe): the remote settings (Setup > Hardware Setup TSDA_RS232 page button, PROPOSAL entry) [05 s10.1] */
#ifndef LJ_WINDOWS_REMOTE_FORM_H
#define LJ_WINDOWS_REMOTE_FORM_H

#include "windows/lj_windows.h"

lj_window *lj_remote_form_open(enum lj_win id);      /* creates (bindings, forms.ini position, lj_win_show) or raises; NULL + log */
void       lj_remote_form_close(enum lj_win id);     /* saves the position, destroys; no-op when closed */
lj_window *lj_remote_form_get(enum lj_win id);       /* NULL when closed */
void       lj_remote_form_refresh(enum lj_win id);   /* repaint from the module state / engine view copy; no-op when closed */
void       lj_remote_form_bind_fns(void);            /* lj_fn rows and lj_desktop actions this family answers; called from lj_win_set_host */

/* ---- remote track (below this line) ---- */

#endif /* LJ_WINDOWS_REMOTE_FORM_H */
