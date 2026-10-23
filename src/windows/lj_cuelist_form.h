/* lj_cuelist_form.h - The Cuelist window and its dialogs (design 10 W3, 03 s6, 8.5). W3 contract (docs/w3-contracts.md); owner: the cuelist-window track.
 * One open / close / get / refresh set per window id (the lj_win_table rows in lj_windows.c are filled already, so the
 * track never edits lj_windows.c); refresh paints from ctx->view / the model copy (seam 3 of lj_windows.h). Every OnXxx
 * the DFM declares is bound (ME-16). */
#ifndef LJ_WINDOWS_CUELIST_FORM_H
#define LJ_WINDOWS_CUELIST_FORM_H

#include <stddef.h>
#include <stdint.h>
#include "model/lj_cuelist.h"
#include "ui/lj_window.h"

/* LJ_WIN_CUELIST_FORM "cuelist_form": TCuelist2Form "Cuelist" 545x506 design (min 545x350), #6B8DCB (Color 13340011); runtime caption "Cuelist [0001 name]" + " Tracking on" (lj_win_caption_cuelist); + TclEditPanels (the edit panel pages), TCuelistLinkOrImportForm (drop of a list: Link / Import), TTimeCodeOffsetForm, TSortTimeCodeForm; [CUELIST2] Width Height ViewEditPanel ViewEditCommand */
lj_window *lj_cuelist_form_open(void);
void       lj_cuelist_form_close(void);
lj_window *lj_cuelist_form_get(void);
void       lj_cuelist_form_refresh(void);

/* LJ_WIN_SAVE_CUELIST "save_cuelist": TSaveCuelist2Form "Save Cuelist" 273x79 (+ TSaveAsCuelistForm "Save as Cuelist#") [03 s6.2] */
lj_window *lj_save_cuelist_open(void);
void       lj_save_cuelist_close(void);
lj_window *lj_save_cuelist_get(void);
void       lj_save_cuelist_refresh(void);

/* LJ_WIN_SELECT_CUELIST "select_cuelist": TSelectCuelist2Form "Select Cuelist": the list of cue lists, double-click / Load Cuelist, Delete Cuelist File [03 s6.2] */
lj_window *lj_select_cuelist_open(void);
void       lj_select_cuelist_close(void);
lj_window *lj_select_cuelist_get(void);
void       lj_select_cuelist_refresh(void);

/* LJ_WIN_CL_SET_CUE_TRIG "cl_set_cue_trig": TClSetCueTrigForm "Set Cue Trig" 169x86: slots 1-12 of the line [03 s6.2] */
lj_window *lj_cl_set_cue_trig_open(void);
void       lj_cl_set_cue_trig_close(void);
lj_window *lj_cl_set_cue_trig_get(void);
void       lj_cl_set_cue_trig_refresh(void);

/* LJ_WIN_LOG_TIMECODE "log_timecode": TLogTimeCodeForm "Log TimeCode" 145x253: Log / Add / Insert / Apply / Start Elapsed / Enable Cue Logging [03 s6.4] */
lj_window *lj_log_timecode_open(void);
void       lj_log_timecode_close(void);
lj_window *lj_log_timecode_get(void);
void       lj_log_timecode_refresh(void);

void lj_cuelist_form_bind_fns(void);   /* the lj_fn rows this file owns (hotkeys of 01 s7), called from lj_win_set_host */

/* ---- cuelist-window track (appended) ---- */

lj_cuelist *lj_cuelist_form_list(void);       /* the edited list: ctx->show->current_cl, a fresh record when there is none; NULL without a show */
void        lj_cuelist_form_new(void);        /* New Cuelist (sbNew, Options > New Cuelist; the app's LJ_ACT_NEW_CUELIST should call this after lj_cl_run_unpin) */
int         lj_cuelist_form_save(int mode /* 0 Save, 1 Save New, 2 Save as number */, uint32_t number);   /* CueLists\NNNNNNNN.Cl2; clears CueList Modified; 0 / -1 */
void        lj_cuelist_form_apply_cmd(int cmd);   /* enum lj_cl_cmd onto the selected line (command panel / ListPop / the recent buttons); inert families refused */
int         lj_cuelist_form_row(uint32_t line, char *dst, size_t cap);   /* the grid text of a 1-based line, '\t' between the six columns; 0 / -1 */
int         lj_cuelist_form_recent(int i);    /* the command on sbCMD<i+1> (enum lj_cl_cmd, 0 = empty) */
const char *lj_cuelist_form_edit_panel(void); /* the TclEditPanels panel shown in pEdit ("pPCTimeout" ...), NULL when none */
int         lj_cuelist_form_tc_src(void);     /* enum lj_cl_tc_src the transport shows (sbMIDITime / sbPCElapsed / sbPCClock) */

/* THotkeyFunctionDialog "Select Function" (LJ_WIN_HOTKEY_FUNCTION; the W2 LOW row): the cue list's Hotkey Command picks
 * a function; Ok writes it into the selected line (param_ms = enum lj_fn) */
lj_window *lj_hotkey_function_open(void);
void       lj_hotkey_function_close(void);
lj_window *lj_hotkey_function_get(void);
void       lj_hotkey_function_refresh(void);

/* W5 media (appended): the picked WAV onto MP Load File line `line` (1-based); load_error follows whether the file exists. 0 / -1 */
int lj_cuelist_form_set_mp_file(uint32_t line, const char *path);

#endif /* LJ_WINDOWS_CUELIST_FORM_H */
