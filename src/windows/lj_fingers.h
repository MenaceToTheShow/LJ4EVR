/* lj_fingers.h - W4 window family FINGERS (docs/w4-contracts.md section 3), owned by the da2532-fingers track. lj_windows.c
 * dispatches every id below through these four functions (the family table), so the track never edits lj_windows.c;
 * it replaces the stub bodies of lj_fingers.c and adds what it needs below the contract block.
 *   LJ_WIN_FINGERS  TLJFingersMainForm: &Fingers > Virtual Fingers (LJ_FN_VIRTUAL_FINGERS) [04 s17, 05 s5, D16]
 *   LJ_WIN_FINGERS_SETUP  TFingersSetupDialog: &Fingers > Fingers Setup (LJ_ACT_FINGERS_SETUP) [05 s5]
 *   LJ_WIN_FINGERS_STATUS  TFingersStatus: &Fingers > Fingers status panel (LJ_FN_FINGERS_STATUS_PANEL) [05 s5]
 *   LJ_WIN_FINGERS_ERROR  TFingersErrorForm: Fingers error box [05 s5]
 *   LJ_WIN_FINGERS_SKIN  TFingersSkinSelectForm: Virtual Fingers Select Skin [05 s5]
 *   LJ_WIN_FINGERS_FUNCTIONS  TFingersFunctionListForm: Fingers Setup F1-F10 function list [05 s5]
 *   LJ_WIN_FINGERS_JOG_FUNCTIONS  TFingersJogFunctionListForm: Fingers Setup Datawheel function list [05 s5]
 *   LJ_WIN_FINGERS_PAGE_FUNCTIONS  TFingersPageFunctionListForm: Fingers Setup fader page function list [05 s5]
 *   LJ_WIN_FINGERS_SLOT_FUNCTIONS  TFingersSlotFunctionListForm: Fingers Setup Cue Slot functions [05 s5]
 *   LJ_WIN_FINGERS_UD_FUNCTIONS  TFingersUDFunctionListForm: Fingers Setup Scroll button function list [05 s5]
 */
#ifndef LJ_WINDOWS_FINGERS_H
#define LJ_WINDOWS_FINGERS_H

#include "windows/lj_windows.h"

lj_window *lj_fingers_open(enum lj_win id);      /* creates (bindings, forms.ini position, lj_win_show) or raises; NULL + log */
void       lj_fingers_close(enum lj_win id);     /* saves the position, destroys; no-op when closed */
lj_window *lj_fingers_get(enum lj_win id);       /* NULL when closed */
void       lj_fingers_refresh(enum lj_win id);   /* repaint from the model / engine view copy (seam 3); no-op when closed */
void       lj_fingers_bind_fns(void);            /* lj_fn rows and lj_desktop actions this family answers; called from lj_win_set_host */

#endif /* LJ_WINDOWS_FINGERS_H */
