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

#include "windows/lj_windows.h"

lj_window *lj_show_dlg_open(enum lj_win id);      /* creates (bindings, forms.ini position, lj_win_show) or raises; NULL + log */
void       lj_show_dlg_close(enum lj_win id);     /* saves the position, destroys; no-op when closed */
lj_window *lj_show_dlg_get(enum lj_win id);       /* NULL when closed */
void       lj_show_dlg_refresh(enum lj_win id);   /* repaint from the model / engine view copy (seam 3); no-op when closed */
void       lj_show_dlg_bind_fns(void);            /* lj_fn rows and lj_desktop actions this family answers; called from lj_win_set_host */

#endif /* LJ_WINDOWS_SHOW_DIALOGS_H */
