/* lj_cue_time.h - Cue time control (TCueTimeForm "Time Control", [03 s5.6], forms.ini
 * [CUETIMEFORM] Top Expanded). The DFM carries only Bevel1, the focus-grabber edit and the
 * menus; the 13 virtual faders (fader 0 = the CUE fader, 1..12 = the slots) are code-built
 * over Bevel1. Fader 0 is cue->cue_time_pct 1..999 (100 = as programmed); faders 1..12 are
 * cue->slot[i].time_pct, 0 = "Cue" (follows the CUE fader), 1..999 = locked. A change writes
 * the record under lj_lock and posts LJ_CMD_CUE_TIME (a = fader, b = pct); the window caption
 * is the cue's "[0000 No name]" (cuetime.gif). Up/Down arrows step the selected faders by 1,
 * Page Up/Down by 10 (KeyPreview; PROPOSAL rig, the step sizes are not in the help). */
#ifndef LJ_WINDOWS_CUE_TIME_H
#define LJ_WINDOWS_CUE_TIME_H

#include <stddef.h>
#include "engine/lj_engine.h"
#include "ui/lj_window.h"

#define LJ_CUE_TIME_FADERS 13

lj_window *lj_cue_time_open(void);
void       lj_cue_time_close(void);
lj_window *lj_cue_time_get(void);
void       lj_cue_time_refresh(void);                          /* repaints when the cue record changed */
int        lj_cue_time_value(int fader);                       /* 0..999 from the working cue; -1 out of range / no cue */
int        lj_cue_time_set(int fader, int pct);                /* clamps (fader 0: 1..999, slots 0..999), writes, posts; with "Synchronize Selected Sequences" every selected slot fader follows; 0 / -1 */
int        lj_cue_time_text(int fader, char *dst, size_t cap); /* the value box: "Cue" at the default (slot 0, cue fader 100), else "240%" */
void       lj_cue_time_select(int fader, int on);              /* the header click: selection for the arrow keys / synchronize */
int        lj_cue_time_key(int vk);                            /* the KeyPreview arrows (VK_UP 0x26, VK_DOWN 0x28, VK_PRIOR 0x21, VK_NEXT 0x22): 1 consumed */
int        lj_cue_time_pos_of(int pct, int track_h);           /* track pixel from the bottom (0 .. track_h-1) for a pct; PROPOSAL piecewise scale */
int        lj_cue_time_pct_at(int pos, int track_h);           /* inverse */
int        lj_cue_time_last_cmd(lj_cmd *out);                  /* commands posted since open (tests); *out = the last */
void       lj_cue_time_bind_fns(void);                         /* LJ_FN_CUE_TIME_CONTROL */

#endif /* LJ_WINDOWS_CUE_TIME_H */
