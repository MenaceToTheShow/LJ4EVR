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
#include "io/lj_actions.h"

lj_window *lj_da2532_open(enum lj_win id);      /* creates (bindings, forms.ini position, lj_win_show) or raises; NULL + log */
void       lj_da2532_close(enum lj_win id);     /* saves the position, destroys; no-op when closed */
lj_window *lj_da2532_get(enum lj_win id);       /* NULL when closed */
void       lj_da2532_refresh(enum lj_win id);   /* repaint from the model / engine view copy (seam 3); no-op when closed */
void       lj_da2532_bind_fns(void);            /* lj_fn rows and lj_desktop actions this family answers; called from lj_win_set_host */


/* ---- da2532-fingers track (W4, D16, I8, notes 05 s4): the 2532 Direct Access model behind the window ----
 * 18 pages x 7 banks (A..G) x 8 keys, each key a latch value and a flash value, plus 18 user keys (functions only, page
 * independent). A latch value is a cue, cue list, BG cue or hotkey function (a page change is the EXT row
 * LJ_FN_DA2532_PAGE_n); a flash value is a sequence (flashed while held) or a function. Keys are driven by the mouse
 * (click = latch, Ctrl+click or the middle button = flash, the Latch/Flash toggle), the keyboard (.Hot lines naming the
 * EXT rows) and MIDI (MIDIMap.dat fn lines; Alt+click on a key arms Learn for its row). The serial 2532 is never opened.
 * The setup lives in <library>\2532Setup.Ini (ours, docs/file-formats.md row returned to the integrator); the window's
 * preferences in Main.Ini [2532] under the menu item names. */
enum lj_da2532_type { LJ_DA_NONE = 0, LJ_DA_FN /* hotkey function, num = enum lj_fn */, LJ_DA_SEQ, LJ_DA_CUE, LJ_DA_CL, LJ_DA_BG, LJ_DA_TYPES };   /* the Color Scheme rows: Functions Sequences Cues Cuelists BG Cues */
enum { LJ_DA_PAGES = 18, LJ_DA_BANKS = 7, LJ_DA_KEYS = 8, LJ_DA_USER = 18, LJ_DA_TEXT = 21 /* Edit1 MaxLength 20 */ };
typedef struct { uint8_t type; uint32_t num; char text[LJ_DA_TEXT]; } lj_da_val;   /* text = Customize Text, "" = the default label */

int  lj_da2532_assign(int page, int bank, int key, int flash, int type, uint32_t num);   /* 1-based page / bank / key; flash 0 = latch value; -1 out of range or a type the half does not take */
int  lj_da2532_assign_user(int key, uint32_t fn);                                      /* user key 1..18 = a function (LJ_FN_NONE clears); -1 out of range */
const lj_da_val *lj_da2532_value(int page, int bank, int key, int flash);              /* NULL out of range */
int  lj_da2532_latch(int key, int down);                                               /* latch key 1..8 of the active bank / page: 0 ran (or recorded), -1 empty or out of range */
int  lj_da2532_flash(int key, int down);
int  lj_da2532_user(int key, int down);
int  lj_da2532_set_page(int page);                                                     /* 1..18; -1 out of range */
int  lj_da2532_page(void);
int  lj_da2532_set_bank(int bank);                                                     /* 1..7 (A..G) */
int  lj_da2532_bank(void);
int  lj_da2532_set_record(int type);                                                   /* LJ_DA_CUE / SEQ / CL / BG arms Record (the next key press stores the current program), LJ_DA_NONE disarms */
void lj_da2532_clear_all(void);                                                        /* Clear all Pages (and user keys) */
int  lj_da2532_save(void);                                                             /* <library>\2532Setup.Ini; 0 / -1 */
int  lj_da2532_load(void);                                                             /* missing file = an empty setup (0); -1 = a present file refused (nothing changed) */
int  lj_da2532_learn(enum lj_fn fn);                                                   /* arms MIDI Learn for an EXT row; the 100 ms refresh binds it. 0 / -1 */
int  lj_da2532_select(int bank, int key);                                               /* the highlighted matrix key the popups act on (1-based); -1 out of range */
int  lj_da2532_edit_text(int user, int flash);                                         /* Customize Text > Edit Text on the selected key (user = 1: the selected user key): opens TEdit2532TextForm; -1 when the key is empty */
int  lj_da2532_function_at(int row);                                                   /* the 2532 Function List row's enum lj_fn (the drag payload), LJ_FN_NONE past the end */
const char *lj_da2532_label(const lj_da_val *v, char *dst, size_t cap);                /* "Cue 12", "Seq 3", the function caption, or the custom text */

#endif /* LJ_WINDOWS_DA2532_H */
