/* lj_master_intensity.h - the Master Intensity window (TMasterIntensityForm,
 * design 7.7, [07 s10], [11 s5]): nine vertical faders built in code over the
 * generated Panel1 (Grand Master + 8 sub-masters with their names and a mode
 * popup each), the generated right panel (All 100% / All 0%, Fade Out / Fade In
 * with the Fadetime list, BlackOut / Restore). Levels are 0..255 (LJ_GM_DOMAIN)
 * and reach the engine as LJ_CMD_SET_MASTER / GM_FADE / BLACKOUT through
 * lj_ctx_post; the faders follow the app's engine view copy (seam 3). Position
 * and prefs: Main.Ini [GLOBALINTENSITY] (Show Pct, MasterOnly, Fadetime, SyncBump). */
#ifndef LJ_WINDOWS_MASTER_INTENSITY_H
#define LJ_WINDOWS_MASTER_INTENSITY_H

#include <stdint.h>
#include "engine/lj_engine.h"
#include "ui/lj_window.h"

lj_window *lj_master_intensity_open(void);
void       lj_master_intensity_close(void);
lj_window *lj_master_intensity_get(void);
void       lj_master_intensity_refresh(void);                /* the 100 ms hook: faders from the view (gm, sub[]), Fade In enable, BlackOut latch */
void       lj_master_intensity_set_view(const lj_engine_status *view);   /* the app's copy; NULL = zeros */
void       lj_master_intensity_set(int which /* 0 = GM, 1..8 */, int level /* 0..255 */);   /* posts SET_MASTER (with the sub-master's mode) and moves the fader */
int        lj_master_intensity_level(int which);              /* the last level this file posted or saw in the view */
void       lj_master_intensity_set_mode(int sub /* 1..8 */, int mode /* enum lj_master_mode */);   /* show->submaster_mode under lj_lock + SET_MASTER */
lj_widget *lj_master_intensity_fader(int which);              /* the lj_w_fader widget, NULL when closed */
const char *const *lj_master_intensity_mode_items(int *n);    /* the mode popup, verbatim exe strings: "Master Proportional" "Master LoTP" "Master HTP" "Master no Effect" */
int        lj_master_intensity_last_cmd(lj_cmd *out);         /* the last command this file posted (tests); 0 = none yet */
void       lj_master_intensity_bind_fns(void);                /* lj_fn_bind for the intensity hotkey rows */

#endif /* LJ_WINDOWS_MASTER_INTENSITY_H */
