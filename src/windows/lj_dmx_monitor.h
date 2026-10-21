/* lj_dmx_monitor.h - DMX Output Monitor (TDMXOUTFORM 356x264 + the 10-per-row grid,
 * design 8.2, [01 s5], [05 s2.5], F49, ME-28): row headers 1, 11, 21 ..., columns
 * +0..+9; cell grey = used by a configured fixture, white = unused, green = the
 * master fixture's channels; text black = unchanged since open, red = changed,
 * green = overwritten. 100 ms refresh from a copy of lj_engine_frame() (seam 3).
 * Status line = LJ2's control function of the highlighted channel (ME-28); the
 * layer_of[] readout only behind Main.Ini [DEBUG] MonitorLayer=1. Preferences
 * (Highlight.., Autoscroll, Enable Value overwrite), overwrite = LJ_CMD_OVERWRITE
 * (CLR = 256, clear-all on close), Tools > Find Channel = the binary search. */
#ifndef LJ_WINDOWS_DMX_MONITOR_H
#define LJ_WINDOWS_DMX_MONITOR_H

#include <stddef.h>
#include <stdint.h>
#include "windows/lj_windows.h"

lj_window *lj_dmx_monitor_open(void);
void       lj_dmx_monitor_close(void);                     /* posts the clear-all overwrite first */
lj_window *lj_dmx_monitor_window(void);
void       lj_dmx_monitor_refresh(void);                   /* the 100 ms hook: frame copy under lj_lock, repaint, autoscroll, drains pending overwrites */
int        lj_dmx_monitor_select(int code);                /* highlight an address code 0..2047 (the icon popup "DMX Output" aligns here); 0 / -1 */
int        lj_dmx_monitor_selected(void);                  /* -1 none */
const char *lj_dmx_monitor_status(void);                   /* the status line text */
int        lj_dmx_monitor_overwrite(int code, int value);  /* value 0..255, 256 = clear; queued as LJ_CMD_OVERWRITE; -1 when overwrite is not enabled */
int        lj_dmx_monitor_overwrite_enabled(void);
void       lj_dmx_monitor_set_overwrite_enabled(int on);

/* the colour table, pure (the test row) */
enum { LJ_DM_PREF_CHANGES = 1, LJ_DM_PREF_USED = 2, LJ_DM_PREF_MASTER = 4, LJ_DM_PREF_AUTOSCROLL = 8 };
void lj_dmx_monitor_cell_colour(int used, int master, int changed, int overwritten, unsigned prefs, uint32_t *bg, uint32_t *fg);

/* Find Channel: binary search over the included channels in <= 11 tries [05 s2.5] */
typedef struct { uint16_t cand[LJ_CHANS]; int n, lo, hi; uint8_t on, off; int steps; } lj_dmx_search;
int  lj_dmx_search_begin(lj_dmx_search *s, const uint8_t include_link[LJ_LINKS], int exclude_used, int on, int off);   /* fills the candidates from the show; -1 when none */
int  lj_dmx_search_half(const lj_dmx_search *s, int *first, int *last);          /* the candidate span set to "on" this step: cand[lo..mid) as 1-based channel numbers; -1 when done */
int  lj_dmx_search_answer(lj_dmx_search *s, int yes);                            /* narrows; returns the found address code when one is left, else -1 */
int  lj_dmx_search_apply(const lj_dmx_search *s);                                /* queues the on/off pattern as overwrites (monitor open); 0 / -1 */

#endif /* LJ_WINDOWS_DMX_MONITOR_H */
