/* lj_ctl_extapp.h - the Extended control of the External Application/Device profile (built-in 90, tag EXTERNAL):
 * TSpecialSettings_ExternalApp, code-built (no DFM, no CHM capture; W5-EXT-1, note 13 exe strings 0x24EAAC-0x24F6A4):
 * group "ZString Interface", label "External Application Command", the command edit (<= 32 characters, 05 s10.2),
 * "Set Command" (writes the edit into the current scene's string for the master fixture, an empty edit removes it) and
 * "Get Command" (reads it back), and the link line "Link: Active" / "Link:-" / "Application Link Undefined" (exe
 * 0x24F6A4, 0x24F158, 0x3328BC) from io/lj_extapp.h. Opened by Fixture toolbar button 7 like every Extended window. */
#ifndef LJ_CONTROLS_CTL_EXTAPP_H
#define LJ_CONTROLS_CTL_EXTAPP_H

#include "controls/lj_ctl_common.h"

void lj_ctl_extapp_build(lj_ctl_win *c);     /* lj_ctl_open_table's builder for LJ_CTL_EXTENDED on profile 90 (instead of the row renderer) */
void lj_ctl_extapp_repaint(lj_ctl_win *c);   /* the link line; the edit follows the scene only on Get Command (typing is never overwritten) */
int  lj_ctl_extapp_set(int fixture, const char *cmd);   /* the Set Command write: fixture 0-based, cmd "" removes; 0, -1 (no scene, refused string, 4 strings already) */
const char *lj_ctl_extapp_get(int fixture);             /* the current scene's string for fixture 0-based, "" when none (a static copy) */

#endif /* LJ_CONTROLS_CTL_EXTAPP_H */
