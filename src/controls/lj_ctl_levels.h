/* lj_ctl_levels.h - the Levels control (note 07 s9.7, levelcontrol.gif 272x250, design
 * 6.3): one 32-px column per enabled level: caption, vertical fader, O/S/F. */
#ifndef LJ_CONTROLS_CTL_LEVELS_H
#define LJ_CONTROLS_CTL_LEVELS_H

#include "controls/lj_ctl_common.h"

enum { LJ_CTL_LEVELS_COL_W = 32 };   /* measured from levelcontrol.gif: tracks at x 12-36, 44-68, ... pitch 32 */

int lj_ctl_levels_row(lj_ctl_win *c, lj_widget *parent, const lj_ctl_row *row, int x, int y);   /* LJ_W_LEVEL_FADER: one column at x; returns the column height */

#endif /* LJ_CONTROLS_CTL_LEVELS_H */
