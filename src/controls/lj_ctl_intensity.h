/* lj_ctl_intensity.h - the Intensity control (design 6.4 mac500 example, note 15/01 s4,
 * note 07 s9.5, intensitycontrol.gif 480x98): the shutter toggle, the strobe and
 * intensity drag bars (common) and the specials block Intensity Pulse / Random
 * Pulse / Random Strobe the Preferences menu shows or hides. */
#ifndef LJ_CONTROLS_CTL_INTENSITY_H
#define LJ_CONTROLS_CTL_INTENSITY_H

#include "controls/lj_ctl_common.h"

int  lj_ctl_intensity_row(lj_ctl_win *c, lj_widget *parent, const lj_ctl_row *row, int x, int y, int w);   /* LJ_W_SHUTTER_TOGGLE, LJ_W_SPECIAL_GROUP; row height */
void lj_ctl_intensity_prefs(lj_ctl_win *c);                                                                  /* Preferences: toggle the specials block and the window width (note 15/01 s4 item 6) */

#endif /* LJ_CONTROLS_CTL_INTENSITY_H */
