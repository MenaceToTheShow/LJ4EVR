/* lj_da2532.h - W4 window family DA2532 (docs/w4-contracts.md section 3), owned by the da2532-fingers track. lj_windows.c
 * dispatches every id below through these four functions (the family table), so the track never edits lj_windows.c;
 * it replaces the stub bodies of lj_da2532.c and adds what it needs below the contract block.
 *   LJ_WIN_DA2532  TDa2532Form_3: C-L toolbar 2532 (LJ_FN_2532_CONTROL) [05 s4, D16]
 *   LJ_WIN_DA2532_FUNCTIONS  TButtonFunctions2532Form: 2532 Direct Access key function list [05 s4]
 *   LJ_WIN_DA2532_COLORS  TColorScheme2532Form: 2532 Direct Access Color Scheme [05 s4]
 *   LJ_WIN_DA2532_TAB_COLORS  TDA2532_TabColorsForm: 2532 Direct Access Tab Colors [05 s4]
 *   LJ_WIN_DA2532_TAB_HEIGHT  TDa2532TabHeight: 2532 Direct Access Set Tab Size [05 s4]
 *   LJ_WIN_DA2532_TEXT  TEdit2532TextForm: 2532 Direct Access Edit 2532 Text [05 s4]
 *   LJ_WIN_DA2532_PAGE_NAMES  TName2532PagesForm: 2532 Direct Access 2532 Page Names [05 s4]
 */
#ifndef LJ_WINDOWS_DA2532_H
#define LJ_WINDOWS_DA2532_H

#include "windows/lj_windows.h"

lj_window *lj_da2532_open(enum lj_win id);      /* creates (bindings, forms.ini position, lj_win_show) or raises; NULL + log */
void       lj_da2532_close(enum lj_win id);     /* saves the position, destroys; no-op when closed */
lj_window *lj_da2532_get(enum lj_win id);       /* NULL when closed */
void       lj_da2532_refresh(enum lj_win id);   /* repaint from the model / engine view copy (seam 3); no-op when closed */
void       lj_da2532_bind_fns(void);            /* lj_fn rows and lj_desktop actions this family answers; called from lj_win_set_host */

#endif /* LJ_WINDOWS_DA2532_H */
