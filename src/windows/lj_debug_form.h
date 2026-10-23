/* lj_debug_form.h - W4 window family DEBUG (docs/w4-contracts.md section 3), owned by the lightcorder-debug track. lj_windows.c
 * dispatches every id below through these four functions (the family table), so the track never edits lj_windows.c;
 * it replaces the stub bodies of lj_debug_form.c and adds what it needs below the contract block.
 *   LJ_WIN_DEBUG  TDebugForm: View > De&bugWindow, hidden (LJ_ACT_DEBUG_WINDOW; unhide method PROPOSAL) [13 s1.2, design 13 item 10]
 *   LJ_WIN_STATICS_DEBUG  TStaticsDebugForm: Debug window Statics debug [13 s1.2]
 *   LJ_WIN_MSG_DEBUG  TMsgDebugForm: Debug window WM_CPOPYDATA DEBUG [13 s1.2]
 *   LJ_WIN_MEMORY_VIEW  TMemoryViewForm: View > System Memory (ViewControlClick Tag 7, LJ_ACT_WINDOWS_MEMORY) [01 s4]
 */
#ifndef LJ_WINDOWS_DEBUG_FORM_H
#define LJ_WINDOWS_DEBUG_FORM_H

#include "windows/lj_windows.h"

lj_window *lj_debug_form_open(enum lj_win id);      /* creates (bindings, forms.ini position, lj_win_show) or raises; NULL + log */
void       lj_debug_form_close(enum lj_win id);     /* saves the position, destroys; no-op when closed */
lj_window *lj_debug_form_get(enum lj_win id);       /* NULL when closed */
void       lj_debug_form_refresh(enum lj_win id);   /* repaint from the model / engine view copy (seam 3); no-op when closed */
void       lj_debug_form_bind_fns(void);            /* lj_fn rows and lj_desktop actions this family answers; called from lj_win_set_host */

/* ---- lightcorder-debug track additions (below the contract block) ---- */
#include <stdint.h>
const char *lj_debug_form_label(const char *name);           /* TDebugForm label caption by DFM name ("Label53"), "" when closed */
int         lj_debug_form_misc_lines(void);                  /* the Misc tab's layer_of[] readout (ME-28): lines "1:001 1111000000000000", 16 channels each */
const char *lj_debug_form_misc_line(int i);
uint32_t    lj_debug_form_load_tenths(uint32_t avg_tick_us, uint32_t period_us);   /* engine load in tenths of a percent (integer, no float) */
void        lj_debug_form_load_sample(uint64_t at_us, uint64_t tick_no, uint64_t tick_us_sum);   /* W4 LORD_ENGINE-9: one refresh's view pair (tick_no, tick_us_sum) into the ring; TDebugForm's refresh calls it */
uint32_t    lj_debug_form_load_avg_us(uint64_t window_us, uint32_t fallback_us);  /* mean tick time over the ticks between the newest sample and the oldest within window_us of it; fallback_us when no tick passed */
void        lj_debug_form_set_memory(uint64_t avail_bytes, uint64_t total_bytes);  /* System Memory: the app's lj_sysmem_query figures (integrator: call beside lj_desktop_memory_set); until then the rows read n/a */

#endif /* LJ_WINDOWS_DEBUG_FORM_H */
