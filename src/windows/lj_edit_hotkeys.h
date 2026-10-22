/* lj_edit_hotkeys.h - Setup > Hotkeys > Edit Hotkeys (TFunctionsForm "Edit Hotkeys", [01 s7], F54):
 * the Function list (grouped, Function / SCRL / Key Combo) with Assign Function / Clear Function(s),
 * the program tabs Sequence (Flash) | Cue | Cuelist | BG Cue (records on disk) with Assign <kind> /
 * Clear Hotkey(s), the Key Combo field (a key pressed while the dialog is open is captured, Alt
 * never: "Normal keyboard operation is suspended when the Hotkey Dialog Is Open"), Scroll Lock
 * Activated, Find Hotkey, Clear Hotkey; Options > Disable Hotkeys in Dialogs / Save / Load / Default /
 * Clear All Hotkeys; Accept Hotkeys copies the edited setup into ctx->hotkeys and writes
 * <library>Hotkeys\SetupN.Hot for the current setup (ctx->prefs->hotkey_setup, 1..8). A combo that
 * already belongs to another function or program is refused with LJ_HOTKEY_CONFLICT_FMT. */
#ifndef LJ_WINDOWS_EDIT_HOTKEYS_H
#define LJ_WINDOWS_EDIT_HOTKEYS_H

#include <stdint.h>
#include "io/lj_hotkeys.h"
#include "windows/lj_windows.h"

#define LJ_HOTKEY_CONFLICT_FMT "Key Combo %s is already assigned to %s"   /* PROPOSAL rig: LJ2's own refusal text is not in the exe pool (13 s2.2) */
#define LJ_HOTKEY_CHANGED      "Hotkeys were changed, close anyway ?"     /* exe 0x1C2BC0 */

lj_window *lj_edit_hotkeys_open(void);              /* LJ_WIN_EDIT_HOTKEYS: the current setup into the working copy */
void       lj_edit_hotkeys_close(void);             /* discards the working copy */
lj_window *lj_edit_hotkeys_window(void);
void       lj_edit_hotkeys_refresh(void);           /* the pending close */

int  lj_edit_hotkeys_setup(int setup);              /* edit setup 1..8 instead (loads <library>Hotkeys\SetupN.Hot); -1 out of range */
int  lj_edit_hotkeys_capture(int key, int shift, int ctrl, int alt);   /* the Key Combo field; -1 for Alt combos and bare modifiers (ignored, as LJ2) */
int  lj_edit_hotkeys_select_fn(enum lj_fn fn);      /* the Function list row of fn; -1 when fn is not listed */
int  lj_edit_hotkeys_select_prog(int tab /* 0 seq 1 cue 2 cl 3 bg */, uint32_t number);   /* switches tab, selects the record row; -1 when absent */
int  lj_edit_hotkeys_assign_fn(void);               /* Assign Function: 0, -1 (nothing captured / no row / conflict: the box is shown) */
int  lj_edit_hotkeys_assign_prog(void);             /* Assign Sequence / Cue / CueList / BGCue: same results */
const lj_hotkeys *lj_edit_hotkeys_work(void);       /* the working copy (NULL when closed) */
int  lj_edit_hotkeys_accept(void);                  /* Accept Hotkeys: ctx->hotkeys = work, SetupN.Hot written; 0 / -1 */

#endif /* LJ_WINDOWS_EDIT_HOTKEYS_H */
