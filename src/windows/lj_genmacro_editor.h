/* lj_genmacro_editor.h - The Generic Macro Editor and its dialogs (14 sA.7-A.8). W3 contract (docs/w3-contracts.md); owner: the genmacro track.
 * One open / close / get / refresh set per window id (the lj_win_table rows in lj_windows.c are filled already, so the
 * track never edits lj_windows.c); refresh paints from ctx->view / the model copy (seam 3 of lj_windows.h). Every OnXxx
 * the DFM declares is bound (ME-16). */
#ifndef LJ_WINDOWS_GENMACRO_EDITOR_H
#define LJ_WINDOWS_GENMACRO_EDITOR_H

#include <stddef.h>
#include "ui/lj_window.h"

/* LJ_WIN_GENMACRO_EDITOR "genmacro_editor": TCueMacroControlForm DFM caption "Generic 8 bit Macro Editor", runtime "Generic Macro Editor [...]" (F19); 564x478 design, min 564x400, #B1B1B1, stay-on-top; Main.Ini [MACRO] Top Left */
lj_window *lj_genmacro_editor_open(void);
void       lj_genmacro_editor_close(void);
lj_window *lj_genmacro_editor_get(void);
void       lj_genmacro_editor_refresh(void);

/* LJ_WIN_SAVE_GENMACRO "save_genmacro": TSaveMacroDialog "Save Macro" 326x120, red bands (+ TSaveAsMacroDialog "Save as Macro#" 220x70; "Macro #%d", "Error : Maximum number of Macros exceeded") */
lj_window *lj_save_genmacro_open(void);
void       lj_save_genmacro_close(void);
lj_window *lj_save_genmacro_get(void);
void       lj_save_genmacro_refresh(void);

/* LJ_WIN_SELECT_GENMACRO "select_genmacro": TLoadMacroList "Select Macro": No | Macro Name, popup Edit Macro / Add Macro to Cue / Delete Macro File (Delete Generic Macros stays LJ_WIN_DELETE_MACRO, W2) */
lj_window *lj_select_genmacro_open(void);
void       lj_select_genmacro_close(void);
lj_window *lj_select_genmacro_get(void);
void       lj_select_genmacro_refresh(void);

int  lj_genmacro_editor_caption(char *dst, size_t cap);   /* "Generic Macro Editor [0001 name]" / "Generic Macro Editor []" (F19); 0 / -1 truncated */
void lj_genmacro_editor_bind_fns(void);   /* the lj_fn rows this file owns (hotkeys of 01 s7), called from lj_win_set_host */

#endif /* LJ_WINDOWS_GENMACRO_EDITOR_H */
