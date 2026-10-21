/* lj_ctl_beam.h - the Beam window (design 6.4; [07 s9.4]; 15/01 s7; beam.gif 206x77) and
 * the LJ_W_DRAGBAR row of the controls-c engine: a 21-px line of [icon] bar [icon],
 * the icons being our glyphs named by row->items (irisopen / irisclosed, far / near,
 * zoomwide / zoomnarrow). */
#ifndef LJ_CONTROLS_CTL_BEAM_H
#define LJ_CONTROLS_CTL_BEAM_H

#include "controls/lj_ctl_colors.h"

lj_window *lj_ctl_beam_create(const lj_profile *p);
void       lj_ctl_beam_refresh(lj_window *win);
void       lj_ctl_beam_destroy(lj_window *win);

int  lj_ctl_c_dragbar_build(lj_ctl_c *c, const lj_ctl_row *r, int x, int y);    /* returns the line height (21) */
int  lj_ctl_c_beam_event(lj_ctl_c_bind *b, int ev);
int  lj_ctl_c_beam_refresh(lj_ctl_c_bind *b);

#endif /* LJ_CONTROLS_CTL_BEAM_H */
