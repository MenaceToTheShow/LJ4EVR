/* lj_ctl_gobos.h - the Gobos window (design 6.4; [07 s9.2]; 15/01 s6; gobocontrol.gif,
 * goboindex.gif) and the gobo-specific rows of the controls-c engine: the
 * Index | Rotate mode pair (rotationmode.gif), the index bar
 * (goboindexcontrol.gif) and the shake button + speed bar (goboshake.gif).
 * Conventions: lj_ctl_colors.h header comment (LORD_FIXTURES-10). */
#ifndef LJ_CONTROLS_CTL_GOBOS_H
#define LJ_CONTROLS_CTL_GOBOS_H

#include "controls/lj_ctl_colors.h"

lj_window *lj_ctl_gobos_create(const lj_profile *p);
void       lj_ctl_gobos_refresh(lj_window *win);
void       lj_ctl_gobos_destroy(lj_window *win);

int  lj_ctl_c_modepair_build(lj_ctl_c *c, const lj_ctl_row *r, int x, int y);   /* two flat buttons, 65x20 each; returns the line height */
int  lj_ctl_c_indexbar_build(lj_ctl_c *c, const lj_ctl_row *r, int x, int y);   /* 133x20 bar; returns the line height */
int  lj_ctl_c_shake_build(lj_ctl_c *c, const lj_ctl_row *r, int x, int y);      /* shake glyph button + bar in a raised panel; returns 33 */
int  lj_ctl_c_gobos_event(lj_ctl_c_bind *b, int ev);                             /* 1 when the bind's role belongs to this file */
int  lj_ctl_c_gobos_refresh(lj_ctl_c_bind *b);                                   /* 1 when handled */

#endif /* LJ_CONTROLS_CTL_GOBOS_H */
