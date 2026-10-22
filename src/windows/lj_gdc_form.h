/* lj_gdc_form.h - Generic DMX Control, Follow spot settings, Sequence fades (F35, 04 s1.4, s8.4, s13). W3 contract (docs/w3-contracts.md); owner: the gdc-fades-followspot track.
 * One open / close / get / refresh set per window id (the lj_win_table rows in lj_windows.c are filled already, so the
 * track never edits lj_windows.c); refresh paints from ctx->view / the model copy (seam 3 of lj_windows.h). Every OnXxx
 * the DFM declares is bound (ME-16). */
#ifndef LJ_WINDOWS_GDC_FORM_H
#define LJ_WINDOWS_GDC_FORM_H

#include "ui/lj_window.h"

/* LJ_WIN_GDC_FORM "gdc_form": TGenericMegaForm "Generic DMX Control" (+ TMegaGridSizeForm "Set Grid Size" 181x76, TMegaGridColorSchemeForm "Grid Color Scheme" 144x173, TMegaGroupCreateForm "Create Channel Group" 206x84); caption notes the hidden-channel count */
lj_window *lj_gdc_form_open(void);
void       lj_gdc_form_close(void);
lj_window *lj_gdc_form_get(void);
void       lj_gdc_form_refresh(void);

/* LJ_WIN_FOLLOWSPOT_FORM "followspot_form": TFollowSpotForm "Edit FollowSpot Settings" 432x398: 20 rows On/Off, name, static; Apply Current Scene to N, Clear Settings in N, Save */
lj_window *lj_followspot_form_open(void);
void       lj_followspot_form_close(void);
lj_window *lj_followspot_form_get(void);
void       lj_followspot_form_refresh(void);

/* LJ_WIN_FOLLOWSPOT_LIST "followspot_list": TFollowListForm "Select Followspot Setting": OK / double-click activates; Options > Edit Settings */
lj_window *lj_followspot_list_open(void);
void       lj_followspot_list_close(void);
lj_window *lj_followspot_list_get(void);
void       lj_followspot_list_refresh(void);

/* LJ_WIN_SEQ_FADES "seq_fades": TDefault2Form DFM caption "Set Fade Status" 239x226: Intensity .. Movement Macro, All Scenes, RG1 "Fade Status" nine transforms (the Fades menu of every control; W2 had parked this class on LJ_WIN_DEFAULT_OUTPUT, which has no window) */
lj_window *lj_seq_fades_open(void);
void       lj_seq_fades_close(void);
lj_window *lj_seq_fades_get(void);
void       lj_seq_fades_refresh(void);

void lj_gdc_form_bind_fns(void);   /* the lj_fn rows this file owns (hotkeys of 01 s7), called from lj_win_set_host */

#endif /* LJ_WINDOWS_GDC_FORM_H */
