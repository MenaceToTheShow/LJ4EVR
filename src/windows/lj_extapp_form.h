/* lj_extapp_form.h - W5 window family EXTAPP (docs/w5-contracts.md seam 9), owned by the extapp track. lj_windows.c dispatches
 * its ids here; the track replaces the stub bodies of lj_extapp_form.c and adds what it needs below the contract block.
 *   LJ_WIN_EXTAPP_SETUP  TExternalAppSetupForm: icon popup Define Application Link (PI_DefineAppLink) on a profile-90 fixture [05 s10.2, I11] */
#ifndef LJ_WINDOWS_EXTAPP_FORM_H
#define LJ_WINDOWS_EXTAPP_FORM_H

#include "windows/lj_windows.h"

lj_window *lj_extapp_form_open(enum lj_win id);      /* creates (bindings, forms.ini position, lj_win_show) or raises; NULL + log */
void       lj_extapp_form_close(enum lj_win id);     /* saves the position, destroys; no-op when closed */
lj_window *lj_extapp_form_get(enum lj_win id);       /* NULL when closed */
void       lj_extapp_form_refresh(enum lj_win id);   /* repaint from the module state / engine view copy; no-op when closed */
void       lj_extapp_form_bind_fns(void);            /* lj_fn rows and lj_desktop actions this family answers; called from lj_win_set_host */
void       lj_extapp_form_target(int fixture);       /* LJ_ACT_EXTAPP_SETUP arg: the profile-90 fixture whose link the window edits (the lj_config_dlg_target pattern) */

/* ---- extapp track (below this line) ---- */

#endif /* LJ_WINDOWS_EXTAPP_FORM_H */
