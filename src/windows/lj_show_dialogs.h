/* lj_show_dialogs.h - W4 window family SHOW (docs/w4-contracts.md section 3), owned by the dlg-show track. lj_windows.c
 * dispatches every id below through these four functions (the family table), so the track never edits lj_windows.c;
 * it replaces the stub bodies of lj_show_dialogs.c and adds what it needs below the contract block.
 *   LJ_WIN_CUE_STACK  TCueStackForm: Cue control menu Cue Builder, hotkey Cue Builder [03 s5.8]
 *   LJ_WIN_MODAL_SELECT_CUE  TModalSelectCueForm: Cuelist edit panel Set Current Cue / cue pick [03 s6]
 *   LJ_WIN_DELETE_SCENE  TDeleteSceneForm: Sequence window Delete (LJ_SB_DELETE) [03 s4.3]
 *   LJ_WIN_INSERT_SCENES  TInsertScenesForm: Sequence window Insert [03 s4.3]
 *   LJ_WIN_SET_SEQUENCE  TSetSequenceForm: Sequence window Start Sequence at : [03 s4]
 *   LJ_WIN_SEQ_USED  TSeqUsedForm: Delete Sequences Check Used Sequences / Mark Unused [01 s8]
 *   LJ_WIN_SELECT_BGCUE_OLD  TSelectBGCueForm: BG slot Edit Sequence / cue-list Load BG Cue picker [03 s7]
 *   LJ_WIN_SAVE_STATIC  TSaveStaticForm: Statics Control Save [03 s8, 14 sB.2]
 *   LJ_WIN_STATICS_CONTROL  TStaticsControlForm: View > Controls Statics Control [03 s8, 14 sB.2]
 *   LJ_WIN_STATIC_SLOT_NAME  TStaticSlotNameForm: Statics 2 slot popup Name [14 sB.3]
 *   LJ_WIN_STATICS_REPLACE  TStaticsReplaceForm: Statics 2 Replace matrix [14 sB.3]
 *   LJ_WIN_NAME_SLOTS  TNameSlotsForm: Cue control Sequence Slot Labels [03 s5]
 *   LJ_WIN_SELECT_CUELIST_MODAL  TSelectCuelist2ModalForm: Startup Options Select Startup Cuelist [01 s6.2]
 *   LJ_WIN_SORT_TIMECODE  TSortTimeCodeForm: Cuelist Tools Sort Waits (ME-120) [03 s6]
 *   LJ_WIN_TIMECODE_OFFSET  TTimeCodeOffsetForm: Cuelist Tools Offset TimeCode (ME-120) [03 s6]
 *   LJ_WIN_MOVEMENT_OPTIONS  TMovementOptionsForm: Movement window Options > Position Options [04 s4]
 *   LJ_WIN_SAVE_PRESET  (existing id)
 */
#ifndef LJ_WINDOWS_SHOW_DIALOGS_H
#define LJ_WINDOWS_SHOW_DIALOGS_H

#include "model/lj_cuelist.h"
#include "windows/lj_windows.h"

lj_window *lj_show_dlg_open(enum lj_win id);      /* creates (bindings, forms.ini position, lj_win_show) or raises; NULL + log */
void       lj_show_dlg_close(enum lj_win id);     /* saves the position, destroys; no-op when closed */
lj_window *lj_show_dlg_get(enum lj_win id);       /* NULL when closed */
void       lj_show_dlg_refresh(enum lj_win id);   /* repaint from the model / engine view copy (seam 3); no-op when closed */
void       lj_show_dlg_bind_fns(void);            /* lj_fn rows and lj_desktop actions this family answers; called from lj_win_set_host */

/* ---- dlg-show (below the contract block). Entry points: the Sequence window's Insert / Delete (lj_seq_form.c), the Cue control's Cue-Builder,
 * Sequence Slot Labels and Select BG Cue (lj_cue_form.c), the Cuelist window's Offset / Sort items and Set Current Cue (lj_cuelist_form.c),
 * Statics 2 Record / Link (lj_statics2_form.c). Callers outside this track: Startup Options (select_cuelist), the Global Patch (set_sequence),
 * Delete Sequences > Check Used (seq_used), the Movement control's Options > Position Options (lj_win_open(LJ_WIN_MOVEMENT_OPTIONS)). */
void    lj_show_dlg_select_cue(void (*done)(uint32_t number, void *ud), void *ud);      /* TModalSelectCueForm "Select Cue": OK hands the highlighted cue to done */
void    lj_show_dlg_select_bgcue(void (*done)(uint32_t number, void *ud), void *ud);    /* TSelectBGCueForm "Select Background Cue" */
void    lj_show_dlg_select_cuelist(void (*done)(uint32_t number, void *ud), void *ud);  /* TSelectCuelist2ModalForm "Select Startup Cuelist" (the Startup Options button) */
void    lj_show_dlg_set_sequence(int channels, void (*done)(int start, void *ud), void *ud);   /* TSetSequenceForm: "Channels to Patch : n", OK hands "Start Sequence at :" (1..2048) */
void    lj_show_dlg_seq_used(int cues_done, int cues_total, int bg_done, int bg_total);  /* TSeqUsedForm "Checking, please wait..": opens on the first call, the caller closes it (lj_win_close) */
void    lj_show_dlg_timecode_offset(int family /* LJ_CLF_MTC or LJ_CLF_PC */);           /* TTimeCodeOffsetForm on the current cue list (ME-120) */
int32_t lj_show_dlg_offset_ms(int h, int m, int s, int f, int fps);                     /* signed H:M:S:F -> ms (frames as lj_cl_hmsf_ms packs them) */
int     lj_show_dlg_offset(lj_cuelist *cl, int family, int32_t delta_ms, int apply, int *nlines);   /* Wait MTC / Wait PC-Clock lines + delta: the over/underflow count (clamped when apply); -1 bad family */
int     lj_show_dlg_sort_waits(lj_cuelist *cl, int cmd /* LJ_CL_WAIT_MTC */);           /* ascending, the block of that wait must be contiguous; 0 / -1 with the exe's message */
void    lj_show_dlg_static_slot_name(int page, int slot);                               /* TStaticSlotNameForm on Statics 2 page / slot (1-based) */
int     lj_show_dlg_name_slot(int slot, const char *name);                              /* Sequence Slot Labels: slot 1..12's text in the open dialog (tests); 0 / -1 */
int     lj_show_dlg_insert_pick(uint32_t number);                                       /* Insert Scene(s) > Select seq with that sequence (tests); 0 / -1 */
int     lj_show_dlg_capture_seq(uint32_t seq);                                          /* Cue Builder Seq Capture: 1 = taken into the preset (the caller does not load) */
int     lj_show_dlg_capture_cue(uint32_t cue);                                          /* Cue Builder Cue Capture: 1 = taken into the stack */
int     lj_show_dlg_cue_stack_add(uint32_t cue);                                        /* the stack rule: one non-transparent cue on top, a transparent cue once; 0 / -1 */
int     lj_show_dlg_cue_builder_go(int stack);                                          /* GO: 0 = the preset into the current cue, 1 = the stack through the cue loader; 0 / -1 */

#endif /* LJ_WINDOWS_SHOW_DIALOGS_H */
