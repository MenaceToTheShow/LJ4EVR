/* lj_debug_form.h - W4 window family DEBUG (docs/w4-contracts.md section 3), owned by the lightcorder-debug track. lj_windows.c
 * dispatches every id below through these four functions (the family table), so the track never edits lj_windows.c;
 * it replaces the stub bodies of lj_debug_form.c and adds what it needs below the contract block.
 *   LJ_WIN_DEBUG  TDebugForm: View > De&bugWindow, hidden (LJ_ACT_DEBUG_WINDOW; unhide method PROPOSAL) [13 s1.2, design 13 item 10]
 *   LJ_WIN_STATICS_DEBUG  TStaticsDebugForm: Debug window Statics debug [13 s1.2]
 *   LJ_WIN_MSG_DEBUG  TMsgDebugForm: Debug window WM_CPOPYDATA DEBUG [13 s1.2]
 *   LJ_WIN_MEMORY_VIEW  TMemoryViewForm: View > System Memory (ViewControlClick Tag 7, LJ_ACT_WINDOWS_MEMORY) [01 s4]
 */
#ifndef LJ_WINDOWS_DEBUG_FORM_H
#define LJ_WINDOWS_DEBUG_FORM_H

#include "windows/lj_windows.h"

lj_window *lj_debug_form_open(enum lj_win id);      /* creates (bindings, forms.ini position, lj_win_show) or raises; NULL + log */
void       lj_debug_form_close(enum lj_win id);     /* saves the position, destroys; no-op when closed */
lj_window *lj_debug_form_get(enum lj_win id);       /* NULL when closed */
void       lj_debug_form_refresh(enum lj_win id);   /* repaint from the model / engine view copy (seam 3); no-op when closed */
void       lj_debug_form_bind_fns(void);            /* lj_fn rows and lj_desktop actions this family answers; called from lj_win_set_host */

#endif /* LJ_WINDOWS_DEBUG_FORM_H */
