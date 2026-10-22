/* lj_snapshot.h - Snapshot from DMX Out (TSnapshotSceneForm, 03 s4.7): Static Snapshot writes the current DMX
 * output into the current scene (or a new scene with "Auto add scene to sequence"), Real-time recording samples
 * 1..20 scenes/s for 2..999 scenes and converts them into the current sequence ("Recording finished / Convert
 * to sequence ?"). Filters: Record Selected Fixtures Only, values different from default only, channels
 * patched from DMX in only, as Snap / as Fade (app/lj_app.h LJ_SNAP_* bits). The frame comes from the engine:
 * LJ_CMD_SNAPSHOT_REQ with a window-owned lj_frame, served when ctx->view.snapshot_tick echoes its tick_no.
 * Main.Ini [MISC] SNAPSHOTSETTINGS (filter bits) RTSNAPSHOTPARAM (rate index) RTSNAPSHOTSCENES (count). */
#ifndef LJ_WINDOWS_SNAPSHOT_H
#define LJ_WINDOWS_SNAPSHOT_H

#include <stdint.h>
#include "windows/lj_windows.h"

enum { LJ_SNAPF_SELECTED_ONLY = 1, LJ_SNAPF_NON_DEFAULT_ONLY = 2, LJ_SNAPF_DMXIN_ONLY = 4, LJ_SNAPF_AS_FADE = 8 };   /* == app/lj_app.h LJ_SNAP_* (windows/ cannot include app/) */

lj_window *lj_snapshot_open(void);
void       lj_snapshot_close(void);
lj_window *lj_snapshot_window(void);
void       lj_snapshot_refresh(void);                    /* lj_snapshot_tick(lj_clock_us()) */

int  lj_snapshot_fold(lj_scene *dst, const lj_frame *f, const lj_show *show, unsigned filters);   /* the frame's out[] into dst per the filters, channels of configured fixtures only; the count written */
int  lj_snapshot_static(void);                           /* "Static Snapshot": posts the request; folded by the next tick that sees it served; 0 / -1 */
int  lj_snapshot_realtime(void);                         /* "Record Realtime" (or Cancel while recording); 0 / -1 with the exe's message when fewer than 2 scenes */
void lj_snapshot_tick(uint64_t now_us);                  /* folds a served frame, posts the next real-time sample when due; the app may call it every idle pass (the 100 ms window refresh caps sampling at 10/s) */
int  lj_snapshot_recording(void);                        /* scenes recorded so far, -1 when not recording */
unsigned lj_snapshot_filters(void);                      /* the check boxes as LJ_SNAPF_* bits */

#endif /* LJ_WINDOWS_SNAPSHOT_H */
