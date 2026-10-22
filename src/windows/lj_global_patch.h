/* lj_global_patch.h - Global Patch (TGlobalPatchForm "Global Patch", 02 s7.1, design 3.5 S1/S5/S6):
 * the 2048-row list (Adr / Fixture / Ch descr. / Value / DMXOut / DMXIn / In mode/Sw / HTP Groups / Inv /
 * Min / Max), the Modify Output / Output Source / DMX in Mode / HTP Groups panel, File > New / Load / Save
 * Global Patch (.GPA, model/lj_patch.h), the Options clears, Preferences > "Enable HTP group master
 * intensity control". Edits go to a working copy; "Apply Patch" copies it into show->patch under lj_lock,
 * posts LJ_CMD_GLOBAL_PATCH (+ LJ_CMD_HTP_GROUPS when a mask changed) and writes <library>\GlobalPatch.GPA.
 * Closing with unapplied edits asks "Global Patch was changed / Apply changes ?" (exe 0x1BD7D8). */
#ifndef LJ_WINDOWS_GLOBAL_PATCH_H
#define LJ_WINDOWS_GLOBAL_PATCH_H

#include "windows/lj_windows.h"

lj_window *lj_global_patch_open(void);
void       lj_global_patch_close(void);
lj_window *lj_global_patch_window(void);
void       lj_global_patch_refresh(void);            /* Timer1 (200 ms): Value column from the engine frame, panel from the selected row */

int  lj_global_patch_select(int ch);                  /* address code 0..2047 -> the row and the edit panel; 0 / -1 */
const lj_patch_ch *lj_global_patch_work(void);        /* the working copy (NULL when closed) */
const char *lj_global_patch_set_values(void);         /* "<- Set Values": the panel into the selected row; NULL = done, else the rejection (also in a message box) */
int  lj_global_patch_menu(const char *item);          /* a File / Options / Preferences / popup item by DFM name (the UI script and tests); 0 / -1 unknown */
int  lj_global_patch_apply(void);                     /* "Apply Patch"; 0 / -1 when closed */
int  lj_global_patch_htp_masters(void);               /* Preferences > Enable HTP group master intensity control (Main.Ini [GLOBALPATCH] HTPMasters, PROPOSAL key) */
void lj_global_patch_set_post(void (*post)(const lj_cmd *cmd));   /* test seam: where Apply posts (NULL = lj_ctx_post) */

#endif /* LJ_WINDOWS_GLOBAL_PATCH_H */
