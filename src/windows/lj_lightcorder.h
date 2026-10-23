/* lj_lightcorder.h - W4 window family LIGHTCORDER (docs/w4-contracts.md section 3), owned by the lightcorder-debug track. lj_windows.c
 * dispatches every id below through these four functions (the family table), so the track never edits lj_windows.c;
 * it replaces the stub bodies of lj_lightcorder.c and adds what it needs below the contract block.
 *   LJ_WIN_LIGHTCORDER  TLightCorderDialog: Cuelist > Tools Record to LightCorder file (M_RecordtoLightCorderClick) [13 s1.2]
 *   LJ_WIN_SAVE_LIGHTCORDER  TSaveLightCorderFileDialog: Record to LightCorder file Save [13 s1.2]
 */
#ifndef LJ_WINDOWS_LIGHTCORDER_H
#define LJ_WINDOWS_LIGHTCORDER_H

#include <stdint.h>
#include "windows/lj_windows.h"

lj_window *lj_lightcorder_open(enum lj_win id);      /* creates (bindings, forms.ini position, lj_win_show) or raises; NULL + log */
void       lj_lightcorder_close(enum lj_win id);     /* saves the position, destroys; no-op when closed */
lj_window *lj_lightcorder_get(enum lj_win id);       /* NULL when closed */
void       lj_lightcorder_refresh(enum lj_win id);   /* repaint from the model / engine view copy (seam 3); no-op when closed */
void       lj_lightcorder_bind_fns(void);            /* lj_fn rows and lj_desktop actions this family answers; called from lj_win_set_host */
void       lj_lightcorder_step(uint64_t now_us);    /* every UI loop pass (lj_app_w4_idle): copies the frame under lj_lock at the chosen rate and writes on this thread; the engine does no I/O (ME-2, ME-24) */

/* ---- lightcorder-debug track additions (below the contract block) ---- */
#include <stddef.h>
int         lj_lightcorder_recording(void);                   /* 1 while a take runs */
uint32_t    lj_lightcorder_frames(void);                      /* frames in the current take (0 when none) */
const char *lj_lightcorder_last_file(void);                   /* the .LLF the last Save File wrote, "" none */
int         lj_lightcorder_max_time_text(char *dst, size_t cap, int card_mb, int channels, int hz);   /* lMaxTime: "Max recording time 3:24 (Min:Sec)" / the Hour:Min:Sec form (exe 0x1FF0DC / 0x1FF0A8) */
int         lj_lightcorder_sys_notes(char *dst, size_t cap);  /* the "LightJockey (system) Notes" text of the current take */

#endif /* LJ_WINDOWS_LIGHTCORDER_H */
