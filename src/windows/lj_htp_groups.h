/* lj_htp_groups.h - HTP Groups (code-built THTPGroupsForm, 02 s7.1, 11 P8, design 3.5 S5): the 24 groups,
 * their names (GroupInfo.dat "htp n name=", lj_htp_names) and their channel sets (lj_patch_ch.htp_mask in
 * show->patch). LJ2 edits membership per channel in the Global Patch "HTP Groups" column; this window is the
 * per-group view of the same masks: add / remove the selected fixtures' intensity channels (intensity class or
 * Extended-GM, the channels S3 masters), clear, rename.
 * Every change is written under lj_lock and posts LJ_CMD_HTP_GROUPS (b = "Enable HTP group master intensity
 * control"). Also binds the hotkey rows "HTP group n - Bump" / "- Toggle" (LJ_CMD_HTP_GROUP). No DFM, no GIF:
 * the whole layout is PROPOSAL rig. */
#ifndef LJ_WINDOWS_HTP_GROUPS_H
#define LJ_WINDOWS_HTP_GROUPS_H

#include "windows/lj_windows.h"

lj_window *lj_htp_groups_open(void);
void       lj_htp_groups_close(void);
lj_window *lj_htp_groups_window(void);
void       lj_htp_groups_refresh(void);

int  lj_htp_groups_assign(int group, int on);            /* group 1..24: the selected fixtures' intensity-class channels join (on) / leave it; the channel count changed, -1 */
int  lj_htp_groups_clear(int group);                     /* every channel leaves group 1..24; 0 / -1 */
int  lj_htp_groups_rename(int group, const char *name);  /* ctx->htp_names + GroupInfo.dat; 0 / -1 */
int  lj_htp_groups_count(int group);                     /* channels in group 1..24 */
void lj_htp_groups_bind_fns(void);                       /* LJ_FN_HTP_BUMP_FLASH_n / LJ_FN_HTP_TOGGLE_n */

#endif /* LJ_WINDOWS_HTP_GROUPS_H */
