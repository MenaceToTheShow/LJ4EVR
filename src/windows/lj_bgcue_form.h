/* lj_bgcue_form.h - the Background Cue window (TBGCueForm "Background Cue" 384x124, [03 s7], design 3.4 rank 5).
 * Five slot rows in LV1 (Seq No | Sequence Name | Scene | Mode) with the numbered on/off buttons over sb1..sb5
 * (green = on, red depressed = off, bgcuecontrol.gif), the BG cue drop-down cb1 and New / Save / Load / Delete.
 * The window never loads a record itself (design 2.3 rules 2, 6): picking a BG cue runs LJ_ACT_LOAD_BGCUE (the app
 * pins bg + bgseq[5] and posts LJ_CMD_LOAD_BG, as it services a cue's latched bg_cue through pending_bg); New/Clear
 * runs LJ_ACT_LOAD_BGCUE 0 (PROPOSAL: the app maps 0 to lj_app_bgcue_clear); a slot button posts LJ_CMD_BG_ON.
 * Rows, scene counters and the toolbar LED follow ctx->view (bg_number, bg_status, bg_scene, bg_enabled, bg_any). */
#ifndef LJ_WINDOWS_BGCUE_FORM_H
#define LJ_WINDOWS_BGCUE_FORM_H

#include <stddef.h>
#include "engine/lj_engine.h"
#include "ui/lj_window.h"

lj_window *lj_bgcue_form_open(void);
void       lj_bgcue_form_close(void);
lj_window *lj_bgcue_form_get(void);
void       lj_bgcue_form_refresh(void);                     /* the 100 ms hook: rows, buttons and the BG LED from ctx->view */
void       lj_bgcue_form_bind_fns(void);                    /* Toggle BGCue control, Clear BGCue, Toggle BG Cue List, BGCue Toggle slot 1-5, All Slots Off/On */
void       lj_bgcue_form_toggle_slot(int slot);             /* 1..5: LJ_CMD_BG_ON with the inverse of the view's bg_enabled */
int        lj_bgcue_form_row(int slot, char *dst, size_t cap);   /* 1..5: the LV1 row as shown, sub-items separated by '\t'; -1 out of range */
lj_widget *lj_bgcue_form_slot_button(int slot);             /* 1..5, NULL when closed */
int        lj_bgcue_form_last_cmd(lj_cmd *out);             /* the last command posted (tests); 0 = none */
const lj_bgcue *lj_bgcue_form_work(void);                  /* the record the window edits (LJ_ACT_BGCUE_APPLY hands it to the app); number 0 = unsaved */
lj_window *lj_save_bg_open(void);                           /* Save Background Cue (TSaveBGForm) */
void       lj_save_bg_close(void);
lj_window *lj_save_bg_get(void);

#endif /* LJ_WINDOWS_BGCUE_FORM_H */
