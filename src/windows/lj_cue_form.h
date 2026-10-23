/* lj_cue_form.h - the Cue control (TCurrentCue, design 7.7, [03 s5], [07 s6]),
 * the Save Cue dialog (TSaveCueForm + TSaveAsCueForm, [03 s5.7]) and the List
 * of Cues (TAdvancedCueForm "Select Cue", [03 s5.8]). The grid (12 slot rows,
 * numbered enable buttons, header at the forms.ini [CUE] C0..C8 widths) is built
 * in code over the generated frame; every DFM handler is bound (ME-16).
 *
 * Seams: the window edits ctx->show->current_cue under lj_lock and posts
 * LJ_CMD_GO/STOP/TRIG/ENABLE/TIME/CHASE through lj_ctx_post (seam 3); loads
 * are the app's (design 2.3 rule 2) and reach it through lj_cue_form_host,
 * since windows/ may not include app/. The engine view it paints from is the
 * app's last lj_engine_view copy (lj_cue_form_set_view). */
#ifndef LJ_WINDOWS_CUE_FORM_H
#define LJ_WINDOWS_CUE_FORM_H

#include <stddef.h>
#include <stdint.h>
#include "engine/lj_engine.h"
#include "ui/lj_window.h"

/* LORD_FIDELITY-13 PROPOSAL (rig row): 1 = the "EDIT CUE " prefix shows while the running
 * cue was loaded with Edit Cue (view.cue_edit) OR any slot is in Edit status [03 s5.1];
 * 0 = cue_edit only. */
#define LJ_CUE_EDIT_PREFIX_RULE 1

typedef struct {
    void *ud;
    int  (*load_cue)(void *ud, uint32_t number, int edit);   /* lj_app_load_cue: file -> tables -> LJ_CMD_LOAD_CUE with pinned pointers */
    void (*repin)(void *ud);                                 /* the current cue's slot options changed: post LOAD_CUE again with the current pins (NULL = the engine reads the pinned record on its own) */
    void (*show)(void *ud, lj_window *w, int open);          /* the platform open (1) / close (0) of a window this file built (lj_win_host wraps it); NULL = headless */
} lj_cue_form_host;
void lj_cue_form_set_host(const lj_cue_form_host *host);     /* once at startup; NULL = headless (tests) */
const lj_cue_form_host *lj_cue_form_get_host(void);        /* NULL when headless; lj_master_intensity.c shows its window through it */
void lj_cue_form_set_view(const lj_engine_status *view);     /* the app's copy (seam 3); NULL = all zero */

/* Cue control */
lj_window *lj_cue_form_open(void);                           /* builds the window (headless: not shown); NULL + log */
void       lj_cue_form_close(void);
lj_window *lj_cue_form_get(void);
void       lj_cue_form_refresh(void);                        /* the 100 ms hook: rows from the view + the cue record, caption, button states */
void       lj_cue_form_repin(void);                          /* the host's repin (a sequence record was saved into the tables); no-op headless */
int        lj_cue_form_add_sequence(uint32_t seq_no);        /* "Add Sequence to Current Cue": first free slot; returns 1..12 or -1 when full */
int        lj_cue_form_set_slot(int slot, int on);               /* ME-168: slot 1..12 enabled = on in the cue record, LJ_CMD_ENABLE posted (the remote 135 set form); -1 no cue / out of range */
void       lj_cue_form_new_cue(void);                        /* clears every slot of the current cue record (number 0, no name) */
int        lj_cue_form_save(int mode /* 0 Save, 1 Save New, 2 Save as number */, uint32_t number);   /* writes Cues\NNNNNNNN.cue, copies into show->cues, clears Cue Modified; 0 / -1 */
int        lj_cue_form_caption(char *dst, size_t cap);       /* "CUE [0001 name]" / "EDIT CUE [...]" (F19) */
void       lj_cue_form_set_cue_time(uint16_t pct);           /* the cue fader 1..999, 100 = as programmed; LJ_CMD_TIME a = 0 */
void       lj_cue_form_select(int slot /* 0..11, -1 = all */, int on);
int        lj_cue_form_selected(int slot);
const int *lj_cue_form_col_widths(void);                     /* the 9 list columns No..Trig (forms.ini [CUE] C0..C8, defaults 30 160 115 45 38 38 45 45 40) */
int        lj_cue_form_cell(int slot, int col, char *dst, size_t cap);   /* the text the grid shows (after a refresh); -1 out of range */
uint32_t   lj_cue_form_row_rgb(int status /* LJ_SLOT_* */, int scenes, int chase_manual, int link, int use_colors);   /* [07 s3.5]: black/maroon/red/grey/blue/green */
int        lj_cue_form_last_cmd(lj_cmd *out);                /* the last command this file posted (tests); 0 = none yet */
void       lj_cue_form_bind_fns(void);                       /* lj_fn_bind for the cue-control hotkey rows (io/lj_actions.h) */

/* Save Cue dialog */
lj_window *lj_save_cue_open(void);
void       lj_save_cue_close(void);
lj_window *lj_save_cue_get(void);

/* List of Cues ("Select Cue") */
lj_window *lj_list_cue_open(void);
void       lj_list_cue_close(void);
lj_window *lj_list_cue_get(void);
void       lj_list_cue_refresh(void);                        /* re-reads the library's Cues folder into lvGlobal */

/* In-client popup list shared by the slot popup and the Master Intensity mode popup
 * (a hand-built lj_menu_desc cannot fire: the menu runtime binds only the form's own
 * menus). Items are caller-owned; pick(i) fires on a left click inside, any other
 * click or Escape closes it. */
void lj_cue_popup_open(lj_window *win, const char *const *items, int n, int x, int y, void (*pick)(int i, void *ud), void *ud);
int  lj_cue_popup_is_open(lj_window *win);

/* ---- W2 windows-a (appended) ---- */
void     lj_cue_form_apply_prefs(void);                      /* forms.ini [CUE] Transparent / StickyFlash -> ctx->transparent_mode / flash_latch, posts LJ_CMD_TRANSPARENT */
uint32_t lj_list_cue_selected(void);                         /* the cue highlighted in the global list, 0 = none */

/* ---- W4 dlg-show (appended) ---- */
int      lj_cue_form_add_sequence_at(uint32_t seq_no, int slot);   /* slot 1..12 asked for (0 = first free): a taken slot asks "No priority slots available - add sequence as
                                                                    * non-priority sequence ?" (Yes = first free); a full cue says "No free sequence slots in cue - sequence
                                                                    * not added to cue" [12 Q9]; the cue grid's drop of a sequence; slot / 0 asked / -1 */

#endif /* LJ_WINDOWS_CUE_FORM_H */
