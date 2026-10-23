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
#include "io/lj_actions.h"

lj_window *lj_fingers_open(enum lj_win id);      /* creates (bindings, forms.ini position, lj_win_show) or raises; NULL + log */
void       lj_fingers_close(enum lj_win id);     /* saves the position, destroys; no-op when closed */
lj_window *lj_fingers_get(enum lj_win id);       /* NULL when closed */
void       lj_fingers_refresh(enum lj_win id);   /* repaint from the model / engine view copy (seam 3); no-op when closed */
void       lj_fingers_bind_fns(void);            /* lj_fn rows and lj_desktop actions this family answers; called from lj_win_set_host */


/* ---- da2532-fingers track (W4, D16, I9, notes 04 s17, 05 s5): the Fingers model behind the windows ----
 * Virtual Fingers is our own drawing of the surface (no skin bitmap, no Martin art): F1-F10, SHIFT 1-3, MODE 1-3, the
 * 3 x 12 cue slot buttons, 12 faders + bump buttons, GM + B/O, the square (status) button, the scroll buttons, pages A-D and
 * 5 jog wheels, Arial Black (arialblack18, F3). Transport = keyboard / MIDI through the LJ_FNF_EXT rows (Alt+click on a
 * control arms MIDI Learn); RS-232 never opens (Fingers Setup: Serial connection greyed). Settings: <library>\Fingers.Ini
 * (LJ2's file name; our keys, PROPOSAL rig 05 s5). Fader values are percent 0..100 (the LJ_FN_MASTER_LEVEL convention). */
enum { LJ_FG_SHIFTS = 4, LJ_FG_PAGES = 4, LJ_FG_FKEYS = 10, LJ_FG_SLOTS = 12, LJ_FG_ROWS = 3, LJ_FG_FADERS = 12, LJ_FG_JOGS = 5 };
enum lj_fg_mode { LJ_FG_DISABLED = 0, LJ_FG_VIRTUAL, LJ_FG_SERIAL /* greyed: never selected */ };
int  lj_fingers_fkey(int key, int down);                  /* F1..F10 at the current shift: 0 ran, -1 empty */
int  lj_fingers_slot(int row, int slot, int down);        /* row 1..3 (top, middle, bottom), slot 1..12: that row's cue slot function on slot n */
int  lj_fingers_fader(int fader, int pct);                /* 1..12, 0..100: the current page's fader page function */
int  lj_fingers_bump(int fader, int down);
int  lj_fingers_gm(int pct);
int  lj_fingers_bo(void);
int  lj_fingers_jog(int jog, int delta);                  /* 1..5, signed steps: the jog wheel function */
int  lj_fingers_fader_value(int fader);                   /* 0..100, -1 out of range; 0 = GM */
int  lj_fingers_set_shift(int shift);                     /* 0..3 */
int  lj_fingers_set_page(int page);                       /* 1..4 (A..D) */
int  lj_fingers_assign_fkey(int shift, int key, int type /* enum lj_da2532_type */, uint32_t num);   /* -1 out of range / invalid */
int  lj_fingers_set_row_function(int row, int index);     /* 1..3, an index into lj_fingers_slot_functions */
int  lj_fingers_set_page_function(int page, int index);   /* 1..4, an index into lj_fingers_page_functions */
int  lj_fingers_set_jog_function(int jog, int index);     /* 1..5, an index into lj_fingers_jog_functions */
extern const char *const lj_fingers_slot_functions[];     /* the help's catalogues verbatim (05 s5.3), NULL-terminated */
extern const char *const lj_fingers_page_functions[];
extern const char *const lj_fingers_jog_functions[];
extern const char *const lj_fingers_ud_functions[];
int  lj_fingers_mode(void);                               /* enum lj_fg_mode from Fingers.Ini */
int  lj_fingers_learn(enum lj_fn fn);                     /* arms MIDI Learn for an EXT row (a CC for the level rows) */
int  lj_fingers_save(void);                               /* <library>\Fingers.Ini; 0 / -1 */
int  lj_fingers_load(void);                               /* missing = defaults; -1 = a present file refused */

#endif /* LJ_WINDOWS_FINGERS_H */
