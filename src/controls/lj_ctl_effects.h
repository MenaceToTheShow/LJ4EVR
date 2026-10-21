/* lj_ctl_effects.h - the Effects window (design 6.4; [07 s9.3]; 15/01 s8; effectcontrol4.gif
 * 246x70) and the LJ_W_EFFECT_BTN row of the controls-c engine: a 27x27 black
 * button carrying the effect's tile, pressed = effect in (buttons[0].value),
 * released = out (row->lo). */
#ifndef LJ_CONTROLS_CTL_EFFECTS_H
#define LJ_CONTROLS_CTL_EFFECTS_H

#include "controls/lj_ctl_colors.h"

lj_window *lj_ctl_effects_create(const lj_profile *p);
void       lj_ctl_effects_refresh(lj_window *win);
void       lj_ctl_effects_destroy(lj_window *win);

int  lj_ctl_c_effectbtn_build(lj_ctl_c *c, const lj_ctl_row *r, int x, int y, int *right);   /* returns the line height (33); *right = x after the button */
int  lj_ctl_c_effects_event(lj_ctl_c_bind *b, int ev);
int  lj_ctl_c_effects_refresh(lj_ctl_c_bind *b);

#endif /* LJ_CONTROLS_CTL_EFFECTS_H */
