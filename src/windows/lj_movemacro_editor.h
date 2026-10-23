/* lj_movemacro_editor.h - Movement macros, Bezier shapes, Fan Out, Auto Delay, Cue macro control (04 s4.2-4.6, 03 s5.11, 17). W3 contract (docs/w3-contracts.md); owner: the movement track.
 * One open / close / get / refresh set per window id (the lj_win_table rows in lj_windows.c are filled already, so the
 * track never edits lj_windows.c); refresh paints from ctx->view / the model copy (seam 3 of lj_windows.h). Every OnXxx
 * the DFM declares is bound (ME-16). */
#ifndef LJ_WINDOWS_MOVEMACRO_EDITOR_H
#define LJ_WINDOWS_MOVEMACRO_EDITOR_H

#include "model/lj_movemacro.h"
#include "ui/lj_window.h"

/* LJ_WIN_MOVEMACRO_EDITOR "movemacro_editor": TMovementMacroForm code-built: no DFM; movementmacro.gif 447x243 and beziershapeeditor.gif 400x145 are reduced captures (not in 07 s14) -> every size PROPOSAL rig; caption "Movement Macro", menu Fades Preferences Auto delay "Bezier Shapes" Help; + TBezNameForm (256x66, the shape name) */
lj_window *lj_movemacro_editor_open(void);
void       lj_movemacro_editor_close(void);
lj_window *lj_movemacro_editor_get(void);
void       lj_movemacro_editor_refresh(void);

/* LJ_WIN_DELETE_MOVEMACRO "delete_movemacro": TDeleteMacrosForm "Delete Macros" 189x400 tool window: the Bezier shapes (.bez) check-list All / None / OK / Cancel (14 sA.8 candidate; the w2-contracts DELETE row) */
lj_window *lj_delete_movemacro_open(void);
void       lj_delete_movemacro_close(void);
lj_window *lj_delete_movemacro_get(void);
void       lj_delete_movemacro_refresh(void);

/* LJ_WIN_FAN_OUT "fan_out": TFanOutForm "Fan Out" [04 s4.2, 17 s5] (+ TCustomOrderForm "Custom Fixture Selection Order", TSaveSelectionForm "Save Custom Selection" 197x74) */
lj_window *lj_fan_out_open(void);
void       lj_fan_out_close(void);
lj_window *lj_fan_out_get(void);
void       lj_fan_out_refresh(void);

/* LJ_WIN_AUTO_DELAY "auto_delay": TAutoDelayForm "Pan/Tilt Macro Auto Delay" 391x201 [04 s4.6, 17 s6]; the only DFM without a Font (VCL default) */
lj_window *lj_auto_delay_open(void);
void       lj_auto_delay_close(void);
lj_window *lj_auto_delay_get(void);
void       lj_auto_delay_refresh(void);

/* LJ_WIN_CUE_MACRO_CONTROL "cue_macro_control": TCueMacroForm "Cue Macro Control" 172x256 [03 s5.11]: amplitude / speed faders, Fade out / Fade in with the time list, Macros Off / On, Freeze */
lj_window *lj_cue_macro_control_open(void);
void       lj_cue_macro_control_close(void);
lj_window *lj_cue_macro_control_get(void);
void       lj_cue_macro_control_refresh(void);

/* ---- W3 movement (appended): what the buttons do, for the tests and the UI script ---- */
int        lj_fan_out_apply(void);                         /* the Delta bars / Center Delta on Home onto the current scene in the Apply Values by order; -1 under 2 fixtures */
int        lj_fan_out_custom_move(int from, int to);       /* the Custom order list: drag row from to row to */
int        lj_auto_delay_apply(void);                      /* Apply: the style / max / count / segments into the selected fixtures' macro delay */
int        lj_auto_delay_set(int max_delay, int count, int segments);
int        lj_cue_macro_control_set(int amp_pct, int speed_pct);   /* the two faders: MACRO_CTL (and the cue's values when it keeps them) */
int        lj_movemacro_editor_save_shape(const char *name);       /* Save Shape with that name into <library>\MacroShapes, then LJ_ACT_BEZ_RELOAD */
lj_window *lj_movemacro_editor_name_window(void);          /* the open TBezNameForm, NULL when none */
int        lj_movemacro_editor_bez(lj_bez *out);           /* the shape on the Bezier editor; -1 when it is closed */
int  lj_position_options_apply(int op /* 0 Mirror Pan, 1 Mirror Tilt, 2 Swap Pan/Tilt */);   /* W4: the selected fixtures in the current scene (17 s4.1); 0 / -1 */
void lj_movemacro_editor_bind_fns(void);   /* the lj_fn rows this file owns (hotkeys of 01 s7), called from lj_win_set_host */

#endif /* LJ_WINDOWS_MOVEMACRO_EDITOR_H */
