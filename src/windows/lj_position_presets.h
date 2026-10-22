/* lj_position_presets.h - Position Presets (TPresetForm, 04 s4.3, F36): 99 pan/tilt presets per fixture
 * (show->preset), references in the scene (scene->preset). The dialog, store / recall / clear / delete and the
 * Movement control's Presets menu and Apply / Clear buttons live in controls/lj_ctl_movement.c since W1 (they
 * need the Movement control's selection set and pan/tilt offsets); this file is the LJ_WIN_POSITION_PRESETS
 * row and the windows-level entry points over them: the window opens through the Movement control (opened for
 * the master fixture when closed), and the hotkeys "Assign Preset# 1..10" / "Toggle Position Preset" bind here.
 * The Save Preset dialog (TSavePresetForm, LJ_WIN_SAVE_PRESET) is not used: Save records into the highlighted
 * or first free number directly (controls track behaviour, W1). */
#ifndef LJ_WINDOWS_POSITION_PRESETS_H
#define LJ_WINDOWS_POSITION_PRESETS_H

#include "windows/lj_windows.h"

lj_window *lj_position_presets_open(void);      /* NULL when the master fixture has no Movement control (no master, no pan/tilt) */
void       lj_position_presets_close(void);
lj_window *lj_position_presets_window(void);
void       lj_position_presets_refresh(void);

int  lj_position_presets_store(int n, int filter);   /* preset 1..99 from the current scene: filter 0 master, 1 selected, 2 all fixtures; 0 / -1 */
int  lj_position_presets_recall(int n);               /* the selection (master's profile) takes preset n: pan/tilt values + the scene reference; 0 / -1 */
void lj_position_presets_bind_fns(void);              /* LJ_FN_ASSIGN_PRESET_1..10, LJ_FN_TOGGLE_POSITION_PRESET */

#endif /* LJ_WINDOWS_POSITION_PRESETS_H */
